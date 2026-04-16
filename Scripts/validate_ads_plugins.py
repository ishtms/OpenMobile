#!/usr/bin/env python3

import argparse
import json
import re
import sys
import zipfile
import xml.etree.ElementTree as ElementTree
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO, Iterable


PROVIDER_SIGNATURES = {
	"OpenMobileAdsAdMob": (
		b"googlemobileads",
		b"play-services-ads",
		b"com/google/android/gms/ads",
		b"usermessagingplatform",
	),
}
ADAPTER_SIGNATURES = {}
ANDROID_NAMESPACE = "http://schemas.android.com/apk/res/android"
ANDROID_MANIFEST_CONTRACTS = {
	"OpenMobileAdsAdMob": {
		"permissions": {
			"android.permission.INTERNET",
			"android.permission.ACCESS_NETWORK_STATE",
			"com.google.android.gms.permission.AD_ID",
		},
		"metadata": {"com.google.android.gms.ads.APPLICATION_ID"},
		"activities": {"com.google.android.gms.ads.AdActivity"},
		"services": {"com.google.android.gms.ads.AdService"},
		"providers": {"com.google.android.gms.ads.MobileAdsInitProvider"},
	},
}
ANDROID_ADAPTER_MANIFEST_CONTRACTS = {}
ANDROID_DEPENDENCY_CONTRACTS = {
	"OpenMobileAdsAdMob": {
		"com.google.android.gms:play-services-ads": "25.4.0",
	},
}
ANDROID_ADAPTER_DEPENDENCY_CONTRACTS = {}
ANDROID_PROVIDER_AUTHORITY_SUFFIXES = {
	"com.google.android.gms.ads.MobileAdsInitProvider": ".mobileadsinitprovider",
}
SCANNABLE_SUFFIXES = {
	".dex",
	".dylib",
	".json",
	".manifest",
	".modules",
	".plist",
	".so",
	".txt",
	".uplugin",
	".upluginmanifest",
	".xml",
}


@dataclass(frozen=True)
class PluginDescriptor:
	name: str
	path: Path
	data: dict

	@classmethod
	def load(cls, path: Path) -> "PluginDescriptor":
		with path.open(encoding="utf-8") as descriptor_file:
			return cls(path.stem, path, json.load(descriptor_file))

	@property
	def dependencies(self) -> set[str]:
		return {
			dependency["Name"]
			for dependency in self.data.get("Plugins", [])
			if dependency.get("Enabled", True)
		}

	@property
	def is_ads_provider(self) -> bool:
		return (
			self.data.get("OpenMobileAdsType") != "MediationAdapter"
			and self.name != "OpenMobileAds"
			and "OpenMobileAds" in self.dependencies
		)

	@property
	def is_ads_adapter(self) -> bool:
		return self.data.get("OpenMobileAdsType") == "MediationAdapter"


@dataclass(frozen=True)
class ResolvedConfiguration:
	plugins: set[str]
	modules: set[str]
	ads_providers: set[str]
	ads_adapters: set[str]


@dataclass(frozen=True)
class ArtifactInventory:
	entries: set[str]
	detected_providers: set[str]
	detected_adapters: set[str]


@dataclass(frozen=True)
class ArtifactExpectation:
	required_providers: set[str] = field(default_factory=set)
	forbidden_providers: set[str] = field(default_factory=set)
	required_adapters: set[str] = field(default_factory=set)
	forbidden_adapters: set[str] = field(default_factory=set)


@dataclass(frozen=True)
class AndroidManifestInventory:
	permissions: set[str]
	metadata: dict[str, tuple[str, ...]]
	activities: dict[str, int]
	services: dict[str, int]
	providers: dict[str, tuple[str, ...]]
	authority_owners: dict[str, tuple[str, ...]]
	package_name: str


@dataclass(frozen=True)
class AndroidManifestExpectation:
	required_providers: set[str] = field(default_factory=set)
	forbidden_providers: set[str] = field(default_factory=set)
	required_adapters: set[str] = field(default_factory=set)
	forbidden_adapters: set[str] = field(default_factory=set)
	expected_metadata: dict[str, str] = field(default_factory=dict)


@dataclass(frozen=True)
class AndroidDependencyInventory:
	requested_versions: dict[str, tuple[str, ...]]
	selected_versions: dict[str, tuple[str, ...]]
	failed_coordinates: set[str]


