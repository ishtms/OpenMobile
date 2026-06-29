import hashlib
import json
import struct
import sys
import tempfile
import unittest
import warnings
import zipfile
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "Scripts"))

from validate_ads_plugins import (
	ADAPTER_SIGNATURES,
	ArtifactExpectation,
	PackageExpectation,
	inspect_artifact,
	validate_artifact,
	validate_package,
	validate_third_party_packages,
)


ADMOB_PLUGIN = REPOSITORY_ROOT / "Providers" / "Ads" / "OpenMobileAdsAdMob"
IOS_PACKAGE_MANIFEST = ADMOB_PLUGIN / "ThirdParty" / "IOS" / "packages.json"
ADMOB_META_ADAPTER = REPOSITORY_ROOT / "Adapters" / "Ads" / "OpenMobileAdsAdMobMeta"
META_IOS_PACKAGE_MANIFEST = ADMOB_META_ADAPTER / "ThirdParty" / "IOS" / "packages.json"
ADMOB_APPLOVIN_ADAPTER = (
	REPOSITORY_ROOT / "Adapters" / "Ads" / "OpenMobileAdsAdMobAppLovin"
)
APPLOVIN_IOS_PACKAGE_MANIFEST = (
	ADMOB_APPLOVIN_ADAPTER / "ThirdParty" / "IOS" / "packages.json"
)
ADMOB_CHARTBOOST_ADAPTER = (
	REPOSITORY_ROOT / "Adapters" / "Ads" / "OpenMobileAdsAdMobChartboost"
)
CHARTBOOST_IOS_PACKAGE_MANIFEST = (
	ADMOB_CHARTBOOST_ADAPTER / "ThirdParty" / "IOS" / "packages.json"
)
ADMOB_UNITY_ADAPTER = (
	REPOSITORY_ROOT / "Adapters" / "Ads" / "OpenMobileAdsAdMobUnity"
)
UNITY_IOS_PACKAGE_MANIFEST = (
	ADMOB_UNITY_ADAPTER / "ThirdParty" / "IOS" / "packages.json"
)


def mach_o_header(cpu_type: int) -> bytes:
	return struct.pack("<IIIIIIII", 0xFEEDFACF, cpu_type, 0, 0, 0, 0, 0, 0)


def elf_header(machine: int) -> bytes:
	return b"\x7fELF\x02\x01" + (b"\0" * 12) + struct.pack("<H", machine)


def add_ios_framework(
	archive: zipfile.ZipFile,
	name: str,
	*,
	cpu_type: int = 0x0100000C,
	signed: bool = True,
	privacy_manifest: bool = True,
) -> None:
	root = f"Payload/Game.app/Frameworks/{name}.framework"
	archive.writestr(f"{root}/{name}", mach_o_header(cpu_type))
	archive.writestr(f"{root}/Info.plist", b"plist")
	if privacy_manifest:
		archive.writestr(f"{root}/PrivacyInfo.xcprivacy", b"privacy")
	if signed:
		archive.writestr(f"{root}/_CodeSignature/CodeResources", b"signature")


