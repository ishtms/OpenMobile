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
	PROVIDER_SIGNATURES,
	collect_ios_attribution_configuration,
	discover_descriptors,
)


ADS_PLUGIN = REPOSITORY_ROOT / "Services" / "OpenMobileAds"
ADMOB_PLUGIN = REPOSITORY_ROOT / "Providers" / "Ads" / "OpenMobileAdsAdMob"
ADMOB_META_ADAPTER = (
	REPOSITORY_ROOT / "Adapters" / "Ads" / "OpenMobileAdsAdMobMeta"
)
ADMOB_APPLOVIN_ADAPTER = (
	REPOSITORY_ROOT / "Adapters" / "Ads" / "OpenMobileAdsAdMobAppLovin"
)


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

	def test_service_ios_att_backend_and_plist_are_platform_isolated(self) -> None:
		descriptor = load_descriptor(ADS_PLUGIN)
		modules = {module["Name"]: module for module in descriptor["Modules"]}
		self.assertEqual("Runtime", modules["OpenMobileAdsIOS"]["Type"])
		self.assertEqual(["IOS"], modules["OpenMobileAdsIOS"]["PlatformAllowList"])

		module_root = ADS_PLUGIN / "Source" / "OpenMobileAdsIOS"
		common_rules = (
			ADS_PLUGIN / "Source" / "OpenMobileAds" / "OpenMobileAds.Build.cs"
		).read_text(encoding="utf-8")
		build_rules = (module_root / "OpenMobileAdsIOS.Build.cs").read_text(
			encoding="utf-8"
		)
		module = (module_root / "Private" / "OpenMobileAdsIOSModule.cpp").read_text(
			encoding="utf-8"
		)
		backend = (
			module_root
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsIOSTrackingAuthorizationBackend.mm"
		).read_text(encoding="utf-8")
		upl_path = (
			module_root
			/ "Private"
			/ "IOS"
			/ "OpenMobileAds_IOS_UPL.xml"
		)
		upl = upl_path.read_text(encoding="utf-8")
		self.assertIn('"AppTrackingTransparency"', build_rules)
		self.assertIn('"AdSupport"', build_rules)
		self.assertIn('"UIKit"', build_rules)
		self.assertIn("AdditionalPropertiesForReceipt", build_rules)
		self.assertIn("OpenMobileAds_IOS_UPL.xml", build_rules)
		self.assertIn("IOpenMobileAdsTrackingAuthorizationBackend", module)
		self.assertIn("RegisterModularFeature", module)
		self.assertIn("ATTrackingManager trackingAuthorizationStatus", backend)
		self.assertIn("requestTrackingAuthorizationWithCompletionHandler", backend)
		self.assertIn("OpenMobileAdsMapAppleTrackingAuthorizationStatus", backend)
		idfa_method = backend.split("HasNonZeroAdvertisingIdentifier", maxsplit=1)[1]
		self.assertIn("advertisingIdentifier", idfa_method)
		self.assertLess(idfa_method.index("GetStatus()"), idfa_method.index("advertisingIdentifier"))
		self.assertIn('property="bEnableTrackingAuthorization"', upl)
		self.assertIn('property="TrackingUsageDescription"', upl)
		self.assertIn("NSUserTrackingUsageDescription", upl)
		self.assertIn("bEnableTrackingAuthorization", common_rules)
		self.assertIn("TrackingUsageDescription", common_rules)
		self.assertIn("ValidateTrackingUsageDescription", common_rules)
		ElementTree.parse(upl_path)

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

	def test_admob_meta_adapter_owns_one_versioned_manifest(self) -> None:
		descriptor = load_descriptor(ADMOB_META_ADAPTER)
		self.assertEqual("MediationAdapter", descriptor["OpenMobileAdsType"])
		self.assertFalse(descriptor["EnabledByDefault"])
		self.assertEqual(
			{"OpenMobileCore", "OpenMobileAds", "OpenMobileAdsAdMob"},
			{plugin["Name"] for plugin in descriptor["Plugins"]},
		)
		modules = {module["Name"]: module for module in descriptor["Modules"]}
		self.assertEqual(
			["Android"],
			modules["OpenMobileAdsAdMobMetaAndroid"]["PlatformAllowList"],
		)
		self.assertEqual(
			["IOS"],
			modules["OpenMobileAdsAdMobMetaIOS"]["PlatformAllowList"],
		)

		metadata = json.loads(
			(ADMOB_META_ADAPTER / "adapter.json").read_text(encoding="utf-8")
		)
		self.assertEqual(1, metadata["schema_version"])
		self.assertEqual("OpenMobileAdsAdMobMeta", metadata["plugin"])
		self.assertEqual("OpenMobileAdsAdMob", metadata["provider"])
		self.assertEqual("MetaAudienceNetwork", metadata["network"])
		self.assertEqual(["Bidding"], metadata["integration_types"])
		self.assertEqual({"Android", "IOS"}, set(metadata["platforms"]))
		android = metadata["platforms"]["Android"]
		self.assertEqual("6.21.0.4", android["adapter_version"])
		self.assertEqual("6.21.0", android["network_sdk_version"])
		self.assertEqual(["25.4.0"], android["tested_provider_sdk_versions"])
		self.assertEqual("23", android["minimum_os_version"])
		self.assertEqual([], android["attribution_identifiers"])
		self.assertEqual(
			{
				("com.google.ads.mediation:facebook", "6.21.0.4"),
				("com.facebook.android:audience-network-sdk", "6.21.0"),
				("androidx.annotation:annotation", "1.5.0"),
				("com.google.ads.mediation:common", "1.1.0"),
				("com.google.android.gms:play-services-ads", "25.4.0"),
				("org.jetbrains.kotlin:kotlin-stdlib", "2.3.0"),
			},
			{
				(dependency["name"], dependency["version"])
				for dependency in android["dependencies"]
			},
		)
		for dependency in android["dependencies"]:
			self.assertEqual("Gradle", dependency["kind"])
			self.assertIn(dependency["relationship"], {"Direct", "Transitive"})
			self.assertIn(dependency["ownership"], {"Owned", "External"})
		ios = metadata["platforms"]["IOS"]
		self.assertEqual("6.22.0.0", ios["adapter_version"])
		self.assertEqual("6.22.0", ios["network_sdk_version"])
		self.assertEqual(["13.8.0"], ios["tested_provider_sdk_versions"])
		self.assertEqual("15.0", ios["minimum_os_version"])
		self.assertEqual(
			{
				("MetaAdapter", "6.22.0.0"),
				("FBAudienceNetwork", "6.22.0"),
				("GoogleMobileAds", "13.8.0"),
			},
			{
				(dependency["name"], dependency["version"])
				for dependency in ios["dependencies"]
			},
		)
		for dependency in ios["dependencies"]:
			self.assertEqual("Framework", dependency["kind"])
			self.assertIn(dependency["relationship"], {"Direct", "Transitive"})
			self.assertIn(dependency["ownership"], {"Owned", "External"})

	def test_admob_applovin_adapter_owns_one_versioned_manifest(self) -> None:
		descriptor = load_descriptor(ADMOB_APPLOVIN_ADAPTER)
		self.assertEqual("MediationAdapter", descriptor["OpenMobileAdsType"])
		self.assertFalse(descriptor["EnabledByDefault"])
		self.assertEqual(
			{"OpenMobileCore", "OpenMobileAds", "OpenMobileAdsAdMob"},
			{plugin["Name"] for plugin in descriptor["Plugins"]},
		)
		modules = {module["Name"]: module for module in descriptor["Modules"]}
		self.assertEqual(
			["Android"],
			modules["OpenMobileAdsAdMobAppLovinAndroid"]["PlatformAllowList"],
		)
		self.assertEqual(
			["IOS"],
			modules["OpenMobileAdsAdMobAppLovinIOS"]["PlatformAllowList"],
		)
		metadata = json.loads(
			(ADMOB_APPLOVIN_ADAPTER / "adapter.json").read_text(encoding="utf-8")
		)
		self.assertEqual("OpenMobileAdsAdMobAppLovin", metadata["plugin"])
		self.assertEqual("AppLovin", metadata["network"])
		self.assertEqual(["Bidding", "Waterfall"], metadata["integration_types"])

		android = metadata["platforms"]["Android"]
		self.assertEqual("13.6.4.0", android["adapter_version"])
		self.assertEqual("13.6.4", android["network_sdk_version"])
		self.assertEqual(
			{"Interstitial", "Rewarded"},
			set(android["supported_formats"]["Bidding"]),
		)
		self.assertEqual(
			{"Banner", "Interstitial", "Rewarded"},
			set(android["supported_formats"]["Waterfall"]),
		)

		ios = metadata["platforms"]["IOS"]
		self.assertEqual("13.6.3.0", ios["adapter_version"])
		self.assertEqual("13.6.3", ios["network_sdk_version"])
		self.assertEqual(
			{"Interstitial", "Rewarded"},
			set(ios["supported_formats"]["Bidding"]),
		)
		self.assertEqual(
			{"Banner", "Interstitial", "Rewarded"},
			set(ios["supported_formats"]["Waterfall"]),
		)
		for platform in (android, ios):
			self.assertEqual("AdapterConsentConsumer", platform["privacy_signals"]["Gdpr"])
			self.assertEqual("AdapterConsentConsumer", platform["privacy_signals"]["UsPrivacy"])

		android_upl_path = (
			ADMOB_APPLOVIN_ADAPTER
			/ "Source"
			/ "OpenMobileAdsAdMobAppLovinAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileAdsAdMobAppLovin_Android_UPL.xml"
		)
		android_upl = android_upl_path.read_text(encoding="utf-8")
		self.assertIn("com.google.ads.mediation:applovin", android_upl)
		self.assertIn("strictly '13.6.4.0'", android_upl)
		self.assertIn("setHasUserConsent", android_upl)
		self.assertIn("setDoNotSell", android_upl)
		ElementTree.parse(android_upl_path)

		for platform_name in ("Android", "IOS"):
			module_name = f"OpenMobileAdsAdMobAppLovin{platform_name}"
			module_root = ADMOB_APPLOVIN_ADAPTER / "Source" / module_name
			module_text = "\n".join(
				path.read_text(encoding="utf-8")
				for path in module_root.rglob("*")
				if path.is_file() and path.suffix in {".cpp", ".h", ".mm"}
			)
			self.assertIn("IOpenMobileAdsConsentSignalConsumer", module_text)
			self.assertIn("RegisterModularFeature", module_text)
			self.assertIn("AppLovin", module_text)

		ios_upl_path = (
			ADMOB_APPLOVIN_ADAPTER
			/ "Source"
			/ "OpenMobileAdsAdMobAppLovinIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobAppLovin_IOS_UPL.xml"
		)
		ios_root = ElementTree.parse(ios_upl_path).getroot()
		upl_identifiers = [
			element.text
			for element in ios_root.findall(".//string")
			if element.text and element.text.endswith(".skadnetwork")
		]
		self.assertEqual(len(upl_identifiers), len(set(upl_identifiers)))
		self.assertEqual(
			set(ios["attribution"]["skadnetwork_identifiers"]),
			set(upl_identifiers),
		)
		self.assertIn(
			"removeElement",
			ElementTree.tostring(
				ios_root.find("iosPListUpdates"),
				encoding="unicode",
			),
		)

	def test_admob_provider_owns_native_dependency_metadata(self) -> None:
		metadata = json.loads(
			(ADMOB_PLUGIN / "native-dependencies.json").read_text(encoding="utf-8")
		)
		self.assertEqual(1, metadata["schema_version"])
		self.assertEqual("OpenMobileAdsAdMob", metadata["plugin"])
		self.assertEqual(
			{
				("com.google.android.gms:play-services-ads", "25.4.0"),
				("com.google.android.ump:user-messaging-platform", "4.0.0"),
			},
			{
				(dependency["name"], dependency["version"])
				for dependency in metadata["platforms"]["Android"]["dependencies"]
			},
		)
		self.assertEqual(
			{
				("GoogleMobileAds", "13.8.0"),
				("UserMessagingPlatform", "3.1.0"),
			},
			{
				(dependency["binary_name"], dependency["version"])
				for dependency in metadata["platforms"]["IOS"]["dependencies"]
			},
		)

	def test_admob_meta_ios_frameworks_are_adapter_owned(self) -> None:
		module_root = ADMOB_META_ADAPTER / "Source" / "OpenMobileAdsAdMobMetaIOS"
		build_rules = (
			module_root / "OpenMobileAdsAdMobMetaIOS.Build.cs"
		).read_text(encoding="utf-8")
		upl_path = (
			module_root
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobMeta_IOS_UPL.xml"
		)
		ElementTree.parse(upl_path)
		self.assertEqual(1, build_rules.count("Framework.FrameworkMode.LinkAndCopy"))
		self.assertIn('"MetaAdapter"', build_rules)
		self.assertIn('"FBAudienceNetwork"', build_rules)
		meta_framework = build_rules[
			build_rules.index('"MetaAdapter"'):
			build_rules.index('"FBAudienceNetwork"')
		]
		self.assertIn("Framework.FrameworkMode.Link", meta_framework)
		self.assertNotIn("Framework.FrameworkMode.LinkAndCopy", meta_framework)
		self.assertIn('"Swift"', build_rules)
		self.assertIn('"AppTrackingTransparency"', build_rules)
		self.assertIn("AdditionalPropertiesForReceipt", build_rules)
		self.assertIn("OpenMobileAdsAdMobMeta_IOS_UPL.xml", build_rules)
		module = (
			module_root / "Private" / "OpenMobileAdsAdMobMetaIOSModule.cpp"
		).read_text(encoding="utf-8")
		participant = (
			module_root
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobMetaIOSInitializationParticipant.mm"
		).read_text(encoding="utf-8")
		self.assertIn("IOpenMobileAdsInitializationParticipant", module)
		self.assertIn("RegisterModularFeature", module)
		self.assertIn("UnregisterModularFeature", module)
		self.assertIn("FBAdSettings", participant)
		self.assertIn("setAdvertiserTrackingEnabled", participant)
		self.assertIn("EOpenMobileAdsTrackingAuthorizationStatus::Authorized", participant)
		self.assertIn("@available(iOS 17.0", participant)
		self.assertIn("dispatch_sync", participant)
		for token in (
			"ValidateCompatibility",
			'GetObjectField("compatibility")',
			'GetStringField("minimum")',
			'GetStringField("maximum_exclusive")',
			'GetStringArrayField("tested")',
			'GetObjectField("provider_sdk")',
			'GetObjectField("network_sdk")',
			"packages.json",
			"adapter.json",
		):
			self.assertIn(token, build_rules)

	def test_admob_meta_android_packaging_is_adapter_owned(self) -> None:
		module_root = (
			ADMOB_META_ADAPTER / "Source" / "OpenMobileAdsAdMobMetaAndroid"
		)
		build_rules = (
			module_root / "OpenMobileAdsAdMobMetaAndroid.Build.cs"
		).read_text(encoding="utf-8")
		upl_path = (
			module_root
			/ "Private"
			/ "Android"
			/ "OpenMobileAdsAdMobMeta_Android_UPL.xml"
		)
		upl_root = ElementTree.parse(upl_path).getroot()
		gradle = ElementTree.tostring(
			upl_root.find("buildGradleAdditions"),
			encoding="unicode",
		)
		self.assertIn("AdditionalPropertiesForReceipt", build_rules)
		self.assertIn("OpenMobileAdsAdMobMeta_Android_UPL.xml", build_rules)
		self.assertIn("OpenMobileAdsAdMobMeta_Dependencies.gradle", build_rules)
		self.assertIn("com.google.ads.mediation:facebook", gradle)
		self.assertIn("strictly '6.21.0.4'", gradle)
		self.assertIn("OpenMobileAdsAdMobMeta_Dependencies.gradle", gradle)
		build_settings = ElementTree.tostring(
			upl_root.find("registerBuildSettings"),
			encoding="unicode",
		)
		self.assertIn("OpenMobileAdsAdMobMetaAndroidDependencyContract=2", build_settings)
		copy_destinations = {
			element.get("dst") for element in upl_root.findall("./gradleCopies/copyFile")
		}
		self.assertEqual(
			{
				"$S(BuildDir)/gradle/OpenMobileAdsAdMobMeta_Dependencies.gradle",
				"$S(BuildDir)/gradle/AFSProject/OpenMobileAdsAdMobMeta_Dependencies.gradle",
			},
			copy_destinations,
		)
		dependency_validation = (
			module_root
			/ "Private"
			/ "Android"
			/ "OpenMobileAdsAdMobMeta_Dependencies.gradle"
		).read_text(encoding="utf-8")
		for token in (
			"resolutionResult",
			"com.google.ads.mediation:facebook",
			"com.facebook.android:audience-network-sdk",
			"25.4.0",
			"6.21.0.4",
			"6.21.0",
			'it.name == "pre${variantName}Build"',
		):
			self.assertIn(token, dependency_validation)
		self.assertNotIn("com.google.ads.mediation:facebook", (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileAdsAdMob_Android_UPL.xml"
		).read_text(encoding="utf-8"))

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
		self.assertIn("com.google.android.ump:user-messaging-platform", gradle_additions)
		self.assertIn("strictly '4.0.0'", gradle_additions)
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
		self.assertIn("OpenMobileAdsAdMobAndroidDependencyContract=5", build_settings)
		self.assertIn("OpenMobileAdsAdMobAndroidRuntimeContract=17", build_settings)
		game_activity_imports = ElementTree.tostring(
			root.find("gameActivityImportAdditions"),
			encoding="unicode",
		)
		self.assertIn(
			"com.google.android.gms.ads.appopen.AppOpenAd.AppOpenAdLoadCallback",
			game_activity_imports,
		)
		self.assertNotIn(
			"import com.google.android.gms.ads.appopen.AppOpenAdLoadCallback;",
			game_activity_imports,
		)
		game_activity_additions = ElementTree.tostring(
			root.find("gameActivityClassAdditions"),
			encoding="unicode",
		)
		for token in (
			"AndroidThunkJava_LoadOpenMobileInterstitialAd",
			"AndroidThunkJava_ShowOpenMobileInterstitialAd",
			"openMobileLoadedInterstitialAds.remove",
			"nativeOpenMobileInterstitialAdLoadCompleted",
			"AndroidThunkJava_ShowOpenMobileRewardedAd",
			"openMobileLoadedRewardedAds.remove",
			"setServerSideVerificationOptions",
			"setUserId(serverVerificationUserId)",
			"setCustomData(serverVerificationCustomData)",
			"nativeOpenMobileRewardedAdImpression",
			"nativeOpenMobileRewardedAdClicked",
			"nativeOpenMobileRewardedAdRevenuePaid",
			"AndroidThunkJava_LoadOpenMobileBannerAd",
			"AndroidThunkJava_ShowOpenMobileBannerAd",
			"AndroidThunkJava_HideOpenMobileBannerAd",
			"AdSize.BANNER",
			"AdSize.MEDIUM_RECTANGLE",
			"AdSize.getLargeAnchoredAdaptiveBannerAdSize",
			"availableWidth",
			"horizontalAlignment",
			"OPEN_MOBILE_DISPLAY_MREC",
			"RewardedInterstitialAd.load",
			"openMobileLoadedRewardedInterstitialAds",
			"AndroidThunkJava_LoadOpenMobileRewardedInterstitialAd",
			"AndroidThunkJava_ShowOpenMobileRewardedInterstitialAd",
			"AppOpenAd.load",
			"openMobileLoadedAppOpenAds",
			"AndroidThunkJava_LoadOpenMobileAppOpenAd",
			"AndroidThunkJava_ShowOpenMobileAppOpenAd",
			"nativeOpenMobileAppOpenAdLoadCompleted",
			"getSystemWindowInsetLeft()",
			"getDisplayCutout()",
			"!entry.bannerReady",
			"detachOpenMobileBanner(entry, false)",
		):
			self.assertIn(token, game_activity_additions)
		self.assertNotIn("URLEncoder", game_activity_additions)
		for token in (
			"AndroidThunkJava_LoadOpenMobileInterstitialAd",
			"AndroidThunkJava_ShowOpenMobileInterstitialAd",
			"nativeOpenMobileInterstitialAdLoadCompleted",
			"AndroidThunkJava_ShowOpenMobileRewardedAd",
			"nativeOpenMobileRewardedAdImpression",
			"nativeOpenMobileRewardedAdClicked",
			"nativeOpenMobileRewardedAdRevenuePaid",
			"AndroidThunkJava_LoadOpenMobileBannerAd",
			"AndroidThunkJava_ShowOpenMobileBannerAd",
			"AndroidThunkJava_HideOpenMobileBannerAd",
			"nativeOpenMobileBannerAdOperationFailed",
			"AndroidThunkJava_LoadOpenMobileAppOpenAd",
			"AndroidThunkJava_ShowOpenMobileAppOpenAd",
			"nativeOpenMobileAppOpenAdLoadCompleted",
		):
			self.assertIn(token, proguard_additions)
		pause_additions = ElementTree.tostring(
			root.find("gameActivityOnPauseAdditions"),
			encoding="unicode",
		)
		resume_additions = ElementTree.tostring(
			root.find("gameActivityOnResumeAdditions"),
			encoding="unicode",
		)
		self.assertIn("entry.adView.pause()", pause_additions)
		self.assertIn("entry.adView.resume()", resume_additions)
		self.assertIn("requestApplyInsets", resume_additions)
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

	def test_admob_native_load_failures_keep_normalized_sdk_codes(self) -> None:
		android_root = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
		)
		android_upl = (
			android_root / "OpenMobileAdsAdMob_Android_UPL.xml"
		).read_text(encoding="utf-8")
		android_backend = (
			android_root / "OpenMobileAdsAdMobAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertEqual(5, android_upl.count("openMobileAdsLoadErrorCode(loadAdError)"))
		for token in (
			'case 3:',
			'case 9:',
			'return "no_fill";',
			'"invalid_request"',
			'"invalid_state"',
			'"internal_error"',
		):
			self.assertIn(token, android_upl)
		for callback in (
			"RewardedAd",
			"RewardedInterstitialAd",
			"InterstitialAd",
			"AppOpenAd",
			"BannerAd",
		):
			start = android_backend.index(
				f"nativeOpenMobile{callback}LoadFailed("
			)
			callback_body = android_backend[start:android_backend.index("\n}", start)]
			self.assertIn("jstring ErrorCode", callback_body)
			self.assertIn("jstring ErrorMessage", callback_body)

		ios_backend = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobIOSBackend.mm"
		).read_text(encoding="utf-8")
		for token in (
			"LoadErrorCode(NSError* Error)",
			"GADErrorNoFill",
			"GADErrorNetworkError",
			"GADErrorMediationAdapterError",
			'TEXT("no_fill")',
		):
			self.assertIn(token, ios_backend)
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
		self.assertIn("com.google.android.ump", dependency_validation)
		self.assertIn("user-messaging-platform", dependency_validation)
		self.assertIn('"4.0.0"', dependency_validation)
		self.assertIn("proguard.txt", dependency_validation)
		self.assertIn("AndroidManifest.xml", dependency_validation)
		self.assertIn('it.name == "pre${variantName}Build"', dependency_validation)

	def test_android_dependency_validation_reads_selected_components(self) -> None:
		dependency_scripts = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileAdsAdMob_Dependencies.gradle",
			ADMOB_META_ADAPTER
			/ "Source"
			/ "OpenMobileAdsAdMobMetaAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileAdsAdMobMeta_Dependencies.gradle",
		)
		for dependency_script in dependency_scripts:
			with self.subTest(script=dependency_script.name):
				validation = dependency_script.read_text(encoding="utf-8")
				self.assertIn("resolutionResult.allComponents.each", validation)
				self.assertIn("component.moduleVersion", validation)
				self.assertNotIn("component.id", validation)
				self.assertIn('[requested.group, requested.module].join(":")', validation)
				self.assertIn('[selected.group, selected.name].join(":")', validation)
				self.assertIn("if (strictVersion)", validation)
				self.assertNotIn("strictVersion ?: requested.version", validation)

	def test_admob_shipping_build_checks_banner_identifiers(self) -> None:
		build_rules = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMob"
			/ "OpenMobileAdsAdMob.Build.cs"
		).read_text(encoding="utf-8")
		shipping_start = build_rules.index("string[] Identifiers")
		shipping_identifiers = build_rules[
			shipping_start:
			build_rules.index("foreach (string Identifier", shipping_start)
		]
		for identifier in ("AndroidBannerAdUnitId", "IOSBannerAdUnitId"):
			self.assertIn(
				f"string {identifier} =",
				build_rules,
			)
			self.assertIn(identifier, shipping_identifiers)

	def test_admob_paid_events_include_winning_source_metadata(self) -> None:
		android_root = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
		)
		android_upl = (
			android_root / "OpenMobileAdsAdMob_Android_UPL.xml"
		).read_text(encoding="utf-8")
		android_backend = (
			android_root / "OpenMobileAdsAdMobAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"getLoadedAdapterResponseInfo",
			"getAdSourceName",
			"getAdSourceId",
			"getAdapterClassName",
			"getAdSourceInstanceName",
			"getAdSourceInstanceId",
		):
			self.assertIn(token, android_upl)
		self.assertIn("FOpenMobileAdsRevenueSource", android_backend)

		ios_backend = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobIOSBackend.mm"
		).read_text(encoding="utf-8")
		for token in (
			"loadedAdNetworkResponseInfo",
			"adSourceName",
			"adSourceID",
			"adNetworkClassName",
			"adSourceInstanceName",
			"adSourceInstanceID",
		):
			self.assertIn(token, ios_backend)

	def test_admob_ump_refreshes_before_presenting_a_required_form(self) -> None:
		android_root = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
		)
		android_upl = (
			android_root / "OpenMobileAdsAdMob_Android_UPL.xml"
		).read_text(encoding="utf-8")
		request_method = android_upl[
			android_upl.index("AndroidThunkJava_RequestOpenMobileUMPConsent"):
			android_upl.index("AndroidThunkJava_PresentRequiredOpenMobileUMPConsentForm")
		]
		form_method = android_upl[
			android_upl.index("AndroidThunkJava_PresentRequiredOpenMobileUMPConsentForm"):
			android_upl.index("AndroidThunkJava_PresentOpenMobileUMPPrivacyOptionsForm")
		]
		privacy_options_method = android_upl[
			android_upl.index("AndroidThunkJava_PresentOpenMobileUMPPrivacyOptionsForm"):
			android_upl.index("AndroidThunkJava_InitializeOpenMobileRewardedAds")
		]
		self.assertIn("setTagForUnderAgeOfConsent", request_method)
		self.assertIn("setConsentDebugSettings", request_method)
		self.assertIn("addTestDeviceHashedId", request_method)
		self.assertIn("requestConsentInfoUpdate", request_method)
		self.assertIn("nativeOpenMobileUMPConsentInfoUpdated", request_method)
		self.assertIn("loadAndShowConsentFormIfRequired", form_method)
		self.assertIn("nativeOpenMobileUMPConsentFormDismissed", form_method)
		self.assertIn("nativeOpenMobileUMPConsentFailed", form_method)
		self.assertIn("showPrivacyOptionsForm", privacy_options_method)
		self.assertIn("nativeOpenMobileUMPConsentFormDismissed", privacy_options_method)
		self.assertIn("nativeOpenMobileUMPConsentFailed", privacy_options_method)
		self.assertIn(
			"AndroidThunkJava_PresentOpenMobileUMPPrivacyOptionsForm(long)",
			android_upl,
		)
		self.assertRegex(
			android_upl,
			r'case 3:\s+return "ump_invalid_operation";',
		)
		self.assertLess(
			android_upl.index("requestConsentInfoUpdate"),
			android_upl.index("loadAndShowConsentFormIfRequired"),
		)

		android_backend = (
			android_root / "OpenMobileAdsAdMobAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('"(Ljava/lang/String;JIIIIZFFFFF)Z"', android_backend)
		self.assertIn('"(JJIIZFFFFF)Z"', android_backend)
		self.assertIn('"(JZ[Ljava/lang/String;I)Z"', android_backend)
		self.assertIn('"(J)Z"', android_backend)
		self.assertIn("Request.Development.bEnableConsentDebug", android_backend)
		self.assertIn("nativeOpenMobileUMPConsentInfoUpdated", android_backend)
		self.assertIn("nativeOpenMobileUMPConsentFormDismissed", android_backend)
		self.assertIn("nativeOpenMobileUMPConsentFailed", android_backend)

		ios_backend = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobIOSBackend.mm"
		).read_text(encoding="utf-8")
		self.assertIn("<UserMessagingPlatform/UserMessagingPlatform.h>", ios_backend)
		self.assertIn("FOpenMobileAdsAdMobIOSBackend::RequestConsentInfo", ios_backend)
		self.assertIn("requestConsentInfoUpdateWithParameters", ios_backend)
		self.assertIn("tagForUnderAgeOfConsent", ios_backend)
		self.assertIn("testDeviceIdentifiers", ios_backend)
		self.assertIn("bEnableConsentDebug && !bUnderAgeOfConsent", ios_backend)
		self.assertIn("FOpenMobileAdsAdMobIOSBackend::PresentRequiredConsentForm", ios_backend)
		self.assertIn("loadAndPresentIfRequiredFromViewController", ios_backend)
		self.assertIn("FOpenMobileAdsAdMobIOSBackend::PresentPrivacyOptionsForm", ios_backend)
		self.assertIn("presentPrivacyOptionsFormFromViewController", ios_backend)
		self.assertIn("NativeConsentInfoUpdated", ios_backend)
		self.assertIn("NativeConsentFormDismissed", ios_backend)
		self.assertIn("NativeConsentFailed", ios_backend)
		self.assertIn("UMPFormErrorCodeUnavailable", ios_backend)
		self.assertIn('TEXT("form_unavailable")', ios_backend)
		self.assertRegex(
			ios_backend,
			r"case UMPConsentStatusNotRequired:\s+return 1;",
		)
		self.assertRegex(
			ios_backend,
			r"case UMPConsentStatusRequired:\s+return 2;",
		)
		self.assertRegex(
			ios_backend,
			r"case UMPPrivacyOptionsRequirementStatusNotRequired:\s+return 1;",
		)
		self.assertRegex(
			ios_backend,
			r"case UMPPrivacyOptionsRequirementStatusRequired:\s+return 2;",
		)
		self.assertLess(
			ios_backend.index("requestConsentInfoUpdateWithParameters"),
			ios_backend.index("loadAndPresentIfRequiredFromViewController"),
		)

	def test_admob_applies_coppa_before_sdk_initialization(self) -> None:
		android_root = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
		)
		android_backend = (
			android_root / "OpenMobileAdsAdMobAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		android_age_mapping = android_backend[
			android_backend.index("int32 ToNativeAgeTreatment"):
			android_backend.index("FString ToNativeMaxAdContentRating")
		]
		self.assertRegex(
			android_age_mapping,
			r"case EOpenMobileAdsAgeTreatment::No:\s+return 0;",
		)
		self.assertRegex(
			android_age_mapping,
			r"case EOpenMobileAdsAgeTreatment::Yes:\s+return 1;",
		)
		self.assertRegex(android_age_mapping, r"default:\s+return -1;")

		android_upl = (
			android_root / "OpenMobileAdsAdMob_Android_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertLess(
			android_upl.index(".setTagForChildDirectedTreatment"),
			android_upl.index("MobileAds.initialize"),
		)
		self.assertLess(
			android_upl.index("MobileAds.setRequestConfiguration"),
			android_upl.index("MobileAds.initialize"),
		)

		ios_backend = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobIOSBackend.mm"
		).read_text(encoding="utf-8")
		ios_age_mapping = ios_backend[
			ios_backend.index("NSNumber* ToNSNumber"):
			ios_backend.index("GADMaxAdContentRating ToMaxAdContentRating")
		]
		self.assertRegex(
			ios_age_mapping,
			r"case EOpenMobileAdsAgeTreatment::No:\s+return @NO;",
		)
		self.assertRegex(
			ios_age_mapping,
			r"case EOpenMobileAdsAgeTreatment::Yes:\s+return @YES;",
		)
		self.assertRegex(ios_age_mapping, r"default:\s+return nil;")
		self.assertLess(
			ios_backend.index("Configuration.tagForChildDirectedTreatment"),
			ios_backend.index("startWithCompletionHandler"),
		)

	def test_admob_applies_under_age_before_sdk_initialization(self) -> None:
		android_root = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
		)
		android_backend = (
			android_root / "OpenMobileAdsAdMobAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn(
			"Request.Privacy.UnderAgeOfConsent",
			android_backend,
		)
		android_upl = (
			android_root / "OpenMobileAdsAdMob_Android_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertLess(
			android_upl.index(".setTagForUnderAgeOfConsent"),
			android_upl.index("MobileAds.initialize"),
		)
		self.assertLess(
			android_upl.index("MobileAds.setRequestConfiguration"),
			android_upl.index("MobileAds.initialize"),
		)

		ios_backend = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobIOSBackend.mm"
		).read_text(encoding="utf-8")
		self.assertIn("Request.Privacy.UnderAgeOfConsent", ios_backend)
		self.assertLess(
			ios_backend.index("Configuration.tagForUnderAgeOfConsent"),
			ios_backend.index("startWithCompletionHandler"),
		)

	def test_admob_applies_explicit_us_privacy_mode_before_ad_load(self) -> None:
		android_root = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
		)
		android_upl = (
			android_root / "OpenMobileAdsAdMob_Android_UPL.xml"
		).read_text(encoding="utf-8")
		android_load = android_upl[
			android_upl.index("AndroidThunkJava_LoadOpenMobileRewardedAd"):
			android_upl.index("AndroidThunkJava_CancelOpenMobileRewardedAdLoad")
		]
		android_consent_signals = android_upl[
			android_upl.index("applyOpenMobileDataProcessingMode"):
			android_upl.index("AndroidThunkJava_LoadOpenMobileRewardedAd")
		]
		android_interstitial_load = android_upl[
			android_upl.index("AndroidThunkJava_LoadOpenMobileInterstitialAd"):
			android_upl.index("AndroidThunkJava_CancelOpenMobileInterstitialAdLoad")
		]
		android_app_open_load = android_upl[
			android_upl.index("AndroidThunkJava_LoadOpenMobileAppOpenAd"):
			android_upl.index("AndroidThunkJava_CancelOpenMobileAppOpenAdLoad")
		]
		self.assertIn("final int dataProcessingMode", android_load)
		self.assertIn("applyOpenMobileDataProcessingMode", android_load)
		self.assertIn('putInt("gad_rdp", 1)', android_consent_signals)
		self.assertIn('remove("gad_rdp")', android_consent_signals)
		self.assertLess(
			android_load.index("applyOpenMobileDataProcessingMode"),
			android_load.index("RewardedAd.load"),
		)
		self.assertLess(
			android_interstitial_load.index("applyOpenMobileDataProcessingMode"),
			android_interstitial_load.index("InterstitialAd.load"),
		)
		self.assertLess(
			android_app_open_load.index("applyOpenMobileDataProcessingMode"),
			android_app_open_load.index("AppOpenAd.load"),
		)
		android_backend = (
			android_root / "OpenMobileAdsAdMobAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('"(Ljava/lang/String;JI)Z"', android_backend)
		self.assertIn('"(I)Z"', android_backend)
		self.assertIn('"AndroidThunkJava_ResetOpenMobileUMPConsent"', android_backend)
		self.assertIn('"()Z"', android_backend)
		self.assertIn(
			"AndroidThunkJava_ResetOpenMobileUMPConsent",
			android_consent_signals,
		)
		self.assertIn(
			"UserMessagingPlatform.getConsentInformation(activityContext).reset()",
			android_consent_signals,
		)
		self.assertIn("getConsentStatus()", android_consent_signals)
		self.assertIn("ConsentInformation.ConsentStatus.UNKNOWN", android_consent_signals)
		self.assertIn('remove("gad_rdp")', android_consent_signals)
		self.assertIn(".commit()", android_consent_signals)

		ios_backend = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobIOSBackend.mm"
		).read_text(encoding="utf-8")
		ios_consent_signals = ios_backend[
			ios_backend.index("void ApplyDataProcessingMode"):
			ios_backend.index("FOpenMobileAdsAdMobIOSBackend::LoadRewardedAd")
		]
		ios_load = ios_backend[
			ios_backend.index("FOpenMobileAdsAdMobIOSBackend::LoadRewardedAd"):
			ios_backend.index("FOpenMobileAdsAdMobIOSBackend::CancelRewardedAd")
		]
		self.assertIn('setBool:YES forKey:@"gad_rdp"', ios_consent_signals)
		self.assertIn('removeObjectForKey:@"gad_rdp"', ios_consent_signals)
		self.assertIn("FOpenMobileAdsAdMobIOSBackend::ResetConsentForTesting", ios_consent_signals)
		self.assertIn("[UMPConsentInformation.sharedInstance reset]", ios_consent_signals)
		self.assertIn("consentStatus", ios_consent_signals)
		self.assertIn("UMPConsentStatusUnknown", ios_consent_signals)
		self.assertIn("ApplyDataProcessingMode(DataProcessingMode)", ios_load)
		self.assertLess(
			ios_load.index("ApplyDataProcessingMode(DataProcessingMode)"),
			ios_load.index("GADRewardedAd loadWithAdUnitID"),
		)
		self.assertIn("GADInterstitialAd loadWithAdUnitID", ios_load)
		for token in (
			"GADAdSizeBanner",
			"GADAdSizeMediumRectangle",
			"GADLargeAnchoredAdaptiveBannerAdSizeWithWidth",
			"ResolveOpenMobileBannerSize",
			"ScheduleOpenMobileBannerLayout",
			"GADAdSizeEqualToSize",
			"availableWidth",
			"OpenMobileBannerLayoutObserver",
			"EOpenMobileAdsBannerHorizontalAlignment::Left",
			"EOpenMobileAdsBannerAnchor::Center",
			"GADRewardedInterstitialAd loadWithAdUnitID",
			"GOpenMobileLoadedRewardedInterstitialAds",
			"FOpenMobileAdsAdMobIOSBackend::LoadRewardedInterstitialAd",
			"FOpenMobileAdsAdMobIOSBackend::ShowRewardedInterstitialAd",
			"GADAppOpenAd loadWithAdUnitID",
			"GOpenMobileLoadedAppOpenAds",
			"OpenMobileAppOpenAdDelegate",
			"FOpenMobileAdsAdMobIOSBackend::LoadAppOpenAd",
			"FOpenMobileAdsAdMobIOSBackend::ShowAppOpenAd",
			"FOpenMobileAdsAdMobIOSBackend::LoadBannerAd",
			"FOpenMobileAdsAdMobIOSBackend::ShowBannerAd",
			"FOpenMobileAdsAdMobIOSBackend::HideBannerAd",
			"safeAreaLayoutGuide",
			"CGSizeFromGADAdSize",
			"removeFromSuperview",
		):
			self.assertIn(token, ios_backend)

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
		attribution, attribution_errors = collect_ios_attribution_configuration(
			discover_descriptors(REPOSITORY_ROOT),
			{"OpenMobileAdsAdMob"},
		)
		self.assertEqual([], attribution_errors)
		self.assertEqual(
			set(attribution.skad_network_ids),
			upl_identifiers,
		)
		build_settings = ElementTree.tostring(
			root.find("registerBuildSettings"),
			encoding="unicode",
		)
		self.assertIn("OpenMobileAdsAdMobIOSPlistContract=3", build_settings)

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
		self.assertIn("AdditionalBundleResources.Add", ios_build_rules)
		self.assertIn("OpenMobileAdsAdMobPrivacy.bundle", ios_build_rules)
		privacy_bundle = (
			ADMOB_PLUGIN
			/ "Resources"
			/ "IOS"
			/ "OpenMobileAdsAdMobPrivacy.bundle"
		)
		self.assertTrue((privacy_bundle / "Info.plist").is_file())
		self.assertTrue((privacy_bundle / "PrivacyInfo.xcprivacy").is_file())

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

	def test_consent_reset_is_guarded_from_shipping(self) -> None:
		subsystem = (
			ADS_PLUGIN
			/ "Source"
			/ "OpenMobileAds"
			/ "Private"
			/ "OpenMobileAdsSubsystem.cpp"
		).read_text(encoding="utf-8")
		reset = subsystem[
			subsystem.index("UOpenMobileAdsSubsystem::ResetConsentForTesting"):
			subsystem.index("UOpenMobileAdsSubsystem::PresentPrivacyOptionsForm")
		]
		self.assertIn("#if UE_BUILD_SHIPPING", reset)
		self.assertIn("Consent reset is disabled in Shipping builds", reset)
		self.assertIn("IsDevelopmentTestModeEnabled", reset)

	def test_admob_maps_supported_debug_geographies(self) -> None:
		build_rules = (
			ADS_PLUGIN / "Source" / "OpenMobileAds" / "OpenMobileAds.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn("string DebugGeography", build_rules)
		self.assertIn(
			"Consent Debug Geography must be disabled for Shipping builds",
			build_rules,
		)

		android_root = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobAndroid"
			/ "Private"
			/ "Android"
		)
		android_backend = (
			android_root / "OpenMobileAdsAdMobAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		android_upl = (
			android_root / "OpenMobileAdsAdMob_Android_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertIn('"(JZ[Ljava/lang/String;I)Z"', android_backend)
		self.assertIn("GetEffectiveDebugGeography", android_backend)
		self.assertIn("final int debugGeography", android_upl)
		self.assertIn("setDebugGeography", android_upl)
		self.assertIn("DEBUG_GEOGRAPHY_EEA", android_upl)
		self.assertIn("DEBUG_GEOGRAPHY_REGULATED_US_STATE", android_upl)
		self.assertIn("DEBUG_GEOGRAPHY_OTHER", android_upl)

		ios_backend = (
			ADMOB_PLUGIN
			/ "Source"
			/ "OpenMobileAdsAdMobIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileAdsAdMobIOSBackend.mm"
		).read_text(encoding="utf-8")
		self.assertIn("GetEffectiveDebugGeography", ios_backend)
		self.assertIn("DebugSettings.geography", ios_backend)
		self.assertIn("UMPDebugGeographyEEA", ios_backend)
		self.assertIn("UMPDebugGeographyRegulatedUSState", ios_backend)
		self.assertIn("UMPDebugGeographyOther", ios_backend)


if __name__ == "__main__":
	unittest.main()
