#!/usr/bin/env python3

import argparse
import json
import sys
import zipfile
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
		return self.name != "OpenMobileAds" and "OpenMobileAds" in self.dependencies


@dataclass(frozen=True)
class ResolvedConfiguration:
	plugins: set[str]
	modules: set[str]
	ads_providers: set[str]


@dataclass(frozen=True)
class ArtifactInventory:
	entries: set[str]
	detected_providers: set[str]


@dataclass(frozen=True)
class ArtifactExpectation:
	required_providers: set[str] = field(default_factory=set)
	forbidden_providers: set[str] = field(default_factory=set)


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
	return ResolvedConfiguration(resolved, modules, providers)


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


def inspect_artifact(path: Path) -> ArtifactInventory:
	entries: set[str] = set()
	detected: set[str] = set()
	provider_markers = {
		provider: tuple(marker.lower() for marker in markers)
		for provider, markers in PROVIDER_SIGNATURES.items()
	}

	marker_owners = {
		marker: provider
		for provider, markers in provider_markers.items()
		for marker in markers
	}

	def inspect_name(name: str) -> bool:
		lower_name = name.lower()
		entries.add(lower_name)
		for provider, markers in provider_markers.items():
			if any(marker.decode("ascii") in lower_name for marker in markers):
				detected.add(provider)
		return Path(lower_name).suffix in SCANNABLE_SUFFIXES

	def inspect_stream(stream: BinaryIO) -> None:
		for marker in stream_contains_markers(stream, tuple(marker_owners)):
			detected.add(marker_owners[marker])

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

	return ArtifactInventory(entries, detected)


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
	}, indent=2))
	return 0


def run_artifact_command(arguments: argparse.Namespace) -> int:
	inventory = inspect_artifact(arguments.artifact)
	errors = validate_artifact(
		inventory,
		ArtifactExpectation(set(arguments.require_provider), set(arguments.forbid_provider)),
	)
	if errors:
		for error in errors:
			print(f"ads artifact validation failed: {error}", file=sys.stderr)
		return 1
	print(
		f"OpenMobile Ads artifact validation passed ({len(inventory.entries)} entries)."
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
	artifact_parser.set_defaults(handler=run_artifact_command)

	return parser.parse_args()


if __name__ == "__main__":
	arguments = parse_arguments()
	raise SystemExit(arguments.handler(arguments))