class AdsPackageIntegrationTests(unittest.TestCase):
	def test_ios_package_contains_only_signed_device_frameworks(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			package = Path(temporary_directory) / "Game.ipa"
			with zipfile.ZipFile(package, "w") as archive:
				add_ios_framework(archive, "GoogleMobileAds")
				add_ios_framework(archive, "UserMessagingPlatform")

			errors = validate_package(
				inspect_artifact(package),
				PackageExpectation(
					platform="IOS",
					architectures={"arm64"},
					required_providers={"OpenMobileAdsAdMob"},
				),
			)

			self.assertEqual([], errors)

	def test_ios_package_rejects_unsigned_simulator_and_build_payload(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			package = Path(temporary_directory) / "Game.ipa"
			with warnings.catch_warnings():
				warnings.simplefilter("ignore", UserWarning)
				with zipfile.ZipFile(package, "w") as archive:
					add_ios_framework(
						archive,
						"GoogleMobileAds",
						cpu_type=0x01000007,
					)
					add_ios_framework(
						archive,
						"UserMessagingPlatform",
						signed=False,
					)
					archive.writestr(
						"Payload/Game.app/Frameworks/GoogleMobileAds.framework/Headers/GADAd.h",
						b"header",
					)
					archive.writestr(
						"Payload/Game.app/GoogleMobileAds.xcframework/ios-arm64/Info.plist",
						b"plist",
					)
					archive.writestr("Payload/Game.app/duplicate.txt", b"first")
					archive.writestr("Payload/Game.app/duplicate.txt", b"second")

			errors = validate_package(
				inspect_artifact(package),
				PackageExpectation(
					platform="IOS",
					architectures={"arm64"},
					required_providers={"OpenMobileAdsAdMob"},
				),
			)

			self.assertTrue(any("duplicate package entry" in error for error in errors))
			self.assertTrue(any("unsigned framework" in error for error in errors))
			self.assertTrue(any("x86_64" in error for error in errors))
			self.assertTrue(any("build-time iOS payload" in error for error in errors))

	def test_ios_meta_adapter_package_contains_only_its_enabled_frameworks(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			package = Path(temporary_directory) / "Game.ipa"
			with zipfile.ZipFile(package, "w") as archive:
				add_ios_framework(archive, "GoogleMobileAds")
				add_ios_framework(archive, "UserMessagingPlatform")
				add_ios_framework(archive, "FBAudienceNetwork")
				archive.writestr(
					"Payload/Game.app/Game",
					mach_o_header(0x0100000C) + b" GADMediationAdapterFacebook",
				)

			errors = validate_package(
				inspect_artifact(package),
				PackageExpectation(
					platform="IOS",
					architectures={"arm64"},
					required_providers={"OpenMobileAdsAdMob"},
					required_adapters={"OpenMobileAdsAdMobMeta"},
				),
			)
			self.assertEqual([], errors)

	def test_android_package_enforces_requested_abis(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			package = Path(temporary_directory) / "Game.apk"
			with zipfile.ZipFile(package, "w") as archive:
				archive.writestr("classes.dex", b"com/google/android/gms/ads")
				archive.writestr("lib/arm64-v8a/libUnreal.so", elf_header(183))

			expectation = PackageExpectation(
				platform="Android",
				architectures={"arm64-v8a"},
				required_providers={"OpenMobileAdsAdMob"},
			)
			self.assertEqual(
				[],
				validate_package(inspect_artifact(package), expectation),
			)

			with zipfile.ZipFile(package, "a") as archive:
				archive.writestr("lib/x86_64/libUnexpected.so", elf_header(62))
				archive.writestr("libs/google-mobile-ads.aar", b"raw dependency")

			errors = validate_package(inspect_artifact(package), expectation)
			self.assertTrue(any("unexpected Android ABI x86_64" in error for error in errors))
			self.assertTrue(any("unmerged Android dependency" in error for error in errors))

	def test_android_meta_adapter_payload_is_opt_in(self) -> None:
		self.assertIn("OpenMobileAdsAdMobMeta", ADAPTER_SIGNATURES)
		with tempfile.TemporaryDirectory() as temporary_directory:
			package = Path(temporary_directory) / "Game.apk"
			with zipfile.ZipFile(package, "w") as archive:
				archive.writestr(
					"classes.dex",
					b"com/google/android/gms/ads com/google/ads/mediation/facebook/FacebookMediationAdapter",
				)

			inventory = inspect_artifact(package)
			self.assertEqual(
				[],
				validate_artifact(
					inventory,
					ArtifactExpectation(
						required_providers={"OpenMobileAdsAdMob"},
						required_adapters={"OpenMobileAdsAdMobMeta"},
					),
				),
			)
			errors = validate_artifact(
				inventory,
				ArtifactExpectation(forbidden_adapters={"OpenMobileAdsAdMobMeta"}),
			)
			self.assertTrue(any("disabled native payload" in error for error in errors))

	def test_applovin_adapter_payload_is_opt_in(self) -> None:
		self.assertIn("OpenMobileAdsAdMobAppLovin", ADAPTER_SIGNATURES)
		with tempfile.TemporaryDirectory() as temporary_directory:
			android_package = Path(temporary_directory) / "Game.apk"
			with zipfile.ZipFile(android_package, "w") as archive:
				archive.writestr(
					"classes.dex",
					b"com/google/android/gms/ads "
					b"com/google/ads/mediation/applovin/AppLovinMediationAdapter",
				)

			android_inventory = inspect_artifact(android_package)
			self.assertEqual(
				[],
				validate_artifact(
					android_inventory,
					ArtifactExpectation(
						required_providers={"OpenMobileAdsAdMob"},
						required_adapters={"OpenMobileAdsAdMobAppLovin"},
					),
				),
			)
			self.assertTrue(any(
				"disabled native payload" in error
				for error in validate_artifact(
					android_inventory,
					ArtifactExpectation(
						forbidden_adapters={"OpenMobileAdsAdMobAppLovin"},
					),
				)
			))

			ios_package = Path(temporary_directory) / "Game.ipa"
			with zipfile.ZipFile(ios_package, "w") as archive:
				add_ios_framework(archive, "GoogleMobileAds")
				add_ios_framework(archive, "AppLovinSDK")
				archive.writestr(
					"Payload/Game.app/Game",
					mach_o_header(0x0100000C) + b" GADMediationAdapterAppLovin",
				)
			self.assertEqual(
				[],
				validate_artifact(
					inspect_artifact(ios_package),
					ArtifactExpectation(
						required_providers={"OpenMobileAdsAdMob"},
						required_adapters={"OpenMobileAdsAdMobAppLovin"},
					),
				),
			)

	def test_chartboost_adapter_payload_is_opt_in(self) -> None:
		self.assertIn("OpenMobileAdsAdMobChartboost", ADAPTER_SIGNATURES)
		with tempfile.TemporaryDirectory() as temporary_directory:
			android_package = Path(temporary_directory) / "Game.apk"
			with zipfile.ZipFile(android_package, "w") as archive:
				archive.writestr(
					"classes.dex",
					b"com/google/android/gms/ads "
					b"com/google/ads/mediation/chartboost/ChartboostMediationAdapter",
				)
			android_inventory = inspect_artifact(android_package)
			self.assertEqual(
				[],
				validate_artifact(
					android_inventory,
					ArtifactExpectation(
						required_providers={"OpenMobileAdsAdMob"},
						required_adapters={"OpenMobileAdsAdMobChartboost"},
					),
				),
			)
			self.assertTrue(any(
				"disabled native payload" in error
				for error in validate_artifact(
					android_inventory,
					ArtifactExpectation(
						forbidden_adapters={"OpenMobileAdsAdMobChartboost"},
					),
				)
			))

			ios_package = Path(temporary_directory) / "Game.ipa"
			with zipfile.ZipFile(ios_package, "w") as archive:
				add_ios_framework(archive, "GoogleMobileAds")
				archive.writestr(
					"Payload/Game.app/Game",
					b"GADMediationAdapterChartboost ChartboostSDK",
				)
				archive.writestr(
					"Payload/Game.app/OpenMobileAdsAdMobChartboostPrivacy.bundle/PrivacyInfo.xcprivacy",
					b"privacy",
				)
			self.assertEqual(
				[],
				validate_artifact(
					inspect_artifact(ios_package),
					ArtifactExpectation(
						required_providers={"OpenMobileAdsAdMob"},
						required_adapters={"OpenMobileAdsAdMobChartboost"},
					),
				),
			)

	def test_unity_adapter_payload_is_opt_in(self) -> None:
		self.assertIn("OpenMobileAdsAdMobUnity", ADAPTER_SIGNATURES)
		with tempfile.TemporaryDirectory() as temporary_directory:
			android_package = Path(temporary_directory) / "Game.apk"
			with zipfile.ZipFile(android_package, "w") as archive:
				archive.writestr(
					"classes.dex",
					b"com/google/android/gms/ads "
					b"com/google/ads/mediation/unity/UnityMediationAdapter",
				)
			android_inventory = inspect_artifact(android_package)
			self.assertEqual(
				[],
				validate_artifact(
					android_inventory,
					ArtifactExpectation(
						required_providers={"OpenMobileAdsAdMob"},
						required_adapters={"OpenMobileAdsAdMobUnity"},
					),
				),
			)
			self.assertTrue(any(
				"disabled native payload" in error
				for error in validate_artifact(
					android_inventory,
					ArtifactExpectation(
						forbidden_adapters={"OpenMobileAdsAdMobUnity"},
					),
				)
			))

			ios_package = Path(temporary_directory) / "Game.ipa"
			with zipfile.ZipFile(ios_package, "w") as archive:
				add_ios_framework(archive, "GoogleMobileAds")
				archive.writestr(
					"Payload/Game.app/Game",
					mach_o_header(0x0100000C) + b" GADMediationAdapterUnity",
				)
				archive.writestr(
					"Payload/Game.app/OpenMobileAdsAdMobUnityPrivacy.bundle/PrivacyInfo.xcprivacy",
					b"privacy",
				)
			self.assertEqual(
				[],
				validate_artifact(
					inspect_artifact(ios_package),
					ArtifactExpectation(
						required_providers={"OpenMobileAdsAdMob"},
						required_adapters={"OpenMobileAdsAdMobUnity"},
					),
				),
			)

	def test_ios_app_executable_exposes_static_adapter_markers(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			package = Path(temporary_directory) / "Game.ipa"
			with zipfile.ZipFile(package, "w") as archive:
				archive.writestr(
					"Payload/Game.app/Game",
					mach_o_header(0x0100000C)
					+ b" GADMediationAdapterAppLovin"
					+ b" GADMediationAdapterChartboost"
					+ b" GADMediationAdapterFacebook"
					+ b" GADMediationAdapterUnity",
				)

			inventory = inspect_artifact(package)

		self.assertEqual(
			{
				"OpenMobileAdsAdMobAppLovin",
				"OpenMobileAdsAdMobChartboost",
				"OpenMobileAdsAdMobMeta",
				"OpenMobileAdsAdMobUnity",
			},
			inventory.detected_adapters,
		)

	def test_ios_network_payload_does_not_substitute_for_static_adapter(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			cases = (
				("OpenMobileAdsAdMobAppLovin", "AppLovinSDK"),
				("OpenMobileAdsAdMobMeta", "FBAudienceNetwork"),
				("OpenMobileAdsAdMobUnity", "UnityAds"),
			)
			for adapter, framework in cases:
				package = root / f"{adapter}.ipa"
				with zipfile.ZipFile(package, "w") as archive:
					add_ios_framework(archive, "GoogleMobileAds")
					add_ios_framework(archive, framework)
				errors = validate_artifact(
					inspect_artifact(package),
					ArtifactExpectation(required_adapters={adapter}),
				)
				self.assertTrue(any("missing native payload" in error for error in errors))

			chartboost_package = root / "OpenMobileAdsAdMobChartboost.ipa"
			with zipfile.ZipFile(chartboost_package, "w") as archive:
				archive.writestr(
					"Payload/Game.app/OpenMobileAdsAdMobChartboostPrivacy.bundle/PrivacyInfo.xcprivacy",
					b"privacy",
				)
			chartboost_errors = validate_artifact(
				inspect_artifact(chartboost_package),
				ArtifactExpectation(
					required_adapters={"OpenMobileAdsAdMobChartboost"},
				),
			)
		self.assertTrue(any("missing native payload" in error for error in chartboost_errors))

	def test_third_party_manifest_checks_every_binary_and_license(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			root = Path(temporary_directory)
			artifact = root / "Example.xcframework.zip"
			artifact.write_bytes(b"framework archive")
			license_path = root / "Licenses" / "Example-LICENSE"
			license_path.parent.mkdir()
			license_path.write_text("license", encoding="utf-8")
			manifest = root / "packages.json"
			manifest.write_text(
				json.dumps({
					"schema_version": 1,
					"packages": [{
						"name": "Example SDK",
						"version": "1.0.0",
						"source_url": "https://example.com/sdk.zip",
						"terms_url": "https://example.com/terms",
						"redistribution": "Distribution is subject to the linked terms.",
						"license_files": ["Licenses/Example-LICENSE"],
						"artifacts": [{
							"path": artifact.name,
							"sha256": hashlib.sha256(artifact.read_bytes()).hexdigest(),
							"role": "runtime",
						}],
					}],
				}),
				encoding="utf-8",
			)

			self.assertEqual([], validate_third_party_packages(manifest))

			(root / "Unlisted.a").write_bytes(b"binary")
			artifact.write_bytes(b"changed")
			errors = validate_third_party_packages(manifest)
			self.assertTrue(any("checksum mismatch" in error for error in errors))
			self.assertTrue(any("unlisted third-party binary" in error for error in errors))

	def test_checked_in_ios_packages_have_current_provenance(self) -> None:
		self.assertEqual([], validate_third_party_packages(IOS_PACKAGE_MANIFEST))
		self.assertEqual([], validate_third_party_packages(META_IOS_PACKAGE_MANIFEST))
		self.assertEqual([], validate_third_party_packages(APPLOVIN_IOS_PACKAGE_MANIFEST))
		self.assertEqual([], validate_third_party_packages(CHARTBOOST_IOS_PACKAGE_MANIFEST))
		self.assertEqual([], validate_third_party_packages(UNITY_IOS_PACKAGE_MANIFEST))

	def test_ios_build_rules_embed_both_google_frameworks(self) -> None:
		build_rules = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "OpenMobileAdsAdMobIOS.Build.cs"
		).read_text(encoding="utf-8")

		self.assertEqual(2, build_rules.count("Framework.FrameworkMode.LinkAndCopy"))
		self.assertIn('"GoogleMobileAds"', build_rules)
		self.assertIn('"UserMessagingPlatform"', build_rules)

	def test_ios_meta_build_rules_embed_only_adapter_frameworks(self) -> None:
		build_rules = (
			ADMOB_META_ADAPTER
			/ "Source"
			/ "OpenMobileAdsAdMobMetaIOS"
			/ "OpenMobileAdsAdMobMetaIOS.Build.cs"
		).read_text(encoding="utf-8")

		self.assertEqual(1, build_rules.count("Framework.FrameworkMode.LinkAndCopy"))
		self.assertIn('"MetaAdapter"', build_rules)
		self.assertIn('"FBAudienceNetwork"', build_rules)
		self.assertNotIn('"GoogleMobileAds"', build_rules)

	def test_ios_applovin_build_rules_embed_only_the_network_framework(self) -> None:
		build_rules = (
			ADMOB_APPLOVIN_ADAPTER
			/ "Source"
			/ "OpenMobileAdsAdMobAppLovinIOS"
			/ "OpenMobileAdsAdMobAppLovinIOS.Build.cs"
		).read_text(encoding="utf-8")

		self.assertEqual(1, build_rules.count("Framework.FrameworkMode.LinkAndCopy"))
		self.assertIn('"AppLovinAdapter"', build_rules)
		self.assertIn('"AppLovinSDK"', build_rules)
		self.assertNotIn('new Framework(\n\t\t\t"GoogleMobileAds"', build_rules)

	def test_ios_chartboost_build_rules_link_static_payload_and_bundle_privacy(self) -> None:
		build_rules = (
			ADMOB_CHARTBOOST_ADAPTER
			/ "Source/OpenMobileAdsAdMobChartboostIOS"
			/ "OpenMobileAdsAdMobChartboostIOS.Build.cs"
		).read_text(encoding="utf-8")

		self.assertEqual(2, build_rules.count("Framework.FrameworkMode.Link"))
		self.assertNotIn("Framework.FrameworkMode.LinkAndCopy", build_rules)
		self.assertIn('"ChartboostAdapter"', build_rules)
		self.assertIn('"ChartboostSDK"', build_rules)
		self.assertIn("OpenMobileAdsAdMobChartboostPrivacy.bundle", build_rules)
		self.assertNotIn('new Framework(\n\t\t\t"GoogleMobileAds"', build_rules)

		privacy_bundle = (
			ADMOB_CHARTBOOST_ADAPTER
			/ "Resources/IOS/OpenMobileAdsAdMobChartboostPrivacy.bundle/PrivacyInfo.xcprivacy"
		)
		with zipfile.ZipFile(
			ADMOB_CHARTBOOST_ADAPTER / "ThirdParty/IOS/ChartboostSDK.xcframework.zip"
		) as archive:
			packaged_manifest = archive.read(
				"ChartboostSDK.xcframework/ios-arm64/ChartboostSDK.framework/PrivacyInfo.xcprivacy"
			)
		self.assertEqual(packaged_manifest, privacy_bundle.read_bytes())

	def test_ios_unity_build_rules_link_static_payload_and_bundle_privacy(self) -> None:
		build_rules = (
			ADMOB_UNITY_ADAPTER
			/ "Source/OpenMobileAdsAdMobUnityIOS"
			/ "OpenMobileAdsAdMobUnityIOS.Build.cs"
		).read_text(encoding="utf-8")

		self.assertEqual(2, build_rules.count("Framework.FrameworkMode.Link"))
		self.assertNotIn("Framework.FrameworkMode.LinkAndCopy", build_rules)
		self.assertIn('"UnityAdapter"', build_rules)
		self.assertIn('"UnityAds"', build_rules)
		self.assertIn("OpenMobileAdsAdMobUnityPrivacy.bundle", build_rules)
		self.assertNotIn('new Framework(\n\t\t\t"GoogleMobileAds"', build_rules)

		privacy_bundle = (
			ADMOB_UNITY_ADAPTER
			/ "Resources/IOS/OpenMobileAdsAdMobUnityPrivacy.bundle/PrivacyInfo.xcprivacy"
		)
		with zipfile.ZipFile(
			ADMOB_UNITY_ADAPTER / "ThirdParty/IOS/UnityAds.xcframework.zip"
		) as archive:
			packaged_manifest = archive.read(
				"UnityAds.xcframework/ios-arm64/UnityAds.framework/PrivacyInfo.xcprivacy"
			)
		self.assertEqual(packaged_manifest, privacy_bundle.read_bytes())


if __name__ == "__main__":
	unittest.main()