@dataclass(frozen=True)
class AndroidDependencyExpectation:
	required_providers: set[str] = field(default_factory=set)
	forbidden_providers: set[str] = field(default_factory=set)
	required_adapters: set[str] = field(default_factory=set)
	forbidden_adapters: set[str] = field(default_factory=set)


def module_is_eligible(module: dict, platform: str, target_type: str) -> bool:
	allow_list = module.get("PlatformAllowList")
	if allow_list is not None and platform not in allow_list:
		return False

	module_type = module.get("Type", "Runtime")
	if target_type == "Editor":
		return module_type in {"Runtime", "RuntimeAndProgram", "Editor", "DeveloperTool"}
	return module_type in {"Runtime", "RuntimeAndProgram"}


def resolve_configuration(
	descriptors: dict[str, PluginDescriptor],
	requested_plugins: Iterable[str],
	*,
	platform: str,
	target_type: str,
) -> ResolvedConfiguration:
	resolved: set[str] = set()
	pending = list(requested_plugins)
	while pending:
		plugin_name = pending.pop()
		if plugin_name in resolved:
			continue
		if plugin_name not in descriptors:
			raise ValueError(f"unknown plugin '{plugin_name}'")
		resolved.add(plugin_name)
		pending.extend(descriptors[plugin_name].dependencies - resolved)

	modules = {
		module["Name"]
		for plugin_name in resolved
		for module in descriptors[plugin_name].data.get("Modules", [])
		if module_is_eligible(module, platform, target_type)
	}
	providers = {
		plugin_name
		for plugin_name in resolved
		if descriptors[plugin_name].is_ads_provider
	}
	adapters = {
		plugin_name
		for plugin_name in resolved
		if descriptors[plugin_name].is_ads_adapter
	}
	for adapter_name in adapters:
		adapter = descriptors[adapter_name]
		provider_dependencies = {
			dependency
			for dependency in adapter.dependencies
			if dependency in descriptors and descriptors[dependency].is_ads_provider
		}
		if "OpenMobileAds" not in adapter.dependencies or len(provider_dependencies) != 1:
			raise ValueError(
				f"mediation adapter '{adapter_name}' must depend on OpenMobileAds and exactly one provider"
			)
	for provider_name in providers:
		adapter_dependencies = {
			dependency
			for dependency in descriptors[provider_name].dependencies
			if dependency in descriptors and descriptors[dependency].is_ads_adapter
		}
		if adapter_dependencies:
			raise ValueError(
				f"provider '{provider_name}' must not depend on mediation adapter "
				f"'{sorted(adapter_dependencies)[0]}'"
			)
	return ResolvedConfiguration(resolved, modules, providers, adapters)


def stream_contains_markers(stream: BinaryIO, markers: tuple[bytes, ...]) -> set[bytes]:
	found: set[bytes] = set()
	remaining = set(markers)
	longest_marker = max((len(marker) for marker in markers), default=1)
	overlap = b""
	while remaining:
		chunk = stream.read(1024 * 1024)
		if not chunk:
			break
		searchable = (overlap + chunk).lower()
		for marker in tuple(remaining):
			if marker in searchable:
				found.add(marker)
				remaining.remove(marker)
		overlap = searchable[-(longest_marker - 1):]
	return found


