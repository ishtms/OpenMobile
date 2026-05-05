#!/usr/bin/env python3

import argparse
import hashlib
import json
import plistlib
import re
import struct
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
ADAPTER_SIGNATURES = {
	"OpenMobileAdsAdMobMeta": (
		b"com.google.ads.mediation:facebook",
		b"com/google/ads/mediation/facebook",
		b"com/facebook/ads",
	),
}
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
		"com.google.android.ump:user-messaging-platform": "4.0.0",
	},
}
ANDROID_ADAPTER_DEPENDENCY_CONTRACTS = {
	"OpenMobileAdsAdMobMeta": {
		"com.google.ads.mediation:facebook": "6.22.0.0",
		"com.facebook.android:audience-network-sdk": "6.22.0",
		"androidx.annotation:annotation": "1.5.0",
		"com.google.ads.mediation:common": "1.1.0",
		"com.google.android.gms:play-services-ads": "25.4.0",
		"org.jetbrains.kotlin:kotlin-stdlib": "2.3.0",
	},
}
IOS_PLIST_CONTRACTS = {
	"OpenMobileAdsAdMob": {
		"scalar_keys": {"GADApplicationIdentifier"},
		"skad_network_ids": {
			"cstr6suwn9.skadnetwork",
			"4fzdc2evr5.skadnetwork",
			"2fnua5tdw4.skadnetwork",
			"ydx93a7ass.skadnetwork",
			"p78axxw29g.skadnetwork",
			"v72qych5uu.skadnetwork",
			"ludvb6z3bs.skadnetwork",
			"cp8zw746q7.skadnetwork",
			"3sh42y64q3.skadnetwork",
			"c6k4g5qg8m.skadnetwork",
			"s39g8k73mm.skadnetwork",
			"wg4vff78zm.skadnetwork",
			"3qy4746246.skadnetwork",
			"f38h382jlk.skadnetwork",
			"hs6bdukanm.skadnetwork",
			"mlmmfzh3r3.skadnetwork",
			"v4nxqhlyqp.skadnetwork",
			"wzmmz9fp6w.skadnetwork",
			"su67r6k2v3.skadnetwork",
			"yclnxrl5pm.skadnetwork",
			"t38b2kh725.skadnetwork",
			"7ug5zh24hu.skadnetwork",
			"gta9lk7p23.skadnetwork",
			"vutu7akeur.skadnetwork",
			"y5ghdn5j9k.skadnetwork",
			"v9wttpbfk9.skadnetwork",
			"n38lu8286q.skadnetwork",
			"47vhws6wlr.skadnetwork",
			"kbd757ywx3.skadnetwork",
			"9t245vhmpl.skadnetwork",
			"a2p9lx4jpn.skadnetwork",
			"22mmun2rn5.skadnetwork",
			"44jx6755aq.skadnetwork",
			"k674qkevps.skadnetwork",
			"4468km3ulz.skadnetwork",
			"2u9pt9hc89.skadnetwork",
			"8s468mfl3y.skadnetwork",
			"klf5c3l5u5.skadnetwork",
			"ppxm28t8ap.skadnetwork",
			"kbmxgpxpgc.skadnetwork",
			"uw77j35x4d.skadnetwork",
			"578prtvx9j.skadnetwork",
			"4dzt52r2t5.skadnetwork",
			"tl55sbb4fm.skadnetwork",
			"c3frkrj4fj.skadnetwork",
			"e5fvkxwrpn.skadnetwork",
			"8c4e2ghe7u.skadnetwork",
			"3rd42ekr43.skadnetwork",
			"97r2b46745.skadnetwork",
			"3qcr597p9d.skadnetwork",
		},
	},
}
IOS_ADAPTER_PLIST_CONTRACTS = {}
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
THIRD_PARTY_BINARY_SUFFIXES = {
	".a",
	".aar",
	".dylib",
	".jar",
	".so",
	".zip",
}
IOS_PACKAGE_CONTRACTS = {
	"OpenMobileAdsAdMob": {
		"frameworks": {
			"GoogleMobileAds",
			"UserMessagingPlatform",
		},
	},
}
IOS_ADAPTER_PACKAGE_CONTRACTS = {}
ANDROID_ABI_ARCHITECTURES = {
	"arm64-v8a": "arm64",
	"armeabi-v7a": "arm",
	"x86": "x86",
	"x86_64": "x86_64",
}
CPU_ARCHITECTURES = {
	7: "x86",
	12: "arm",
	0x01000007: "x86_64",
	0x0100000C: "arm64",
}
ELF_ARCHITECTURES = {
	3: "x86",
	40: "arm",
	62: "x86_64",
	183: "arm64",
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


def validate_adapter_metadata(descriptor: PluginDescriptor) -> list[str]:
	manifest_path = descriptor.path.parent / "adapter.json"
	if not manifest_path.is_file():
		return [f"mediation adapter '{descriptor.name}' is missing adapter.json"]

	try:
		with manifest_path.open(encoding="utf-8") as manifest_file:
			metadata = json.load(manifest_file)
	except (json.JSONDecodeError, OSError) as error:
		return [f"mediation adapter '{descriptor.name}' has unreadable metadata: {error}"]

	errors: list[str] = []
	if metadata.get("schema_version") != 1:
		errors.append("adapter metadata schema_version must be 1")
	if metadata.get("plugin") != descriptor.name:
		errors.append(
			f"adapter metadata plugin '{metadata.get('plugin', '')}' does not match plugin '{descriptor.name}'"
		)

	for field_name in ("provider", "network", "display_name"):
		if not isinstance(metadata.get(field_name), str) or not metadata[field_name].strip():
			errors.append(f"adapter metadata {field_name} must not be empty")

	integration_types = metadata.get("integration_types")
	if not isinstance(integration_types, list) or not integration_types:
		errors.append("adapter metadata integration_types must not be empty")
	else:
		for integration_type in integration_types:
			if integration_type not in {"Bidding", "Waterfall"}:
				errors.append(
					f"adapter metadata has unsupported integration type '{integration_type}'"
				)

	platforms = metadata.get("platforms")
	if not isinstance(platforms, dict) or not platforms:
		errors.append("adapter metadata platforms must not be empty")
		platforms = {}
	for platform_name, platform in platforms.items():
		if platform_name not in {"Android", "IOS"}:
			errors.append(f"adapter metadata has unsupported platform '{platform_name}'")
		if not isinstance(platform, dict):
			errors.append(f"adapter metadata platform '{platform_name}' must be an object")
			continue
		for field_name in (
			"adapter_version",
			"network_sdk_version",
			"minimum_os_version",
		):
			if not isinstance(platform.get(field_name), str) or not platform[field_name].strip():
				errors.append(
					f"adapter metadata {platform_name}.{field_name} must not be empty"
				)
		tested_versions = platform.get("tested_provider_sdk_versions")
		if not isinstance(tested_versions, list) or not tested_versions:
			errors.append(
				f"adapter metadata {platform_name}.tested_provider_sdk_versions must not be empty"
			)

		dependencies = platform.get("dependencies")
		if not isinstance(dependencies, list) or not dependencies:
			errors.append(f"adapter metadata {platform_name}.dependencies must not be empty")
		else:
			seen_dependencies: set[str] = set()
			for dependency in dependencies:
				if not isinstance(dependency, dict):
					errors.append(
						f"adapter metadata {platform_name} dependency must be an object"
					)
					continue
				name = dependency.get("name")
				version = dependency.get("version")
				if not isinstance(name, str) or not name.strip():
					errors.append(
						f"adapter metadata {platform_name} dependency name must not be empty"
					)
					continue
				if name in seen_dependencies:
					errors.append(
						f"adapter metadata {platform_name} has duplicate dependency '{name}'"
					)
				seen_dependencies.add(name)
				if not isinstance(version, str) or not version.strip():
					errors.append(
						f"adapter metadata {platform_name} dependency '{name}' version must not be empty"
					)

		attribution_identifiers = platform.get("attribution_identifiers")
		if not isinstance(attribution_identifiers, list):
			errors.append(
				f"adapter metadata {platform_name}.attribution_identifiers must be an array"
			)
		elif len(attribution_identifiers) != len(set(attribution_identifiers)):
			errors.append(
				f"adapter metadata {platform_name}.attribution_identifiers contains duplicates"
			)

	sources = metadata.get("sources")
	if not isinstance(sources, dict):
		errors.append("adapter metadata sources must be an object")
		sources = {}
	for field_name in (
		"integration_guide",
		"adapter_repository",
		"adapter_license",
		"network_terms",
	):
		if not isinstance(sources.get(field_name), str) or not sources[field_name].strip():
			errors.append(f"adapter metadata source {field_name} must not be empty")

	return errors


@dataclass(frozen=True)
class ResolvedConfiguration:
	plugins: set[str]
	modules: set[str]
	ads_providers: set[str]
	ads_adapters: set[str]


@dataclass(frozen=True)
class ArtifactInventory:
	entries: set[str]
	entry_counts: dict[str, int]
	native_architectures: dict[str, tuple[str, ...]]
	detected_providers: set[str]
	detected_adapters: set[str]


@dataclass(frozen=True)
class ArtifactExpectation:
	required_providers: set[str] = field(default_factory=set)
	forbidden_providers: set[str] = field(default_factory=set)
	required_adapters: set[str] = field(default_factory=set)
	forbidden_adapters: set[str] = field(default_factory=set)


@dataclass(frozen=True)
class PackageExpectation:
	platform: str
	architectures: set[str] = field(default_factory=set)
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


@dataclass(frozen=True)
class IOSPlistInventory:
	values: dict[str, tuple[str, ...]]
	value_types: dict[str, tuple[str, ...]]
	skad_network_ids: tuple[str, ...]
	url_schemes: tuple[str, ...]
	duplicate_keys: tuple[str, ...]
	malformed_entries: tuple[str, ...]


@dataclass(frozen=True)
class IOSPlistExpectation:
	required_providers: set[str] = field(default_factory=set)
	forbidden_providers: set[str] = field(default_factory=set)
	required_adapters: set[str] = field(default_factory=set)
	forbidden_adapters: set[str] = field(default_factory=set)
	expected_values: dict[str, str] = field(default_factory=dict)


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


def inspect_native_architectures(header: bytes) -> tuple[str, ...]:
	if len(header) >= 20 and header.startswith(b"\x7fELF"):
		endianness = {1: "<", 2: ">"}.get(header[5])
		if endianness is None:
			return ()
		machine = struct.unpack_from(f"{endianness}H", header, 18)[0]
		architecture = ELF_ARCHITECTURES.get(machine)
		return (architecture,) if architecture else ()

	thin_mach_o = {
		b"\xce\xfa\xed\xfe": "<",
		b"\xcf\xfa\xed\xfe": "<",
		b"\xfe\xed\xfa\xce": ">",
		b"\xfe\xed\xfa\xcf": ">",
	}
	endianness = thin_mach_o.get(header[:4])
	if endianness and len(header) >= 8:
		cpu_type = struct.unpack_from(f"{endianness}I", header, 4)[0]
		architecture = CPU_ARCHITECTURES.get(cpu_type)
		return (architecture,) if architecture else ()

	fat_mach_o = {
		b"\xca\xfe\xba\xbe": (">", 20),
		b"\xbe\xba\xfe\xca": ("<", 20),
		b"\xca\xfe\xba\xbf": (">", 32),
		b"\xbf\xba\xfe\xca": ("<", 32),
	}
	fat_format = fat_mach_o.get(header[:4])
	if fat_format is None or len(header) < 8:
		return ()
	endianness, entry_size = fat_format
	entry_count = struct.unpack_from(f"{endianness}I", header, 4)[0]
	if entry_count > 64 or len(header) < 8 + (entry_count * entry_size):
		return ()
	architectures: set[str] = set()
	for index in range(entry_count):
		cpu_type = struct.unpack_from(
			f"{endianness}I",
			header,
			8 + (index * entry_size),
		)[0]
		architecture = CPU_ARCHITECTURES.get(cpu_type)
		if architecture:
			architectures.add(architecture)
	return tuple(sorted(architectures))


def is_native_artifact_entry(name: str) -> bool:
	path = Path(name)
	if path.suffix in {".dylib", ".so"}:
		return True
	parts = path.parts
	if len(parts) < 2 or not parts[-2].endswith(".framework"):
		return False
	return parts[-1] == Path(parts[-2]).stem


def inspect_artifact(
	path: Path,
	*,
	provider_signatures: dict[str, tuple[bytes, ...]] = PROVIDER_SIGNATURES,
	adapter_signatures: dict[str, tuple[bytes, ...]] = ADAPTER_SIGNATURES,
) -> ArtifactInventory:
	entries: set[str] = set()
	entry_counts: dict[str, int] = {}
	native_architectures: dict[str, tuple[str, ...]] = {}
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

	def inspect_name(name: str) -> tuple[str, bool, bool]:
		lower_name = name.lower()
		entries.add(lower_name)
		entry_counts[lower_name] = entry_counts.get(lower_name, 0) + 1
		for provider, markers in provider_markers.items():
			if any(marker.decode("ascii") in lower_name for marker in markers):
				detected_providers.add(provider)
		for adapter, markers in adapter_markers.items():
			if any(marker.decode("ascii") in lower_name for marker in markers):
				detected_adapters.add(adapter)
		return (
			lower_name,
			Path(lower_name).suffix in SCANNABLE_SUFFIXES,
			is_native_artifact_entry(lower_name),
		)

	def inspect_stream(
		stream: BinaryIO,
		name: str,
		*,
		scan_markers: bool,
		scan_architectures: bool,
	) -> None:
		if scan_architectures:
			native_architectures[name] = inspect_native_architectures(stream.read(4096))
		if not scan_markers:
			return
		if scan_architectures:
			stream.seek(0)
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
			name, scan_markers, scan_architectures = inspect_name(
				str(artifact_file.relative_to(path))
			)
			if not scan_markers and not scan_architectures:
				continue
			with artifact_file.open("rb") as artifact_stream:
				inspect_stream(
					artifact_stream,
					name,
					scan_markers=scan_markers,
					scan_architectures=scan_architectures,
				)
	elif zipfile.is_zipfile(path):
		with zipfile.ZipFile(path) as archive:
			for entry in archive.infolist():
				if entry.is_dir():
					inspect_name(entry.filename)
					continue
				name, scan_markers, scan_architectures = inspect_name(entry.filename)
				if not scan_markers and not scan_architectures:
					continue
				with archive.open(entry) as artifact_stream:
					inspect_stream(
						artifact_stream,
						name,
						scan_markers=scan_markers,
						scan_architectures=scan_architectures,
					)
	else:
		with path.open("rb") as artifact_stream:
			name, scan_markers, scan_architectures = inspect_name(path.name)
			inspect_stream(
				artifact_stream,
				name,
				scan_markers=scan_markers,
				scan_architectures=scan_architectures,
			)

	return ArtifactInventory(
		entries=entries,
		entry_counts=entry_counts,
		native_architectures=native_architectures,
		detected_providers=detected_providers,
		detected_adapters=detected_adapters,
	)


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


def validate_android_package(
	inventory: ArtifactInventory,
	expectation: PackageExpectation,
) -> list[str]:
	errors: list[str] = []
	native_entries = {
		entry: architectures
		for entry, architectures in inventory.native_architectures.items()
		if re.search(r"(?:^|/)lib/[^/]+/[^/]+\.so$", entry)
	}
	for entry, architectures in sorted(native_entries.items()):
		match = re.search(r"(?:^|/)lib/([^/]+)/[^/]+\.so$", entry)
		if match is None:
			continue
		abi = match.group(1)
		if expectation.architectures and abi not in expectation.architectures:
			errors.append(f"unexpected Android ABI {abi} in {entry}")
		expected_native_architecture = ANDROID_ABI_ARCHITECTURES.get(abi)
		if expected_native_architecture and architectures != (expected_native_architecture,):
			detected = ", ".join(architectures) if architectures else "unknown"
			errors.append(
				f"Android ABI {abi} contains {detected} native binary {entry}"
			)

	for architecture in sorted(expectation.architectures):
		if not any(
			re.search(rf"(?:^|/)lib/{re.escape(architecture)}/[^/]+\.so$", entry)
			for entry in native_entries
		):
			errors.append(f"missing Android ABI {architecture}")

	unmerged_dependencies = sorted(
		entry
		for entry in inventory.entries
		if Path(entry).suffix == ".aar"
		or (
			Path(entry).suffix == ".jar"
			and ("/libs/" in f"/{entry}" or Path(entry).name == "classes.jar")
		)
	)
	if unmerged_dependencies:
		errors.append(
			"unmerged Android dependency in package: " + unmerged_dependencies[0]
		)

	ios_payload = sorted(
		entry
		for entry in inventory.entries
		if ".framework/" in entry
		or ".xcframework/" in entry
		or entry.endswith(".dylib")
	)
	if ios_payload:
		errors.append("iOS payload found in Android package: " + ios_payload[0])
	return errors


def framework_entry_suffix(framework: str, suffix: str) -> str:
	return f"/frameworks/{framework.lower()}.framework/{suffix.lower()}"


def find_framework_entry(
	entries: Iterable[str],
	framework: str,
	suffix: str,
) -> str | None:
	expected_suffix = framework_entry_suffix(framework, suffix)
	for entry in entries:
		if f"/{entry}".endswith(expected_suffix):
			return entry
	return None


def validate_ios_package(
	inventory: ArtifactInventory,
	expectation: PackageExpectation,
) -> list[str]:
	errors: list[str] = []
	required_frameworks: set[str] = set()
	for provider in expectation.required_providers:
		required_frameworks.update(
			IOS_PACKAGE_CONTRACTS.get(provider, {}).get("frameworks", set())
		)
	for adapter in expectation.required_adapters:
		required_frameworks.update(
			IOS_ADAPTER_PACKAGE_CONTRACTS.get(adapter, {}).get("frameworks", set())
		)

	for framework in sorted(required_frameworks):
		binary_entry = find_framework_entry(
			inventory.entries,
			framework,
			framework,
		)
		if binary_entry is None:
			errors.append(f"missing embedded iOS framework {framework}")
			continue
		if find_framework_entry(
			inventory.entries,
			framework,
			"_CodeSignature/CodeResources",
		) is None:
			errors.append(f"unsigned framework {framework}")
		if find_framework_entry(
			inventory.entries,
			framework,
			"PrivacyInfo.xcprivacy",
		) is None:
			errors.append(f"missing privacy manifest for framework {framework}")
		architectures = set(inventory.native_architectures.get(binary_entry, ()))
		if architectures != expectation.architectures:
			detected = ", ".join(sorted(architectures)) if architectures else "unknown"
			expected = ", ".join(sorted(expectation.architectures)) or "device default"
			errors.append(
				f"framework {framework} contains {detected}, expected {expected}"
			)

	build_payload = sorted(
		entry
		for entry in inventory.entries
		if ".xcframework/" in entry
		or "/headers/" in f"/{entry}"
		or "/privateheaders/" in f"/{entry}"
		or "/modules/" in f"/{entry}"
		or "-simulator/" in entry
		or Path(entry).suffix == ".a"
		or (
			Path(entry).suffix == ".zip"
			and (
				"/frameworks/" in f"/{entry}"
				or any(
					marker.decode("ascii") in entry
					for markers in PROVIDER_SIGNATURES.values()
					for marker in markers
				)
			)
		)
	)
	if build_payload:
		errors.append("build-time iOS payload in application: " + build_payload[0])

	android_payload = sorted(
		entry
		for entry in inventory.entries
		if entry.endswith("classes.dex")
		or re.search(r"(?:^|/)lib/[^/]+/[^/]+\.so$", entry)
		or entry.endswith("androidmanifest.xml")
	)
	if android_payload:
		errors.append("Android payload found in iOS package: " + android_payload[0])
	return errors


def validate_package(
	inventory: ArtifactInventory,
	expectation: PackageExpectation,
) -> list[str]:
	errors = validate_artifact(
		inventory,
		ArtifactExpectation(
			required_providers=expectation.required_providers,
			forbidden_providers=expectation.forbidden_providers,
			required_adapters=expectation.required_adapters,
			forbidden_adapters=expectation.forbidden_adapters,
		),
	)
	errors.extend(
		f"duplicate package entry {entry}"
		for entry, count in sorted(inventory.entry_counts.items())
		if count > 1
	)
	platform = expectation.platform.lower()
	if platform == "android":
		errors.extend(validate_android_package(inventory, expectation))
	elif platform == "ios":
		errors.extend(validate_ios_package(inventory, expectation))
	else:
		errors.append(f"unsupported package platform {expectation.platform}")
	return errors


def sha256_file(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as source:
		while chunk := source.read(1024 * 1024):
			digest.update(chunk)
	return digest.hexdigest()


def safe_manifest_path(root: Path, relative_path: str) -> Path | None:
	if not relative_path or Path(relative_path).is_absolute():
		return None
	path = (root / relative_path).resolve()
	try:
		path.relative_to(root.resolve())
	except ValueError:
		return None
	return path


def validate_third_party_packages(manifest_path: Path) -> list[str]:
	errors: list[str] = []
	if not manifest_path.is_file():
		return [f"missing third-party package manifest {manifest_path}"]
	try:
		manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
	except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
		return [f"invalid third-party package manifest {manifest_path}: {error}"]
	if not isinstance(manifest, dict):
		return [f"invalid third-party package manifest {manifest_path}: expected object"]
	if manifest.get("schema_version") != 1:
		errors.append("third-party package manifest schema_version must be 1")

	root = manifest_path.parent
	listed_artifacts: set[str] = set()
	packages = manifest.get("packages")
	if not isinstance(packages, list) or not packages:
		return errors + ["third-party package manifest has no packages"]
	for package in packages:
		if not isinstance(package, dict):
			errors.append("third-party package entry must be an object")
			continue
		name = package.get("name")
		if not isinstance(name, str) or not name.strip():
			name = "unnamed package"
			errors.append("third-party package is missing a name")
		for field_name in ("version", "redistribution"):
			value = package.get(field_name)
			if not isinstance(value, str) or not value.strip():
				errors.append(f"{name} is missing {field_name}")
		for field_name in ("source_url", "terms_url"):
			value = package.get(field_name)
			if not isinstance(value, str) or not value.startswith("https://"):
				errors.append(f"{name} has invalid {field_name}")

		license_files = package.get("license_files")
		if not isinstance(license_files, list) or not license_files:
			errors.append(f"{name} has no license files")
		else:
			for relative_path in license_files:
				if not isinstance(relative_path, str):
					errors.append(f"{name} has an invalid license file entry")
					continue
				path = safe_manifest_path(root, relative_path)
				if path is None or not path.is_file() or path.stat().st_size == 0:
					errors.append(f"{name} has missing license file {relative_path}")

		artifacts = package.get("artifacts")
		if not isinstance(artifacts, list) or not artifacts:
			errors.append(f"{name} has no artifacts")
			continue
		for artifact in artifacts:
			if not isinstance(artifact, dict):
				errors.append(f"{name} has an invalid artifact entry")
				continue
			relative_path = artifact.get("path")
			checksum = artifact.get("sha256")
			role = artifact.get("role")
			if not isinstance(relative_path, str):
				errors.append(f"{name} has an artifact without a path")
				continue
			if relative_path in listed_artifacts:
				errors.append(f"duplicate third-party artifact {relative_path}")
			listed_artifacts.add(relative_path)
			path = safe_manifest_path(root, relative_path)
			if path is None or not path.is_file():
				errors.append(f"missing third-party artifact {relative_path}")
				continue
			if not isinstance(role, str) or not role.strip():
				errors.append(f"{relative_path} is missing its package role")
			if not isinstance(checksum, str) or not re.fullmatch(r"[0-9a-f]{64}", checksum):
				errors.append(f"{relative_path} has invalid sha256")
			elif sha256_file(path) != checksum:
				errors.append(f"checksum mismatch for {relative_path}")

	discovered_artifacts = {
		str(path.relative_to(root))
		for path in root.rglob("*")
		if path.is_file()
		and (
			path.suffix.lower() in THIRD_PARTY_BINARY_SUFFIXES
			or is_native_artifact_entry(str(path.relative_to(root)).lower())
		)
	}
	errors.extend(
		f"unlisted third-party binary {path}"
		for path in sorted(discovered_artifacts - listed_artifacts)
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


def _plist_value_text(element: ElementTree.Element) -> str:
	return element.text or ""


def _inspect_xml_ios_plist(path: Path) -> IOSPlistInventory:
	root = ElementTree.parse(path).getroot()
	top_dictionary = root if root.tag == "dict" else root.find("dict")
	if top_dictionary is None:
		raise ValueError("iOS plist has no root dictionary")

	values: dict[str, list[str]] = {}
	value_types: dict[str, list[str]] = {}
	skad_network_ids: list[str] = []
	url_schemes: list[str] = []
	duplicate_keys: list[str] = []
	malformed_entries: list[str] = []

	def inspect_skad_network_items(value: ElementTree.Element, path_name: str) -> None:
		if value.tag != "array":
			malformed_entries.append(f"{path_name} must be an array")
			return
		for index, item in enumerate(value):
			if item.tag != "dict":
				malformed_entries.append(f"{path_name}[{index}] must be a dictionary")
				continue
			children = list(item)
			identifiers = [
				children[child_index + 1]
				for child_index in range(len(children) - 1)
				if children[child_index].tag == "key"
				and _plist_value_text(children[child_index]) == "SKAdNetworkIdentifier"
			]
			if len(identifiers) != 1 or identifiers[0].tag != "string":
				malformed_entries.append(
					f"{path_name}[{index}] must contain one string SKAdNetworkIdentifier"
				)
				continue
			identifier = _plist_value_text(identifiers[0]).strip()
			if not identifier:
				malformed_entries.append(f"{path_name}[{index}] has an empty identifier")
				continue
			skad_network_ids.append(identifier)

	def inspect_url_schemes(value: ElementTree.Element, path_name: str) -> None:
		if value.tag != "array":
			malformed_entries.append(f"{path_name} must be an array")
			return
		for index, item in enumerate(value):
			if item.tag != "string" or not _plist_value_text(item).strip():
				malformed_entries.append(f"{path_name}[{index}] must be a non-empty string")
				continue
			url_schemes.append(_plist_value_text(item).strip())

	def inspect_value(value: ElementTree.Element, path_name: str) -> None:
		if value.tag == "dict":
			inspect_dictionary(value, path_name, False)
		elif value.tag == "array":
			for index, item in enumerate(value):
				inspect_value(item, f"{path_name}[{index}]")

	def inspect_dictionary(
		dictionary: ElementTree.Element,
		path_name: str,
		is_top_level: bool,
	) -> None:
		children = list(dictionary)
		seen_keys: set[str] = set()
		index = 0
		while index < len(children):
			key_element = children[index]
			if key_element.tag != "key":
				malformed_entries.append(f"{path_name} contains a value without a key")
				inspect_value(key_element, f"{path_name}[{index}]")
				index += 1
				continue
			key = _plist_value_text(key_element)
			if not key:
				malformed_entries.append(f"{path_name} contains an empty key")
			if index + 1 >= len(children):
				malformed_entries.append(f"{path_name}.{key} has no value")
				break
			value = children[index + 1]
			if key in seen_keys:
				duplicate_keys.append(key)
			seen_keys.add(key)
			if is_top_level:
				values.setdefault(key, []).append(_plist_value_text(value))
				value_types.setdefault(key, []).append(value.tag)
			if key == "SKAdNetworkItems":
				inspect_skad_network_items(value, f"{path_name}.{key}")
			elif key == "CFBundleURLSchemes":
				inspect_url_schemes(value, f"{path_name}.{key}")
			inspect_value(value, f"{path_name}.{key}")
			index += 2

	inspect_dictionary(top_dictionary, "root", True)
	return IOSPlistInventory(
		{name: tuple(entries) for name, entries in values.items()},
		{name: tuple(entries) for name, entries in value_types.items()},
		tuple(skad_network_ids),
		tuple(url_schemes),
		tuple(duplicate_keys),
		tuple(malformed_entries),
	)


def _inspect_binary_ios_plist(path: Path) -> IOSPlistInventory:
	with path.open("rb") as plist_file:
		root = plistlib.load(plist_file)
	if not isinstance(root, dict):
		raise ValueError("iOS plist has no root dictionary")

	values = {
		key: (value if isinstance(value, str) else "",)
		for key, value in root.items()
	}
	value_types = {
		key: ("string" if isinstance(value, str) else type(value).__name__,)
		for key, value in root.items()
	}
	malformed_entries: list[str] = []
	skad_network_ids: list[str] = []
	url_schemes: list[str] = []

	def inspect_value(value: object, path_name: str) -> None:
		if isinstance(value, dict):
			for key, child in value.items():
				child_path = f"{path_name}.{key}"
				if key == "SKAdNetworkItems":
					if not isinstance(child, list):
						malformed_entries.append(f"{child_path} must be an array")
					else:
						for index, item in enumerate(child):
							identifier = item.get("SKAdNetworkIdentifier") if isinstance(item, dict) else None
							if not isinstance(identifier, str) or not identifier.strip():
								malformed_entries.append(
									f"{child_path}[{index}] must contain one string SKAdNetworkIdentifier"
								)
							else:
								skad_network_ids.append(identifier.strip())
				elif key == "CFBundleURLSchemes":
					if not isinstance(child, list):
						malformed_entries.append(f"{child_path} must be an array")
					else:
						for index, scheme in enumerate(child):
							if not isinstance(scheme, str) or not scheme.strip():
								malformed_entries.append(
									f"{child_path}[{index}] must be a non-empty string"
								)
							else:
								url_schemes.append(scheme.strip())
				inspect_value(child, child_path)
		elif isinstance(value, list):
			for index, child in enumerate(value):
				inspect_value(child, f"{path_name}[{index}]")

	inspect_value(root, "root")
	return IOSPlistInventory(
		values,
		value_types,
		tuple(skad_network_ids),
		tuple(url_schemes),
		(),
		tuple(malformed_entries),
	)


def inspect_ios_plist(path: Path) -> IOSPlistInventory:
	with path.open("rb") as plist_file:
		is_binary = plist_file.read(8) == b"bplist00"
	return _inspect_binary_ios_plist(path) if is_binary else _inspect_xml_ios_plist(path)


def _duplicate_values(values: tuple[str, ...]) -> set[str]:
	counts: dict[str, int] = {}
	for value in values:
		counts[value] = counts.get(value, 0) + 1
	return {value for value, count in counts.items() if count > 1}


def validate_ios_plist(
	inventory: IOSPlistInventory,
	expectation: IOSPlistExpectation,
) -> list[str]:
	errors = [f"malformed iOS plist entry: {entry}" for entry in inventory.malformed_entries]
	errors.extend(
		f"duplicate iOS plist key '{key}'"
		for key in sorted(set(inventory.duplicate_keys))
	)

	required_contracts = {
		name: IOS_PLIST_CONTRACTS[name]
		for name in expectation.required_providers
		if name in IOS_PLIST_CONTRACTS
	}
	required_contracts.update({
		name: IOS_ADAPTER_PLIST_CONTRACTS[name]
		for name in expectation.required_adapters
		if name in IOS_ADAPTER_PLIST_CONTRACTS
	})
	for owner, contract in sorted(required_contracts.items()):
		for key in sorted(contract["scalar_keys"]):
			if key not in inventory.values:
				errors.append(f"missing iOS plist key '{key}' for {owner}")
			elif any(value_type != "string" for value_type in inventory.value_types[key]):
				errors.append(f"iOS plist key '{key}' for {owner} must be a string")
			elif not all(value.strip() for value in inventory.values[key]):
				errors.append(f"iOS plist key '{key}' for {owner} must not be empty")
		missing_identifiers = contract["skad_network_ids"] - set(inventory.skad_network_ids)
		for identifier in sorted(missing_identifiers):
			errors.append(f"missing SKAdNetworkIdentifier '{identifier}' for {owner}")

	for key, expected_value in sorted(expectation.expected_values.items()):
		actual_values = inventory.values.get(key, ())
		if not actual_values:
			errors.append(f"missing iOS plist key '{key}'")
			continue
		if len(set(actual_values)) > 1:
			errors.append(f"conflicting values for iOS plist key '{key}'")
		if set(actual_values) != {expected_value}:
			errors.append(
				f"iOS plist key '{key}' has '{actual_values[-1]}', expected '{expected_value}'"
			)

	for identifier in sorted(_duplicate_values(inventory.skad_network_ids)):
		errors.append(f"duplicate SKAdNetworkIdentifier '{identifier}'")
	for scheme in sorted(_duplicate_values(inventory.url_schemes)):
		errors.append(f"duplicate URL scheme '{scheme}'")

	tracking_values = inventory.values.get("NSUserTrackingUsageDescription", ())
	if tracking_values and not all(value.strip() for value in tracking_values):
		errors.append("iOS tracking usage description is empty")

	for owner in sorted(expectation.forbidden_providers):
		contract = IOS_PLIST_CONTRACTS.get(owner, {})
		if any(key in inventory.values for key in contract.get("scalar_keys", set())):
			errors.append(f"found disabled iOS plist entry for {owner}")
	for owner in sorted(expectation.forbidden_adapters):
		contract = IOS_ADAPTER_PLIST_CONTRACTS.get(owner, {})
		if any(key in inventory.values for key in contract.get("scalar_keys", set())):
			errors.append(f"found disabled iOS plist entry for adapter {owner}")
	return errors


def discover_descriptors(repository_root: Path) -> dict[str, PluginDescriptor]:
	descriptors: dict[str, PluginDescriptor] = {}
	for search_root in (
		"Adapters",
		"Foundation",
		"Native",
		"Services",
		"Providers",
		"Tests/Plugins",
	):
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
	metadata_errors = [
		error
		for adapter_name in sorted(configuration.ads_adapters)
		for error in validate_adapter_metadata(descriptors[adapter_name])
	]
	if metadata_errors:
		for error in metadata_errors:
			print(f"ads adapter metadata validation failed: {error}", file=sys.stderr)
		return 1
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


def run_package_command(arguments: argparse.Namespace) -> int:
	inventory = inspect_artifact(arguments.artifact)
	errors = validate_package(
		inventory,
		PackageExpectation(
			platform=arguments.platform,
			architectures=set(arguments.architecture),
			required_providers=set(arguments.require_provider),
			forbidden_providers=set(arguments.forbid_provider),
			required_adapters=set(arguments.require_adapter),
			forbidden_adapters=set(arguments.forbid_adapter),
		),
	)
	if errors:
		for error in errors:
			print(f"ads package validation failed: {error}", file=sys.stderr)
		return 1
	print(
		"OpenMobile Ads package validation passed "
		f"({len(inventory.entries)} entries)."
	)
	return 0


def run_third_party_command(arguments: argparse.Namespace) -> int:
	errors = validate_third_party_packages(arguments.manifest)
	if errors:
		for error in errors:
			print(f"ads third-party validation failed: {error}", file=sys.stderr)
		return 1
	print("OpenMobile Ads third-party package validation passed.")
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


def run_plist_command(arguments: argparse.Namespace) -> int:
	expected_values = {}
	for assignment in arguments.expected_value:
		name, separator, value = assignment.partition("=")
		if not separator or not name or not value:
			raise ValueError("--expected-value must use NAME=VALUE")
		expected_values[name] = value
	inventory = inspect_ios_plist(arguments.plist)
	errors = validate_ios_plist(
		inventory,
		IOSPlistExpectation(
			set(arguments.require_provider),
			set(arguments.forbid_provider),
			set(arguments.require_adapter),
			set(arguments.forbid_adapter),
			expected_values,
		),
	)
	if errors:
		for error in errors:
			print(f"ads iOS plist validation failed: {error}", file=sys.stderr)
		return 1
	print(
		"OpenMobile Ads iOS plist validation passed "
		f"({len(inventory.skad_network_ids)} SKAdNetwork identifiers)."
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

	package_parser = subparsers.add_parser("package")
	package_parser.add_argument("artifact", type=Path)
	package_parser.add_argument("--platform", choices=("Android", "IOS"), required=True)
	package_parser.add_argument(
		"--architecture",
		action="append",
		choices=("arm64", "arm64-v8a", "armeabi-v7a", "x86", "x86_64"),
		required=True,
	)
	package_parser.add_argument("--require-provider", action="append", default=[])
	package_parser.add_argument("--forbid-provider", action="append", default=[])
	package_parser.add_argument("--require-adapter", action="append", default=[])
	package_parser.add_argument("--forbid-adapter", action="append", default=[])
	package_parser.set_defaults(handler=run_package_command)

	third_party_parser = subparsers.add_parser("third-party")
	third_party_parser.add_argument("manifest", type=Path)
	third_party_parser.set_defaults(handler=run_third_party_command)

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

	plist_parser = subparsers.add_parser("plist")
	plist_parser.add_argument("plist", type=Path)
	plist_parser.add_argument("--require-provider", action="append", default=[])
	plist_parser.add_argument("--forbid-provider", action="append", default=[])
	plist_parser.add_argument("--require-adapter", action="append", default=[])
	plist_parser.add_argument("--forbid-adapter", action="append", default=[])
	plist_parser.add_argument("--expected-value", action="append", default=[])
	plist_parser.set_defaults(handler=run_plist_command)

	return parser.parse_args()


if __name__ == "__main__":
	arguments = parse_arguments()
	raise SystemExit(arguments.handler(arguments))
