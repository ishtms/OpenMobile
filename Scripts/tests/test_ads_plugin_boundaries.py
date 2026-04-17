import json
import re
import sys
import unittest
import xml.etree.ElementTree as ElementTree
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "Scripts"))

from validate_ads_plugins import (
	ADAPTER_SIGNATURES,
	IOS_PLIST_CONTRACTS,
	PROVIDER_SIGNATURES,
)


ADS_PLUGIN = REPOSITORY_ROOT / "Services" / "OpenMobileAds"
ADMOB_PLUGIN = REPOSITORY_ROOT / "Providers" / "Ads" / "OpenMobileAdsAdMob"


def load_descriptor(plugin_root: Path) -> dict:
	with next(plugin_root.glob("*.uplugin")).open(encoding="utf-8") as descriptor_file:
		return json.load(descriptor_file)


class AdsPluginBoundaryTests(unittest.TestCase):
	def test_production_payload_signatures_only_name_real_integrations(self) -> None:
		self.assertNotIn("OpenMobileAdsMock", PROVIDER_SIGNATURES)
		self.assertNotIn("OpenMobileAdsMockAdapter", ADAPTER_SIGNATURES)

	def test_service_plugin_has_no_vendor_payload(self) -> None:
		text_suffixes = {".cs", ".cpp", ".h", ".ini", ".md", ".uplugin", ".xml"}
		for path in ADS_PLUGIN.rglob("*"):
			if not path.is_file() or path.suffix not in text_suffixes:
				continue
			self.assertNotIn("GoogleMobileAds", path.read_text(encoding="utf-8"))
			self.assertNotIn("play-services-ads", path.read_text(encoding="utf-8"))

		self.assertFalse((ADS_PLUGIN / "ThirdParty").exists())
		self.assertEqual([], list(ADS_PLUGIN.rglob("*_UPL.xml")))

	def test_admob_modules_match_their_platform_and_editor_boundaries(self) -> None:
		descriptor = load_descriptor(ADMOB_PLUGIN)
		modules = {module["Name"]: module for module in descriptor["Modules"]}

		self.assertEqual("Runtime", modules["OpenMobileAdsAdMob"]["Type"])
		self.assertNotIn("PlatformAllowList", modules["OpenMobileAdsAdMob"])
		self.assertEqual(["Android"], modules["OpenMobileAdsAdMobAndroid"]["PlatformAllowList"])
		self.assertEqual(["IOS"], modules["OpenMobileAdsAdMobIOS"]["PlatformAllowList"])
		self.assertEqual("Editor", modules["OpenMobileAdsAdMobEditor"]["Type"])

		module_root = ADMOB_PLUGIN / "Source" / "OpenMobileAdsAdMobEditor"
		self.assertTrue((module_root / "OpenMobileAdsAdMobEditor.Build.cs").is_file())
		self.assertTrue((module_root / "Private" / "OpenMobileAdsAdMobEditorModule.cpp").is_file())

	def test_service_editor_validation_is_not_runtime_eligible(self) -> None:
		descriptor = load_descriptor(ADS_PLUGIN)
		modules = {module["Name"]: module for module in descriptor["Modules"]}

		self.assertEqual("Runtime", modules["OpenMobileAds"]["Type"])
		self.assertEqual("Editor", modules["OpenMobileAdsEditor"]["Type"])
		self.assertEqual("DeveloperTool", modules["OpenMobileAdsConsumerTests"]["Type"])
		module_root = ADS_PLUGIN / "Source" / "OpenMobileAdsEditor"
		self.assertTrue((module_root / "OpenMobileAdsEditor.Build.cs").is_file())
		self.assertTrue((module_root / "Private" / "OpenMobileAdsEditorModule.cpp").is_file())

	def test_admob_declares_service_dependency_without_reverse_dependency(self) -> None:
		service_dependencies = {
			plugin["Name"] for plugin in load_descriptor(ADS_PLUGIN).get("Plugins", [])
		}
		provider_dependencies = {
			plugin["Name"] for plugin in load_descriptor(ADMOB_PLUGIN).get("Plugins", [])
		}

		self.assertNotIn("OpenMobileAdsAdMob", service_dependencies)
		self.assertIn("OpenMobileAds", provider_dependencies)
		self.assertIn("OpenMobileCore", provider_dependencies)

	def test_native_payload_is_owned_by_a_provider_plugin(self) -> None:
		providers_root = REPOSITORY_ROOT / "Providers" / "Ads"
		payload_paths = [
			path
			for path in providers_root.rglob("*")
			if path.is_file()
			and (
				path.name.endswith("_UPL.xml")
				or path.suffix in {".aar", ".framework", ".zip"}
			)
		]
		self.assertNotEqual([], payload_paths)

		for payload_path in payload_paths:
			owner = next(
				(
					parent
					for parent in payload_path.parents
					if list(parent.glob("*.uplugin"))
				),
				None,
			)
			self.assertIsNotNone(owner, f"{payload_path} has no provider descriptor")
			self.assertEqual(ADMOB_PLUGIN, owner)

	def test_admob_android_upl_owns_manifest_and_gradle_configuration(self) -> None:
		upl_path = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileAdsAdMob_Android_UPL.xml"
		)
		root = ElementTree.parse(upl_path).getroot()
		android_name = "{http://schemas.android.com/apk/res/android}name"
		permissions = {
			element.get(android_name)
			for element in root.findall("./androidManifestUpdates/addPermission")
		}
		self.assertEqual(
			{"android.permission.INTERNET", "android.permission.ACCESS_NETWORK_STATE"},
			permissions,
		)
		attribute_values = {
			element.get("value")
			for element in root.findall("./androidManifestUpdates/addAttribute")
		}
		self.assertIn("com.google.android.gms.ads.APPLICATION_ID", attribute_values)
		self.assertIn("$S(OpenMobileAdsAdMobAndroidAppId)", attribute_values)

		gradle_additions = ElementTree.tostring(
			root.find("buildGradleAdditions"),
			encoding="unicode",
		)
		self.assertIn("google()", gradle_additions)
		self.assertIn("mavenCentral()", gradle_additions)
		self.assertIn("com.google.android.gms:play-services-ads", gradle_additions)
		self.assertIn("strictly '25.4.0'", gradle_additions)
		self.assertIn("OpenMobileAdsAdMob_Android.gradle", gradle_additions)
		self.assertIn("OpenMobileAdsAdMob_Dependencies.gradle", gradle_additions)
		minimum_sdk_api = ElementTree.tostring(
			root.find("minimumSDKAPI"),
			encoding="unicode",
		)
		self.assertIn("35", minimum_sdk_api)
		proguard_additions = ElementTree.tostring(
			root.find("proguardAdditions"),
			encoding="unicode",
		)
		self.assertIn("AndroidThunkJava_InitializeOpenMobileRewardedAds", proguard_additions)
		self.assertIn("nativeOpenMobileRewardedAdFailed", proguard_additions)
		build_settings = ElementTree.tostring(
			root.find("registerBuildSettings"),
			encoding="unicode",
		)
		self.assertIn("OpenMobileAdsAdMobAndroidManifestContract=4", build_settings)
		self.assertIn("OpenMobileAdsAdMobAndroidDependencyContract=3", build_settings)
		self.assertIn("OpenMobileAdsAdMobAndroidRuntimeContract=1", build_settings)
		copy_destinations = {
			element.get("dst") for element in root.findall("./gradleCopies/copyFile")
		}
		self.assertEqual(
			{
				"$S(BuildDir)/gradle/OpenMobileAdsAdMob_Android.gradle",
				"$S(BuildDir)/gradle/AFSProject/OpenMobileAdsAdMob_Android.gradle",
				"$S(BuildDir)/gradle/OpenMobileAdsAdMob_Dependencies.gradle",
				"$S(BuildDir)/gradle/AFSProject/OpenMobileAdsAdMob_Dependencies.gradle",
			},
			copy_destinations,
		)
		android_build_rules = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "OpenMobileAdsAdMobAndroid.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn("ExternalDependencies.Add", android_build_rules)
		self.assertIn("OpenMobileAdsAdMob_Android_UPL.xml", android_build_rules)
		self.assertIn("OpenMobileAdsAdMob_Android.gradle", android_build_rules)
		self.assertIn("OpenMobileAdsAdMob_Dependencies.gradle", android_build_rules)

		dependency_validation = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileAdsAdMob_Dependencies.gradle"
		).read_text(encoding="utf-8")
		self.assertIn("MIN_SDK_VERSION", dependency_validation)
		self.assertIn("COMPILE_SDK_VERSION", dependency_validation)
		self.assertIn("GradleVersion.current()", dependency_validation)
		self.assertIn("ANDROID_TOOLS_BUILD_GRADLE_VERSION", dependency_validation)
		self.assertIn("JavaVersion.current()", dependency_validation)
		self.assertIn("resolutionResult", dependency_validation)
		self.assertIn("proguard.txt", dependency_validation)
		self.assertIn("AndroidManifest.xml", dependency_validation)
		self.assertIn('it.name == "pre${variantName}Build"', dependency_validation)

	def test_admob_ios_upl_owns_safe_plist_merging(self) -> None:
		upl_path = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMob_IOS_UPL.xml"
		)
		root = ElementTree.parse(upl_path).getroot()
		plist_updates = ElementTree.tostring(
			root.find("iosPListUpdates"),
			encoding="unicode",
		)
		self.assertIn("OpenMobileAdsAdMobHasAppId", plist_updates)
		self.assertIn("OpenMobileAdsAdMobSKAdItemsFound", plist_updates)
		self.assertIn("OpenMobileAdsAdMobSeenSKAdNetworkIdentifiers", plist_updates)
		self.assertIn("removeElement", plist_updates)
		upl_identifiers = {
			element.text
			for element in root.findall(".//string")
			if element.text and element.text.endswith(".skadnetwork")
		}
		self.assertEqual(
			IOS_PLIST_CONTRACTS["OpenMobileAdsAdMob"]["skad_network_ids"],
			upl_identifiers,
		)
		build_settings = ElementTree.tostring(
			root.find("registerBuildSettings"),
			encoding="unicode",
		)
		self.assertIn("OpenMobileAdsAdMobIOSPlistContract=2", build_settings)

		common_build_rules = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMob"
			/ "OpenMobileAdsAdMob.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn("ValidateIOSAppId", common_build_rules)
		self.assertIn("AdditionalPlistData", common_build_rules)
		self.assertIn("ValidateAdditionalPlistData", common_build_rules)
		self.assertIn("UE_BUILD_FROM_XCODE", common_build_rules)

		ios_build_rules = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "OpenMobileAdsAdMobIOS.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn("ExternalDependencies.Add", ios_build_rules)
		self.assertIn("OpenMobileAdsAdMob_IOS_UPL.xml", ios_build_rules)

	def test_public_api_and_build_rules_have_no_vendor_dependencies(self) -> None:
		public_root = ADS_PLUGIN / "Source" / "OpenMobileAds" / "Public"
		forbidden_tokens = {
			"admob",
			"googlemobileads",
			"play-services-ads",
			"appsflyer",
			"applovin",
			"ironSource".lower(),
			"levelplay",
		}
		for header in public_root.glob("*.h"):
			contents = header.read_text(encoding="utf-8").lower()
			for token in forbidden_tokens:
				self.assertNotIn(token, contents, f"{header} leaks {token}")
			for include in re.findall(r'^#include\s+[<\"]([^>\"]+)', contents, re.MULTILINE):
				self.assertNotIn("thirdparty", include)
				self.assertFalse(include.startswith("android/"))
				self.assertFalse(include.startswith("ios/"))

		build_rules = (
			ADS_PLUGIN / "Source" / "OpenMobileAds" / "OpenMobileAds.Build.cs"
		).read_text(encoding="utf-8").lower()
		for token in forbidden_tokens:
			self.assertNotIn(token, build_rules)

		consumer_rules = (
			ADS_PLUGIN
			/ "Source"
			/ "OpenMobileAdsConsumerTests"
			/ "OpenMobileAdsConsumerTests.Build.cs"
		).read_text(encoding="utf-8")
		dependencies = set(re.findall(r'"([A-Za-z0-9]+)"', consumer_rules))
		self.assertEqual({"Core", "CoreUObject", "OpenMobileAds"}, dependencies)


if __name__ == "__main__":
	unittest.main()