def inspect_artifact(
	path: Path,
	*,
	provider_signatures: dict[str, tuple[bytes, ...]] = PROVIDER_SIGNATURES,
	adapter_signatures: dict[str, tuple[bytes, ...]] = ADAPTER_SIGNATURES,
) -> ArtifactInventory:
	entries: set[str] = set()
	detected_providers: set[str] = set()
	detected_adapters: set[str] = set()
	provider_markers = {
		provider: tuple(marker.lower() for marker in markers)
		for provider, markers in provider_signatures.items()
	}
	adapter_markers = {
		adapter: tuple(marker.lower() for marker in markers)
		for adapter, markers in adapter_signatures.items()
	}

	marker_owners = {
		marker: ("provider", provider)
		for provider, markers in provider_markers.items()
		for marker in markers
	}
	marker_owners.update({
		marker: ("adapter", adapter)
		for adapter, markers in adapter_markers.items()
		for marker in markers
	})

	def inspect_name(name: str) -> bool:
		lower_name = name.lower()
		entries.add(lower_name)
		for provider, markers in provider_markers.items():
			if any(marker.decode("ascii") in lower_name for marker in markers):
				detected_providers.add(provider)
		for adapter, markers in adapter_markers.items():
			if any(marker.decode("ascii") in lower_name for marker in markers):
				detected_adapters.add(adapter)
		return Path(lower_name).suffix in SCANNABLE_SUFFIXES

	def inspect_stream(stream: BinaryIO) -> None:
		for marker in stream_contains_markers(stream, tuple(marker_owners)):
			payload_type, owner = marker_owners[marker]
			if payload_type == "provider":
				detected_providers.add(owner)
			else:
				detected_adapters.add(owner)

	if path.is_dir():
		for artifact_file in path.rglob("*"):
			if not artifact_file.is_file():
				continue
			if not inspect_name(str(artifact_file.relative_to(path))):
				continue
			with artifact_file.open("rb") as artifact_stream:
				inspect_stream(artifact_stream)
	elif zipfile.is_zipfile(path):
		with zipfile.ZipFile(path) as archive:
			for entry in archive.infolist():
				if entry.is_dir():
					entries.add(entry.filename.lower())
					continue
				if not inspect_name(entry.filename):
					continue
				with archive.open(entry) as artifact_stream:
					inspect_stream(artifact_stream)
	else:
		with path.open("rb") as artifact_stream:
			inspect_name(path.name)
			inspect_stream(artifact_stream)

	return ArtifactInventory(entries, detected_providers, detected_adapters)


def validate_artifact(
	inventory: ArtifactInventory,
	expectation: ArtifactExpectation,
) -> list[str]:
	errors = [
		f"missing native payload for {provider}"
		for provider in sorted(expectation.required_providers - inventory.detected_providers)
	]
	errors.extend(
		f"found disabled native payload for {provider}"
		for provider in sorted(expectation.forbidden_providers & inventory.detected_providers)
	)
	errors.extend(
		f"missing native payload for adapter {adapter}"
		for adapter in sorted(expectation.required_adapters - inventory.detected_adapters)
	)
	errors.extend(
		f"found disabled native payload for adapter {adapter}"
		for adapter in sorted(expectation.forbidden_adapters & inventory.detected_adapters)
	)
	return errors


def inspect_android_manifest(path: Path) -> AndroidManifestInventory:
	root = ElementTree.parse(path).getroot()
	android_name = f"{{{ANDROID_NAMESPACE}}}name"
	android_value = f"{{{ANDROID_NAMESPACE}}}value"
	android_authorities = f"{{{ANDROID_NAMESPACE}}}authorities"

	permissions = {
		element.get(android_name, "")
		for element in root.findall("uses-permission")
		if element.get(android_name)
	}
	metadata: dict[str, list[str]] = {}
	activities: dict[str, int] = {}
	services: dict[str, int] = {}
	providers: dict[str, list[str]] = {}
	authority_owners: dict[str, list[str]] = {}
	application = root.find("application")
	if application is not None:
		for element in application.findall("meta-data"):
			name = element.get(android_name, "")
			if name:
				metadata.setdefault(name, []).append(element.get(android_value, ""))
		for element in application.findall("activity"):
			name = element.get(android_name, "")
			if name:
				activities[name] = activities.get(name, 0) + 1
		for element in application.findall("service"):
			name = element.get(android_name, "")
			if name:
				services[name] = services.get(name, 0) + 1
		for element in application.findall("provider"):
			name = element.get(android_name, "")
			if not name:
				continue
			authorities = element.get(android_authorities, "")
			providers.setdefault(name, []).append(authorities)
			for authority in authorities.split(";"):
				authority = authority.strip()
				if authority:
					authority_owners.setdefault(authority, []).append(name)

	return AndroidManifestInventory(
		permissions,
		{name: tuple(values) for name, values in metadata.items()},
		activities,
		services,
		{name: tuple(values) for name, values in providers.items()},
		{name: tuple(owners) for name, owners in authority_owners.items()},
		root.get("package", ""),
	)


