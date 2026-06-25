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
		b"metaadapter",
		b"fbaudiencenetwork",
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


def _load_android_dependency_contracts(root: Path, manifest_name: str) -> dict[str, dict[str, str]]:
	contracts: dict[str, dict[str, str]] = {}
	for manifest_path in sorted(root.rglob(manifest_name)):
		try:
			metadata = json.loads(manifest_path.read_text(encoding="utf-8"))
		except (OSError, json.JSONDecodeError):
			continue
		plugin = metadata.get("plugin")
		dependencies = (
			metadata.get("platforms", {})
			.get("Android", {})
			.get("dependencies", [])
		)
		if not isinstance(plugin, str) or not isinstance(dependencies, list):
			continue
		contracts[plugin] = {
			dependency["name"]: dependency["version"]
			for dependency in dependencies
			if isinstance(dependency, dict)
			and dependency.get("kind") == "Gradle"
			and dependency.get("ownership") == "Owned"
			and isinstance(dependency.get("name"), str)
			and isinstance(dependency.get("version"), str)
		}
	return contracts


_REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
ANDROID_DEPENDENCY_CONTRACTS = _load_android_dependency_contracts(
	_REPOSITORY_ROOT / "Providers" / "Ads",
	"native-dependencies.json",
)
ANDROID_ADAPTER_DEPENDENCY_CONTRACTS = _load_android_dependency_contracts(
	_REPOSITORY_ROOT / "Adapters" / "Ads",
	"adapter.json",
)
IOS_PLIST_CONTRACTS = {
	"OpenMobileAdsAdMob": {
		"scalar_keys": {"GADApplicationIdentifier"},
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
IOS_ADAPTER_PACKAGE_CONTRACTS = {
	"OpenMobileAdsAdMobMeta": {
		"frameworks": {"FBAudienceNetwork"},
		"static_frameworks": {"MetaAdapter"},
		"privacy_manifest_frameworks": {"FBAudienceNetwork"},
	},
}
IOS_REQUIRED_REASON_API_CONTRACTS = {
	"NSPrivacyAccessedAPICategoryDiskSpace": {"E174.1"},
	"NSPrivacyAccessedAPICategoryFileTimestamp": {"C617.1"},
	"NSPrivacyAccessedAPICategorySystemBootTime": {"35F9.1"},
	"NSPrivacyAccessedAPICategoryUserDefaults": {"CA92.1"},
}
IOS_AD_ATTRIBUTION_KIT_MINIMUM = (17, 4, 0, 0)
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


@dataclass
class AdapterCompatibilityResult:
	errors: list[str]
	warnings: list[str]


@dataclass(frozen=True)
class NativeDependencyRequirement:
	plugin: str
	platform: str
	kind: str
	name: str
	version: tuple[int, int, int, int] | None
	minimum: tuple[int, int, int, int] | None
	maximum_exclusive: tuple[int, int, int, int] | None
	version_text: str
	relationship: str
	via: str
	ownership: str
	provided_classes: tuple[str, ...]
	binary_name: str

	@property
	def chain(self) -> str:
		if self.via:
			return f"{self.plugin} -> {self.via} -> {self.name}"
		return f"{self.plugin} -> {self.name}"


def _parse_version(value: str) -> tuple[int, int, int, int] | None:
	parts = value.split(".")
	if not 1 <= len(parts) <= 4 or any(not part.isdigit() for part in parts):
		return None
	return tuple(int(part) for part in parts) + (0,) * (4 - len(parts))


def validate_adapter_compatibility(
	descriptor: PluginDescriptor,
	platform_name: str,
	*,
	provider_versions: dict[str, str],
	adapter_versions: dict[str, str],
	network_versions: dict[str, str],
) -> AdapterCompatibilityResult:
	manifest_path = descriptor.path.parent / "adapter.json"
	try:
		with manifest_path.open(encoding="utf-8") as manifest_file:
			metadata = json.load(manifest_file)
	except (json.JSONDecodeError, OSError) as error:
		return AdapterCompatibilityResult(
			[f"adapter metadata could not be read: {error}"],
			[],
		)

	platform = metadata.get("platforms", {}).get(platform_name)
	if not isinstance(platform, dict):
		return AdapterCompatibilityResult(
			[f"adapter metadata does not support platform '{platform_name}'"],
			[],
		)
	compatibility = platform.get("compatibility")
	if not isinstance(compatibility, dict):
		return AdapterCompatibilityResult(
			[f"adapter metadata has no compatibility contract for '{platform_name}'"],
			[],
		)

	errors: list[str] = []
	warnings: list[str] = []
	components = (
		("provider SDK", "provider_sdk", provider_versions),
		("adapter", "adapter", adapter_versions),
		("network SDK", "network_sdk", network_versions),
	)
	for display_name, contract_name, observed_versions in components:
		contract = compatibility.get(contract_name)
		if not isinstance(contract, dict):
			errors.append(f"adapter metadata is missing the {display_name} compatibility range")
			continue
		minimum_text = contract.get("minimum", "")
		maximum_text = contract.get("maximum_exclusive", "")
		minimum = _parse_version(minimum_text) if isinstance(minimum_text, str) else None
		maximum = _parse_version(maximum_text) if isinstance(maximum_text, str) else None
		tested = contract.get("tested", [])
		if minimum is None or maximum is None or minimum >= maximum:
			errors.append(f"adapter metadata has an invalid {display_name} compatibility range")
			continue
		if not isinstance(tested, list):
			tested = []

		if not observed_versions:
			warnings.append(f"{display_name} version is unavailable")
			continue
		parsed_observations: dict[str, tuple[int, int, int, int]] = {}
		for source, version_text in sorted(observed_versions.items()):
			parsed = _parse_version(version_text)
			if parsed is None:
				warnings.append(
					f"{source} {display_name} version '{version_text}' could not be parsed"
				)
				continue
			parsed_observations[source] = parsed
			if parsed < minimum or parsed >= maximum:
				errors.append(
					f"{source} {display_name} version '{version_text}' is outside supported range "
					f"[{minimum_text}, {maximum_text})"
				)
			elif version_text not in tested:
				warnings.append(
					f"{source} {display_name} version '{version_text}' is supported but not tested"
				)
		if len(set(parsed_observations.values())) > 1:
			errors.append(
				f"conflicting {display_name} versions: "
				+ ", ".join(
					f"{source}={observed_versions[source]}"
					for source in sorted(parsed_observations)
				)
			)

	return AdapterCompatibilityResult(errors, warnings)


def _read_native_dependency_data(
	descriptor: PluginDescriptor,
	platform_name: str,
) -> tuple[list[dict], list[str]]:
	metadata_name = "adapter.json" if descriptor.is_ads_adapter else "native-dependencies.json"
	metadata_path = descriptor.path.parent / metadata_name
	if not metadata_path.is_file():
		return [], [
			f"plugin {descriptor.name} is missing {metadata_name}; add native dependency metadata"
		]
	try:
		with metadata_path.open(encoding="utf-8") as metadata_file:
			metadata = json.load(metadata_file)
	except (json.JSONDecodeError, OSError) as error:
		return [], [f"plugin {descriptor.name} has unreadable {metadata_name}: {error}"]
	if metadata.get("schema_version") != 1 or metadata.get("plugin") != descriptor.name:
		return [], [
			f"plugin {descriptor.name} has invalid ownership data in {metadata_name}"
		]
	platform = metadata.get("platforms", {}).get(platform_name)
	if not isinstance(platform, dict):
		return [], [
			f"plugin {descriptor.name} has no {platform_name} dependency metadata in {metadata_name}"
		]
	dependencies = platform.get("dependencies")
	if not isinstance(dependencies, list) or not dependencies:
		return [], [
			f"plugin {descriptor.name} has invalid {platform_name} dependencies in {metadata_name}"
		]
	return dependencies, []


def _parse_native_dependency_requirement(
	plugin_name: str,
	platform_name: str,
	dependency: object,
	index: int,
) -> tuple[NativeDependencyRequirement | None, list[str]]:
	prefix = f"plugin {plugin_name} {platform_name} dependency {index}"
	if not isinstance(dependency, dict):
		return None, [f"{prefix} must be an object"]
	kind = dependency.get("kind")
	name = dependency.get("name")
	relationship = dependency.get("relationship")
	ownership = dependency.get("ownership")
	via = dependency.get("via", "")
	provided_classes = dependency.get("provided_classes", [])
	binary_name = dependency.get("binary_name", "")
	errors: list[str] = []
	if kind not in {"Gradle", "Framework"}:
		errors.append(f"{prefix} has unsupported kind '{kind}'")
	expected_kind = "Gradle" if platform_name == "Android" else "Framework"
	if kind in {"Gradle", "Framework"} and kind != expected_kind:
		errors.append(f"{prefix} kind must be {expected_kind}")
	if not isinstance(name, str) or not name:
		errors.append(f"{prefix} name must not be empty")
	if relationship not in {"Direct", "Transitive"}:
		errors.append(f"{prefix} relationship must be Direct or Transitive")
	if not isinstance(via, str):
		errors.append(f"{prefix} via must be a string")
	elif relationship == "Transitive" and not via:
		errors.append(f"{prefix} must name its transitive dependency chain")
	if ownership not in {"Owned", "External"}:
		errors.append(f"{prefix} ownership must be Owned or External")
	if (
		not isinstance(provided_classes, list)
		or any(not isinstance(class_name, str) or not class_name for class_name in provided_classes)
		or len(provided_classes) != len(set(provided_classes))
	):
		errors.append(f"{prefix} provided_classes must contain unique class names")
		provided_classes = []
	if binary_name and not isinstance(binary_name, str):
		errors.append(f"{prefix} binary_name must be a string")
		binary_name = ""
	if kind == "Framework" and ownership == "Owned" and not binary_name:
		errors.append(f"{prefix} must name its owned framework binary")

	version_text = dependency.get("version")
	minimum_text = dependency.get("minimum")
	maximum_text = dependency.get("maximum_exclusive")
	version = _parse_version(version_text) if isinstance(version_text, str) else None
	minimum = _parse_version(minimum_text) if isinstance(minimum_text, str) else None
	maximum = _parse_version(maximum_text) if isinstance(maximum_text, str) else None
	if version_text is not None:
		if version is None or minimum_text is not None or maximum_text is not None:
			errors.append(f"{prefix} must define one valid exact version or one valid range")
		constraint_text = str(version_text)
	else:
		if minimum is None or maximum is None or minimum >= maximum:
			errors.append(f"{prefix} must define one valid exact version or one valid range")
		constraint_text = f"[{minimum_text}, {maximum_text})"
	if errors:
		return None, errors
	return NativeDependencyRequirement(
		plugin=plugin_name,
		platform=platform_name,
		kind=kind,
		name=name,
		version=version,
		minimum=minimum,
		maximum_exclusive=maximum,
		version_text=constraint_text,
		relationship=relationship,
		via=via,
		ownership=ownership,
		provided_classes=tuple(provided_classes),
		binary_name=binary_name,
	), []


def _native_versions_overlap(
	left: NativeDependencyRequirement,
	right: NativeDependencyRequirement,
) -> bool:
	if left.version is not None and right.version is not None:
		return left.version == right.version
	if left.version is not None:
		return (
			right.minimum is not None
			and right.maximum_exclusive is not None
			and right.minimum <= left.version < right.maximum_exclusive
		)
	if right.version is not None:
		return (
			left.minimum is not None
			and left.maximum_exclusive is not None
			and left.minimum <= right.version < left.maximum_exclusive
		)
	return (
		left.minimum is not None
		and left.maximum_exclusive is not None
		and right.minimum is not None
		and right.maximum_exclusive is not None
		and max(left.minimum, right.minimum)
		< min(left.maximum_exclusive, right.maximum_exclusive)
	)


def validate_native_dependency_compatibility(
	descriptors: dict[str, PluginDescriptor],
	enabled_plugins: set[str],
	platform_name: str,
) -> list[str]:
	requirements: list[NativeDependencyRequirement] = []
	errors: list[str] = []
	for plugin_name in sorted(enabled_plugins):
		descriptor = descriptors.get(plugin_name)
		if descriptor is None:
			errors.append(f"enabled plugin {plugin_name} has no descriptor")
			continue
		dependencies, metadata_errors = _read_native_dependency_data(
			descriptor,
			platform_name,
		)
		errors.extend(metadata_errors)
		for index, dependency in enumerate(dependencies):
			requirement, requirement_errors = _parse_native_dependency_requirement(
				plugin_name,
				platform_name,
				dependency,
				index,
			)
			errors.extend(requirement_errors)
			if requirement is not None:
				requirements.append(requirement)
	if errors:
		return errors

	by_dependency: dict[tuple[str, str], list[NativeDependencyRequirement]] = {}
	seen_requirements: set[tuple[str, str, str]] = set()
	for requirement in requirements:
		key = (requirement.kind.casefold(), requirement.name.casefold())
		by_dependency.setdefault(key, []).append(requirement)
		owner_key = (requirement.plugin, *key)
		if owner_key in seen_requirements:
			errors.append(
				f"plugin {requirement.plugin} declares duplicate native dependency {requirement.name}"
			)
		seen_requirements.add(owner_key)
	for group in by_dependency.values():
		for index, left in enumerate(group):
			for right in group[index + 1:]:
				if _native_versions_overlap(left, right):
					continue
				errors.append(
					f"{platform_name} native dependency conflict for {left.name}: "
					f"{left.chain} requires {left.version_text}; "
					f"{right.chain} requires {right.version_text}. "
					"Align the versions or disable one plugin."
				)

	class_owners: dict[str, list[NativeDependencyRequirement]] = {}
	framework_owners: dict[str, list[NativeDependencyRequirement]] = {}
	for requirement in requirements:
		if requirement.ownership != "Owned":
			continue
		for class_name in requirement.provided_classes:
			class_owners.setdefault(class_name, []).append(requirement)
		if requirement.kind == "Framework":
			framework_owners.setdefault(requirement.binary_name, []).append(requirement)
	for class_name, owners in sorted(class_owners.items()):
		if len({owner.name.casefold() for owner in owners}) > 1:
			errors.append(
				f"{platform_name} duplicate class {class_name} is owned by "
				+ ", ".join(owner.chain for owner in owners)
				+ ". Remove one dependency or exclude the duplicate classes."
			)
	for framework_name, owners in sorted(framework_owners.items()):
		if len(owners) > 1:
			errors.append(
				f"{platform_name} duplicate framework {framework_name} is owned by "
				+ ", ".join(owner.chain for owner in owners)
				+ ". Keep one framework owner and mark other references External."
			)
	return errors


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
	declared_integration_types = {
		integration_type
		for integration_type in (
			integration_types if isinstance(integration_types, list) else []
		)
		if isinstance(integration_type, str)
	}
	known_formats = {
		"Banner",
		"Interstitial",
		"Rewarded",
		"RewardedInterstitial",
		"AppOpen",
		"NativeDisplay",
		"AnchoredAdaptiveBanner",
		"MediumRectangle",
	}
	required_privacy_signals = {
		"Gdpr",
		"UsPrivacy",
		"ChildDirected",
		"UnderAgeOfConsent",
	}
	privacy_propagation_methods = {
		"AdapterAutomatic",
		"AdapterConsentConsumer",
		"ProviderForwarded",
		"NotApplicable",
	}

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

		supported_formats = platform.get("supported_formats")
		if not isinstance(supported_formats, dict) or not supported_formats:
			errors.append(
				f"adapter metadata {platform_name}.supported_formats must define every integration type"
			)
		else:
			if set(supported_formats) != declared_integration_types:
				errors.append(
					f"adapter metadata {platform_name}.supported_formats must match integration_types"
				)
			for integration_type, formats in supported_formats.items():
				if not isinstance(formats, list) or not formats:
					errors.append(
						f"adapter metadata {platform_name}.supported_formats.{integration_type} must not be empty"
					)
					continue
				if all(isinstance(format_name, str) for format_name in formats) and len(
					formats
				) != len(set(formats)):
					errors.append(
						f"adapter metadata {platform_name}.supported_formats.{integration_type} contains duplicates"
					)
				for format_name in formats:
					if not isinstance(format_name, str) or format_name not in known_formats:
						errors.append(
							f"adapter metadata {platform_name} has unsupported ad format '{format_name}'"
						)

		privacy_signals = platform.get("privacy_signals")
		if not isinstance(privacy_signals, dict):
			errors.append(
				f"adapter metadata {platform_name}.privacy_signals must be an object"
			)
		elif set(privacy_signals) != required_privacy_signals:
			errors.append(
				f"adapter metadata {platform_name}.privacy_signals must define "
				"Gdpr, UsPrivacy, ChildDirected, and UnderAgeOfConsent"
			)
		else:
			for signal_name, propagation_method in privacy_signals.items():
				if (
					not isinstance(propagation_method, str)
					or propagation_method not in privacy_propagation_methods
				):
					errors.append(
						f"adapter metadata {platform_name}.privacy_signals.{signal_name} "
						f"has unsupported propagation method '{propagation_method}'"
					)
		tested_versions = platform.get("tested_provider_sdk_versions")
		if not isinstance(tested_versions, list) or not tested_versions:
			errors.append(
				f"adapter metadata {platform_name}.tested_provider_sdk_versions must not be empty"
			)

		compatibility = platform.get("compatibility")
		required_compatibility = {"provider_sdk", "adapter", "network_sdk"}
		if (
			not isinstance(compatibility, dict)
			or set(compatibility) != required_compatibility
		):
			errors.append(
				f"adapter metadata {platform_name}.compatibility must define provider_sdk, adapter, and network_sdk"
			)
		else:
			for component_name, contract in compatibility.items():
				if not isinstance(contract, dict):
					errors.append(
						f"adapter metadata {platform_name}.{component_name} compatibility must be an object"
					)
					continue
				minimum = contract.get("minimum")
				maximum = contract.get("maximum_exclusive")
				tested_component_versions = contract.get("tested")
				parsed_minimum = _parse_version(minimum) if isinstance(minimum, str) else None
				parsed_maximum = _parse_version(maximum) if isinstance(maximum, str) else None
				if (
					parsed_minimum is None
					or parsed_maximum is None
					or parsed_minimum >= parsed_maximum
				):
					errors.append(
						f"adapter metadata {platform_name}.{component_name} compatibility range is invalid"
					)
				if not isinstance(tested_component_versions, list) or not tested_component_versions:
					errors.append(
						f"adapter metadata {platform_name}.{component_name} tested versions must not be empty"
					)
				elif len(tested_component_versions) != len(set(tested_component_versions)):
					errors.append(
						f"adapter metadata {platform_name}.{component_name} tested versions contain duplicates"
					)
				if isinstance(tested_component_versions, list):
					for tested_version in tested_component_versions:
						parsed_tested = (
							_parse_version(tested_version)
							if isinstance(tested_version, str)
							else None
						)
						if (
							parsed_tested is None
							or parsed_minimum is None
							or parsed_maximum is None
							or parsed_tested < parsed_minimum
							or parsed_tested >= parsed_maximum
						):
							errors.append(
								f"adapter metadata {platform_name}.{component_name} tested version "
								f"'{tested_version}' is outside its compatibility range"
							)
				if component_name == "provider_sdk" and isinstance(tested_component_versions, list):
					if set(tested_component_versions) != set(tested_versions or []):
						errors.append(
							f"adapter metadata {platform_name}.tested_provider_sdk_versions conflicts "
							"with compatibility tested versions"
						)
				if component_name in {"adapter", "network_sdk"} and isinstance(
					tested_component_versions,
					list,
				):
					version_field = (
						"adapter_version"
						if component_name == "adapter"
						else "network_sdk_version"
					)
					if platform.get(version_field) not in tested_component_versions:
						errors.append(
							f"adapter metadata {platform_name}.{version_field} conflicts "
							"with compatibility tested versions"
						)

		dependencies = platform.get("dependencies")
		if not isinstance(dependencies, list) or not dependencies:
			errors.append(f"adapter metadata {platform_name}.dependencies must not be empty")
		else:
			seen_dependencies: set[str] = set()
			for dependency_index, dependency in enumerate(dependencies):
				if not isinstance(dependency, dict):
					errors.append(
						f"adapter metadata {platform_name} dependency must be an object"
					)
					continue
				_, native_dependency_errors = _parse_native_dependency_requirement(
					descriptor.name,
					platform_name,
					dependency,
					dependency_index,
				)
				errors.extend(native_dependency_errors)
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

		if platform_name == "IOS":
			if not isinstance(platform.get("attribution"), dict):
				errors.append(f"adapter metadata {platform_name}.attribution must be an object")
		else:
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
		"privacy_guide",
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
	strict_versions: dict[str, tuple[str, ...]]
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
	ad_attribution_kit_ids: tuple[str, ...]
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
	required_skad_network_ids: set[str] = field(default_factory=set)
	required_ad_attribution_kit_ids: set[str] = field(default_factory=set)


@dataclass(frozen=True)
class IOSPrivacyManifestInventory:
	manifests: dict[str, dict]
	digests: dict[str, str]
	malformed_paths: tuple[str, ...]
	duplicate_paths: tuple[str, ...]


@dataclass(frozen=True)
class IOSPrivacyManifestExpectation:
	plugin: str
	bundle_path: str
	sha256: str
	tracking: bool
	tracking_domains: tuple[str, ...]
	required_reason_apis: dict[str, tuple[str, ...]]
	collected_data_types: tuple[str, ...]


@dataclass(frozen=True)
class IOSAttributionConfiguration:
	skad_network_ids: tuple[str, ...]
	ad_attribution_kit_ids: tuple[str, ...]


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


def inspect_ios_privacy_manifests(path: Path) -> IOSPrivacyManifestInventory:
	manifests: dict[str, dict] = {}
	digests: dict[str, str] = {}
	malformed_paths: list[str] = []
	duplicate_paths: list[str] = []
	seen_paths: set[str] = set()

	def inspect_manifest(name: str, contents: bytes) -> None:
		normalized_name = name.replace("\\", "/")
		key = normalized_name.casefold()
		if key in seen_paths:
			duplicate_paths.append(normalized_name)
			return
		seen_paths.add(key)
		digests[normalized_name] = hashlib.sha256(contents).hexdigest()
		try:
			manifest = plistlib.loads(contents)
		except (plistlib.InvalidFileException, ValueError, TypeError):
			malformed_paths.append(normalized_name)
			return
		if not isinstance(manifest, dict):
			malformed_paths.append(normalized_name)
			return
		manifests[normalized_name] = manifest

	if path.is_dir():
		for manifest_path in path.rglob("PrivacyInfo.xcprivacy"):
			if manifest_path.is_file():
				inspect_manifest(
					str(manifest_path.relative_to(path)),
					manifest_path.read_bytes(),
				)
	elif zipfile.is_zipfile(path):
		with zipfile.ZipFile(path) as archive:
			for entry in archive.infolist():
				if entry.is_dir() or Path(entry.filename).name != "PrivacyInfo.xcprivacy":
					continue
				inspect_manifest(entry.filename, archive.read(entry))
	elif path.name == "PrivacyInfo.xcprivacy":
		inspect_manifest(path.name, path.read_bytes())
	return IOSPrivacyManifestInventory(
		manifests,
		digests,
		tuple(sorted(malformed_paths)),
		tuple(sorted(duplicate_paths)),
	)


def _privacy_manifest_metadata(
	descriptor: PluginDescriptor,
	*,
	required: bool,
) -> tuple[list[dict], list[str]]:
	if descriptor.is_ads_adapter:
		metadata_path = descriptor.path.parent / "adapter.json"
		container_path = "platforms.IOS.privacy_manifests"
	else:
		metadata_path = descriptor.path.parent / "apple-metadata.json"
		container_path = "privacy_manifests"
	if not metadata_path.is_file():
		return (
			[],
			[f"plugin {descriptor.name} is missing {metadata_path.name}"] if required else [],
		)
	try:
		metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
	except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
		return [], [f"plugin {descriptor.name} has invalid Apple metadata: {error}"]
	if metadata.get("schema_version") != 1 or metadata.get("plugin") != descriptor.name:
		return [], [f"plugin {descriptor.name} has invalid ownership in {metadata_path.name}"]
	if descriptor.is_ads_adapter:
		manifests = metadata.get("platforms", {}).get("IOS", {}).get("privacy_manifests")
	else:
		manifests = metadata.get("privacy_manifests")
	if not isinstance(manifests, list) or not manifests:
		return [], [f"plugin {descriptor.name} has no {container_path} metadata"]
	return manifests, []


def _privacy_manifest_expectations(
	descriptor: PluginDescriptor,
	*,
	required: bool,
	validate_sources: bool,
) -> tuple[list[IOSPrivacyManifestExpectation], list[str]]:
	metadata_entries, errors = _privacy_manifest_metadata(
		descriptor,
		required=required,
	)
	expectations: list[IOSPrivacyManifestExpectation] = []
	for index, entry in enumerate(metadata_entries):
		prefix = f"plugin {descriptor.name} privacy manifest {index}"
		if not isinstance(entry, dict):
			errors.append(f"{prefix} must be an object")
			continue
		bundle_path = entry.get("bundle_path")
		checksum = entry.get("sha256")
		tracking = entry.get("tracking")
		tracking_domains = entry.get("tracking_domains")
		required_reason_apis = entry.get("required_reason_apis")
		collected_data_types = entry.get("collected_data_types")
		if (
			not isinstance(bundle_path, str)
			or not bundle_path.endswith("PrivacyInfo.xcprivacy")
			or Path(bundle_path).is_absolute()
			or ".." in Path(bundle_path).parts
		):
			errors.append(f"{prefix} has invalid bundle_path")
			continue
		if not isinstance(checksum, str) or not re.fullmatch(r"[0-9a-f]{64}", checksum):
			errors.append(f"{prefix} has invalid sha256")
			continue
		if not isinstance(tracking, bool):
			errors.append(f"{prefix} tracking must be boolean")
			continue
		if (
			not isinstance(tracking_domains, list)
			or any(not isinstance(domain, str) or not domain for domain in tracking_domains)
			or len(tracking_domains) != len(set(tracking_domains))
		):
			errors.append(f"{prefix} has invalid tracking_domains")
			continue
		if not isinstance(required_reason_apis, dict) or any(
			not isinstance(api_type, str)
			or not api_type
			or not isinstance(reasons, list)
			or not reasons
			or any(not isinstance(reason, str) or not reason for reason in reasons)
			or len(reasons) != len(set(reasons))
			for api_type, reasons in required_reason_apis.items()
		):
			errors.append(f"{prefix} has invalid required_reason_apis")
			continue
		for api_type, reasons in required_reason_apis.items():
			unreviewed_reasons = set(reasons) - IOS_REQUIRED_REASON_API_CONTRACTS.get(
				api_type,
				set(),
			)
			if unreviewed_reasons:
				errors.append(
					f"{prefix} has unreviewed required-reason API declaration "
					f"{api_type}: {', '.join(sorted(unreviewed_reasons))}"
				)
		if (
			not isinstance(collected_data_types, list)
			or any(not isinstance(data_type, str) or not data_type for data_type in collected_data_types)
			or len(collected_data_types) != len(set(collected_data_types))
		):
			errors.append(f"{prefix} has invalid collected_data_types")
			continue

		if validate_sources:
			source = entry.get("source")
			source_path = (
				safe_manifest_path(descriptor.path.parent, source)
				if isinstance(source, str)
				else None
			)
			source_contents: bytes | None = None
			if source_path is None or not source_path.is_file():
				errors.append(f"{prefix} source is missing")
			else:
				archive_manifest = entry.get("archive_manifest")
				if archive_manifest is None:
					source_contents = source_path.read_bytes()
				elif not isinstance(archive_manifest, str) or not zipfile.is_zipfile(source_path):
					errors.append(f"{prefix} archive_manifest is invalid")
				else:
					try:
						with zipfile.ZipFile(source_path) as archive:
							source_contents = archive.read(archive_manifest)
					except (KeyError, OSError, zipfile.BadZipFile):
						errors.append(f"{prefix} source archive is missing its manifest")
			if (
				source_contents is not None
				and hashlib.sha256(source_contents).hexdigest() != checksum
			):
				errors.append(f"{prefix} metadata is stale because its source checksum changed")

		expectations.append(IOSPrivacyManifestExpectation(
			descriptor.name,
			bundle_path,
			checksum,
			tracking,
			tuple(tracking_domains),
			{
				api_type: tuple(reasons)
				for api_type, reasons in required_reason_apis.items()
			},
			tuple(collected_data_types),
		))
	return expectations, errors


def _privacy_manifest_declarations(
	manifest: dict,
) -> tuple[dict[str, set[str]], set[str], set[str]]:
	api_reasons: dict[str, set[str]] = {}
	for entry in manifest.get("NSPrivacyAccessedAPITypes", []):
		if not isinstance(entry, dict):
			continue
		api_type = entry.get("NSPrivacyAccessedAPIType")
		reasons = entry.get("NSPrivacyAccessedAPITypeReasons")
		if isinstance(api_type, str) and isinstance(reasons, list):
			api_reasons.setdefault(api_type, set()).update(
				reason for reason in reasons if isinstance(reason, str)
			)
	tracking_domains = {
		domain
		for domain in manifest.get("NSPrivacyTrackingDomains", [])
		if isinstance(domain, str)
	}
	collected_data_types = {
		entry["NSPrivacyCollectedDataType"]
		for entry in manifest.get("NSPrivacyCollectedDataTypes", [])
		if isinstance(entry, dict)
		and isinstance(entry.get("NSPrivacyCollectedDataType"), str)
	}
	return api_reasons, tracking_domains, collected_data_types


def validate_ios_privacy_manifests(
	inventory: IOSPrivacyManifestInventory,
	descriptors: dict[str, PluginDescriptor],
	enabled_plugins: set[str],
) -> list[str]:
	errors = [
		f"malformed privacy manifest {path}"
		for path in inventory.malformed_paths
	]
	errors.extend(
		f"duplicate privacy manifest archive entry {path}"
		for path in inventory.duplicate_paths
	)
	expectations: list[IOSPrivacyManifestExpectation] = []
	known_expectations: list[IOSPrivacyManifestExpectation] = []
	for plugin_name, descriptor in sorted(descriptors.items()):
		if not (descriptor.is_ads_provider or descriptor.is_ads_adapter):
			continue
		plugin_expectations, metadata_errors = _privacy_manifest_expectations(
			descriptor,
			required=plugin_name in enabled_plugins,
			validate_sources=plugin_name in enabled_plugins,
		)
		known_expectations.extend(plugin_expectations)
		if plugin_name in enabled_plugins:
			expectations.extend(plugin_expectations)
			errors.extend(metadata_errors)

	bundle_owners: dict[str, set[str]] = {}
	for expectation in expectations:
		bundle_owners.setdefault(expectation.bundle_path.casefold(), set()).add(
			expectation.plugin
		)
	for bundle_path, owners in sorted(bundle_owners.items()):
		if len(owners) > 1:
			errors.append(
				f"conflicting privacy manifest metadata for {bundle_path}: "
				+ ", ".join(sorted(owners))
			)

	for expectation in expectations:
		matches = [
			path
			for path in inventory.manifests
			if path.casefold().endswith(expectation.bundle_path.casefold())
		]
		if not matches:
			errors.append(
				f"missing privacy manifest {expectation.bundle_path} for {expectation.plugin}"
			)
			continue
		if len(matches) > 1:
			errors.append(
				f"duplicate packaged privacy manifest {expectation.bundle_path} for {expectation.plugin}"
			)
			continue
		path = matches[0]
		manifest = inventory.manifests[path]
		if inventory.digests.get(path) != expectation.sha256:
			errors.append(f"privacy manifest contents changed for {expectation.plugin}: {path}")
		if manifest.get("NSPrivacyTracking", False) is not expectation.tracking:
			errors.append(f"privacy tracking declaration changed for {expectation.plugin}: {path}")
		api_reasons, tracking_domains, collected_data_types = (
			_privacy_manifest_declarations(manifest)
		)
		for domain in expectation.tracking_domains:
			if domain not in tracking_domains:
				errors.append(
					f"missing privacy tracking domain {domain} for {expectation.plugin}: {path}"
				)
		for api_type, reasons in expectation.required_reason_apis.items():
			missing_reasons = set(reasons) - api_reasons.get(api_type, set())
			if missing_reasons:
				errors.append(
					f"missing required-reason API declaration {api_type} "
					f"for {expectation.plugin}: {path}"
				)
		for data_type in expectation.collected_data_types:
			if data_type not in collected_data_types:
				errors.append(
					f"missing collected data type {data_type} for {expectation.plugin}: {path}"
				)

	for expectation in known_expectations:
		if expectation.plugin in enabled_plugins:
			continue
		if any(
			path.casefold().endswith(expectation.bundle_path.casefold())
			for path in inventory.manifests
		):
			owner_type = "adapter" if descriptors[expectation.plugin].is_ads_adapter else "provider"
			errors.append(
				f"found privacy manifest for disabled {owner_type} {expectation.plugin}"
			)
	return errors


def _ios_attribution_metadata(
	descriptor: PluginDescriptor,
) -> tuple[dict | None, list[str]]:
	metadata_path = descriptor.path.parent / (
		"adapter.json" if descriptor.is_ads_adapter else "apple-metadata.json"
	)
	try:
		metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
	except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
		return None, [f"plugin {descriptor.name} has invalid attribution metadata: {error}"]
	if metadata.get("schema_version") != 1 or metadata.get("plugin") != descriptor.name:
		return None, [f"plugin {descriptor.name} has invalid attribution metadata ownership"]
	if descriptor.is_ads_adapter:
		platform = metadata.get("platforms", {}).get("IOS")
		attribution = platform.get("attribution") if isinstance(platform, dict) else None
		expected_sdk_version = platform.get("network_sdk_version") if isinstance(platform, dict) else None
	else:
		attribution = metadata.get("attribution")
		expected_sdk_version = None
		packages_path = descriptor.path.parent / "ThirdParty/IOS/packages.json"
		try:
			packages = json.loads(packages_path.read_text(encoding="utf-8")).get("packages", [])
		except (OSError, UnicodeDecodeError, json.JSONDecodeError):
			packages = []
		for package in packages:
			if package.get("name") == "Google Mobile Ads SDK for iOS":
				expected_sdk_version = package.get("version")
				break
	if not isinstance(attribution, dict):
		return None, [f"plugin {descriptor.name} has no iOS attribution metadata"]

	errors: list[str] = []
	source_sdk_version = attribution.get("source_sdk_version")
	if (
		not isinstance(source_sdk_version, str)
		or not source_sdk_version
		or source_sdk_version != expected_sdk_version
	):
		errors.append(
			f"plugin {descriptor.name} has stale attribution metadata for SDK "
			f"{expected_sdk_version or 'unknown'}"
		)
	last_reviewed = attribution.get("last_reviewed")
	if not isinstance(last_reviewed, str) or not re.fullmatch(r"\d{4}-\d{2}-\d{2}", last_reviewed):
		errors.append(f"plugin {descriptor.name} attribution last_reviewed must use YYYY-MM-DD")
	source_url = attribution.get("source_url")
	if not isinstance(source_url, str) or not source_url.startswith("https://"):
		errors.append(f"plugin {descriptor.name} attribution source_url must use HTTPS")
	upl_path = attribution.get("upl_path")
	resolved_upl_path = (
		safe_manifest_path(descriptor.path.parent, upl_path)
		if isinstance(upl_path, str)
		else None
	)
	if resolved_upl_path is None or not resolved_upl_path.is_file():
		errors.append(f"plugin {descriptor.name} attribution upl_path is missing")

	skad_network_ids = attribution.get("skadnetwork_identifiers")
	if not isinstance(skad_network_ids, list) or any(
		not isinstance(identifier, str) for identifier in skad_network_ids
	):
		errors.append(f"plugin {descriptor.name} skadnetwork_identifiers must be an array of strings")
		skad_network_ids = []
	else:
		if len(skad_network_ids) != len(set(skad_network_ids)):
			errors.append(f"plugin {descriptor.name} SKAdNetwork identifiers contain duplicates")
		for identifier in skad_network_ids:
			if not re.fullmatch(r"[a-z0-9]{10}\.skadnetwork", identifier):
				errors.append(
					f"plugin {descriptor.name} has malformed SKAdNetwork identifier '{identifier}'"
				)

	ad_attribution_kit = attribution.get("adattributionkit")
	if not isinstance(ad_attribution_kit, dict):
		errors.append(f"plugin {descriptor.name} has no AdAttributionKit metadata")
		ad_attribution_kit = {}
	minimum_os_version = ad_attribution_kit.get("minimum_os_version")
	parsed_minimum_os_version = (
		_parse_version(minimum_os_version)
		if isinstance(minimum_os_version, str)
		else None
	)
	if (
		not isinstance(minimum_os_version, str)
		or parsed_minimum_os_version is None
	):
		errors.append(f"plugin {descriptor.name} has invalid AdAttributionKit minimum_os_version")
	elif parsed_minimum_os_version < IOS_AD_ATTRIBUTION_KIT_MINIMUM:
		errors.append(
			f"plugin {descriptor.name} AdAttributionKit availability is below iOS 17.4"
		)
	ad_attribution_kit_ids = ad_attribution_kit.get("identifiers")
	if not isinstance(ad_attribution_kit_ids, list) or any(
		not isinstance(identifier, str) for identifier in ad_attribution_kit_ids
	):
		errors.append(f"plugin {descriptor.name} AdAttributionKit identifiers must be an array of strings")
		ad_attribution_kit_ids = []
	else:
		if len(ad_attribution_kit_ids) != len(set(ad_attribution_kit_ids)):
			errors.append(f"plugin {descriptor.name} AdAttributionKit identifiers contain duplicates")
		for identifier in ad_attribution_kit_ids:
			if not re.fullmatch(r"[a-z0-9]+\.adattributionkit", identifier):
				errors.append(
					f"plugin {descriptor.name} has malformed AdAttributionKit identifier '{identifier}'"
				)
	runtime_hooks = ad_attribution_kit.get("runtime_hooks")
	if not isinstance(runtime_hooks, list) or any(
		not isinstance(hook, str) or not hook for hook in runtime_hooks
	):
		errors.append(f"plugin {descriptor.name} AdAttributionKit runtime_hooks must be an array")
	source_url = ad_attribution_kit.get("source_url")
	if not isinstance(source_url, str) or not source_url.startswith("https://"):
		errors.append(f"plugin {descriptor.name} AdAttributionKit source_url must use HTTPS")

	return {
		"upl_path": resolved_upl_path,
		"skad_network_ids": tuple(skad_network_ids),
		"ad_attribution_kit_ids": tuple(ad_attribution_kit_ids),
	}, errors


def collect_ios_attribution_configuration(
	descriptors: dict[str, PluginDescriptor],
	enabled_plugins: set[str],
) -> tuple[IOSAttributionConfiguration, list[str]]:
	errors: list[str] = []
	skad_network_ids: set[str] = set()
	ad_attribution_kit_ids: set[str] = set()
	for plugin_name in sorted(enabled_plugins):
		descriptor = descriptors.get(plugin_name)
		if descriptor is None:
			errors.append(f"unknown enabled plugin '{plugin_name}'")
			continue
		if not (descriptor.is_ads_provider or descriptor.is_ads_adapter):
			continue
		attribution, metadata_errors = _ios_attribution_metadata(descriptor)
		errors.extend(metadata_errors)
		if attribution is None:
			continue
		skad_network_ids.update(attribution["skad_network_ids"])
		ad_attribution_kit_ids.update(attribution["ad_attribution_kit_ids"])
	return IOSAttributionConfiguration(
		tuple(sorted(skad_network_ids)),
		tuple(sorted(ad_attribution_kit_ids)),
	), errors


def validate_ios_attribution_upl(descriptor: PluginDescriptor) -> list[str]:
	attribution, errors = _ios_attribution_metadata(descriptor)
	if attribution is None:
		return errors
	upl_path = attribution["upl_path"]
	if upl_path is None:
		return errors
	try:
		root = ElementTree.parse(upl_path).getroot()
	except (OSError, ElementTree.ParseError) as error:
		return errors + [f"plugin {descriptor.name} has invalid attribution UPL: {error}"]
	plist_updates = root.find("iosPListUpdates")
	if plist_updates is None:
		return errors + [f"plugin {descriptor.name} attribution UPL has no iosPListUpdates"]
	strings = [
		(element.text or "").strip()
		for element in plist_updates.findall(".//string")
	]
	actual_skad_ids = [identifier for identifier in strings if identifier.endswith(".skadnetwork")]
	actual_ad_attribution_kit_ids = [
		identifier for identifier in strings if identifier.endswith(".adattributionkit")
	]
	for label, actual, expected in (
		("SKAdNetwork", actual_skad_ids, attribution["skad_network_ids"]),
		("AdAttributionKit", actual_ad_attribution_kit_ids, attribution["ad_attribution_kit_ids"]),
	):
		if len(actual) != len(set(actual)):
			errors.append(f"plugin {descriptor.name} attribution UPL has duplicate {label} identifiers")
		if set(actual) != set(expected):
			errors.append(f"plugin {descriptor.name} attribution UPL has stale {label} identifiers")
	return errors


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
	required_privacy_manifests: set[str] = set()
	for provider in expectation.required_providers:
		required_privacy_manifests.update(
			IOS_PACKAGE_CONTRACTS.get(provider, {}).get(
				"privacy_manifest_frameworks",
				IOS_PACKAGE_CONTRACTS.get(provider, {}).get("frameworks", set()),
			)
		)
	for adapter in expectation.required_adapters:
		required_privacy_manifests.update(
			IOS_ADAPTER_PACKAGE_CONTRACTS.get(adapter, {}).get(
				"privacy_manifest_frameworks",
				set(),
			)
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
		if framework in required_privacy_manifests and find_framework_entry(
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
	strict_versions: dict[str, set[str]] = {}
	failed_coordinates: set[str] = set()
	for line in contents.splitlines():
		match = ANDROID_DEPENDENCY_PATTERN.search(line)
		if match is None:
			continue
		coordinate = f"{match.group('group')}:{match.group('name')}"
		raw_requested = match.group("requested")
		requested = _normalize_requested_version(raw_requested)
		selected = match.group("selected") or requested
		requested_versions.setdefault(coordinate, set()).add(requested)
		selected_versions.setdefault(coordinate, set()).add(selected)
		if raw_requested.startswith("{strictly "):
			strict_versions.setdefault(coordinate, set()).add(requested)
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
		{
			coordinate: tuple(sorted(versions))
			for coordinate, versions in strict_versions.items()
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
			strict = set(inventory.strict_versions.get(coordinate, ()))
			if not requested:
				errors.append(f"missing Android dependency '{coordinate}:{expected_version}' for {owner}")
				continue
			unexpected_strict = strict - {expected_version}
			if unexpected_strict:
				errors.append(
					f"Android dependency '{coordinate}' has unsupported strict version(s) "
					f"{', '.join(sorted(unexpected_strict))}; expected {expected_version}"
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


def _inspect_xml_ios_plist_root(root: ElementTree.Element) -> IOSPlistInventory:
	top_dictionary = root if root.tag == "dict" else root.find("dict")
	if top_dictionary is None:
		raise ValueError("iOS plist has no root dictionary")

	values: dict[str, list[str]] = {}
	value_types: dict[str, list[str]] = {}
	skad_network_ids: list[str] = []
	ad_attribution_kit_ids: list[str] = []
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

	def inspect_ad_attribution_kit_ids(value: ElementTree.Element, path_name: str) -> None:
		if value.tag != "array":
			malformed_entries.append(f"{path_name} must be an array")
			return
		for index, item in enumerate(value):
			if item.tag != "string" or not _plist_value_text(item).strip():
				malformed_entries.append(f"{path_name}[{index}] must be a non-empty string")
				continue
			ad_attribution_kit_ids.append(_plist_value_text(item).strip())

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
			elif key == "AdNetworkIdentifiers":
				inspect_ad_attribution_kit_ids(value, f"{path_name}.{key}")
			elif key == "CFBundleURLSchemes":
				inspect_url_schemes(value, f"{path_name}.{key}")
			inspect_value(value, f"{path_name}.{key}")
			index += 2

	inspect_dictionary(top_dictionary, "root", True)
	return IOSPlistInventory(
		{name: tuple(entries) for name, entries in values.items()},
		{name: tuple(entries) for name, entries in value_types.items()},
		tuple(skad_network_ids),
		tuple(ad_attribution_kit_ids),
		tuple(url_schemes),
		tuple(duplicate_keys),
		tuple(malformed_entries),
	)


def _inspect_binary_ios_plist_root(root: object) -> IOSPlistInventory:
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
	ad_attribution_kit_ids: list[str] = []
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
				elif key == "AdNetworkIdentifiers":
					if not isinstance(child, list):
						malformed_entries.append(f"{child_path} must be an array")
					else:
						for index, identifier in enumerate(child):
							if not isinstance(identifier, str) or not identifier.strip():
								malformed_entries.append(
									f"{child_path}[{index}] must be a non-empty string"
								)
							else:
								ad_attribution_kit_ids.append(identifier.strip())
				inspect_value(child, child_path)
		elif isinstance(value, list):
			for index, child in enumerate(value):
				inspect_value(child, f"{path_name}[{index}]")

	inspect_value(root, "root")
	return IOSPlistInventory(
		values,
		value_types,
		tuple(skad_network_ids),
		tuple(ad_attribution_kit_ids),
		tuple(url_schemes),
		(),
		tuple(malformed_entries),
	)


def inspect_ios_plist_contents(contents: bytes) -> IOSPlistInventory:
	if contents.startswith(b"bplist00"):
		return _inspect_binary_ios_plist_root(plistlib.loads(contents))
	return _inspect_xml_ios_plist_root(ElementTree.fromstring(contents))


def inspect_ios_plist(path: Path) -> IOSPlistInventory:
	return inspect_ios_plist_contents(path.read_bytes())


def inspect_ios_package_plist(path: Path) -> tuple[IOSPlistInventory | None, list[str]]:
	candidates: list[tuple[str, bytes]] = []
	if path.is_dir():
		for plist_path in path.rglob("Info.plist"):
			if plist_path.parent.suffix == ".app":
				candidates.append((str(plist_path.relative_to(path)), plist_path.read_bytes()))
	elif zipfile.is_zipfile(path):
		with zipfile.ZipFile(path) as archive:
			for entry in archive.infolist():
				normalized = entry.filename.replace("\\", "/")
				if entry.is_dir() or not re.fullmatch(r"Payload/[^/]+\.app/Info\.plist", normalized):
					continue
				candidates.append((entry.filename, archive.read(entry)))
	if not candidates:
		return None, ["missing final iOS application Info.plist"]
	if len(candidates) > 1:
		return None, ["multiple final iOS application Info.plist files found"]
	try:
		return inspect_ios_plist_contents(candidates[0][1]), []
	except (ElementTree.ParseError, plistlib.InvalidFileException, ValueError, TypeError) as error:
		return None, [f"invalid final iOS application Info.plist: {error}"]


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

	for identifier in sorted(expectation.required_skad_network_ids - set(inventory.skad_network_ids)):
		errors.append(f"missing SKAdNetworkIdentifier '{identifier}'")
	for identifier in sorted(
		expectation.required_ad_attribution_kit_ids - set(inventory.ad_attribution_kit_ids)
	):
		errors.append(f"missing AdAttributionKit network identifier '{identifier}'")

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
	for identifier in inventory.skad_network_ids:
		if not re.fullmatch(r"[a-z0-9]{10}\.skadnetwork", identifier):
			errors.append(f"malformed SKAdNetworkIdentifier '{identifier}'")
	for identifier in sorted(_duplicate_values(inventory.ad_attribution_kit_ids)):
		errors.append(f"duplicate AdAttributionKit network identifier '{identifier}'")
	for identifier in inventory.ad_attribution_kit_ids:
		if not re.fullmatch(r"[a-z0-9]+\.adattributionkit", identifier):
			errors.append(f"malformed AdAttributionKit network identifier '{identifier}'")
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


def _parse_version_observations(assignments: list[str], option_name: str) -> dict[str, str]:
	observations: dict[str, str] = {}
	for assignment in assignments:
		source, separator, version = assignment.partition("=")
		if not separator or not source or not version:
			raise ValueError(f"{option_name} must use SOURCE=VERSION")
		if source in observations:
			raise ValueError(f"{option_name} has duplicate source '{source}'")
		observations[source] = version
	return observations


def run_compatibility_command(arguments: argparse.Namespace) -> int:
	descriptors = discover_descriptors(arguments.repository)
	descriptor = descriptors.get(arguments.adapter)
	if descriptor is None or not descriptor.is_ads_adapter:
		print(
			f"ads adapter compatibility validation failed: unknown adapter '{arguments.adapter}'",
			file=sys.stderr,
		)
		return 1
	metadata_errors = validate_adapter_metadata(descriptor)
	if metadata_errors:
		for error in metadata_errors:
			print(f"ads adapter metadata validation failed: {error}", file=sys.stderr)
		return 1
	try:
		result = validate_adapter_compatibility(
			descriptor,
			arguments.platform,
			provider_versions=_parse_version_observations(
				arguments.provider_version,
				"--provider-version",
			),
			adapter_versions=_parse_version_observations(
				arguments.adapter_version,
				"--adapter-version",
			),
			network_versions=_parse_version_observations(
				arguments.network_version,
				"--network-version",
			),
		)
	except ValueError as error:
		print(f"ads adapter compatibility validation failed: {error}", file=sys.stderr)
		return 1
	for warning in result.warnings:
		print(f"ads adapter compatibility warning: {warning}", file=sys.stderr)
	if result.errors:
		for error in result.errors:
			print(f"ads adapter compatibility validation failed: {error}", file=sys.stderr)
		return 1
	print("OpenMobile Ads adapter compatibility validation passed.")
	return 0


def run_native_conflicts_command(arguments: argparse.Namespace) -> int:
	descriptors = discover_descriptors(arguments.repository)
	configuration = resolve_configuration(
		descriptors,
		arguments.plugin,
		platform=arguments.platform,
		target_type="Game",
	)
	errors = validate_native_dependency_compatibility(
		descriptors,
		configuration.ads_providers | configuration.ads_adapters,
		arguments.platform,
	)
	if errors:
		for error in errors:
			print(f"ads native dependency validation failed: {error}", file=sys.stderr)
		return 1
	print("OpenMobile Ads native dependency validation passed.")
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
	required_providers = set(arguments.require_provider)
	required_adapters = set(arguments.require_adapter)
	errors = validate_package(
		inventory,
		PackageExpectation(
			platform=arguments.platform,
			architectures=set(arguments.architecture),
			required_providers=required_providers,
			forbidden_providers=set(arguments.forbid_provider),
			required_adapters=required_adapters,
			forbidden_adapters=set(arguments.forbid_adapter),
		),
	)
	if arguments.platform == "IOS":
		descriptors = discover_descriptors(arguments.repository)
		enabled_plugins = required_providers | required_adapters
		errors.extend(validate_ios_privacy_manifests(
			inspect_ios_privacy_manifests(arguments.artifact),
			descriptors,
			enabled_plugins,
		))
		attribution, attribution_errors = collect_ios_attribution_configuration(
			descriptors,
			enabled_plugins,
		)
		errors.extend(attribution_errors)
		for plugin_name in sorted(enabled_plugins):
			descriptor = descriptors.get(plugin_name)
			if descriptor is not None:
				errors.extend(validate_ios_attribution_upl(descriptor))
		plist_inventory, plist_errors = inspect_ios_package_plist(arguments.artifact)
		errors.extend(plist_errors)
		if plist_inventory is not None:
			errors.extend(validate_ios_plist(
				plist_inventory,
				IOSPlistExpectation(
					required_providers=required_providers,
					required_adapters=required_adapters,
					required_skad_network_ids=set(attribution.skad_network_ids),
					required_ad_attribution_kit_ids=set(
						attribution.ad_attribution_kit_ids
					),
				),
			))
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
	descriptors = discover_descriptors(arguments.repository)
	enabled_plugins = set(arguments.require_provider) | set(arguments.require_adapter)
	attribution, errors = collect_ios_attribution_configuration(
		descriptors,
		enabled_plugins,
	)
	for plugin_name in sorted(enabled_plugins):
		descriptor = descriptors.get(plugin_name)
		if descriptor is not None:
			errors.extend(validate_ios_attribution_upl(descriptor))
	inventory = inspect_ios_plist(arguments.plist)
	errors = validate_ios_plist(
		inventory,
		IOSPlistExpectation(
			set(arguments.require_provider),
			set(arguments.forbid_provider),
			set(arguments.require_adapter),
			set(arguments.forbid_adapter),
			expected_values,
			set(attribution.skad_network_ids),
			set(attribution.ad_attribution_kit_ids),
		),
	) + errors
	if errors:
		for error in errors:
			print(f"ads iOS plist validation failed: {error}", file=sys.stderr)
		return 1
	print(
		"OpenMobile Ads iOS plist validation passed "
		f"({len(inventory.skad_network_ids)} SKAdNetwork identifiers)."
	)
	return 0


def run_attribution_command(arguments: argparse.Namespace) -> int:
	descriptors = discover_descriptors(arguments.repository)
	configuration = resolve_configuration(
		descriptors,
		arguments.plugin,
		platform="IOS",
		target_type="Game",
	)
	enabled_plugins = configuration.ads_providers | configuration.ads_adapters
	attribution, errors = collect_ios_attribution_configuration(
		descriptors,
		enabled_plugins,
	)
	for plugin_name in sorted(enabled_plugins):
		errors.extend(validate_ios_attribution_upl(descriptors[plugin_name]))
	if errors:
		for error in errors:
			print(f"ads Apple attribution validation failed: {error}", file=sys.stderr)
		return 1
	print(json.dumps({
		"SKAdNetworkItems": [
			{"SKAdNetworkIdentifier": identifier}
			for identifier in attribution.skad_network_ids
		],
		"AdNetworkIdentifiers": list(attribution.ad_attribution_kit_ids),
	}, indent=2))
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

	compatibility_parser = subparsers.add_parser("compatibility")
	compatibility_parser.add_argument("--repository", type=Path, default=Path.cwd())
	compatibility_parser.add_argument("--adapter", required=True)
	compatibility_parser.add_argument(
		"--platform",
		choices=("Android", "IOS"),
		required=True,
	)
	compatibility_parser.add_argument("--provider-version", action="append", default=[])
	compatibility_parser.add_argument("--adapter-version", action="append", default=[])
	compatibility_parser.add_argument("--network-version", action="append", default=[])
	compatibility_parser.set_defaults(handler=run_compatibility_command)

	native_conflicts_parser = subparsers.add_parser("native-conflicts")
	native_conflicts_parser.add_argument("--repository", type=Path, default=Path.cwd())
	native_conflicts_parser.add_argument("--plugin", action="append", required=True)
	native_conflicts_parser.add_argument(
		"--platform",
		choices=("Android", "IOS"),
		required=True,
	)
	native_conflicts_parser.set_defaults(handler=run_native_conflicts_command)

	artifact_parser = subparsers.add_parser("artifact")
	artifact_parser.add_argument("artifact", type=Path)
	artifact_parser.add_argument("--require-provider", action="append", default=[])
	artifact_parser.add_argument("--forbid-provider", action="append", default=[])
	artifact_parser.add_argument("--require-adapter", action="append", default=[])
	artifact_parser.add_argument("--forbid-adapter", action="append", default=[])
	artifact_parser.set_defaults(handler=run_artifact_command)

	package_parser = subparsers.add_parser("package")
	package_parser.add_argument("artifact", type=Path)
	package_parser.add_argument("--repository", type=Path, default=Path.cwd())
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
	plist_parser.add_argument("--repository", type=Path, default=Path.cwd())
	plist_parser.add_argument("--require-provider", action="append", default=[])
	plist_parser.add_argument("--forbid-provider", action="append", default=[])
	plist_parser.add_argument("--require-adapter", action="append", default=[])
	plist_parser.add_argument("--forbid-adapter", action="append", default=[])
	plist_parser.add_argument("--expected-value", action="append", default=[])
	plist_parser.set_defaults(handler=run_plist_command)

	attribution_parser = subparsers.add_parser("attribution")
	attribution_parser.add_argument("--repository", type=Path, default=Path.cwd())
	attribution_parser.add_argument("--plugin", action="append", required=True)
	attribution_parser.set_defaults(handler=run_attribution_command)

	return parser.parse_args()


if __name__ == "__main__":
	arguments = parse_arguments()
	raise SystemExit(arguments.handler(arguments))
