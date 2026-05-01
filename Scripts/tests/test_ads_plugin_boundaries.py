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
		self.assertIn("OpenMobileAdsAdMobAndroidDependencyContract=4", build_settings)
		self.assertIn("OpenMobileAdsAdMobAndroidRuntimeContract=14", build_settings)
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
