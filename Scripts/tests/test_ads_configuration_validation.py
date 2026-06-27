import json
import plistlib
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "Scripts"))

from validate_ads_plugins import (
	ADAPTER_SIGNATURES,
	AndroidDependencyExpectation,
	AndroidManifestExpectation,
	ArtifactExpectation,
	IOSPlistExpectation,
	PluginDescriptor,
	PROVIDER_SIGNATURES,
	collect_ios_attribution_configuration,
	discover_descriptors,
	inspect_artifact,
	inspect_android_dependency_graph,
	inspect_android_manifest,
	inspect_ios_plist,
	resolve_configuration,
	validate_artifact,
	validate_android_dependencies,
	validate_android_manifest,
	validate_adapter_metadata,
	validate_adapter_compatibility,
	validate_ios_plist,
	validate_native_dependency_compatibility,
)


TEST_PROVIDER_SIGNATURES = {
	**PROVIDER_SIGNATURES,
	"OpenMobileAdsMock": (b"openmobileadsmockpayload",),
}
TEST_ADAPTER_SIGNATURES = {
	**ADAPTER_SIGNATURES,
	"OpenMobileAdsMockAdapter": (b"openmobileadsmockadapterpayload",),
}


def repository_descriptor(relative_path: str) -> PluginDescriptor:
	path = REPOSITORY_ROOT / relative_path
	return PluginDescriptor.load(path)


def native_dependency_fixture(
	root: Path,
	name: str,
	platform: str,
	dependencies: list[dict] | None,
) -> PluginDescriptor:
	plugin_root = root / name
	plugin_root.mkdir()
	descriptor_path = plugin_root / f"{name}.uplugin"
	descriptor_path.write_text(
		json.dumps({
			"Plugins": [
				{"Name": "OpenMobileCore", "Enabled": True},
				{"Name": "OpenMobileAds", "Enabled": True},
			],
		}),
		encoding="utf-8",
	)
	if dependencies is not None:
		(plugin_root / "native-dependencies.json").write_text(
			json.dumps({
				"schema_version": 1,
				"plugin": name,
				"platforms": {
					platform: {"dependencies": dependencies},
				},
			}),
			encoding="utf-8",
		)
	return PluginDescriptor.load(descriptor_path)


