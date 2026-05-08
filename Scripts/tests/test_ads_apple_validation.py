import argparse
import contextlib
import hashlib
import io
import json
import plistlib
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "Scripts"))

from validate_ads_plugins import (
	collect_ios_attribution_configuration,
	discover_descriptors,
	inspect_ios_privacy_manifests,
	PluginDescriptor,
	run_package_command,
	validate_ios_attribution_upl,
	validate_ios_privacy_manifests,
)


def framework_privacy_manifest(archive_path: Path, framework: str) -> bytes:
	with zipfile.ZipFile(archive_path) as archive:
		entry = next(
			name
			for name in archive.namelist()
			if name.endswith(
				f"/ios-arm64/{framework}.framework/PrivacyInfo.xcprivacy"
			)
		)
		return archive.read(entry)


class AdsAppleValidationTests(unittest.TestCase):
	def setUp(self) -> None:
		self.descriptors = discover_descriptors(REPOSITORY_ROOT)
		self.google_mobile_ads = framework_privacy_manifest(
			REPOSITORY_ROOT
			/ "Providers/Ads/OpenMobileAdsAdMob/ThirdParty/IOS/GoogleMobileAds.xcframework.zip",
			"GoogleMobileAds",
		)
		self.ump = framework_privacy_manifest(
			REPOSITORY_ROOT
			/ "Providers/Ads/OpenMobileAdsAdMob/ThirdParty/IOS/UserMessagingPlatform.xcframework.zip",
			"UserMessagingPlatform",
		)
		self.meta = framework_privacy_manifest(
			REPOSITORY_ROOT
			/ "Adapters/Ads/OpenMobileAdsAdMobMeta/ThirdParty/IOS/FBAudienceNetwork.xcframework.zip",
			"FBAudienceNetwork",
		)
		self.provider = (
			REPOSITORY_ROOT
			/ "Providers/Ads/OpenMobileAdsAdMob/Resources/IOS"
			/ "OpenMobileAdsAdMobPrivacy.bundle/PrivacyInfo.xcprivacy"
		).read_bytes()

	def write_archive(
		self,
		path: Path,
		*,
		include_adapter: bool,
		include_provider_bundle: bool = True,
		overrides: dict[str, bytes] | None = None,
	) -> None:
		manifests = {
			"Payload/Game.app/Frameworks/GoogleMobileAds.framework/PrivacyInfo.xcprivacy": self.google_mobile_ads,
			"Payload/Game.app/Frameworks/UserMessagingPlatform.framework/PrivacyInfo.xcprivacy": self.ump,
		}
		if include_provider_bundle:
			manifests[
				"Payload/Game.app/OpenMobileAdsAdMobPrivacy.bundle/PrivacyInfo.xcprivacy"
			] = self.provider
		if include_adapter:
			manifests[
				"Payload/Game.app/Frameworks/FBAudienceNetwork.framework/PrivacyInfo.xcprivacy"
			] = self.meta
		manifests.update(overrides or {})
		with zipfile.ZipFile(path, "w") as archive:
			for name, contents in manifests.items():
				archive.writestr(name, contents)

	@staticmethod
	def append_provider_package_payload(path: Path, plist: dict | None = None) -> None:
		with zipfile.ZipFile(path, "a") as archive:
			for framework in ("GoogleMobileAds", "UserMessagingPlatform"):
				root = f"Payload/Game.app/Frameworks/{framework}.framework"
				archive.writestr(
					f"{root}/{framework}",
					struct.pack("<IIIIIIII", 0xFEEDFACF, 0x0100000C, 0, 0, 0, 0, 0, 0),
				)
				archive.writestr(f"{root}/_CodeSignature/CodeResources", b"signature")
			if plist is not None:
				archive.writestr("Payload/Game.app/Info.plist", plistlib.dumps(plist))

	def test_enabled_provider_and_adapter_privacy_manifests_are_preserved(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			archive_path = Path(temporary_directory) / "Game.ipa"
			self.write_archive(archive_path, include_adapter=True)

			inventory = inspect_ios_privacy_manifests(archive_path)
			errors = validate_ios_privacy_manifests(
				inventory,
				self.descriptors,
				{"OpenMobileAdsAdMob", "OpenMobileAdsAdMobMeta"},
			)

		self.assertEqual(4, len(inventory.manifests))
		self.assertEqual([], errors)

	def test_disabled_adapter_privacy_manifest_is_rejected(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			archive_path = Path(temporary_directory) / "Game.ipa"
			self.write_archive(archive_path, include_adapter=True)

			errors = validate_ios_privacy_manifests(
				inspect_ios_privacy_manifests(archive_path),
				self.descriptors,
				{"OpenMobileAdsAdMob"},
			)

		self.assertTrue(any("disabled adapter" in error for error in errors), errors)

	def test_privacy_validation_detects_lost_tracking_api_and_data_declarations(self) -> None:
		meta = plistlib.loads(self.meta)
		meta.pop("NSPrivacyTrackingDomains")
		meta["NSPrivacyCollectedDataTypes"] = [
			entry
			for entry in meta["NSPrivacyCollectedDataTypes"]
			if entry["NSPrivacyCollectedDataType"]
			!= "NSPrivacyCollectedDataTypeDeviceID"
		]
		google = plistlib.loads(self.google_mobile_ads)
		google["NSPrivacyAccessedAPITypes"] = [
			entry
			for entry in google["NSPrivacyAccessedAPITypes"]
			if entry["NSPrivacyAccessedAPIType"]
			!= "NSPrivacyAccessedAPICategoryUserDefaults"
		]
		with tempfile.TemporaryDirectory() as temporary_directory:
			archive_path = Path(temporary_directory) / "Game.ipa"
			self.write_archive(
				archive_path,
				include_adapter=True,
				overrides={
					"Payload/Game.app/Frameworks/GoogleMobileAds.framework/PrivacyInfo.xcprivacy": plistlib.dumps(google),
					"Payload/Game.app/Frameworks/FBAudienceNetwork.framework/PrivacyInfo.xcprivacy": plistlib.dumps(meta),
				},
			)

			errors = validate_ios_privacy_manifests(
				inspect_ios_privacy_manifests(archive_path),
				self.descriptors,
				{"OpenMobileAdsAdMob", "OpenMobileAdsAdMobMeta"},
			)

		self.assertTrue(any("tracking domain" in error for error in errors), errors)
		self.assertTrue(any("required-reason API" in error for error in errors), errors)
		self.assertTrue(any("collected data type" in error for error in errors), errors)

	def test_malformed_privacy_manifest_is_rejected(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			archive_path = Path(temporary_directory) / "Game.ipa"
			self.write_archive(
				archive_path,
				include_adapter=False,
				overrides={
					"Payload/Game.app/Broken.bundle/PrivacyInfo.xcprivacy": b"not a plist",
				},
			)

			inventory = inspect_ios_privacy_manifests(archive_path)
			errors = validate_ios_privacy_manifests(
				inventory,
				self.descriptors,
				{"OpenMobileAdsAdMob"},
			)

		self.assertTrue(any("malformed privacy manifest" in error for error in errors), errors)

	def test_ios_package_command_requires_provider_owned_privacy_manifest(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			archive_path = Path(temporary_directory) / "Game.ipa"
			self.write_archive(
				archive_path,
				include_adapter=False,
				include_provider_bundle=False,
			)
			self.append_provider_package_payload(archive_path)

			error_output = io.StringIO()
			with contextlib.redirect_stderr(error_output):
				result = run_package_command(argparse.Namespace(
					artifact=archive_path,
					platform="IOS",
					architecture=["arm64"],
					repository=REPOSITORY_ROOT,
					require_provider=["OpenMobileAdsAdMob"],
					forbid_provider=[],
					require_adapter=[],
					forbid_adapter=[],
				))

		self.assertEqual(1, result)
		self.assertIn(
			"missing privacy manifest OpenMobileAdsAdMobPrivacy.bundle/PrivacyInfo.xcprivacy",
			error_output.getvalue(),
		)

	def test_unreviewed_required_reason_declaration_is_rejected(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			plugin_root = Path(temporary_directory) / "DummyProvider"
			plugin_root.mkdir()
			manifest_path = plugin_root / "PrivacyInfo.xcprivacy"
			manifest_path.write_bytes(self.provider)
			metadata = {
				"schema_version": 1,
				"plugin": "DummyProvider",
				"privacy_manifests": [{
					"bundle_path": "PrivacyInfo.xcprivacy",
					"source": "PrivacyInfo.xcprivacy",
					"sha256": hashlib.sha256(self.provider).hexdigest(),
					"tracking": False,
					"tracking_domains": [],
					"required_reason_apis": {
						"NSPrivacyAccessedAPICategoryUserDefaults": ["CA92.2"],
					},
					"collected_data_types": [],
				}],
			}
			(plugin_root / "apple-metadata.json").write_text(
				json.dumps(metadata),
				encoding="utf-8",
			)
			descriptor = PluginDescriptor(
				"DummyProvider",
				plugin_root / "DummyProvider.uplugin",
				{"Plugins": [{"Name": "OpenMobileAds", "Enabled": True}]},
			)

			errors = validate_ios_privacy_manifests(
				inspect_ios_privacy_manifests(manifest_path),
				{descriptor.name: descriptor},
				{descriptor.name},
			)

		self.assertTrue(any("unreviewed required-reason API" in error for error in errors), errors)

	def test_ios_package_command_validates_final_attribution_plist(self) -> None:
		configuration, metadata_errors = collect_ios_attribution_configuration(
			self.descriptors,
			{"OpenMobileAdsAdMob"},
		)
		self.assertEqual([], metadata_errors)
		missing_identifier = configuration.skad_network_ids[-1]
		with tempfile.TemporaryDirectory() as temporary_directory:
			archive_path = Path(temporary_directory) / "Game.ipa"
			self.write_archive(archive_path, include_adapter=False)
			self.append_provider_package_payload(archive_path, {
				"GADApplicationIdentifier": "ca-app-pub-1234567890123456~1234567890",
				"SKAdNetworkItems": [
					{"SKAdNetworkIdentifier": identifier}
					for identifier in configuration.skad_network_ids[:-1]
				],
			})

			error_output = io.StringIO()
			with contextlib.redirect_stderr(error_output):
				result = run_package_command(argparse.Namespace(
					artifact=archive_path,
					platform="IOS",
					architecture=["arm64"],
					repository=REPOSITORY_ROOT,
					require_provider=["OpenMobileAdsAdMob"],
					forbid_provider=[],
					require_adapter=[],
					forbid_adapter=[],
				))

		self.assertEqual(1, result)
		self.assertIn(f"missing SKAdNetworkIdentifier '{missing_identifier}'", error_output.getvalue())

	def test_ios_package_command_accepts_complete_apple_metadata(self) -> None:
		configuration, metadata_errors = collect_ios_attribution_configuration(
			self.descriptors,
			{"OpenMobileAdsAdMob"},
		)
		self.assertEqual([], metadata_errors)
		with tempfile.TemporaryDirectory() as temporary_directory:
			archive_path = Path(temporary_directory) / "Game.ipa"
			self.write_archive(archive_path, include_adapter=False)
			self.append_provider_package_payload(archive_path, {
				"GADApplicationIdentifier": "ca-app-pub-1234567890123456~1234567890",
				"SKAdNetworkItems": [
					{"SKAdNetworkIdentifier": identifier}
					for identifier in configuration.skad_network_ids
				],
			})

			output = io.StringIO()
			with contextlib.redirect_stdout(output):
				result = run_package_command(argparse.Namespace(
					artifact=archive_path,
					platform="IOS",
					architecture=["arm64"],
					repository=REPOSITORY_ROOT,
					require_provider=["OpenMobileAdsAdMob"],
					forbid_provider=[],
					require_adapter=[],
					forbid_adapter=[],
				))

		self.assertEqual(0, result, output.getvalue())

	def test_checked_in_attribution_metadata_is_deterministic_and_matches_upl(self) -> None:
		configuration, errors = collect_ios_attribution_configuration(
			self.descriptors,
			{"OpenMobileAdsAdMob", "OpenMobileAdsAdMobMeta"},
		)

		self.assertEqual([], errors)
		self.assertEqual(50, len(configuration.skad_network_ids))
		self.assertEqual(tuple(sorted(configuration.skad_network_ids)), configuration.skad_network_ids)
		self.assertIn("v9wttpbfk9.skadnetwork", configuration.skad_network_ids)
		self.assertIn("n38lu8286q.skadnetwork", configuration.skad_network_ids)
		self.assertEqual((), configuration.ad_attribution_kit_ids)
		self.assertEqual([], validate_ios_attribution_upl(
			self.descriptors["OpenMobileAdsAdMob"],
		))
		self.assertEqual([], validate_ios_attribution_upl(
			self.descriptors["OpenMobileAdsAdMobMeta"],
		))

	def test_attribution_aggregation_uses_only_enabled_adapters(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			first = self.write_attribution_adapter(
				root,
				"FirstAdapter",
				"abc123def4.skadnetwork",
				ad_attribution_kit_ids=("f2d92a.adattributionkit",),
			)
			second = self.write_attribution_adapter(
				root,
				"SecondAdapter",
				"987zyx654w.skadnetwork",
				ad_attribution_kit_ids=("2jida.adattributionkit",),
			)
			descriptors = {first.name: first, second.name: second}

			single, single_errors = collect_ios_attribution_configuration(
				descriptors,
				{first.name},
			)
			multiple, multiple_errors = collect_ios_attribution_configuration(
				descriptors,
				{first.name, second.name},
			)

		self.assertEqual([], single_errors)
		self.assertEqual(("abc123def4.skadnetwork",), single.skad_network_ids)
		self.assertEqual(("f2d92a.adattributionkit",), single.ad_attribution_kit_ids)
		self.assertEqual([], multiple_errors)
		self.assertEqual(
			("987zyx654w.skadnetwork", "abc123def4.skadnetwork"),
			multiple.skad_network_ids,
		)
		self.assertEqual(
			("2jida.adattributionkit", "f2d92a.adattributionkit"),
			multiple.ad_attribution_kit_ids,
		)

	def test_attribution_metadata_rejects_duplicates_invalid_ids_and_stale_versions(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			descriptor = self.write_attribution_adapter(
				root,
				"BrokenAdapter",
				"abc123def4.skadnetwork",
				additional_skad_ids=("abc123def4.skadnetwork", "INVALID.skadnetwork"),
				ad_attribution_kit_ids=("UPPER.adattributionkit",),
				ad_attribution_minimum_os_version="16.0",
				source_sdk_version="2.0.0",
			)

			_, errors = collect_ios_attribution_configuration(
				{descriptor.name: descriptor},
				{descriptor.name},
			)

		self.assertTrue(any("duplicates" in error for error in errors), errors)
		self.assertTrue(any("malformed SKAdNetwork" in error for error in errors), errors)
		self.assertTrue(any("malformed AdAttributionKit" in error for error in errors), errors)
		self.assertTrue(any("below iOS 17.4" in error for error in errors), errors)
		self.assertTrue(any("stale attribution metadata" in error for error in errors), errors)

	def test_attribution_command_generates_plist_inputs_from_enabled_plugins(self) -> None:
		result = subprocess.run(
			[
				sys.executable,
				str(REPOSITORY_ROOT / "Scripts/validate_ads_plugins.py"),
				"attribution",
				"--repository",
				str(REPOSITORY_ROOT),
				"--plugin",
				"OpenMobileAdsAdMobMeta",
			],
			capture_output=True,
			text=True,
			check=False,
		)

		self.assertEqual(0, result.returncode, result.stderr)
		generated = json.loads(result.stdout)
		self.assertEqual(50, len(generated["SKAdNetworkItems"]))
		self.assertEqual(
			[],
			generated["AdNetworkIdentifiers"],
		)
		self.assertEqual(
			sorted(generated["SKAdNetworkItems"], key=lambda item: item["SKAdNetworkIdentifier"]),
			generated["SKAdNetworkItems"],
		)

	@staticmethod
	def write_attribution_adapter(
		root: Path,
		name: str,
		skad_id: str,
		*,
		additional_skad_ids: tuple[str, ...] = (),
		ad_attribution_kit_ids: tuple[str, ...] = (),
		ad_attribution_minimum_os_version: str = "17.4",
		source_sdk_version: str = "1.0.0",
	) -> PluginDescriptor:
		plugin_root = root / name
		upl_path = plugin_root / "Source" / "IOS" / f"{name}_IOS_UPL.xml"
		upl_path.parent.mkdir(parents=True)
		upl_path.write_text(
			f"<root><iosPListUpdates><addElements tag=\"dict\"><string>{skad_id}</string>"
			+ "".join(f"<string>{identifier}</string>" for identifier in additional_skad_ids)
			+ "".join(f"<string>{identifier}</string>" for identifier in ad_attribution_kit_ids)
			+ "</addElements></iosPListUpdates></root>",
			encoding="utf-8",
		)
		metadata = {
			"schema_version": 1,
			"plugin": name,
			"platforms": {
				"IOS": {
					"network_sdk_version": "1.0.0",
					"attribution": {
						"source_sdk_version": source_sdk_version,
						"last_reviewed": "2026-08-22",
						"source_url": "https://example.com/attribution",
						"upl_path": str(upl_path.relative_to(plugin_root)),
						"skadnetwork_identifiers": [skad_id, *additional_skad_ids],
						"adattributionkit": {
							"minimum_os_version": ad_attribution_minimum_os_version,
							"identifiers": list(ad_attribution_kit_ids),
							"runtime_hooks": [],
							"source_url": "https://example.com/adattributionkit",
						},
					},
				},
			},
		}
		(plugin_root / "adapter.json").write_text(json.dumps(metadata), encoding="utf-8")
		return PluginDescriptor(
			name,
			plugin_root / f"{name}.uplugin",
			{"OpenMobileAdsType": "MediationAdapter"},
		)


if __name__ == "__main__":
	unittest.main()