def validate_android_manifest(
	inventory: AndroidManifestInventory,
	expectation: AndroidManifestExpectation,
) -> list[str]:
	errors: list[str] = []
	required_contracts = {
		name: ANDROID_MANIFEST_CONTRACTS[name]
		for name in expectation.required_providers
		if name in ANDROID_MANIFEST_CONTRACTS
	}
	required_contracts.update({
		name: ANDROID_ADAPTER_MANIFEST_CONTRACTS[name]
		for name in expectation.required_adapters
		if name in ANDROID_ADAPTER_MANIFEST_CONTRACTS
	})
	for owner, contract in sorted(required_contracts.items()):
		for permission in sorted(contract["permissions"] - inventory.permissions):
			errors.append(f"missing Android permission '{permission}' for {owner}")
		for name in sorted(contract["metadata"] - inventory.metadata.keys()):
			errors.append(f"missing Android metadata '{name}' for {owner}")
		for name in sorted(contract["activities"] - inventory.activities.keys()):
			errors.append(f"missing Android activity '{name}' for {owner}")
		for name in sorted(contract["services"] - inventory.services.keys()):
			errors.append(f"missing Android service '{name}' for {owner}")
		for name in sorted(contract["providers"] - inventory.providers.keys()):
			errors.append(f"missing Android provider '{name}' for {owner}")
		for name in sorted(contract["providers"] & inventory.providers.keys()):
			if not any(inventory.providers[name]):
				errors.append(f"missing Android provider authority for '{name}'")
			authority_suffix = ANDROID_PROVIDER_AUTHORITY_SUFFIXES.get(name)
			if authority_suffix and inventory.package_name:
				expected_authority = f"{inventory.package_name}{authority_suffix}"
				for authorities in inventory.providers[name]:
					if expected_authority not in {
						authority.strip() for authority in authorities.split(";")
					}:
						errors.append(
							f"Android provider '{name}' must use authority "
							f"'{expected_authority}'"
						)

	for name, values in sorted(inventory.metadata.items()):
		if len(set(values)) > 1:
			errors.append(f"conflicting metadata values for '{name}'")
	for name, expected_value in sorted(expectation.expected_metadata.items()):
		values = inventory.metadata.get(name, ())
		if not values:
			if not any(name in contract["metadata"] for contract in required_contracts.values()):
				errors.append(f"missing Android metadata '{name}'")
		elif len(set(values)) == 1 and values[0] != expected_value:
			errors.append(
				f"Android metadata '{name}' has '{values[0]}', expected '{expected_value}'"
			)
	for authority, owners in sorted(inventory.authority_owners.items()):
		if len(set(owners)) > 1:
			errors.append(
				f"Android provider authority '{authority}' is shared by "
				f"{', '.join(sorted(set(owners)))}"
			)

	for name, count in sorted({**inventory.activities, **inventory.services}.items()):
		if count > 1:
			errors.append(f"duplicate Android component '{name}'")
	for name, authorities in sorted(inventory.providers.items()):
		if len(authorities) > 1:
			errors.append(f"duplicate Android provider '{name}'")

	for owner in sorted(expectation.forbidden_providers):
		contract = ANDROID_MANIFEST_CONTRACTS.get(owner)
		if contract is not None and _manifest_has_owned_entry(inventory, contract):
			errors.append(f"found disabled Android manifest entry for {owner}")
	for owner in sorted(expectation.forbidden_adapters):
		contract = ANDROID_ADAPTER_MANIFEST_CONTRACTS.get(owner)
		if contract is not None and _manifest_has_owned_entry(inventory, contract):
			errors.append(f"found disabled Android manifest entry for adapter {owner}")
	return errors


def _manifest_has_owned_entry(
	inventory: AndroidManifestInventory,
	contract: dict[str, set[str]],
) -> bool:
	return bool(
		contract["metadata"] & inventory.metadata.keys()
		or contract["activities"] & inventory.activities.keys()
		or contract["services"] & inventory.services.keys()
		or contract["providers"] & inventory.providers.keys()
	)


ANDROID_DEPENDENCY_PATTERN = re.compile(
	r"(?P<group>[A-Za-z0-9_.-]+):(?P<name>[A-Za-z0-9_.-]+):"
	r"(?P<requested>\{strictly [^}]+\}|\[[^]]+\]|[^\s()]+)"
	r"(?:\s+->\s+(?P<selected>[^\s()]+))?"
)


def _normalize_requested_version(version: str) -> str:
	if version.startswith("{strictly ") and version.endswith("}"):
		return version[len("{strictly "):-1]
	if version.startswith("[") and version.endswith("]") and "," not in version:
		return version[1:-1]
	return version