class AdsConfigurationValidationTests(unittest.TestCase):
	def setUp(self) -> None:
		self.core = repository_descriptor("Foundation/OpenMobileCore/OpenMobileCore.uplugin")
		self.service = repository_descriptor("Services/OpenMobileAds/OpenMobileAds.uplugin")
		self.admob = repository_descriptor(
			"Providers/Ads/OpenMobileAdsAdMob/OpenMobileAdsAdMob.uplugin"
		)
		self.admob_meta = repository_descriptor(
			"Adapters/Ads/OpenMobileAdsAdMobMeta/OpenMobileAdsAdMobMeta.uplugin"
		)
		self.admob_applovin = repository_descriptor(
			"Adapters/Ads/OpenMobileAdsAdMobAppLovin/OpenMobileAdsAdMobAppLovin.uplugin"
		)
		self.admob_chartboost = repository_descriptor(
			"Adapters/Ads/OpenMobileAdsAdMobChartboost/OpenMobileAdsAdMobChartboost.uplugin"
		)
		mock_data = {
			"Modules": [
				{
					"Name": "OpenMobileAdsMock",
					"Type": "DeveloperTool",
					"LoadingPhase": "Default",
				}
			],
			"Plugins": [
				{"Name": "OpenMobileCore", "Enabled": True},
				{"Name": "OpenMobileAds", "Enabled": True},
			],
		}
		self.mock = PluginDescriptor("OpenMobileAdsMock", Path("mock.uplugin"), mock_data)
		adapter_data = {
			"OpenMobileAdsType": "MediationAdapter",
			"Modules": [
				{
					"Name": "OpenMobileAdsMockAdapter",
					"Type": "Runtime",
					"LoadingPhase": "Default",
				}
			],
			"Plugins": [
				{"Name": "OpenMobileCore", "Enabled": True},
				{"Name": "OpenMobileAds", "Enabled": True},
				{"Name": "OpenMobileAdsMock", "Enabled": True},
			],
		}
		self.mock_adapter = PluginDescriptor(
			"OpenMobileAdsMockAdapter",
			Path("mock-adapter.uplugin"),
			adapter_data,
		)
		self.descriptors = {
			descriptor.name: descriptor
			for descriptor in (
				self.core,
				self.service,
				self.admob,
				self.admob_applovin,
				self.admob_chartboost,
				self.admob_meta,
				self.mock,
				self.mock_adapter,
			)
		}

	def test_service_only_configuration_has_no_provider_payload(self) -> None:
		configuration = resolve_configuration(
			self.descriptors,
			["OpenMobileAds"],
			platform="Android",
			target_type="Game",
		)

		self.assertEqual({"OpenMobileAds", "OpenMobileCore"}, configuration.plugins)
		self.assertEqual({"OpenMobileAds", "OpenMobileCore"}, configuration.modules)
		self.assertEqual(set(), configuration.ads_providers)
		self.assertEqual(set(), configuration.ads_adapters)

	def test_repository_discovery_includes_adapter_plugins(self) -> None:
		descriptors = discover_descriptors(REPOSITORY_ROOT)
		self.assertIn("OpenMobileAdsAdMobMeta", descriptors)
		self.assertTrue(descriptors["OpenMobileAdsAdMobMeta"].is_ads_adapter)
		self.assertIn("OpenMobileAdsAdMobAppLovin", descriptors)
		self.assertTrue(descriptors["OpenMobileAdsAdMobAppLovin"].is_ads_adapter)
		self.assertIn("OpenMobileAdsAdMobChartboost", descriptors)
		self.assertTrue(descriptors["OpenMobileAdsAdMobChartboost"].is_ads_adapter)

	def test_single_provider_configuration_selects_one_platform_module(self) -> None:
		configuration = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMob"],
			platform="Android",
			target_type="Game",
		)

		self.assertEqual({"OpenMobileAdsAdMob"}, configuration.ads_providers)
		self.assertIn("OpenMobileAdsAdMobAndroid", configuration.modules)
		self.assertNotIn("OpenMobileAdsAdMobIOS", configuration.modules)
		self.assertNotIn("OpenMobileAdsAdMobEditor", configuration.modules)
		self.assertEqual(set(), configuration.ads_adapters)

	def test_multi_provider_configuration_keeps_providers_independent(self) -> None:
		configuration = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMob", "OpenMobileAdsMock"],
			platform="Mac",
			target_type="Editor",
		)

		self.assertEqual(
			{"OpenMobileAdsAdMob", "OpenMobileAdsMock"},
			configuration.ads_providers,
		)
		self.assertNotIn("OpenMobileAdsAdMob", self.mock.dependencies)

	def test_mediation_adapter_is_opt_in(self) -> None:
		provider_only = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsMock"],
			platform="Android",
			target_type="Game",
		)
		self.assertEqual(set(), provider_only.ads_adapters)
		self.assertNotIn("OpenMobileAdsMockAdapter", provider_only.modules)

		with_adapter = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsMockAdapter"],
			platform="Android",
			target_type="Game",
		)
		self.assertEqual({"OpenMobileAdsMock"}, with_adapter.ads_providers)
		self.assertEqual({"OpenMobileAdsMockAdapter"}, with_adapter.ads_adapters)
		self.assertIn("OpenMobileAdsMockAdapter", with_adapter.modules)

	def test_real_admob_meta_adapter_is_opt_in(self) -> None:
		provider_only = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMob"],
			platform="Android",
			target_type="Game",
		)
		self.assertNotIn("OpenMobileAdsAdMobMeta", provider_only.ads_adapters)
		self.assertNotIn("OpenMobileAdsAdMobMetaAndroid", provider_only.modules)

		with_adapter = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMobMeta"],
			platform="Android",
			target_type="Game",
		)
		self.assertEqual({"OpenMobileAdsAdMob"}, with_adapter.ads_providers)
		self.assertEqual({"OpenMobileAdsAdMobMeta"}, with_adapter.ads_adapters)
		self.assertIn("OpenMobileAdsAdMobMetaAndroid", with_adapter.modules)
		self.assertEqual([], validate_adapter_metadata(self.admob_meta))

		ios_adapter = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMobMeta"],
			platform="IOS",
			target_type="Game",
		)
		self.assertIn("OpenMobileAdsAdMobMetaIOS", ios_adapter.modules)
		self.assertNotIn("OpenMobileAdsAdMobMetaAndroid", ios_adapter.modules)

	def test_real_admob_applovin_adapter_is_opt_in(self) -> None:
		provider_only = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMob"],
			platform="Android",
			target_type="Game",
		)
		self.assertNotIn("OpenMobileAdsAdMobAppLovin", provider_only.ads_adapters)
		self.assertNotIn("OpenMobileAdsAdMobAppLovinAndroid", provider_only.modules)

		with_adapter = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMobAppLovin"],
			platform="Android",
			target_type="Game",
		)
		self.assertEqual({"OpenMobileAdsAdMob"}, with_adapter.ads_providers)
		self.assertEqual({"OpenMobileAdsAdMobAppLovin"}, with_adapter.ads_adapters)
		self.assertIn("OpenMobileAdsAdMobAppLovinAndroid", with_adapter.modules)
		self.assertNotIn("OpenMobileAdsAdMobAppLovinIOS", with_adapter.modules)
		self.assertEqual([], validate_adapter_metadata(self.admob_applovin))

		ios_adapter = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMobAppLovin"],
			platform="IOS",
			target_type="Game",
		)
		self.assertIn("OpenMobileAdsAdMobAppLovinIOS", ios_adapter.modules)
		self.assertNotIn("OpenMobileAdsAdMobAppLovinAndroid", ios_adapter.modules)

	def test_real_admob_chartboost_adapter_is_opt_in(self) -> None:
		provider_only = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMob"],
			platform="Android",
			target_type="Game",
		)
		self.assertNotIn("OpenMobileAdsAdMobChartboost", provider_only.ads_adapters)
		self.assertNotIn("OpenMobileAdsAdMobChartboostAndroid", provider_only.modules)

		with_adapter = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMobChartboost"],
			platform="Android",
			target_type="Game",
		)
		self.assertEqual({"OpenMobileAdsAdMob"}, with_adapter.ads_providers)
		self.assertEqual({"OpenMobileAdsAdMobChartboost"}, with_adapter.ads_adapters)
		self.assertIn("OpenMobileAdsAdMobChartboostAndroid", with_adapter.modules)
		self.assertNotIn("OpenMobileAdsAdMobChartboostIOS", with_adapter.modules)
		self.assertEqual([], validate_adapter_metadata(self.admob_chartboost))

		ios_adapter = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMobChartboost"],
			platform="IOS",
			target_type="Game",
		)
		self.assertIn("OpenMobileAdsAdMobChartboostIOS", ios_adapter.modules)
		self.assertNotIn("OpenMobileAdsAdMobChartboostAndroid", ios_adapter.modules)

	def test_adapter_metadata_requires_explicit_format_and_privacy_contracts(self) -> None:
		metadata = json.loads(
			(self.admob_meta.path.parent / "adapter.json").read_text(encoding="utf-8")
		)
		for platform_name, platform in metadata["platforms"].items():
			self.assertEqual(
				set(metadata["integration_types"]),
				set(platform["supported_formats"]),
			)
			self.assertEqual(
				{
					"Banner",
					"Interstitial",
					"Rewarded",
					"RewardedInterstitial",
				},
				set(platform["supported_formats"]["Bidding"]),
			)
			self.assertEqual(
				{
					"Gdpr",
					"UsPrivacy",
					"ChildDirected",
					"UnderAgeOfConsent",
				},
				set(platform["privacy_signals"]),
			)

		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			descriptor_path = root / self.admob_meta.path.name
			descriptor_path.write_text(
				json.dumps(self.admob_meta.data),
				encoding="utf-8",
			)
			for platform in metadata["platforms"].values():
				platform.pop("supported_formats")
				platform.pop("privacy_signals")
			(root / "adapter.json").write_text(
				json.dumps(metadata),
				encoding="utf-8",
			)
			descriptor = PluginDescriptor.load(descriptor_path)

			errors = validate_adapter_metadata(descriptor)

		self.assertTrue(any("supported_formats" in error for error in errors), errors)
		self.assertTrue(any("privacy_signals" in error for error in errors), errors)

	def test_adapter_metadata_rejects_incomplete_or_mismatched_manifests(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			descriptor_path = root / "BrokenAdapter.uplugin"
			descriptor_path.write_text("{}", encoding="utf-8")
			metadata_path = root / "adapter.json"
			metadata_path.write_text(
				json.dumps({
					"schema_version": 2,
					"plugin": "OtherAdapter",
					"provider": "",
					"network": "",
					"display_name": "",
					"integration_types": ["Auction"],
					"platforms": {
						"Android": {
							"adapter_version": "",
							"network_sdk_version": "",
							"tested_provider_sdk_versions": [],
							"minimum_os_version": "",
							"dependencies": [
								{"name": "duplicate", "version": "1"},
								{"name": "duplicate", "version": "2"},
							],
						},
					},
					"sources": {},
				}),
				encoding="utf-8",
			)
			descriptor = PluginDescriptor(
				"BrokenAdapter",
				descriptor_path,
				{"OpenMobileAdsType": "MediationAdapter"},
			)

			errors = validate_adapter_metadata(descriptor)

			for expected in (
				"schema_version",
				"does not match plugin",
				"provider must not be empty",
				"network must not be empty",
				"display_name must not be empty",
				"unsupported integration type",
				"adapter_version must not be empty",
				"network_sdk_version must not be empty",
				"tested_provider_sdk_versions must not be empty",
				"compatibility must define provider_sdk, adapter, and network_sdk",
				"minimum_os_version must not be empty",
				"duplicate dependency",
				"attribution_identifiers must be an array",
				"integration_guide must not be empty",
			):
				self.assertTrue(
					any(expected in error for error in errors),
					f"missing validation for {expected}: {errors}",
				)

	def test_meta_adapter_compatibility_accepts_exact_compiled_versions(self) -> None:
		android = validate_adapter_compatibility(
			self.admob_meta,
			"Android",
			provider_versions={"compiled": "25.4.0"},
			adapter_versions={"compiled": "6.21.0.4"},
			network_versions={"compiled": "6.21.0"},
		)
		self.assertEqual([], android.errors)
		self.assertEqual([], android.warnings)

		ios = validate_adapter_compatibility(
			self.admob_meta,
			"IOS",
			provider_versions={"compiled": "13.8.0"},
			adapter_versions={"compiled": "6.22.0.0"},
			network_versions={"compiled": "6.22.0"},
		)
		self.assertEqual([], ios.errors)
		self.assertEqual([], ios.warnings)

	def test_chartboost_adapter_compatibility_rejects_unverified_sdk_updates(self) -> None:
		android = validate_adapter_compatibility(
			self.admob_chartboost,
			"Android",
			provider_versions={"compiled": "25.4.0"},
			adapter_versions={"compiled": "9.13.0.0"},
			network_versions={"compiled": "9.13.0"},
		)
		ios = validate_adapter_compatibility(
			self.admob_chartboost,
			"IOS",
			provider_versions={"compiled": "13.8.0"},
			adapter_versions={"compiled": "9.13.0.0"},
			network_versions={"compiled": "9.13.0"},
		)
		unverified = validate_adapter_compatibility(
			self.admob_chartboost,
			"IOS",
			provider_versions={"compiled": "13.8.0"},
			adapter_versions={"compiled": "9.13.0.0"},
			network_versions={"compiled": "9.14.0"},
		)

		self.assertEqual([], android.errors)
		self.assertEqual([], ios.errors)
		self.assertTrue(any("outside supported range" in error for error in unverified.errors))

	def test_adapter_metadata_rejects_versions_that_conflict_with_compatibility(self) -> None:
		metadata_path = self.admob_meta.path.parent / "adapter.json"
		metadata = json.loads(
			metadata_path.read_text(encoding="utf-8")
		)
		metadata["platforms"]["IOS"]["compatibility"]["adapter"]["tested"] = [
			"6.21.0.0"
		]
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			descriptor_path = root / self.admob_meta.path.name
			descriptor_path.write_text(
				json.dumps(self.admob_meta.data),
				encoding="utf-8",
			)
			(root / "adapter.json").write_text(
				json.dumps(metadata),
				encoding="utf-8",
			)
			descriptor = PluginDescriptor.load(descriptor_path)

			errors = validate_adapter_metadata(descriptor)

		self.assertTrue(
			any("IOS.adapter_version conflicts" in error for error in errors),
			errors,
		)

	def test_adapter_compatibility_warns_for_untested_or_missing_versions(self) -> None:
		untested = validate_adapter_compatibility(
			self.admob_meta,
			"IOS",
			provider_versions={"runtime": "13.9.0"},
			adapter_versions={"runtime": "6.22.0.0"},
			network_versions={"runtime": "6.22.0"},
		)
		self.assertEqual([], untested.errors)
		self.assertTrue(any("13.9.0" in warning and "not tested" in warning for warning in untested.warnings))

		missing = validate_adapter_compatibility(
			self.admob_meta,
			"Android",
			provider_versions={},
			adapter_versions={},
			network_versions={},
		)
		self.assertEqual([], missing.errors)
		self.assertEqual(3, len(missing.warnings))
		self.assertTrue(all("unavailable" in warning for warning in missing.warnings))

	def test_adapter_compatibility_rejects_known_conflicts_and_warns_for_unknown_values(self) -> None:
		conflicting = validate_adapter_compatibility(
			self.admob_meta,
			"IOS",
			provider_versions={"compiled": "14.0.0"},
			adapter_versions={"compiled": "6.21.1.1"},
			network_versions={"compiled": "6.21.1"},
		)
		self.assertEqual(3, len(conflicting.errors))
		self.assertTrue(all("outside supported range" in error for error in conflicting.errors))

		unknown = validate_adapter_compatibility(
			self.admob_meta,
			"IOS",
			provider_versions={"runtime": "future"},
			adapter_versions={"runtime": "6.22.0.0"},
			network_versions={"runtime": "6.22.0"},
		)
		self.assertEqual([], unknown.errors)
		self.assertTrue(any("could not be parsed" in warning for warning in unknown.warnings))

		version_skew = validate_adapter_compatibility(
			self.admob_meta,
			"IOS",
			provider_versions={"compiled": "13.8.0", "runtime": "13.9.0"},
			adapter_versions={"compiled": "6.22.0.0"},
			network_versions={"compiled": "6.22.0"},
		)
		self.assertTrue(any("conflicting provider SDK versions" in error for error in version_skew.errors))

	def test_compatibility_command_checks_compiled_and_runtime_versions(self) -> None:
		command = [
			sys.executable,
			str(REPOSITORY_ROOT / "Scripts" / "validate_ads_plugins.py"),
			"compatibility",
			"--repository",
			str(REPOSITORY_ROOT),
			"--adapter",
			"OpenMobileAdsAdMobMeta",
			"--platform",
			"IOS",
			"--provider-version",
			"compiled=13.8.0",
			"--adapter-version",
			"compiled=6.22.0.0",
			"--network-version",
			"compiled=6.22.0",
		]
		compatible = subprocess.run(command, capture_output=True, text=True)
		self.assertEqual(0, compatible.returncode, compatible.stderr)
		self.assertIn("compatibility validation passed", compatible.stdout)

		conflicting = subprocess.run(
			command + ["--provider-version", "runtime=13.9.0"],
			capture_output=True,
			text=True,
		)
		self.assertEqual(1, conflicting.returncode)
		self.assertIn("conflicting provider SDK versions", conflicting.stderr)

	def test_native_dependency_requirements_accept_the_supported_adapter_graph(self) -> None:
		for platform in ("Android", "IOS"):
			self.assertEqual(
				[],
				validate_native_dependency_compatibility(
					self.descriptors,
					{"OpenMobileAdsAdMob", "OpenMobileAdsAdMobMeta"},
					platform,
				),
			)

	def test_native_conflicts_command_resolves_enabled_provider_and_adapter_metadata(self) -> None:
		result = subprocess.run(
			[
				sys.executable,
				str(REPOSITORY_ROOT / "Scripts" / "validate_ads_plugins.py"),
				"native-conflicts",
				"--repository",
				str(REPOSITORY_ROOT),
				"--plugin",
				"OpenMobileAdsAdMobMeta",
				"--platform",
				"Android",
			],
			capture_output=True,
			text=True,
		)

		self.assertEqual(0, result.returncode, result.stderr)
		self.assertIn("native dependency validation passed", result.stdout)

	def test_native_dependency_requirements_reject_exact_range_conflicts(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			provider = native_dependency_fixture(
				root,
				"ProviderA",
				"Android",
				[{
					"kind": "Gradle",
					"name": "com.example:shared",
					"minimum": "1.0.0",
					"maximum_exclusive": "2.0.0",
					"relationship": "Direct",
					"ownership": "Owned",
				}],
			)
			adapter = native_dependency_fixture(
				root,
				"ProviderB",
				"Android",
				[{
					"kind": "Gradle",
					"name": "com.example:shared",
					"version": "2.1.0",
					"relationship": "Transitive",
					"via": "com.example:adapter",
					"ownership": "External",
				}],
			)

			errors = validate_native_dependency_compatibility(
				{provider.name: provider, adapter.name: adapter},
				{provider.name, adapter.name},
				"Android",
			)

		self.assertEqual(1, len(errors), errors)
		self.assertIn("com.example:shared", errors[0])
		self.assertIn("ProviderB -> com.example:adapter", errors[0])
		self.assertIn("align", errors[0].lower())

	def test_native_dependency_requirements_allow_external_gradle_resolution(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			first = native_dependency_fixture(
				root,
				"AdapterA",
				"Android",
				[{
					"kind": "Gradle",
					"name": "com.example:shared-runtime",
					"version": "1.0.0",
					"relationship": "Transitive",
					"via": "com.example:first-adapter",
					"ownership": "External",
				}],
			)
			second = native_dependency_fixture(
				root,
				"AdapterB",
				"Android",
				[{
					"kind": "Gradle",
					"name": "com.example:shared-runtime",
					"version": "1.1.0",
					"relationship": "Transitive",
					"via": "com.example:second-adapter",
					"ownership": "External",
				}],
			)

			errors = validate_native_dependency_compatibility(
				{first.name: first, second.name: second},
				{first.name, second.name},
				"Android",
			)

		self.assertEqual([], errors)

	def test_native_dependency_requirements_report_missing_metadata(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			provider = native_dependency_fixture(
				root,
				"MissingMetadataProvider",
				"IOS",
				None,
			)

			errors = validate_native_dependency_compatibility(
				{provider.name: provider},
				{provider.name},
				"IOS",
			)

		self.assertEqual(1, len(errors), errors)
		self.assertIn("MissingMetadataProvider", errors[0])
		self.assertIn("native-dependencies.json", errors[0])

	def test_native_dependency_requirements_detect_multiple_provider_payload_collisions(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			android_a = native_dependency_fixture(
				root,
				"AndroidProviderA",
				"Android",
				[{
					"kind": "Gradle",
					"name": "com.example:first",
					"version": "1.0.0",
					"relationship": "Direct",
					"ownership": "Owned",
					"provided_classes": ["com.example.Duplicate"],
				}],
			)
			android_b = native_dependency_fixture(
				root,
				"AndroidProviderB",
				"Android",
				[{
					"kind": "Gradle",
					"name": "com.example:second",
					"version": "1.0.0",
					"relationship": "Direct",
					"ownership": "Owned",
					"provided_classes": ["com.example.Duplicate"],
				}],
			)
			ios_a = native_dependency_fixture(
				root,
				"IOSProviderA",
				"IOS",
				[{
					"kind": "Framework",
					"name": "FirstPackage",
					"version": "1.0.0",
					"relationship": "Direct",
					"ownership": "Owned",
					"binary_name": "DuplicateKit",
				}],
			)
			ios_b = native_dependency_fixture(
				root,
				"IOSProviderB",
				"IOS",
				[{
					"kind": "Framework",
					"name": "SecondPackage",
					"version": "1.0.0",
					"relationship": "Direct",
					"ownership": "Owned",
					"binary_name": "DuplicateKit",
				}],
			)

			android_errors = validate_native_dependency_compatibility(
				{android_a.name: android_a, android_b.name: android_b},
				{android_a.name, android_b.name},
				"Android",
			)
			ios_errors = validate_native_dependency_compatibility(
				{ios_a.name: ios_a, ios_b.name: ios_b},
				{ios_a.name, ios_b.name},
				"IOS",
			)

		self.assertTrue(any("duplicate class com.example.Duplicate" in error for error in android_errors))
		self.assertTrue(any("duplicate framework DuplicateKit" in error for error in ios_errors))

	def test_mediation_adapter_dependency_contract_is_enforced(self) -> None:
		malformed_adapter = PluginDescriptor(
			"MalformedAdapter",
			Path("malformed-adapter.uplugin"),
			{
				"OpenMobileAdsType": "MediationAdapter",
				"Plugins": [{"Name": "OpenMobileAds", "Enabled": True}],
			},
		)
		descriptors = {**self.descriptors, malformed_adapter.name: malformed_adapter}
		with self.assertRaisesRegex(ValueError, "exactly one provider"):
			resolve_configuration(
				descriptors,
				[malformed_adapter.name],
				platform="Android",
				target_type="Game",
			)

		provider_with_adapter = PluginDescriptor(
			"ProviderWithAdapter",
			Path("provider-with-adapter.uplugin"),
			{
				"Plugins": [
					{"Name": "OpenMobileAds", "Enabled": True},
					{"Name": "OpenMobileAdsMockAdapter", "Enabled": True},
				],
			},
		)
		descriptors[provider_with_adapter.name] = provider_with_adapter
		with self.assertRaisesRegex(ValueError, "must not depend on mediation adapter"):
			resolve_configuration(
				descriptors,
				[provider_with_adapter.name],
				platform="Android",
				target_type="Game",
			)

	def test_artifact_validation_checks_provider_payload_isolation(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			provider_artifact = Path(temporary_directory) / "provider-app.zip"
			with zipfile.ZipFile(provider_artifact, "w") as archive:
				archive.writestr("Payload/Game.app/Frameworks/GoogleMobileAds.framework/GoogleMobileAds", b"sdk")

			provider_inventory = inspect_artifact(provider_artifact)
			self.assertEqual(
				[],
				validate_artifact(
					provider_inventory,
					ArtifactExpectation(required_providers={"OpenMobileAdsAdMob"}),
				),
			)
			self.assertNotEqual(
				[],
				validate_artifact(
					provider_inventory,
					ArtifactExpectation(forbidden_providers={"OpenMobileAdsAdMob"}),
				),
			)

			service_artifact = Path(temporary_directory) / "service-app.zip"
			with zipfile.ZipFile(service_artifact, "w") as archive:
				archive.writestr("Payload/Game.app/Game", b"game")
			self.assertEqual(
				[],
				validate_artifact(
					inspect_artifact(service_artifact),
					ArtifactExpectation(forbidden_providers={"OpenMobileAdsAdMob"}),
				),
			)

	def test_multi_provider_and_optional_adapter_artifacts(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			multi_artifact = Path(temporary_directory) / "multi-provider-app.zip"
			with zipfile.ZipFile(multi_artifact, "w") as archive:
				archive.writestr(
					"Payload/Game.app/Frameworks/GoogleMobileAds.framework/GoogleMobileAds",
					b"sdk",
				)
				archive.writestr(
					"Payload/Game.app/Game.modules",
					b"openmobileadsmockpayload",
				)
			inventory = inspect_artifact(
				multi_artifact,
				provider_signatures=TEST_PROVIDER_SIGNATURES,
				adapter_signatures=TEST_ADAPTER_SIGNATURES,
			)
			self.assertEqual(
				[],
				validate_artifact(
					inventory,
					ArtifactExpectation(
						required_providers={"OpenMobileAdsAdMob", "OpenMobileAdsMock"},
						forbidden_adapters={"OpenMobileAdsMockAdapter"},
					),
				),
			)

			adapter_artifact = Path(temporary_directory) / "adapter-app.zip"
			with zipfile.ZipFile(adapter_artifact, "w") as archive:
				archive.writestr(
					"Payload/Game.app/Game.modules",
					b"openmobileadsmockpayload openmobileadsmockadapterpayload",
				)
			self.assertEqual(
				[],
				validate_artifact(
					inspect_artifact(
						adapter_artifact,
						provider_signatures=TEST_PROVIDER_SIGNATURES,
						adapter_signatures=TEST_ADAPTER_SIGNATURES,
					),
					ArtifactExpectation(
						required_providers={"OpenMobileAdsMock"},
						required_adapters={"OpenMobileAdsMockAdapter"},
					),
				),
			)

	def test_admob_android_manifest_contract(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			manifest = Path(temporary_directory) / "AndroidManifest.xml"
			manifest.write_text(
				"""<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.example.game">
    <uses-permission android:name="android.permission.INTERNET"/>
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE"/>
    <uses-permission android:name="com.google.android.gms.permission.AD_ID"/>
    <application>
        <meta-data android:name="com.google.android.gms.ads.APPLICATION_ID"
            android:value="ca-app-pub-1234567890123456~1234567890"/>
        <activity android:name="com.google.android.gms.ads.AdActivity"/>
        <service android:name="com.google.android.gms.ads.AdService"/>
        <provider android:name="com.google.android.gms.ads.MobileAdsInitProvider"
            android:authorities="com.example.game.mobileadsinitprovider"/>
    </application>
</manifest>
""",
				encoding="utf-8",
			)

			errors = validate_android_manifest(
				inspect_android_manifest(manifest),
				AndroidManifestExpectation(
					required_providers={"OpenMobileAdsAdMob"},
					expected_metadata={
						"com.google.android.gms.ads.APPLICATION_ID":
							"ca-app-pub-1234567890123456~1234567890",
					},
				),
			)

			self.assertEqual([], errors)

	def test_admob_android_manifest_reports_missing_entries(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			manifest = Path(temporary_directory) / "AndroidManifest.xml"
			manifest.write_text(
				"""<manifest xmlns:android="http://schemas.android.com/apk/res/android"
		package="com.example.game">
    <application/>
</manifest>
""",
				encoding="utf-8",
			)

			errors = validate_android_manifest(
				inspect_android_manifest(manifest),
				AndroidManifestExpectation(
					required_providers={"OpenMobileAdsAdMob"},
					expected_metadata={
						"com.google.android.gms.ads.APPLICATION_ID":
							"ca-app-pub-1234567890123456~1234567890",
					},
				),
			)

			self.assertTrue(any("APPLICATION_ID" in error for error in errors))
			self.assertTrue(any("AdActivity" in error for error in errors))
			self.assertTrue(any("AdService" in error for error in errors))
			self.assertTrue(any("MobileAdsInitProvider" in error for error in errors))

	def test_admob_android_manifest_reports_conflicts(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			manifest = Path(temporary_directory) / "AndroidManifest.xml"
			manifest.write_text(
				"""<manifest xmlns:android="http://schemas.android.com/apk/res/android"
		package="com.example.game">
	    <uses-permission android:name="android.permission.INTERNET"/>
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE"/>
    <uses-permission android:name="com.google.android.gms.permission.AD_ID"/>
    <application>
        <meta-data android:name="com.google.android.gms.ads.APPLICATION_ID"
            android:value="ca-app-pub-1234567890123456~1234567890"/>
        <meta-data android:name="com.google.android.gms.ads.APPLICATION_ID"
            android:value="ca-app-pub-9999999999999999~9999999999"/>
        <activity android:name="com.google.android.gms.ads.AdActivity"/>
        <service android:name="com.google.android.gms.ads.AdService"/>
        <provider android:name="com.google.android.gms.ads.MobileAdsInitProvider"
            android:authorities="com.example.shared"/>
        <provider android:name="com.example.OtherProvider"
            android:authorities="com.example.shared"/>
    </application>
</manifest>
""",
				encoding="utf-8",
			)

			errors = validate_android_manifest(
				inspect_android_manifest(manifest),
				AndroidManifestExpectation(
					required_providers={"OpenMobileAdsAdMob"},
					expected_metadata={
						"com.google.android.gms.ads.APPLICATION_ID":
							"ca-app-pub-1234567890123456~1234567890",
					},
				),
			)

			self.assertTrue(any("conflicting metadata" in error for error in errors))
			self.assertTrue(any("provider authority" in error for error in errors))
			self.assertTrue(any("must use authority" in error for error in errors))

	def test_disabled_admob_has_no_owned_manifest_entries(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			manifest = Path(temporary_directory) / "AndroidManifest.xml"
			manifest.write_text(
				"""<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <application>
        <activity android:name="com.google.android.gms.ads.AdActivity"/>
    </application>
</manifest>
""",
				encoding="utf-8",
			)

			errors = validate_android_manifest(
				inspect_android_manifest(manifest),
				AndroidManifestExpectation(
					forbidden_providers={"OpenMobileAdsAdMob"},
				),
			)

			self.assertEqual(
				["found disabled Android manifest entry for OpenMobileAdsAdMob"],
				errors,
			)

	def test_admob_android_dependency_graph_uses_the_tested_sdk(self) -> None:
		inventory = inspect_android_dependency_graph(
			"""debugRuntimeClasspath - Runtime classpath of 'debug'.
+--- com.google.android.gms:play-services-ads:{strictly 25.4.0} -> 25.4.0
|    +--- com.google.android.gms:play-services-ads-api:[25.4.0] -> 25.4.0
|    \\--- org.jetbrains.kotlin:kotlin-stdlib:2.1.0
+--- com.google.android.ump:user-messaging-platform:{strictly 4.0.0} -> 4.0.0
\\--- androidx.appcompat:appcompat:1.2.0
"""
		)

		self.assertEqual(
			[],
			validate_android_dependencies(
				inventory,
				AndroidDependencyExpectation(
					required_providers={"OpenMobileAdsAdMob"},
				),
			),
		)

	def test_admob_android_dependency_graph_reports_conflicting_requests(self) -> None:
		inventory = inspect_android_dependency_graph(
			"""debugRuntimeClasspath - Runtime classpath of 'debug'.
+--- com.google.android.gms:play-services-ads:{strictly 25.4.0} -> 25.4.0
+--- com.google.android.gms:play-services-ads:{strictly 24.7.0} -> 25.4.0
\\--- com.google.android.ump:user-messaging-platform:{strictly 4.0.0} -> 4.0.0
"""
		)

		errors = validate_android_dependencies(
			inventory,
			AndroidDependencyExpectation(
				required_providers={"OpenMobileAdsAdMob"},
			),
		)

		self.assertTrue(any("24.7.0" in error for error in errors))

	def test_admob_android_dependency_graph_allows_compatible_soft_requests(self) -> None:
		inventory = inspect_android_dependency_graph(
			"""debugRuntimeClasspath - Runtime classpath of 'debug'.
+--- com.google.android.gms:play-services-ads:{strictly 25.4.0} -> 25.4.0
+--- com.google.android.ump:user-messaging-platform:{strictly 4.0.0} -> 4.0.0
\\--- com.google.android.ump:user-messaging-platform:3.2.0 -> 4.0.0
"""
		)

		errors = validate_android_dependencies(
			inventory,
			AndroidDependencyExpectation(
				required_providers={"OpenMobileAdsAdMob"},
			),
		)

		self.assertEqual([], errors)

	def test_meta_adapter_android_dependencies_match_the_versioned_manifest(self) -> None:
		inventory = inspect_android_dependency_graph(
			"""debugRuntimeClasspath - Runtime classpath of 'debug'.
+--- com.google.android.gms:play-services-ads:{strictly 25.4.0} -> 25.4.0
+--- com.google.android.ump:user-messaging-platform:{strictly 4.0.0} -> 4.0.0
+--- com.google.ads.mediation:facebook:{strictly 6.21.0.4} -> 6.21.0.4
|    +--- com.facebook.android:audience-network-sdk:6.21.0
|    +--- androidx.annotation:annotation:1.5.0
|    +--- com.google.ads.mediation:common:1.1.0
|    +--- com.google.android.gms:play-services-ads:25.4.0
|    \\--- org.jetbrains.kotlin:kotlin-stdlib:2.3.0
\\--- com.google.android.gms:play-services-ads:24.0.0 -> 25.4.0
"""
		)

		self.assertEqual(
			[],
			validate_android_dependencies(
				inventory,
				AndroidDependencyExpectation(
					required_providers={"OpenMobileAdsAdMob"},
					required_adapters={"OpenMobileAdsAdMobMeta"},
				),
			),
		)

	def test_meta_adapter_android_dependencies_reject_conflicts_and_disabled_payload(self) -> None:
		inventory = inspect_android_dependency_graph(
			"""debugRuntimeClasspath - Runtime classpath of 'debug'.
+--- com.google.ads.mediation:facebook:{strictly 6.21.0.4} -> 6.21.0.4
+--- com.facebook.android:audience-network-sdk:6.22.0
+--- androidx.annotation:annotation:1.5.0
+--- com.google.ads.mediation:common:1.1.0
+--- com.google.android.gms:play-services-ads:25.4.0
\\--- org.jetbrains.kotlin:kotlin-stdlib:2.3.0
"""
		)
		errors = validate_android_dependencies(
			inventory,
			AndroidDependencyExpectation(
				required_adapters={"OpenMobileAdsAdMobMeta"},
			),
		)
		self.assertTrue(any("6.22.0" in error for error in errors))

		disabled_errors = validate_android_dependencies(
			inventory,
			AndroidDependencyExpectation(
				forbidden_adapters={"OpenMobileAdsAdMobMeta"},
			),
		)
		self.assertEqual(
			["found disabled Android dependency for adapter OpenMobileAdsAdMobMeta"],
			disabled_errors,
		)

	def test_disabled_meta_adapter_allows_shared_provider_dependencies(self) -> None:
		inventory = inspect_android_dependency_graph(
			"""debugRuntimeClasspath - Runtime classpath of 'debug'.
+--- com.google.android.gms:play-services-ads:{strictly 25.4.0} -> 25.4.0
\\--- com.google.android.ump:user-messaging-platform:{strictly 4.0.0} -> 4.0.0
"""
		)

		self.assertEqual(
			[],
			validate_android_dependencies(
				inventory,
				AndroidDependencyExpectation(
					required_providers={"OpenMobileAdsAdMob"},
					forbidden_adapters={"OpenMobileAdsAdMobMeta"},
				),
			),
		)

	def test_disabled_admob_has_no_android_dependency(self) -> None:
		inventory = inspect_android_dependency_graph(
			"""debugRuntimeClasspath - Runtime classpath of 'debug'.
\\--- androidx.appcompat:appcompat:1.2.0
"""
		)

		self.assertEqual(
			[],
			validate_android_dependencies(
				inventory,
				AndroidDependencyExpectation(
					forbidden_providers={"OpenMobileAdsAdMob"},
				),
			),
		)

	def test_admob_ios_plist_preserves_project_owned_values(self) -> None:
		attribution, metadata_errors = collect_ios_attribution_configuration(
			self.descriptors,
			{"OpenMobileAdsAdMob"},
		)
		self.assertEqual([], metadata_errors)
		with tempfile.TemporaryDirectory() as temporary_directory:
			plist_path = Path(temporary_directory) / "Info.plist"
			plist_path.write_bytes(plistlib.dumps({
				"GADApplicationIdentifier": "ca-app-pub-1234567890123456~1234567890",
				"SKAdNetworkItems": [
					{"SKAdNetworkIdentifier": identifier}
					for identifier in attribution.skad_network_ids
				] + [{"SKAdNetworkIdentifier": "exmplbuyer.skadnetwork"}],
				"NSUserTrackingUsageDescription": "Ads help keep this game free.",
				"CFBundleURLTypes": [{
					"CFBundleURLName": "com.example.game",
					"CFBundleURLSchemes": ["openmobile-example"],
				}],
			}, fmt=plistlib.FMT_BINARY))

			inventory = inspect_ios_plist(plist_path)
			errors = validate_ios_plist(
				inventory,
				IOSPlistExpectation(
					required_providers={"OpenMobileAdsAdMob"},
						expected_values={
						"GADApplicationIdentifier":
							"ca-app-pub-1234567890123456~1234567890",
						},
						required_skad_network_ids=set(attribution.skad_network_ids),
					),
			)

			self.assertEqual([], errors)
			self.assertIn("exmplbuyer.skadnetwork", inventory.skad_network_ids)
			self.assertEqual(("openmobile-example",), inventory.url_schemes)

	def test_admob_ios_plist_reports_conflicts_and_duplicates(self) -> None:
		attribution, metadata_errors = collect_ios_attribution_configuration(
			self.descriptors,
			{"OpenMobileAdsAdMob"},
		)
		self.assertEqual([], metadata_errors)
		with tempfile.TemporaryDirectory() as temporary_directory:
			plist_path = Path(temporary_directory) / "Info.plist"
			plist_path.write_text(
				"""<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
    <key>GADApplicationIdentifier</key><string>ca-app-pub-111~111</string>
    <key>GADApplicationIdentifier</key><string>ca-app-pub-222~222</string>
    <key>SKAdNetworkItems</key>
    <array>
        <dict><key>SKAdNetworkIdentifier</key><string>cstr6suwn9.skadnetwork</string></dict>
        <dict><key>SKAdNetworkIdentifier</key><string>cstr6suwn9.skadnetwork</string></dict>
    </array>
    <key>NSUserTrackingUsageDescription</key><string></string>
    <key>CFBundleURLTypes</key>
    <array>
        <dict><key>CFBundleURLSchemes</key><array><string>duplicate</string></array></dict>
        <dict><key>CFBundleURLSchemes</key><array><string>duplicate</string></array></dict>
    </array>
</dict>
</plist>
""",
				encoding="utf-8",
			)

			errors = validate_ios_plist(
				inspect_ios_plist(plist_path),
				IOSPlistExpectation(
					required_providers={"OpenMobileAdsAdMob"},
					expected_values={
						"GADApplicationIdentifier": "ca-app-pub-333~333",
					},
					required_skad_network_ids=set(attribution.skad_network_ids),
				),
			)

			self.assertTrue(any("duplicate iOS plist key" in error for error in errors))
			self.assertTrue(any("conflicting values" in error for error in errors))
			self.assertTrue(any("duplicate SKAdNetworkIdentifier" in error for error in errors))
			self.assertTrue(any("missing SKAdNetworkIdentifier" in error for error in errors))
			self.assertTrue(any("tracking usage description is empty" in error for error in errors))
			self.assertTrue(any("duplicate URL scheme" in error for error in errors))

	def test_disabled_admob_has_no_ios_plist_entry(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			plist_path = Path(temporary_directory) / "Info.plist"
			plist_path.write_bytes(plistlib.dumps({
				"GADApplicationIdentifier": "ca-app-pub-1234567890123456~1234567890",
			}))

			self.assertEqual(
				["found disabled iOS plist entry for OpenMobileAdsAdMob"],
				validate_ios_plist(
					inspect_ios_plist(plist_path),
					IOSPlistExpectation(
						forbidden_providers={"OpenMobileAdsAdMob"},
					),
				),
			)


if __name__ == "__main__":
	unittest.main()
