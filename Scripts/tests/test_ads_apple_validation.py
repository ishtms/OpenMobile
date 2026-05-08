import argparse
import contextlib
import hashlib
import io
import json
import plistlib
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "Scripts"))

from validate_ads_plugins import (
	discover_descriptors,
	inspect_ios_privacy_manifests,
	PluginDescriptor,
	run_package_command,
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
			with zipfile.ZipFile(archive_path, "a") as archive:
				for framework in ("GoogleMobileAds", "UserMessagingPlatform"):
					root = f"Payload/Game.app/Frameworks/{framework}.framework"
					archive.writestr(
						f"{root}/{framework}",
						struct.pack("<IIIIIIII", 0xFEEDFACF, 0x0100000C, 0, 0, 0, 0, 0, 0),
					)
					archive.writestr(f"{root}/_CodeSignature/CodeResources", b"signature")

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


if __name__ == "__main__":
	unittest.main()