def inspect_android_dependency_graph(contents: str) -> AndroidDependencyInventory:
	requested_versions: dict[str, set[str]] = {}
	selected_versions: dict[str, set[str]] = {}
	failed_coordinates: set[str] = set()
	for line in contents.splitlines():
		match = ANDROID_DEPENDENCY_PATTERN.search(line)
		if match is None:
			continue
		coordinate = f"{match.group('group')}:{match.group('name')}"
		requested = _normalize_requested_version(match.group("requested"))
		selected = match.group("selected") or requested
		requested_versions.setdefault(coordinate, set()).add(requested)
		selected_versions.setdefault(coordinate, set()).add(selected)
		if " FAILED" in line:
			failed_coordinates.add(coordinate)
	return AndroidDependencyInventory(
		{
			coordinate: tuple(sorted(versions))
			for coordinate, versions in requested_versions.items()
		},
		{
			coordinate: tuple(sorted(versions))
			for coordinate, versions in selected_versions.items()
		},
		failed_coordinates,
	)


def validate_android_dependencies(
	inventory: AndroidDependencyInventory,
	expectation: AndroidDependencyExpectation,
) -> list[str]:
	errors: list[str] = []
	required_contracts = {
		name: ANDROID_DEPENDENCY_CONTRACTS[name]
		for name in expectation.required_providers
		if name in ANDROID_DEPENDENCY_CONTRACTS
	}
	required_contracts.update({
		name: ANDROID_ADAPTER_DEPENDENCY_CONTRACTS[name]
		for name in expectation.required_adapters
		if name in ANDROID_ADAPTER_DEPENDENCY_CONTRACTS
	})
	for owner, contract in sorted(required_contracts.items()):
		for coordinate, expected_version in sorted(contract.items()):
			requested = set(inventory.requested_versions.get(coordinate, ()))
			selected = set(inventory.selected_versions.get(coordinate, ()))
			if not requested:
				errors.append(f"missing Android dependency '{coordinate}:{expected_version}' for {owner}")
				continue
			unexpected_requests = requested - {expected_version}
			if unexpected_requests:
				errors.append(
					f"Android dependency '{coordinate}' has unsupported requested version(s) "
					f"{', '.join(sorted(unexpected_requests))}; expected {expected_version}"
				)
			if selected != {expected_version}:
				errors.append(
					f"Android dependency '{coordinate}' resolved to "
					f"{', '.join(sorted(selected)) or 'nothing'}; expected {expected_version}"
				)
			if coordinate in inventory.failed_coordinates:
				errors.append(f"Android dependency '{coordinate}' failed to resolve")

	for owner in sorted(expectation.forbidden_providers):
		contract = ANDROID_DEPENDENCY_CONTRACTS.get(owner, {})
		if any(coordinate in inventory.requested_versions for coordinate in contract):
			errors.append(f"found disabled Android dependency for {owner}")
	for owner in sorted(expectation.forbidden_adapters):
		contract = ANDROID_ADAPTER_DEPENDENCY_CONTRACTS.get(owner, {})
		if any(coordinate in inventory.requested_versions for coordinate in contract):
			errors.append(f"found disabled Android dependency for adapter {owner}")
	return errors


def discover_descriptors(repository_root: Path) -> dict[str, PluginDescriptor]:
	descriptors: dict[str, PluginDescriptor] = {}
	for search_root in ("Foundation", "Native", "Services", "Providers", "Tests/Plugins"):
		root = repository_root / search_root
		if not root.exists():
			continue
		for path in root.rglob("*.uplugin"):
			descriptor = PluginDescriptor.load(path)
			if descriptor.name in descriptors:
				raise ValueError(f"duplicate plugin descriptor '{descriptor.name}'")
			descriptors[descriptor.name] = descriptor
	return descriptors


def run_graph_command(arguments: argparse.Namespace) -> int:
	descriptors = discover_descriptors(arguments.repository)
	configuration = resolve_configuration(
		descriptors,
		arguments.plugin,
		platform=arguments.platform,
		target_type=arguments.target_type,
	)
	print(json.dumps({
		"plugins": sorted(configuration.plugins),
		"modules": sorted(configuration.modules),
		"ads_providers": sorted(configuration.ads_providers),
		"ads_adapters": sorted(configuration.ads_adapters),
	}, indent=2))
	return 0


def run_artifact_command(arguments: argparse.Namespace) -> int:
	inventory = inspect_artifact(arguments.artifact)
	errors = validate_artifact(
		inventory,
		ArtifactExpectation(
			set(arguments.require_provider),
			set(arguments.forbid_provider),
			set(arguments.require_adapter),
			set(arguments.forbid_adapter),
		),
	)
	if errors:
		for error in errors:
			print(f"ads artifact validation failed: {error}", file=sys.stderr)
		return 1
	print(
		f"OpenMobile Ads artifact validation passed ({len(inventory.entries)} entries)."
	)
	return 0


def run_manifest_command(arguments: argparse.Namespace) -> int:
	expected_metadata = {}
	for assignment in arguments.expected_metadata:
		name, separator, value = assignment.partition("=")
		if not separator or not name or not value:
			raise ValueError("--expected-metadata must use NAME=VALUE")
		expected_metadata[name] = value
	errors = validate_android_manifest(
		inspect_android_manifest(arguments.manifest),
		AndroidManifestExpectation(
			set(arguments.require_provider),
			set(arguments.forbid_provider),
			set(arguments.require_adapter),
			set(arguments.forbid_adapter),
			expected_metadata,
		),
	)
	if errors:
		for error in errors:
			print(f"ads Android manifest validation failed: {error}", file=sys.stderr)
		return 1
	print("OpenMobile Ads Android manifest validation passed.")
	return 0


def run_dependencies_command(arguments: argparse.Namespace) -> int:
	inventory = inspect_android_dependency_graph(
		arguments.dependencies.read_text(encoding="utf-8")
	)
	errors = validate_android_dependencies(
		inventory,
		AndroidDependencyExpectation(
			set(arguments.require_provider),
			set(arguments.forbid_provider),
			set(arguments.require_adapter),
			set(arguments.forbid_adapter),
		),
	)
	if errors:
		for error in errors:
			print(f"ads Android dependency validation failed: {error}", file=sys.stderr)
		return 1
	print(
		"OpenMobile Ads Android dependency validation passed "
		f"({len(inventory.requested_versions)} modules)."
	)
	return 0


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser()
	subparsers = parser.add_subparsers(dest="command", required=True)

	graph_parser = subparsers.add_parser("graph")
	graph_parser.add_argument("--repository", type=Path, default=Path.cwd())
	graph_parser.add_argument("--plugin", action="append", required=True)
	graph_parser.add_argument("--platform", required=True)
	graph_parser.add_argument("--target-type", choices=("Game", "Editor"), required=True)
	graph_parser.set_defaults(handler=run_graph_command)

	artifact_parser = subparsers.add_parser("artifact")
	artifact_parser.add_argument("artifact", type=Path)
	artifact_parser.add_argument("--require-provider", action="append", default=[])
	artifact_parser.add_argument("--forbid-provider", action="append", default=[])
	artifact_parser.add_argument("--require-adapter", action="append", default=[])
	artifact_parser.add_argument("--forbid-adapter", action="append", default=[])
	artifact_parser.set_defaults(handler=run_artifact_command)

	manifest_parser = subparsers.add_parser("manifest")
	manifest_parser.add_argument("manifest", type=Path)
	manifest_parser.add_argument("--require-provider", action="append", default=[])
	manifest_parser.add_argument("--forbid-provider", action="append", default=[])
	manifest_parser.add_argument("--require-adapter", action="append", default=[])
	manifest_parser.add_argument("--forbid-adapter", action="append", default=[])
	manifest_parser.add_argument("--expected-metadata", action="append", default=[])
	manifest_parser.set_defaults(handler=run_manifest_command)

	dependencies_parser = subparsers.add_parser("dependencies")
	dependencies_parser.add_argument("dependencies", type=Path)
	dependencies_parser.add_argument("--require-provider", action="append", default=[])
	dependencies_parser.add_argument("--forbid-provider", action="append", default=[])
	dependencies_parser.add_argument("--require-adapter", action="append", default=[])
	dependencies_parser.add_argument("--forbid-adapter", action="append", default=[])
	dependencies_parser.set_defaults(handler=run_dependencies_command)

	return parser.parse_args()


if __name__ == "__main__":
	arguments = parse_arguments()
	raise SystemExit(arguments.handler(arguments))
