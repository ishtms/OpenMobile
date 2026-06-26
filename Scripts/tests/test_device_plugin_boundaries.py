import json
import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CORE_PLUGIN = REPOSITORY_ROOT / "Foundation" / "OpenMobileCore"
DEVICE_PLUGIN = REPOSITORY_ROOT / "Native" / "OpenMobileDevice"


def load_descriptor() -> dict:
	with (DEVICE_PLUGIN / "OpenMobileDevice.uplugin").open(encoding="utf-8") as file:
		return json.load(file)


class DevicePluginBoundaryTests(unittest.TestCase):
	def test_modules_match_runtime_platform_and_editor_boundaries(self) -> None:
		modules = {module["Name"]: module for module in load_descriptor()["Modules"]}

		self.assertEqual("Runtime", modules["OpenMobileDevice"]["Type"])
		self.assertNotIn("PlatformAllowList", modules["OpenMobileDevice"])
		self.assertEqual(
			["Android"],
			modules["OpenMobileDeviceAndroid"]["PlatformAllowList"],
		)
		self.assertEqual(["IOS"], modules["OpenMobileDeviceIOS"]["PlatformAllowList"])
		self.assertEqual("Editor", modules["OpenMobileDeviceEditor"]["Type"])

		for module_name in (
			"OpenMobileDeviceAndroid",
			"OpenMobileDeviceIOS",
			"OpenMobileDeviceEditor",
		):
			module_root = DEVICE_PLUGIN / "Source" / module_name
			self.assertTrue((module_root / f"{module_name}.Build.cs").is_file())
			self.assertTrue((module_root / "Private" / f"{module_name}Module.cpp").is_file())

	def test_device_only_dependency_graph_is_isolated(self) -> None:
		descriptor_dependencies = {
			plugin["Name"] for plugin in load_descriptor().get("Plugins", [])
		}
		self.assertEqual({"OpenMobileCore"}, descriptor_dependencies)

		for build_rules in (DEVICE_PLUGIN / "Source").glob("*/*.Build.cs"):
			contents = build_rules.read_text(encoding="utf-8")
			for forbidden_dependency in (
				"OpenMobileAds",
				"OpenMobileHaptics",
				"OpenMobileMedia",
				"OpenMobileSensors",
				"UMG",
			):
				self.assertNotIn(forbidden_dependency, contents, str(build_rules))

	def test_core_dependency_is_declared_and_remains_one_way(self) -> None:
		build_rules = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "OpenMobileDevice.Build.cs"
		).read_text(encoding="utf-8")
		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")

		self.assertIn('"OpenMobileCore"', build_rules)
		self.assertIn("OpenMobileCore", readme)
		for path in CORE_PLUGIN.rglob("*"):
			if not path.is_file() or path.suffix not in {
				".cs",
				".cpp",
				".h",
				".uplugin",
			}:
				continue
			self.assertNotIn(
				"OpenMobileDevice",
				path.read_text(encoding="utf-8"),
				str(path),
			)

	def test_shared_module_has_no_native_sdk_or_provider_payload(self) -> None:
		shared_module = DEVICE_PLUGIN / "Source" / "OpenMobileDevice"
		for path in shared_module.rglob("*"):
			if not path.is_file() or path.suffix not in {".cs", ".cpp", ".h"}:
				continue
			contents = path.read_text(encoding="utf-8")
			for forbidden_token in (
				"jni.h",
				"JNIEnv",
				"Android/AndroidJNI",
				"UIKit/",
				"Foundation/",
				"GoogleMobileAds",
				"play-services-ads",
			):
				self.assertNotIn(forbidden_token, contents, str(path))

		self.assertFalse((DEVICE_PLUGIN / "ThirdParty").exists())

	def test_public_contract_excludes_personal_identifiers(self) -> None:
		public_headers = DEVICE_PLUGIN / "Source" / "OpenMobileDevice" / "Public"
		for path in public_headers.glob("*.h"):
			contents = path.read_text(encoding="utf-8").casefold()
			for forbidden_token in (
				"imei",
				"serialnumber",
				"androidid",
				"idfv",
				"macaddress",
				"advertisingid",
				"installedapps",
			):
				self.assertNotIn(forbidden_token, contents, str(path))

	def test_device_subsystem_has_no_unconditional_polling(self) -> None:
		subsystem = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertNotIn("TickStatus", subsystem)
		self.assertNotIn("TickerHandle", subsystem)
		self.assertNotIn("AddTicker", subsystem)

	def test_endpoint_reachability_is_explicit_bounded_and_private(self) -> None:
		action = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceEndpointReachabilityAsyncAction.cpp"
		).read_text(encoding="utf-8")
		header = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceEndpointReachabilityAsyncAction.h"
		).read_text(encoding="utf-8")
		build_rules = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "OpenMobileDevice.Build.cs"
		).read_text(encoding="utf-8")

		self.assertIn("UOpenMobileDeviceAsyncActionBase", header)
		self.assertIn("GetAddressInfoAsync", action)
		self.assertIn("CreateUniqueSocket", action)
		self.assertIn("WaitForWrite", action)
		self.assertIn("SetTimeout", action)
		self.assertIn("SetActivityTimeout", action)
		self.assertIn("SetResponseBodyReceiveStreamDelegateV2", action)
		self.assertIn("MaximumConcurrentRequests", action)
		self.assertIn("CancelNativeOperation", action)
		self.assertNotIn("UE_LOG", action)
		self.assertNotIn("GetContent", action)
		self.assertIn('"HTTP"', build_rules)
		self.assertIn('"Sockets"', build_rules)

	def test_window_metrics_use_the_active_platform_window(self) -> None:
		android = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidDisplay.cpp"
		).read_text(encoding="utf-8")
		ios = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSDisplay.mm"
		).read_text(encoding="utf-8")

		for token in (
			"GetNativeWindowResolution",
			"screenWidthDp",
			"screenHeightDp",
			"densityDpi",
			"getDisplayId",
			"isInMultiWindowMode",
		):
			self.assertIn(token, android)
		self.assertNotIn("widthPixels", android)

		for token in (
			"IOSView",
			"View.bounds",
			"View.ViewSize",
			"View.window.screen",
			"contentScaleFactor",
			"windowScene",
		):
			self.assertIn(token, ios)
		self.assertNotIn("mainScreen", ios)

	def test_refresh_rate_information_preserves_mode_constraints(self) -> None:
		android = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidDisplay.cpp"
		).read_text(encoding="utf-8")
		ios = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSDisplay.mm"
		).read_text(encoding="utf-8")
		display_types = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceDisplayTypes.h"
		).read_text(encoding="utf-8")

		for token in (
			"getRefreshRate",
			"getSupportedModes",
			"getPhysicalWidth",
			"getPhysicalHeight",
			"getAlternativeRefreshRates",
		):
			self.assertIn(token, android)
		self.assertIn("maximumFramesPerSecond", ios)
		self.assertNotIn("GetFramePace", ios)
		self.assertIn("SupportedRefreshModes", display_types)
		self.assertIn("bVariableRefreshRateSupported", display_types)

	def test_refresh_rate_control_is_explicit_and_does_not_change_frame_pacing(self) -> None:
		android = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidDisplay.cpp"
		).read_text(encoding="utf-8")
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")
		control_sources = "\n".join(
			path.read_text(encoding="utf-8")
			for path in (DEVICE_PLUGIN / "Source").rglob("*RefreshRateControl*")
			if path.is_file()
		)

		self.assertIn("ApplyOpenMobileDeviceAndroidPreferredRefreshRate", android)
		self.assertIn("ClearOpenMobileDeviceAndroidPreferredRefreshRate", android)
		self.assertIn("preferredRefreshRate", android_upl)
		self.assertIn("runOnUiThread", android_upl)
		self.assertIn("does not alter Unreal frame pacing", ios_backend)
		for forbidden_token in (
			"FPlatformRHIFramePacer",
			"r.VSync",
			"SetFrameRateLimit",
			"t.MaxFPS",
		):
			self.assertNotIn(forbidden_token, control_sources)

	def test_window_insets_are_active_directional_and_classified(self) -> None:
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		android_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidDisplay.cpp"
		).read_text(encoding="utf-8")
		ios_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSDisplay.mm"
		).read_text(encoding="utf-8")

		for token in (
			"getRootWindowInsets",
			"WindowInsets.Type.systemBars",
			"WindowInsets.Type.systemGestures",
			"getDisplayCutout",
		):
			self.assertIn(token, android_upl)
		android_type_check = android_upl.split(
			"public String[] AndroidThunkJava_OpenMobileDeviceCheckClipboardContentTypes",
			1,
		)[1].split(
			"public String[] AndroidThunkJava_OpenMobileDeviceWriteClipboard",
			1,
		)[0]
		self.assertNotIn("getPrimaryClip()", android_type_check)
		self.assertIn("ApplyOpenMobileDeviceAndroidWindowInsets", android_display)
		for token in (
			"safeAreaInsets",
			"statusBarManager",
			"HomeIndicator",
			"FOpenMobileDeviceWindowInsets::Apply",
		):
			self.assertIn(token, ios_display)

	def test_display_cutouts_use_native_geometry_without_ios_guessing(self) -> None:
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		android_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidDisplay.cpp"
		).read_text(encoding="utf-8")
		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")
		ios_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSDisplay.mm"
		).read_text(encoding="utf-8")

		for token in (
			"getBoundingRects",
			"getLocationOnScreen",
			"getWaterfallInsets",
		):
			self.assertIn(token, android_upl)
		self.assertIn("ApplyOpenMobileDeviceAndroidDisplayCutout", android_display)
		self.assertIn("does not expose display-cutout rectangles", ios_backend)
		self.assertNotIn("DisplayCutouts", ios_display)

	def test_window_orientation_uses_ui_state_and_safe_frame_events(self) -> None:
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		android_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidDisplay.cpp"
		).read_text(encoding="utf-8")
		ios_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSDisplay.mm"
		).read_text(encoding="utf-8")
		monitoring = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceMonitoringService.cpp"
		).read_text(encoding="utf-8")

		for token in ("getRotation", "getRealSize"):
			self.assertIn(token, android_upl)
		self.assertIn("ApplyOpenMobileDeviceAndroidWindowOrientation", android_display)
		self.assertIn("WindowScene.interfaceOrientation", ios_display)
		self.assertNotIn("UIDeviceOrientation", ios_display)
		self.assertIn("OnSafeFrameChangedEvent", monitoring)

	def test_orientation_control_uses_reversible_platform_policy(self) -> None:
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		android_control = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidOrientationControl.cpp"
		).read_text(encoding="utf-8")
		ios_control = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSOrientationControl.mm"
		).read_text(encoding="utf-8")

		for token in (
			"getRequestedOrientation",
			"setRequestedOrientation",
			"SCREEN_ORIENTATION_SENSOR_PORTRAIT",
			"SCREEN_ORIENTATION_SENSOR_LANDSCAPE",
			"SCREEN_ORIENTATION_REVERSE_PORTRAIT",
			"SCREEN_ORIENTATION_REVERSE_LANDSCAPE",
		):
			self.assertIn(token, android_upl)
		self.assertIn("AndroidThunkJava_OpenMobileDeviceApplyOrientationPolicy", android_control)
		self.assertIn("AndroidThunkJava_OpenMobileDeviceClearOrientationPolicy", android_control)
		for token in (
			"supportedInterfaceOrientations",
			"setNeedsUpdateOfSupportedInterfaceOrientations",
			"requestGeometryUpdateWithPreferences",
			"UIWindowSceneGeometryPreferencesIOS",
			"UIInterfaceOrientationMaskPortraitUpsideDown",
			"UIInterfaceOrientationMaskLandscapeLeft",
			"UIInterfaceOrientationMaskLandscapeRight",
			"presentedViewController",
			"UISceneActivationStateBackground",
		):
			self.assertIn(token, ios_control)
		self.assertNotIn("setValue:forKey", ios_control)

	def test_multi_window_events_use_public_platform_signals(self) -> None:
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		android_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidDisplay.cpp"
		).read_text(encoding="utf-8")
		android_monitor = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidWindowMonitor.cpp"
		).read_text(encoding="utf-8")
		ios_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSDisplay.mm"
		).read_text(encoding="utf-8")
		monitoring = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceMonitoringService.cpp"
		).read_text(encoding="utf-8")

		for token in (
			"onMultiWindowModeChanged",
			"onPictureInPictureModeChanged",
			"AndroidThunkJava_OpenMobileDeviceStartWindowMonitoring",
			"AndroidThunkJava_OpenMobileDeviceStopWindowMonitoring",
			"nativeOpenMobileDeviceWindowChanged",
		):
			self.assertIn(token, android_upl)
		for token in (
			"AndroidThunkJava_OpenMobileDeviceStartWindowMonitoring",
			"AndroidThunkJava_OpenMobileDeviceStopWindowMonitoring",
			"nativeOpenMobileDeviceWindowChanged",
		):
			self.assertIn(token, android_monitor)
		for token in (
			"isInMultiWindowMode",
			"isInPictureInPictureMode",
			"FOpenMobileDeviceWindowMode::FromAndroid",
		):
			self.assertIn(token, android_display)
		self.assertIn("FOpenMobileDeviceWindowMode::FromIOS", ios_display)
		self.assertIn("WindowDebounceSeconds", monitoring)
		self.assertNotIn("windowConfiguration", android_display)
		self.assertNotIn("getWindowingMode", android_display)

	def test_foldable_support_is_pinned_and_android_only(self) -> None:
		android_upl_path = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		)
		android_upl = android_upl_path.read_text(encoding="utf-8")
		android_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidDisplay.cpp"
		).read_text(encoding="utf-8")
		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")

		self.assertIn("androidx.window:window-java:1.5.1", android_upl)
		for token in (
			"WindowInfoTrackerCallbackAdapter",
			"WindowLayoutInfo",
			"FoldingFeature.State.FLAT",
			"FoldingFeature.State.HALF_OPENED",
			"FoldingFeature.Orientation.HORIZONTAL",
			"FoldingFeature.Orientation.VERTICAL",
			"isSeparating",
			"getLocationInWindow",
			"AndroidThunkJava_OpenMobileDeviceGetFoldableInfo",
		):
			self.assertIn(token, android_upl)
		for token in (
			"AndroidThunkJava_OpenMobileDeviceGetFoldableInfo",
			"FOpenMobileDeviceFoldableInfo::Apply",
		):
			self.assertIn(token, android_display)
		android_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("GetAndroidBuildVersion() >= 23", android_backend)
		self.assertIn("Android 6.0 (API 23)", android_backend)
		self.assertIn("FoldablePosture", ios_backend)
		self.assertIn("NotSupported", ios_backend)

		for module_root in (
			DEVICE_PLUGIN / "Source" / "OpenMobileDevice",
			DEVICE_PLUGIN / "Source" / "OpenMobileDeviceIOS",
		):
			for path in module_root.rglob("*"):
				if path.is_file() and path.suffix in {".cs", ".cpp", ".h", ".mm", ".xml"}:
					self.assertNotIn(
						"androidx.window",
						path.read_text(encoding="utf-8"),
						str(path),
					)

	def test_hdr_capability_uses_active_display_and_separate_output_state(self) -> None:
		android_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidDisplay.cpp"
		).read_text(encoding="utf-8")
		ios_display = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSDisplay.mm"
		).read_text(encoding="utf-8")
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		android_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")

		for token in (
			"getHdrCapabilities",
			"getSupportedHdrTypes",
			"isWideColorGamut",
			"isScreenWideColorGamut",
			"FOpenMobileDeviceHdrInfo::Apply",
			"GRHIIsHDREnabled",
		):
			self.assertIn(token, android_display)
		for token in (
			"potentialEDRHeadroom",
			"UIDisplayGamutP3",
			"FOpenMobileDeviceHdrInfo::Apply",
			"GRHIIsHDREnabled",
			"TARGET_OS_SIMULATOR",
		):
			self.assertIn(token, ios_display)
		for token in (
			"DisplayManager.DisplayListener",
			"registerDisplayListener",
			"unregisterDisplayListener",
			"onDisplayChanged",
		):
			self.assertIn(token, android_upl)
		self.assertIn("HdrWideColor", android_backend)
		self.assertIn("GetAndroidBuildVersion() >= 24", android_backend)
		self.assertIn("HdrWideColor", ios_backend)
		self.assertIn("TARGET_OS_SIMULATOR", ios_backend)

		common_build = (
			DEVICE_PLUGIN / "Source" / "OpenMobileDevice" / "OpenMobileDevice.Build.cs"
		).read_text(encoding="utf-8")
		android_build = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "OpenMobileDeviceAndroid.Build.cs"
		).read_text(encoding="utf-8")
		ios_build = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "OpenMobileDeviceIOS.Build.cs"
		).read_text(encoding="utf-8")
		self.assertNotIn('"RHI"', common_build)
		self.assertIn('"RHI"', android_build)
		self.assertIn('"RHI"', ios_build)

	def test_brightness_control_is_scoped_reversible_and_permissionless(self) -> None:
		android_control = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBrightnessControl.cpp"
		).read_text(encoding="utf-8")
		ios_control = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBrightnessControl.mm"
		).read_text(encoding="utf-8")
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceBrightnessControlService.cpp"
		).read_text(encoding="utf-8")

		for token in (
			"AndroidThunkJava_OpenMobileDeviceGetBrightness",
			"AndroidThunkJava_OpenMobileDeviceApplyBrightness",
			"AndroidThunkJava_OpenMobileDeviceClearBrightness",
			"WindowManager.LayoutParams",
			"screenBrightness",
			"Settings.System.SCREEN_BRIGHTNESS",
			"CountDownLatch",
		):
			self.assertIn(token, android_upl)
		for forbidden_token in (
			"getBrightnessInfo",
			"BrightnessInfo",
			"WRITE_SETTINGS",
			"Settings.System.put",
		):
			self.assertNotIn(forbidden_token, android_upl)
		for token in (
			"AndroidThunkJava_OpenMobileDeviceGetBrightness",
			"AndroidThunkJava_OpenMobileDeviceApplyBrightness",
			"AndroidThunkJava_OpenMobileDeviceClearBrightness",
		):
			self.assertIn(token, android_control)
		for token in (
			"View.window.screen",
			"Screen.brightness",
			"TARGET_OS_SIMULATOR",
			"FOpenMobileDeviceBrightnessOverrideState",
		):
			self.assertIn(token, ios_control)
		for token in (
			"ApplicationWillEnterBackgroundDelegate",
			"ApplicationHasEnteredForegroundDelegate",
			"OnSafeFrameChangedEvent",
		):
			self.assertIn(token, service)

	def test_keep_screen_awake_is_foreground_scoped_and_permissionless(self) -> None:
		android_control = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidKeepScreenAwakeControl.cpp"
		).read_text(encoding="utf-8")
		ios_control = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSKeepScreenAwakeControl.mm"
		).read_text(encoding="utf-8")
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceKeepScreenAwakeControlService.cpp"
		).read_text(encoding="utf-8")

		for token in (
			"AndroidThunkJava_OpenMobileDeviceApplyKeepScreenAwake",
			"AndroidThunkJava_OpenMobileDeviceClearKeepScreenAwake",
			"FLAG_KEEP_SCREEN_ON",
			"addFlags",
			"clearFlags",
		):
			self.assertIn(token, android_upl)
		self.assertNotIn("WAKE_LOCK", android_upl)
		for token in (
			"AndroidThunkJava_OpenMobileDeviceApplyKeepScreenAwake",
			"AndroidThunkJava_OpenMobileDeviceClearKeepScreenAwake",
		):
			self.assertIn(token, android_control)
		for token in (
			"idleTimerDisabled",
			"View.window",
			"TARGET_OS_SIMULATOR",
		):
			self.assertIn(token, ios_control)
		for token in (
			"ApplicationWillEnterBackgroundDelegate",
			"ApplicationHasEnteredForegroundDelegate",
			"OnSafeFrameChangedEvent",
		):
			self.assertIn(token, service)

	def test_system_ui_control_preserves_navigation_and_settles_insets(self) -> None:
		android_control = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidSystemUiControl.cpp"
		).read_text(encoding="utf-8")
		ios_control = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSSystemUiControl.mm"
		).read_text(encoding="utf-8")
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		monitoring = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceMonitoringService.cpp"
		).read_text(encoding="utf-8")

		for token in (
			"AndroidThunkJava_OpenMobileDeviceApplySystemUiMode",
			"AndroidThunkJava_OpenMobileDeviceClearSystemUiMode",
			"WindowInsetsController",
			"setDecorFitsSystemWindows",
			"BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE",
			"SYSTEM_UI_FLAG_IMMERSIVE_STICKY",
			"SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN",
			"SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION",
			"postOnAnimation",
			"OpenMobileDeviceNotifyWindowChanged",
		):
			self.assertIn(token, android_upl)
		for token in (
			"AndroidThunkJava_OpenMobileDeviceApplySystemUiMode",
			"AndroidThunkJava_OpenMobileDeviceClearSystemUiMode",
		):
			self.assertIn(token, android_control)
		for token in (
			"edgesForExtendedLayout",
			"prefersStatusBarHidden",
			"prefersHomeIndicatorAutoHidden",
			"setNeedsStatusBarAppearanceUpdate",
			"setNeedsUpdateOfHomeIndicatorAutoHidden",
			"method_setImplementation",
			"NotifyWindowSettled",
			"TARGET_OS_SIMULATOR",
		):
			self.assertIn(token, ios_control)
		self.assertNotIn("preferredScreenEdgesDeferringSystemGestures", ios_control)
		self.assertIn("NotifyWindowSettled", monitoring)

	def test_public_consumer_uses_only_documented_device_header(self) -> None:
		consumer = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceEditor"
			/ "Private"
			/ "Tests"
			/ "OpenMobileDevicePublicConsumerTests.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('#include "OpenMobileDevice.h"', consumer)
		self.assertNotIn("/Internal/", consumer)
		self.assertNotIn("/Private/", consumer)
		self.assertNotIn("OpenMobileDeviceBackendRegistry", consumer)
		self.assertNotIn("OpenMobileDeviceMonitoringService", consumer)
		self.assertNotIn("IOpenMobileDeviceBackend", consumer)

		umbrella = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDevice.h"
		).read_text(encoding="utf-8")
		for public_contract in (
			"OpenMobileDeviceAsyncActionBase.h",
			"OpenMobileDeviceBlueprintLibrary.h",
			"OpenMobileDeviceCapabilities.h",
			"OpenMobileDeviceMonitoring.h",
			"OpenMobileDeviceRefreshRateControl.h",
			"OpenMobileDeviceSettings.h",
			"OpenMobileDeviceStorageQueryAsyncAction.h",
			"OpenMobileDeviceSubsystem.h",
		):
			self.assertIn(public_contract, umbrella)

	def test_native_callbacks_use_shared_game_thread_dispatch(self) -> None:
		service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceMonitoringService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('#include "OpenMobileAsync.h"', service)
		self.assertIn("OpenMobile::DispatchToGameThread", service)
		self.assertIn("SourceSequence <= State->LastNativeSequence", service)
		self.assertIn("State->CallbackToken != CallbackToken", service)

	def test_device_settings_are_used_for_fallback_polling(self) -> None:
		service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceMonitoringService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("GetDefault<UOpenMobileDeviceSettings>()", service)
		self.assertIn("GetValidatedFallbackPollingIntervalSeconds", service)

	def test_legacy_battery_and_volume_reads_are_backend_owned(self) -> None:
		blueprint_library = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceBlueprintLibrary.cpp"
		).read_text(encoding="utf-8")
		self.assertNotIn("FPlatformMisc", blueprint_library)
		self.assertIn("GetPowerSnapshot", blueprint_library)
		self.assertIn("GetMediaVolumeSnapshot", blueprint_library)

		for platform in ("Android", "IOS"):
			backend = (
				DEVICE_PLUGIN
				/ "Source"
				/ f"OpenMobileDevice{platform}"
				/ "Private"
				/ f"OpenMobileDevice{platform}Backend.cpp"
			).read_text(encoding="utf-8")
			self.assertIn(f"GetOpenMobileDevice{platform}PowerSnapshot", backend)
			self.assertNotIn("FPlatformMisc::GetBatteryLevel", backend)
			self.assertIn("FPlatformMisc::GetDeviceVolume", backend)

	def test_battery_precision_and_observers_are_platform_owned(self) -> None:
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for token in (
			"BatteryManager.EXTRA_LEVEL",
			"BatteryManager.EXTRA_SCALE",
			"BatteryManager.EXTRA_PRESENT",
			"BatteryManager.EXTRA_STATUS",
			"BatteryManager.EXTRA_PLUGGED",
			"ACTION_BATTERY_CHANGED",
			"OpenMobileDeviceBatteryReceiver",
			"unregisterReceiver(OpenMobileDeviceBatteryReceiver)",
			"isPowerSaveMode",
			"ACTION_POWER_SAVE_MODE_CHANGED",
			"getCurrentThermalStatus",
			"addThermalStatusListener",
			"removeThermalStatusListener",
			"getThermalHeadroom",
		):
			self.assertIn(token, android_upl)

		android_battery = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBattery.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ApplyRatio", android_battery)
		self.assertIn("NotifyNativeChange", android_battery)

		ios_battery = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBattery.mm"
		).read_text(encoding="utf-8")
		self.assertIn("TARGET_OS_SIMULATOR", ios_battery)
		self.assertIn("Device.batteryLevel", ios_battery)
		self.assertIn("Device.batteryState", ios_battery)
		self.assertIn("UIDeviceBatteryLevelDidChangeNotification", ios_battery)
		self.assertIn("UIDeviceBatteryStateDidChangeNotification", ios_battery)
		self.assertIn("lowPowerModeEnabled", ios_battery)
		self.assertIn("NSProcessInfoPowerStateDidChangeNotification", ios_battery)
		self.assertIn("bRestoreBatteryMonitoringDisabled", ios_battery)
		self.assertIn("removeObserver", ios_battery)

		battery_info = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceBatteryInfo.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('TEXT("Android:%lld")', battery_info)
		self.assertIn('TEXT("IOS:%lld")', battery_info)
		self.assertNotIn("BatteryPercent.Value", battery_info)
		self.assertIn("ApplyAndroidChargingSource", battery_info)
		self.assertIn("ApplyIOSChargingSource", battery_info)
		self.assertIn("ApplyAndroidPowerSavingState", battery_info)
		self.assertIn("ApplyIOSPowerSavingState", battery_info)
		self.assertIn("ApplyAndroidThermalState", battery_info)
		self.assertIn("ApplyIOSThermalState", battery_info)
		self.assertNotIn("Scalability", battery_info)

		android_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("Wireless requires API 17", android_backend)
		self.assertIn("Dock maps to Other on API 33", android_backend)
		self.assertIn("power-save mode is available on API 21", android_backend)
		self.assertIn("thermal status is advisory and available on API 29", android_backend)
		self.assertIn('TEXT("Android 10 (API 29)")', android_backend)
		self.assertIn("forecast windows from 0 through 60 seconds on API 30", android_backend)
		self.assertIn('TEXT("Android 11 (API 30)")', android_backend)

		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("does not expose a public charging-source API", ios_backend)
		self.assertIn("Low Power Mode is available on iOS 9", ios_backend)
		self.assertIn("Simulator does not provide device Low Power Mode", ios_backend)
		self.assertIn("thermal state is advisory and available on iOS 11", ios_backend)
		self.assertIn("Simulator does not provide device thermal state", ios_backend)
		self.assertIn("does not expose a public thermal-headroom", ios_backend)

		self.assertIn("thermalState", ios_battery)
		self.assertIn("NSProcessInfoThermalStateDidChangeNotification", ios_battery)

		thermal_headroom = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceThermalHeadroom.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("MinimumSampleIntervalSeconds", thermal_headroom)
		self.assertIn("MaximumTrendGapSeconds", thermal_headroom)
		self.assertIn("MaximumForecastSeconds", thermal_headroom)
		self.assertIn("FMath::IsFinite", thermal_headroom)

		android_battery_module = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBattery.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ShouldSample", android_battery_module)
		self.assertIn("ApplyLatest", android_battery_module)
		self.assertIn("DefaultForecastSeconds", android_battery_module)

		android_module = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidModule.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ApplicationWillEnterBackgroundDelegate", android_module)
		self.assertIn("ApplicationHasEnteredForegroundDelegate", android_module)
		self.assertIn("ResetOpenMobileDeviceAndroidThermalHeadroomTrend", android_module)

		subsystem_header = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceSubsystem.h"
		).read_text(encoding="utf-8")
		for event in (
			"OnBatteryChanged",
			"OnPowerSavingModeChanged",
			"OnThermalChanged",
			"OnPowerSnapshotChanged",
			"OnNativeBatteryChanged",
			"OnNativePowerSavingModeChanged",
			"OnNativeThermalChanged",
		):
			self.assertIn(event, subsystem_header)

		subsystem = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceSubsystem.cpp"
		).read_text(encoding="utf-8")
		for comparator in (
			"EquivalentBattery",
			"EquivalentPowerSavingMode",
			"EquivalentThermal",
		):
			self.assertIn(comparator, subsystem)
		battery_broadcast = subsystem.index("OnBatteryChanged.Broadcast")
		power_saving_broadcast = subsystem.index("OnPowerSavingModeChanged.Broadcast")
		thermal_broadcast = subsystem.index("OnThermalChanged.Broadcast")
		combined_broadcast = subsystem.index("OnPowerSnapshotChanged.Broadcast")
		self.assertLess(battery_broadcast, power_saving_broadcast)
		self.assertLess(power_saving_broadcast, thermal_broadcast)
		self.assertLess(thermal_broadcast, combined_broadcast)
		self.assertIn("PowerSavingEvents", android_backend)
		self.assertIn("ThermalEvents", android_backend)
		self.assertIn("PowerSavingEvents", ios_backend)
		self.assertIn("ThermalEvents", ios_backend)

	def test_memory_snapshot_uses_platform_owned_sources(self) -> None:
		memory_info = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceMemoryInfo.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("MAX_int64", memory_info)
		self.assertIn("AvailablePhysicalBytes <= TotalPhysicalBytes", memory_info)
		self.assertNotIn("FPlatformMemory", memory_info)

		android_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FPlatformMemory::GetStats", android_backend)
		self.assertIn("PhysicalMemory", android_backend)

		ios_memory = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSMemory.mm"
		).read_text(encoding="utf-8")
		self.assertIn("TARGET_OS_SIMULATOR", ios_memory)
		self.assertIn("physicalMemory", ios_memory)
		self.assertIn("os_proc_available_memory", ios_memory)
		self.assertIn("true,", ios_memory)

		blueprint_library = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceBlueprintLibrary.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FormatByteCount", blueprint_library)
		self.assertIn("FText::AsMemory", blueprint_library)

		resource_types = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceResourceTypes.h"
		).read_text(encoding="utf-8")
		for field in (
			"LatestPressureEventState",
			"NativeMemoryPressureLevel",
			"PressureEventTimeUtc",
			"PressureEventSequence",
		):
			self.assertIn(field, resource_types)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for token in (
			"ComponentCallbacks2",
			"onTrimMemory",
			"onLowMemory",
			"registerComponentCallbacks",
			"unregisterComponentCallbacks",
			"nativeOpenMobileDeviceMemoryPressure",
		):
			self.assertIn(token, android_upl)

		android_memory_monitor = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidMemoryMonitor.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("NormalizeAndroidTrimLevel", android_memory_monitor)
		self.assertIn("NotifyNativeChange", android_memory_monitor)
		self.assertNotIn("UE_LOG", android_memory_monitor)
		self.assertNotIn("TArray", android_memory_monitor)

		ios_memory_monitor = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSMemoryMonitor.mm"
		).read_text(encoding="utf-8")
		self.assertIn("UIApplicationDidReceiveMemoryWarningNotification", ios_memory_monitor)
		self.assertIn("NotifyNativeChange", ios_memory_monitor)
		self.assertIn("removeObserver", ios_memory_monitor)
		self.assertNotIn("UE_LOG", ios_memory_monitor)

		self.assertIn("StartOpenMobileDeviceAndroidMemoryMonitoring", android_backend)
		self.assertIn("MemoryPressureEvents", android_backend)
		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("StartOpenMobileDeviceIOSMemoryMonitoring", ios_backend)
		self.assertIn("MemoryPressureEvents", ios_backend)

	def test_storage_space_is_async_and_volume_scoped(self) -> None:
		storage_info = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceStorageInfo.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("MAX_int64", storage_info)
		self.assertIn("AvailableBytes <= TotalBytes", storage_info)
		self.assertIn("ImportantUsageAvailableBytes <= TotalBytes", storage_info)

		async_query = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceStorageQueryAsyncAction.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("EAsyncExecution::ThreadPool", async_query)
		self.assertIn("OpenMobile::DispatchToGameThread", async_query)
		self.assertIn("IsCallbackCurrent", async_query)
		self.assertIn("StampStorageSnapshot", async_query)
		self.assertIn("CacheStorageSnapshot", async_query)

		subsystem = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertNotIn(
			"FOpenMobileDeviceSnapshotService::GetStorageSnapshot()",
			subsystem,
		)
		self.assertIn("LastStorageBackendGeneration", subsystem)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for token in (
			"getFilesDir()",
			"android.os.StatFs",
			"getTotalBytes()",
			"getAvailableBytes()",
		):
			self.assertIn(token, android_upl)
		self.assertNotIn("getExternalStorageDirectory", android_upl)

		android_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("QueryOpenMobileDeviceAndroidStorage", android_backend)
		self.assertIn("internal application data volume", android_backend)

		ios_storage = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSStorage.mm"
		).read_text(encoding="utf-8")
		for token in (
			"NSHomeDirectory()",
			"NSURLVolumeTotalCapacityKey",
			"NSURLVolumeAvailableCapacityKey",
			"NSURLVolumeAvailableCapacityForImportantUsageKey",
			"TARGET_OS_SIMULATOR",
		):
			self.assertIn(token, ios_storage)

		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("QueryOpenMobileDeviceIOSStorage", ios_backend)
		self.assertIn("important-usage capacity", ios_backend)
		self.assertIn("host Mac volume", ios_backend)

	def test_low_storage_events_are_bounded_and_demand_driven(self) -> None:
		storage_info = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceStorageInfo.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"PreviousLowStorageState",
			"RecoveryThresholdBytes",
			"MAX_int64 - ValidThresholdBytes",
			"AvailableBytes.Value < RecoveryThresholdBytes",
			"AvailableBytes.Value <= ValidThresholdBytes",
		):
			self.assertIn(token, storage_info)

		settings = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceSettings.h"
		).read_text(encoding="utf-8")
		for token in (
			"bUsePlatformDefaultLowStorageThreshold",
			"LowStorageThresholdBytes",
			"LowStorageRecoveryHysteresisBytes",
			"LowStorageFallbackPollingIntervalSeconds",
			"GetMinimumLowStorageFallbackPollingIntervalSeconds",
		):
			self.assertIn(token, settings)

		service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceMonitoringService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("RequiresFallbackPolling", service)
		self.assertIn("ApplyGroupIntervalBounds", service)
		self.assertIn(
			"GetValidatedLowStorageFallbackPollingIntervalSeconds",
			service,
		)

		subsystem = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("RequestStorageRefreshForMonitoring", subsystem)
		self.assertIn("QueryStorage(GetGameInstance())", subsystem)
		self.assertIn("ActiveStorageMonitoringQuery", subsystem)
		self.assertIn("bStorageMonitoringRefreshPending", subsystem)

		android_upl_path = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		)
		ET.parse(android_upl_path)
		android_upl = android_upl_path.read_text(encoding="utf-8")
		for token in (
			"ACTION_DEVICE_STORAGE_LOW",
			"ACTION_DEVICE_STORAGE_OK",
			"OpenMobileDeviceStorageReceiver",
			"nativeOpenMobileDeviceStorageChanged",
			"unregisterReceiver(OpenMobileDeviceStorageReceiver)",
			"catch (RuntimeException ignored)",
		):
			self.assertIn(token, android_upl)

		android_monitor = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidStorageMonitor.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("NotifyNativeChange", android_monitor)
		self.assertIn("FCriticalSection", android_monitor)
		self.assertNotIn("UE_LOG", android_monitor)

		android_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("StartOpenMobileDeviceAndroidStorageMonitoring", android_backend)
		self.assertIn("LowStorageEvents", android_backend)
		self.assertIn("MaximumThresholdBytes = 500ll", android_backend)
		self.assertIn("bUsePlatformDefaultLowStorageThreshold", android_backend)
		self.assertIn(
			"GetValidatedLowStorageRecoveryHysteresisBytes",
			android_backend,
		)

		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("LowStorageEvents", ios_backend)
		self.assertIn("MaximumThresholdBytes = 1024ll", ios_backend)
		self.assertIn("no public low-storage notification", ios_backend)
		self.assertNotIn("StartOpenMobileDeviceIOSStorageMonitoring", ios_backend)

	def test_network_path_uses_os_state_without_endpoint_probes(self) -> None:
		normalizer = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceNetworkPathInfo.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"DeclaredCapability",
			"OsValidatedPath",
			"CaptivePortal",
			"InternetCapable",
			"bInternetValidated",
			"bRestricted",
			"StableOrder",
			"EOpenMobileNetworkTransport::VPN",
			"EOpenMobileNetworkTransport::Other",
		):
			self.assertIn(token, normalizer)

		android_upl_path = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		)
		ET.parse(android_upl_path)
		android_upl = android_upl_path.read_text(encoding="utf-8")
		for token in (
			"android.permission.ACCESS_NETWORK_STATE",
			"getActiveNetwork()",
			"getNetworkCapabilities(network)",
			"NET_CAPABILITY_INTERNET",
			"NET_CAPABILITY_VALIDATED",
			"NET_CAPABILITY_CAPTIVE_PORTAL",
			"NET_CAPABILITY_LOCAL_NETWORK",
			"NET_CAPABILITY_NOT_RESTRICTED",
			"NET_CAPABILITY_NOT_METERED",
			"NET_CAPABILITY_TEMPORARILY_NOT_METERED",
			"notBandwidthConstrainedCapability = 37",
			"hasTransport(type)",
			"TRANSPORT_CELLULAR",
			"TRANSPORT_WIFI",
			"TRANSPORT_BLUETOOTH",
			"TRANSPORT_ETHERNET",
			"TRANSPORT_VPN",
			"TRANSPORT_WIFI_AWARE",
			"TRANSPORT_LOWPAN",
			"TRANSPORT_USB",
			"TRANSPORT_THREAD",
			"TRANSPORT_SATELLITE",
		):
			self.assertIn(token, android_upl)
		for forbidden in (
			"ACCESS_FINE_LOCATION",
			"ACCESS_COARSE_LOCATION",
			"getSSID",
			"getBSSID",
			"TelephonyManager",
			"startCaptivePortalApp",
			"ACTION_CAPTIVE_PORTAL_SIGN_IN",
			"HttpURLConnection",
			"java.net.URL",
		):
			self.assertNotIn(forbidden, android_upl)

		android_network = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidNetwork.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ParseBoolean", android_network)
		self.assertIn("BuildAndroid", android_network)
		self.assertIn("ParseIntoArray", android_network)
		self.assertIn("bMeteredStateAvailable", android_network)
		self.assertIn("bConstrainedStateAvailable", android_network)
		self.assertNotIn("FHttpModule", android_network)

		android_network_monitor = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidNetworkMonitor.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"nativeOpenMobileDeviceNetworkChanged",
			"NotifyNativeChange",
			"StartNetworkMonitoring",
			"StopNetworkMonitoring",
		):
			self.assertIn(token, android_network_monitor)
		for token in (
			"registerDefaultNetworkCallback",
			"unregisterNetworkCallback",
			"onAvailable",
			"onLost",
			"onCapabilitiesChanged",
			"onBlockedStatusChanged",
		):
			self.assertIn(token, android_upl)

		ios_network = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSNetwork.mm"
		).read_text(encoding="utf-8")
		for token in (
			"SCNetworkReachabilityCreateWithAddress",
			"SCNetworkReachabilityGetFlags",
			"kSCNetworkReachabilityFlagsReachable",
			"kSCNetworkReachabilityFlagsConnectionRequired",
			"FPlatformMisc::GetNetworkConnectionType",
			"EOpenMobileNetworkTransport::Wifi",
			"EOpenMobileNetworkTransport::Cellular",
			"EOpenMobileNetworkTransport::Ethernet",
			"GetNetworkConnectionCharacteristics",
			"Policy.bIsExpensive",
			"Policy.bIsConstrained",
		):
			self.assertIn(token, ios_network)
		for forbidden in ("CreateWithName", "NSURLSession", "FHttpModule"):
			self.assertNotIn(forbidden, ios_network)

		ios_network_monitor = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSNetworkMonitor.mm"
		).read_text(encoding="utf-8")
		for token in (
			"OnNetworkConnectionChanged",
			"OnNetworkConnectionCharacteristicsChanged",
			"NotifyNativeChange",
		):
			self.assertIn(token, ios_network_monitor)
		self.assertNotIn("nw_path_monitor_create", ios_network_monitor)

		monitoring_service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceMonitoringService.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"NetworkDebounceSeconds = 0.25f",
			"GetNetworkPathSnapshot",
			"NetworkPathChanged.Broadcast(Snapshot)",
			"PendingNetworkSnapshot",
		):
			self.assertIn(token, monitoring_service)

		for platform in ("Android", "IOS"):
			backend = (
				DEVICE_PLUGIN
				/ "Source"
				/ f"OpenMobileDevice{platform}"
				/ "Private"
				/ f"OpenMobileDevice{platform}Backend.cpp"
			).read_text(encoding="utf-8")
			self.assertIn("NetworkPath", backend)
			self.assertIn("GetNetworkPathSnapshot", backend)

	def test_application_metadata_uses_packaged_sources(self) -> None:
		application_info = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceApplicationInfo.cpp"
		).read_text(encoding="utf-8")
		self.assertNotIn("CFBundle", application_info)
		self.assertNotIn("PackageManager", application_info)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		)
		ET.parse(android_upl)
		android_contents = android_upl.read_text(encoding="utf-8")
		for expected in (
			"getApplicationLabel",
			"getPackageName",
			"versionName",
			"getLongVersionCode",
		):
			self.assertIn(expected, android_contents)

		ios_application = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSApplication.mm"
		).read_text(encoding="utf-8")
		for expected in (
			"localizedInfoDictionary",
			"CFBundleDisplayName",
			"bundleIdentifier",
			"CFBundleShortVersionString",
			"CFBundleVersion",
		):
			self.assertIn(expected, ios_application)

	def test_emulator_detection_uses_non_unique_traits(self) -> None:
		public_identity = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceIdentityTypes.h"
		).read_text(encoding="utf-8")
		self.assertNotIn("Fingerprint", public_identity)

		android_identity = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidIdentity.cpp"
		).read_text(encoding="utf-8")
		for expected in ('"HARDWARE"', '"PRODUCT"', '"FINGERPRINT"'):
			self.assertIn(expected, android_identity)
		for forbidden in (
			'"SERIAL"',
			"ANDROID_ID",
			"AdvertisingId",
			"getDeviceId",
			"getImei",
			"getMacAddress",
			"identifierForVendor",
		):
			self.assertNotIn(forbidden, android_identity)

		ios_identity = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSIdentity.mm"
		).read_text(encoding="utf-8")
		self.assertIn("TARGET_OS_SIMULATOR", ios_identity)
		self.assertIn("ApplyIOS", ios_identity)

	def test_preferred_languages_stay_os_owned(self) -> None:
		locale_info = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceLocaleInfo.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('ReplaceInline(TEXT("_"), TEXT("-"))', locale_info)
		self.assertIn("TSet<FString> Seen", locale_info)
		self.assertNotIn("GetPrioritizedCultureNames", locale_info)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertIn("configuration.getLocales", android_upl)
		self.assertIn("toLanguageTag", android_upl)

		ios_locale = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSLocale.mm"
		).read_text(encoding="utf-8")
		self.assertIn("[NSLocale preferredLanguages]", ios_locale)
		self.assertIn("GetCurrentCulture", ios_locale)
		self.assertNotIn("GetPreferredLanguages", ios_locale)

		self.assertIn("AndroidThunkJava_OpenMobileDeviceGetLocaleDetails", android_upl)
		self.assertIn("locale.toLanguageTag", android_upl)
		self.assertIn("locale.getScript", android_upl)
		self.assertIn("java.util.Currency.getInstance", android_upl)
		self.assertIn("[NSLocale currentLocale]", ios_locale)
		for forbidden in (
			"getLastKnownLocation",
			"getNetworkCountryIso",
			"getSimCountryIso",
			"Storefront",
		):
			self.assertNotIn(forbidden, android_upl)
			self.assertNotIn(forbidden, ios_locale)

		self.assertIn("getCanonicalID", android_upl)
		self.assertIn("getOffset(utcMilliseconds)", android_upl)
		self.assertIn("inDaylightTime(instant)", android_upl)
		self.assertNotIn("getRawOffset", android_upl)
		self.assertIn("secondsFromGMTForDate", ios_locale)
		self.assertIn("isDaylightSavingTimeForDate", ios_locale)
		self.assertIn("localTimeZone", ios_locale)

		snapshot_service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceSnapshotService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("GetLocaleSnapshotAtUtc(FDateTime::UtcNow())", snapshot_service)

	def test_regional_preferences_use_platform_sources_and_unreal_formatting(self) -> None:
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertIn("DateFormat.is24HourFormat(this)", android_upl)
		self.assertIn("LocaleData.getMeasurementSystem", android_upl)
		self.assertIn("locale.getCountry().isEmpty()", android_upl)
		self.assertNotIn("Locale.getDefault().getLanguage", android_upl)

		ios_locale = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSLocale.mm"
		).read_text(encoding="utf-8")
		self.assertIn('dateFormatFromTemplate:@"j"', ios_locale)
		self.assertIn("NSLocaleUsesMetricSystem", ios_locale)
		self.assertIn("[Locale countryCode]", ios_locale)

		blueprint_library = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceBlueprintLibrary.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FText::AsDate(DateTime)", blueprint_library)
		self.assertIn("FText::AsTime(DateTime)", blueprint_library)
		self.assertIn("FText::AsDateTime(DateTime)", blueprint_library)

	def test_locale_change_observers_are_native_and_demand_driven(self) -> None:
		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for action in (
			"ACTION_LOCALE_CHANGED",
			"ACTION_CONFIGURATION_CHANGED",
			"ACTION_TIME_CHANGED",
			"ACTION_TIMEZONE_CHANGED",
			"ACTION_DATE_CHANGED",
		):
			self.assertIn(action, android_upl)
		self.assertIn("registerReceiver", android_upl)
		self.assertIn("unregisterReceiver", android_upl)
		self.assertNotIn("ACTION_TIME_TICK", android_upl)

		android_monitor = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidLocaleMonitor.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("nativeOpenMobileDeviceLocaleChanged", android_monitor)
		self.assertIn("NotifyNativeChange", android_monitor)
		self.assertIn("FCriticalSection", android_monitor)

		ios_monitor = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSLocaleMonitor.mm"
		).read_text(encoding="utf-8")
		for notification in (
			"NSCurrentLocaleDidChangeNotification",
			"UIApplicationSignificantTimeChangeNotification",
			"NSSystemClockDidChangeNotification",
			"NSSystemTimeZoneDidChangeNotification",
		):
			self.assertIn(notification, ios_monitor)
		self.assertIn("removeObserver", ios_monitor)

		for platform in ("Android", "IOS"):
			backend = (
				DEVICE_PLUGIN
				/ "Source"
				/ f"OpenMobileDevice{platform}"
				/ "Private"
				/ f"OpenMobileDevice{platform}Backend.cpp"
			).read_text(encoding="utf-8")
			self.assertIn("LocaleChangeEvents", backend)
			self.assertIn("RegionalFormatting", backend)
			self.assertIn("BeginShutdown", backend)

		subsystem = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceSubsystem.cpp"
		).read_text(encoding="utf-8")
		refresh = subsystem.index("Change.CurrentSnapshot = Snapshot")
		cache = subsystem.index("LastLocaleSnapshot = Snapshot", refresh)
		broadcast = subsystem.index("OnLocaleSnapshotChanged.Broadcast", cache)
		self.assertLess(refresh, cache)
		self.assertLess(cache, broadcast)

	def test_platform_information_reads_are_backend_owned(self) -> None:
		shared_parser = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDevicePlatformInfo.cpp"
		).read_text(encoding="utf-8")
		self.assertNotIn("FPlatformMisc", shared_parser)

		for platform in ("Android", "IOS"):
			backend = (
				DEVICE_PLUGIN
				/ "Source"
				/ f"OpenMobileDevice{platform}"
				/ "Private"
				/ f"OpenMobileDevice{platform}Backend.cpp"
			).read_text(encoding="utf-8")
			self.assertIn("FPlatformMisc::GetOSVersion", backend)
			self.assertIn("FOpenMobileDevicePlatformInfo::BuildSnapshot", backend)

		android_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FAndroidMisc::GetAndroidBuildVersion", android_backend)
		self.assertIn("FAndroidMisc::GetDeviceMake", android_backend)
		self.assertIn("FAndroidMisc::GetDeviceModel", android_backend)
		self.assertIn("GetOpenMobileDeviceAndroidBrand", android_backend)
		self.assertNotIn("GetProductName", android_backend)

		android_identity = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidIdentity.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('"BRAND"', android_identity)
		self.assertIn('"DEVICE"', android_identity)

		ios_identity = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSIdentity.mm"
		).read_text(encoding="utf-8")
		self.assertIn("[UIDevice currentDevice]", ios_identity)
		self.assertIn("model]", ios_identity)
		self.assertIn('"hw.machine"', ios_identity)
		self.assertIn('SIMULATOR_MODEL_IDENTIFIER', ios_identity)

		for identity_source in (android_identity, ios_identity):
			for forbidden_token in (
				"ANDROID_ID",
				"AdvertisingId",
				"advertisingIdentifier",
				"getDeviceId",
				"getImei",
				"getMacAddress",
				"getMeid",
				"getSerial",
				"identifierForVendor",
				'"SERIAL"',
			):
				self.assertNotIn(forbidden_token, identity_source)

		for backend in (
			android_backend,
			(
				DEVICE_PLUGIN
				/ "Source"
				/ "OpenMobileDeviceIOS"
				/ "Private"
				/ "OpenMobileDeviceIOSBackend.cpp"
			).read_text(encoding="utf-8"),
		):
			self.assertIn("ManufacturerBrandModel", backend)
			self.assertIn("HardwareModelIdentifier", backend)
			self.assertIn("FormFactor", backend)

		form_factor = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceFormFactor.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("SmallestWindowWidthDp", form_factor)
		self.assertIn("WindowSizeClass", form_factor)
		self.assertNotIn("Pixels", form_factor)

		self.assertIn("smallestScreenWidthDp", android_identity)
		self.assertIn("screenLayout", android_identity)
		self.assertIn("android.hardware.sensor.hinge_angle", android_identity)
		self.assertIn("userInterfaceIdiom", ios_identity)

		snapshot_service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceSnapshotService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("Backend->GetDeviceFormFactor()", snapshot_service)

		architecture = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceArchitecture.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ToLowerInline", architecture)
		self.assertNotIn("SUPPORTED_ABIS", architecture)
		self.assertIn('"SUPPORTED_ABIS"', android_identity)
		self.assertIn("FPlatformMisc::GetUBTArchitecture", android_backend)
		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FPlatformMisc::GetUBTArchitecture", ios_backend)

		processor_info = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceProcessorInfo.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("LogicalProcessorCount > 0", processor_info)
		self.assertNotIn("FPlatformMisc", processor_info)
		for backend in (android_backend, ios_backend):
			self.assertIn("NumberOfCoresIncludingHyperthreads", backend)
			self.assertIn("LogicalProcessorCount", backend)


	def test_flashlight_state_is_read_only_native_and_demand_driven(self) -> None:
		public_types = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceFlashlightTypes.h"
		).read_text(encoding="utf-8")
		for token in (
			"HardwareState",
			"TorchState",
			"bVariableIntensitySupported",
			"MinimumIntensity",
			"MaximumIntensity",
			"PermissionState",
			"ConflictState",
			"ThermalState",
			"Ownership",
		):
			self.assertIn(token, public_types)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for token in (
			"FLASH_INFO_AVAILABLE",
			"FLASH_INFO_STRENGTH_MAXIMUM_LEVEL",
			"registerTorchCallback",
			"unregisterTorchCallback",
			"onTorchModeChanged",
			"onTorchModeUnavailable",
			"onTorchStrengthLevelChanged",
		):
			self.assertIn(token, android_upl)
		self.assertNotIn("openCamera(", android_upl)

		ios_flashlight = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSFlashlight.mm"
		).read_text(encoding="utf-8")
		for token in (
			"hasTorch",
			"isTorchAvailable",
			"isTorchActive",
			"torchLevel",
			"authorizationStatusForMediaType",
			"systemPressureState",
			"addObserver",
			"removeObserver",
		):
			self.assertIn(token, ios_flashlight)
		self.assertNotIn("AVCaptureSession", ios_flashlight)
		self.assertNotIn("requestAccessForMediaType", ios_flashlight)
		self.assertIn("TARGET_OS_SIMULATOR", ios_flashlight)

	def test_flashlight_control_is_typed_async_and_lifecycle_safe(self) -> None:
		action = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceFlashlightAsyncAction.h"
		).read_text(encoding="utf-8")
		self.assertIn("UOpenMobileDeviceAsyncActionBase", action)
		self.assertIn("FOpenMobileFlashlightOperationResult", action)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for token in (
			"setTorchMode",
			"turnOnTorchWithStrengthLevel",
			"getTorchStrengthLevel",
			"CameraAccessException.CAMERA_IN_USE",
			"CameraAccessException.MAX_CAMERAS_IN_USE",
		):
			self.assertIn(token, android_upl)
		self.assertNotIn("android.permission.CAMERA", android_upl)

		ios_flashlight = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSFlashlight.mm"
		).read_text(encoding="utf-8")
		for token in (
			"lockForConfiguration",
			"unlockForConfiguration",
			"setTorchModeOnWithLevel",
			"AVErrorTorchLevelUnavailable",
		):
			self.assertIn(token, ios_flashlight)
		self.assertNotIn("AVFoundation", action)

	def test_clipboard_operations_are_typed_bounded_and_privacy_aware(self) -> None:
		public_types = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceClipboardTypes.h"
		).read_text(encoding="utf-8")
		for token in (
			"FOpenMobileClipboardWriteRequest",
			"EOpenMobileClipboardOperationState",
			"FOpenMobileClipboardOperationResult",
			"Text",
			"Url",
		):
			self.assertIn(token, public_types)

		policy = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceClipboardPolicy.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("MaximumPayloadBytes", policy)
		self.assertIn("FTCHARToUTF8", policy)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for token in (
			"getPrimaryClipDescription",
			"ClipDescription.MIMETYPE_TEXT_PLAIN",
			"ClipDescription.MIMETYPE_TEXT_URILIST",
			"getPrimaryClip",
			"setPrimaryClip",
			"clearPrimaryClip",
			"Build.VERSION.SDK_INT < 28",
		):
			self.assertIn(token, android_upl)

		ios_clipboard = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSClipboard.mm"
		).read_text(encoding="utf-8")
		for token in (
			"hasStrings",
			"hasURLs",
			"numberOfItems",
			"Pasteboard.string",
			"Pasteboard.URL",
			"Pasteboard.items = @[]",
			"UIApplicationStateActive",
		):
			self.assertIn(token, ios_clipboard)
		ios_type_check = ios_clipboard.split(
			"CheckOpenMobileDeviceIOSClipboardContentTypes()",
			1,
		)[1].split("WriteOpenMobileDeviceIOSClipboard(", 1)[0]
		self.assertNotIn("Pasteboard.string", ios_type_check)
		self.assertNotIn("Pasteboard.URL", ios_type_check)
		self.assertNotIn("UE_LOG", ios_clipboard)

		subsystem = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceSubsystem.h"
		).read_text(encoding="utf-8")
		for token in (
			"CheckClipboardContentTypes",
			"WriteClipboard",
			"ReadClipboard",
			"ClearClipboard",
		):
			self.assertIn(token, subsystem)

	def test_user_initiated_paste_uses_owned_native_consent_paths(self) -> None:
		action = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceUserInitiatedPasteAsyncAction.h"
		).read_text(encoding="utf-8")
		for token in (
			"UOpenMobileDeviceAsyncActionBase",
			"FOpenMobileUserInitiatedPasteRequest",
			"FOpenMobileUserInitiatedPasteResult",
			"RequestUserInitiatedPaste",
		):
			self.assertIn(token, action)

		public_types = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceUserInitiatedPasteTypes.h"
		).read_text(encoding="utf-8")
		self.assertIn("bCallerConfirmsUserInitiated", public_types)
		for token in ("Success", "Cancelled", "Denied", "Failed"):
			self.assertIn(token, public_types)

		service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceUserInitiatedPasteService.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"ApplicationWillEnterBackgroundDelegate",
			"CancelUserInitiatedPaste",
			"bReadWasUserInitiated = true",
			"MaximumPayloadBytes",
		):
			self.assertIn(token, service)

		ios_paste = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSUserInitiatedPaste.mm"
		).read_text(encoding="utf-8")
		for token in (
			"UIPasteControl",
			"UIPasteConfigurationSupporting",
			"pasteItemProviders",
			"loadObjectOfClass",
			"removeFromSuperview",
			"NSProgress",
		):
			self.assertIn(token, ios_paste)
		for forbidden_token in ("UIPasteboard.general", "UE_LOG"):
			self.assertNotIn(forbidden_token, ios_paste)

		android_paste = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidUserInitiatedPaste.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ReadOpenMobileDeviceAndroidClipboard", android_paste)
		self.assertNotIn("UE_LOG", android_paste)

	def test_intent_handler_checks_use_declared_visibility_only(self) -> None:
		public_types = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceIntentHandlerTypes.h"
		).read_text(encoding="utf-8")
		for token in (
			"FOpenMobileIntentHandlerCheckRequest",
			"FOpenMobileIntentHandlerCheckResult",
			"CanHandle",
			"CannotHandle",
			"NotDeclared",
			"ConfigurationLimitExceeded",
		):
			self.assertIn(token, public_types)

		settings = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceSettings.h"
		).read_text(encoding="utf-8")
		self.assertIn("DeclaredUrlSchemes", settings)
		self.assertIn("DeclaredAndroidIntentActions", settings)

		subsystem = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceSubsystem.h"
		).read_text(encoding="utf-8")
		self.assertIn("CheckIntentHandler", subsystem)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for token in (
			'addElement tag="queries"',
			"android.intent.action.VIEW",
			"android.intent.category.BROWSABLE",
			"DeclaredUrlSchemes",
			"DeclaredAndroidIntentActions",
			"resolveActivity",
			"MATCH_DEFAULT_ONLY",
		):
			self.assertIn(token, android_upl)
		self.assertGreaterEqual(android_upl.count('once="true"'), 3)
		for forbidden_token in (
			"QUERY_ALL_PACKAGES",
			"queryIntentActivities",
			"getInstalledApplications",
			"getInstalledPackages",
		):
			self.assertNotIn(forbidden_token, android_upl)

		android_handler = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidIntentHandler.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("AndroidThunkJava_OpenMobileDeviceCheckIntentHandler", android_handler)
		for forbidden_token in (
			"queryIntentActivities",
			"getInstalledApplications",
			"getInstalledPackages",
			"UE_LOG",
		):
			self.assertNotIn(forbidden_token, android_handler)

		ios_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileDevice_IOS_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertIn("LSApplicationQueriesSchemes", ios_upl)
		self.assertIn("DeclaredUrlSchemes", ios_upl)
		ios_handler = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSIntentHandler.mm"
		).read_text(encoding="utf-8")
		self.assertIn("canOpenURL", ios_handler)
		self.assertNotIn("openURL", ios_handler.replace("canOpenURL", ""))
		self.assertNotIn("UE_LOG", ios_handler)

	def test_android_package_checks_are_declared_and_non_enumerating(self) -> None:
		public_types = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceAndroidPackageTypes.h"
		).read_text(encoding="utf-8")
		for token in (
			"FOpenMobileAndroidPackageCheckRequest",
			"FOpenMobileAndroidPackageCheckResult",
			"Installed",
			"Disabled",
			"NotFoundOrNotVisible",
			"NotDeclared",
			"Unsupported",
		):
			self.assertIn(token, public_types)

		settings = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceSettings.h"
		).read_text(encoding="utf-8")
		self.assertIn("DeclaredAndroidPackages", settings)

		subsystem = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceSubsystem.h"
		).read_text(encoding="utf-8")
		self.assertIn("CheckAndroidPackage", subsystem)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for token in (
			"DeclaredAndroidPackages",
			'value="package"',
			"AndroidThunkJava_OpenMobileDeviceCheckPackage",
			"getApplicationInfo",
			"MATCH_DISABLED_COMPONENTS",
			"getApplicationEnabledSetting",
			"NameNotFoundException",
			"SecurityException",
			"IllegalArgumentException",
			"COMPONENT_ENABLED_STATE_DISABLED_USER",
			"COMPONENT_ENABLED_STATE_DISABLED_UNTIL_USED",
		):
			self.assertIn(token, android_upl)
		for forbidden_token in (
			"QUERY_ALL_PACKAGES",
			"getInstalledApplications",
			"getInstalledPackages",
			"MATCH_UNINSTALLED_PACKAGES",
		):
			self.assertNotIn(forbidden_token, android_upl)

		android_check = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidPackageCheck.cpp"
		).read_text(encoding="utf-8")
		self.assertIn(
			"AndroidThunkJava_OpenMobileDeviceCheckPackage",
			android_check,
		)
		self.assertNotIn("UE_LOG", android_check)

		backend_contract = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Internal"
			/ "IOpenMobileDeviceBackend.h"
		).read_text(encoding="utf-8")
		self.assertIn("CheckAndroidPackage", backend_contract)
		self.assertIn(
			"EOpenMobileAndroidPackageCheckState::Unsupported",
			backend_contract,
		)
		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("AndroidPackageCheck", ios_backend)
		self.assertIn("EOpenMobileCapabilityState::NotSupported", ios_backend)

	def test_application_settings_open_is_app_scoped_and_lifecycle_aware(self) -> None:
		public_types = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceApplicationSettingsTypes.h"
		).read_text(encoding="utf-8")
		for state in (
			"Accepted",
			"Unsupported",
			"NoPresenter",
			"NativeFailure",
		):
			self.assertIn(state, public_types)

		service = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceApplicationSettingsService.cpp"
		).read_text(encoding="utf-8")
		for lifecycle_token in (
			"ApplicationWillEnterBackgroundDelegate",
			"ApplicationWillDeactivateDelegate",
			"ApplicationHasEnteredForegroundDelegate",
			"ApplicationHasReactivatedDelegate",
			"RefreshActiveGroups",
			"Returned.Broadcast",
		):
			self.assertIn(lifecycle_token, service)
		self.assertIn("bAwaitingSettingsReturn", service)
		self.assertIn("EOpenMobileApplicationSettingsOpenState::Accepted", service)
		subsystem = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("GetDeviceCapabilityReport", subsystem)
		self.assertIn("OnApplicationSettingsReturned.Broadcast", subsystem)

		backend_contract = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Internal"
			/ "IOpenMobileDeviceBackend.h"
		).read_text(encoding="utf-8")
		self.assertIn("OpenApplicationSettings", backend_contract)
		self.assertIn(
			"EOpenMobileApplicationSettingsOpenState::Unsupported",
			backend_contract,
		)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		settings_method_name = (
			"AndroidThunkJava_OpenMobileDeviceOpenApplicationSettings"
		)
		self.assertIn(settings_method_name, android_upl)
		settings_method = android_upl.split(settings_method_name, 1)[1].split(
			"AndroidThunkJava_OpenMobileDeviceCheckPackage",
			1,
		)[0]
		for required_token in (
			"Settings.ACTION_APPLICATION_DETAILS_SETTINGS",
			'Uri.fromParts("package", getPackageName(), null)',
			"ActivityNotFoundException",
		):
			self.assertIn(required_token, settings_method)
		for forbidden_token in (
			"Settings.ACTION_SETTINGS",
			"Settings.ACTION_APPLICATION_SETTINGS",
			"Settings.ACTION_MANAGE_APPLICATIONS_SETTINGS",
			"resolveActivity",
		):
			self.assertNotIn(forbidden_token, settings_method)

		android_open = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidApplicationSettings.cpp"
		).read_text(encoding="utf-8")
		self.assertIn(
			"AndroidThunkJava_OpenMobileDeviceOpenApplicationSettings",
			android_open,
		)
		self.assertNotIn("UE_LOG", android_open)
		android_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("OpenApplicationSettings", android_backend)
		self.assertIn("EOpenMobileCapabilityState::Available", android_backend)

		ios_open = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSApplicationSettings.mm"
		).read_text(encoding="utf-8")
		for required_token in (
			"UIApplicationOpenSettingsURLString",
			"UIApplicationStateActive",
			"IOSController",
			"canOpenURL",
			"openURL",
		):
			self.assertIn(required_token, ios_open)
		self.assertNotIn("UE_LOG", ios_open)
		ios_backend = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("OpenApplicationSettings", ios_backend)
		self.assertIn("EOpenMobileCapabilityState::Available", ios_backend)

	def test_system_appearance_uses_platform_configuration_not_unreal_theme(self) -> None:
		appearance_policy = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Internal"
			/ "OpenMobileDeviceSystemAppearance.h"
		).read_text(encoding="utf-8")
		self.assertIn("FromAndroidNightMode", appearance_policy)
		self.assertIn("FromIOSUserInterfaceStyle", appearance_policy)

		android_appearance = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidAppearance.cpp"
		).read_text(encoding="utf-8")
		for required_token in (
			"AndroidThunkJava_OpenMobileDeviceGetSystemAppearance",
			"nativeOpenMobileDeviceAppearanceChanged",
			"NotifyNativeChange",
		):
			self.assertIn(required_token, android_appearance)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for required_token in (
			"Configuration.UI_MODE_NIGHT_MASK",
			"Configuration.UI_MODE_NIGHT_NO",
			"Configuration.UI_MODE_NIGHT_YES",
			"AndroidThunkJava_OpenMobileDeviceStartAppearanceMonitoring",
			"AndroidThunkJava_OpenMobileDeviceStopAppearanceMonitoring",
			"nativeOpenMobileDeviceAppearanceChanged",
			"gameActivityonConfigurationChangedAdditions",
		):
			self.assertIn(required_token, android_upl)

		ios_appearance = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSAppearance.mm"
		).read_text(encoding="utf-8")
		for required_token in (
			"IOSController",
			"windowScene",
			"traitCollection.userInterfaceStyle",
			"traitCollectionDidChange",
			"registerForTraitChanges",
			"NotifyNativeChange",
		):
			self.assertIn(required_token, ios_appearance)
		self.assertNotIn("connectedScenes", ios_appearance)

		for source in (android_appearance, ios_appearance):
			for forbidden_token in (
				"FSlateApplication",
				"FAppStyle",
				"GetColorScheme",
			):
				self.assertNotIn(forbidden_token, source)

		for backend_name in ("Android", "IOS"):
			backend = (
				DEVICE_PLUGIN
				/ "Source"
				/ f"OpenMobileDevice{backend_name}"
				/ "Private"
				/ f"OpenMobileDevice{backend_name}Backend.cpp"
			).read_text(encoding="utf-8")
			self.assertIn("SystemAppearance", backend)
			self.assertIn("AppearanceChangeEvents", backend)
			self.assertIn("EOpenMobileDeviceMonitoringGroup::Appearance", backend)

	def test_preferred_text_scale_preserves_platform_values_without_scaling_umg(self) -> None:
		text_scale_policy = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Internal"
			/ "OpenMobileDevicePreferredTextScale.h"
		).read_text(encoding="utf-8")
		self.assertIn("FromAndroidFontScale", text_scale_policy)
		self.assertIn("FromIOSContentSizeCategory", text_scale_policy)

		android_accessibility = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidAccessibility.cpp"
		).read_text(encoding="utf-8")
		for required_token in (
			"AndroidThunkJava_OpenMobileDeviceGetPreferredTextScale",
			"CallFloatMethod",
			"nativeOpenMobileDeviceAccessibilityChanged",
			"NotifyNativeChange",
		):
			self.assertIn(required_token, android_accessibility)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for required_token in (
			"configuration.fontScale",
			"AndroidThunkJava_OpenMobileDeviceStartAccessibilityMonitoring",
			"AndroidThunkJava_OpenMobileDeviceStopAccessibilityMonitoring",
			"nativeOpenMobileDeviceAccessibilityChanged",
			"gameActivityonConfigurationChangedAdditions",
		):
			self.assertIn(required_token, android_upl)

		ios_accessibility = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSAccessibility.mm"
		).read_text(encoding="utf-8")
		for required_token in (
			"preferredContentSizeCategory",
			"UIContentSizeCategoryDidChangeNotification",
			"UIFontMetrics",
			"scaledValueForValue",
			"NotifyNativeChange",
		):
			self.assertIn(required_token, ios_accessibility)

		for backend_name in ("Android", "IOS"):
			backend = (
				DEVICE_PLUGIN
				/ "Source"
				/ f"OpenMobileDevice{backend_name}"
				/ "Private"
				/ f"OpenMobileDevice{backend_name}Backend.cpp"
			).read_text(encoding="utf-8")
			self.assertIn("PreferredTextScale", backend)
			self.assertIn("AccessibilityChangeEvents", backend)
			self.assertIn("EOpenMobileDeviceMonitoringGroup::Accessibility", backend)

		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")
		self.assertIn("PreferredTextScale", readme)
		self.assertIn("does not scale UMG", readme)

	def test_reduced_animation_uses_public_platform_preferences_without_writes(self) -> None:
		reduced_animation_policy = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Internal"
			/ "OpenMobileDeviceReducedAnimation.h"
		).read_text(encoding="utf-8")
		self.assertIn("FromAndroidAnimationScales", reduced_animation_policy)
		self.assertIn("FromIOSReduceMotion", reduced_animation_policy)

		accessibility_types = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceAccessibilityTypes.h"
		).read_text(encoding="utf-8")
		self.assertIn("ReducedAnimationPlatformDetail", accessibility_types)

		android_accessibility = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidAccessibility.cpp"
		).read_text(encoding="utf-8")
		for required_token in (
			"AndroidThunkJava_OpenMobileDeviceGetAnimationScales",
			"GetFloatArrayRegion",
			"FromAndroidAnimationScales",
		):
			self.assertIn(required_token, android_accessibility)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for required_token in (
			"Settings.Global.ANIMATOR_DURATION_SCALE",
			"Settings.Global.TRANSITION_ANIMATION_SCALE",
			"Settings.Global.WINDOW_ANIMATION_SCALE",
			"Settings.Global.getFloat",
			"Settings.Global.getUriFor",
			"ContentObserver",
		):
			self.assertIn(required_token, android_upl)
		self.assertNotIn("WRITE_SETTINGS", android_upl)

		ios_accessibility = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSAccessibility.mm"
		).read_text(encoding="utf-8")
		self.assertIn("UIAccessibilityIsReduceMotionEnabled", ios_accessibility)
		self.assertIn(
			"UIAccessibilityReduceMotionStatusDidChangeNotification",
			ios_accessibility,
		)
		self.assertIn("FromIOSReduceMotion", ios_accessibility)

		for backend_name in ("Android", "IOS"):
			backend = (
				DEVICE_PLUGIN
				/ "Source"
				/ f"OpenMobileDevice{backend_name}"
				/ "Private"
				/ f"OpenMobileDevice{backend_name}Backend.cpp"
			).read_text(encoding="utf-8")
			self.assertIn("ReducedAnimation", backend)
			self.assertIn("AccessibilityChangeEvents", backend)

		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")
		self.assertIn("not semantically equivalent", readme)

	def test_assistive_technology_state_avoids_service_enumeration(self) -> None:
		assistive_policy = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Internal"
			/ "OpenMobileDeviceAssistiveTechnology.h"
		).read_text(encoding="utf-8")
		self.assertIn("FromAndroidTouchExplorationState", assistive_policy)
		self.assertIn("FromIOSVoiceOverState", assistive_policy)

		android_accessibility = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "OpenMobileDeviceAndroidAccessibility.cpp"
		).read_text(encoding="utf-8")
		for required_token in (
			"AndroidThunkJava_OpenMobileDeviceGetTouchExplorationState",
			"CallIntMethod",
			"FromAndroidTouchExplorationState",
		):
			self.assertIn(required_token, android_accessibility)

		android_upl = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		).read_text(encoding="utf-8")
		for required_token in (
			"AccessibilityManager",
			"isTouchExplorationEnabled",
			"TouchExplorationStateChangeListener",
			"addTouchExplorationStateChangeListener",
			"removeTouchExplorationStateChangeListener",
		):
			self.assertIn(required_token, android_upl)
		for forbidden_token in (
			"getEnabledAccessibilityServiceList",
			"getInstalledAccessibilityServiceList",
			"AccessibilityServiceInfo",
			"ENABLED_ACCESSIBILITY_SERVICES",
		):
			self.assertNotIn(forbidden_token, android_upl)

		ios_accessibility = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "OpenMobileDeviceIOSAccessibility.mm"
		).read_text(encoding="utf-8")
		self.assertIn("UIAccessibilityIsVoiceOverRunning", ios_accessibility)
		self.assertIn(
			"UIAccessibilityVoiceOverStatusDidChangeNotification",
			ios_accessibility,
		)
		self.assertIn("FromIOSVoiceOverState", ios_accessibility)
		for forbidden_token in (
			"UIAccessibilityFocusedElement",
			"UIAccessibilityAssistiveTechnologyIdentifier",
			"UE_LOG",
		):
			self.assertNotIn(forbidden_token, ios_accessibility)

		for backend_name in ("Android", "IOS"):
			backend = (
				DEVICE_PLUGIN
				/ "Source"
				/ f"OpenMobileDevice{backend_name}"
				/ "Private"
				/ f"OpenMobileDevice{backend_name}Backend.cpp"
			).read_text(encoding="utf-8")
			self.assertIn("ScreenReader", backend)
			self.assertIn("AccessibilityChangeEvents", backend)

		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")
		self.assertIn("does not prove", readme)

	def test_accessibility_events_are_focused_and_snapshot_ordered(self) -> None:
		subsystem_header = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceSubsystem.h"
		).read_text(encoding="utf-8")
		for required_token in (
			"OnPreferredTextScaleChanged",
			"OnReducedAnimationPreferenceChanged",
			"OnAssistiveTechnologyStateChanged",
			"OnAccessibilitySnapshotChanged",
		):
			self.assertIn(required_token, subsystem_header)

		subsystem_source = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceSubsystem.cpp"
		).read_text(encoding="utf-8")
		broadcasts = (
			"LastAccessibilitySnapshot = Snapshot",
			"OnPreferredTextScaleChanged.Broadcast(Snapshot)",
			"OnReducedAnimationPreferenceChanged.Broadcast(Snapshot)",
			"OnAssistiveTechnologyStateChanged.Broadcast(Snapshot)",
			"OnAccessibilitySnapshotChanged.Broadcast(Snapshot)",
		)
		positions = [subsystem_source.index(token) for token in broadcasts]
		self.assertEqual(sorted(positions), positions)

		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")
		self.assertIn("text scale, reduced animation, assistive technology", readme)
		self.assertIn("complete settled accessibility snapshot", readme)
		self.assertIn("Appearance and Accessibility", readme)

	def test_device_accessibility_scope_stays_read_only(self) -> None:
		public_contract = "\n".join(
			path.read_text(encoding="utf-8")
			for path in (DEVICE_PLUGIN / "Source" / "OpenMobileDevice" / "Public").glob("*.h")
		)
		for forbidden_token in (
			"AnnounceAccessibility",
			"SetAccessibilityFocus",
			"RegisterAccessibilityElement",
			"AddAccessibilityAction",
		):
			self.assertNotIn(forbidden_token, public_contract)

		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")
		for required_token in (
			"Accessibility scope boundary",
			"OpenMobileAccessibility",
			"semantic UI exposure",
			"focus navigation",
			"announcements",
			"custom accessibility actions",
			"native accessibility element bridges",
			"Preference-only sample",
		):
			self.assertIn(required_token, readme)

	def test_native_configuration_is_owned_filtered_and_minimal(self) -> None:
		android_upl_path = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileDevice_Android_UPL.xml"
		)
		ET.parse(android_upl_path)
		android_upl = android_upl_path.read_text(encoding="utf-8")
		self.assertEqual(1, android_upl.count("addPermission"))
		self.assertEqual(1, android_upl.count("implementation("))
		self.assertIn("android.permission.ACCESS_NETWORK_STATE", android_upl)
		self.assertNotIn("<queries>", android_upl)
		for required_token in (
			"androidx.window:window-java:1.5.1",
			'addElement tag="queries"',
			"OpenMobileDeviceUrlSchemeValid",
			"OpenMobileDeviceUrlSchemeBelowLimit",
			"OpenMobileDeviceIntentActionValid",
			"OpenMobileDeviceIntentActionBelowLimit",
			"OpenMobileDevicePackageValid",
			"OpenMobileDeviceSeenPackages",
		):
			self.assertIn(required_token, android_upl)
		for forbidden_token in (
			"android.permission.CAMERA",
			"android.permission.INTERNET",
			"QUERY_ALL_PACKAGES",
			"uses-feature",
		):
			self.assertNotIn(forbidden_token, android_upl)

		ios_upl_path = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "Private"
			/ "IOS"
			/ "OpenMobileDevice_IOS_UPL.xml"
		)
		ET.parse(ios_upl_path)
		ios_upl = ios_upl_path.read_text(encoding="utf-8")
		self.assertIn("OpenMobileDeviceUrlSchemeValid", ios_upl)
		self.assertIn("OpenMobileDeviceUrlSchemeBelowLimit", ios_upl)
		self.assertIn("OpenMobileDeviceExistingScheme", ios_upl)
		for forbidden_token in (
			"NSCameraUsageDescription",
			"NSMicrophoneUsageDescription",
			"NSPhotoLibraryUsageDescription",
		):
			self.assertNotIn(forbidden_token, ios_upl)

		ios_build = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceIOS"
			/ "OpenMobileDeviceIOS.Build.cs"
		).read_text(encoding="utf-8")
		framework_block = ios_build.split("PublicFrameworks.AddRange", 1)[1].split(
			"});",
			1,
		)[0]
		self.assertEqual(
			{
			"AVFoundation",
			"Foundation",
			"SystemConfiguration",
			"UIKit",
			},
			set(re.findall(r'"([A-Za-z]+)"', framework_block)),
		)

		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")
		for required_token in (
			"Native configuration",
			"UEMetadata/PrivacyInfo.xcprivacy",
			"NSPrivacyAccessedAPICategoryDiskSpace",
			"E174.1",
		):
			self.assertIn(required_token, readme)

	def test_editor_mock_is_explicit_and_excluded_from_runtime_builds(self) -> None:
		modules = {module["Name"]: module for module in load_descriptor()["Modules"]}
		self.assertEqual("Editor", modules["OpenMobileDeviceEditor"]["Type"])

		editor_module = DEVICE_PLUGIN / "Source" / "OpenMobileDeviceEditor"
		settings = (
			editor_module / "Public" / "OpenMobileDeviceMockSettings.h"
		).read_text(encoding="utf-8")
		mock_header = (
			editor_module / "Public" / "OpenMobileDeviceEditorMock.h"
		).read_text(encoding="utf-8")
		mock_source = (
			editor_module / "Private" / "OpenMobileDeviceEditorMock.cpp"
		).read_text(encoding="utf-8")

		self.assertIn("Config = EditorPerProjectUserSettings", settings)
		self.assertIn('DisplayName = "OpenMobile Device Mock"', settings)
		self.assertIn('return TEXT("OpenMobile")', settings)
		self.assertIn('return TEXT("OpenMobile Device Mock")', settings)
		self.assertIn("bEnableMockBackend = false", settings)
		for required_token in (
			"Delayed",
			"Duplicate",
			"OutOfOrder",
			"OffThread",
			"Stale",
			"QueueScriptStep",
			"ResetOverrides",
		):
			self.assertIn(required_token, mock_header)
		for required_token in (
			"RegisterBackend",
			"UnregisterBackend",
			"NotifyNativeChange",
			"AsyncTask",
		):
			self.assertIn(required_token, mock_source)
		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")
		for required_token in (
			"Editor mock provider",
			"OpenMobile Device Mock",
			"EditorPerProjectUserSettings",
			"ResetForTests",
		):
			self.assertIn(required_token, readme)

		runtime_module = DEVICE_PLUGIN / "Source" / "OpenMobileDevice"
		for path in runtime_module.rglob("*"):
			if path.is_file() and path.suffix in {".h", ".cpp", ".cs"}:
				self.assertNotIn(
					"OpenMobileDeviceEditorMock",
					path.read_text(encoding="utf-8"),
					str(path),
				)

	def test_device_diagnostics_are_bounded_redacted_and_editor_presented(self) -> None:
		runtime_header = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Public"
			/ "OpenMobileDeviceDiagnostics.h"
		).read_text(encoding="utf-8")
		runtime_source = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDevice"
			/ "Private"
			/ "OpenMobileDeviceDiagnosticsSource.cpp"
		).read_text(encoding="utf-8")
		editor_source = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceEditor"
			/ "Private"
			/ "OpenMobileDeviceDiagnosticsScreen.cpp"
		).read_text(encoding="utf-8")
		output_source = (
			DEVICE_PLUGIN
			/ "Source"
			/ "OpenMobileDeviceEditor"
			/ "Private"
			/ "OpenMobileDeviceDiagnosticsOutput.cpp"
		).read_text(encoding="utf-8")

		for required_token in (
			"Capabilities",
			"BackendName",
			"ActiveMonitoringGroups",
			"ControlLeases",
			"RecentErrors",
			"ConfigurationIssues",
		):
			self.assertIn(required_token, runtime_header)
		for required_token in (
			"MaximumCapabilityCount",
			"MaximumRecentErrorCount",
			"GetActiveGroupsForDiagnostics",
			"GetActiveRequestCountForDiagnostics",
		):
			self.assertIn(required_token, runtime_source)
		for required_token in (
			"RegisterNomadTabSpawner",
			"Refresh",
			"Copy",
			"Export",
			"SMultiLineEditableTextBox",
		):
			self.assertIn(required_token, editor_source)
		for required_token in (
			"MaximumExportBytes",
			"ClipboardCopy",
			"SaveStringToFile",
			"batteryPercent",
			"monitoringGroups",
			"controlLeases",
		):
			self.assertIn(required_token, output_source)
		for forbidden_token in (
			"PackageIdentifier",
			"ClipboardContent",
			"CurrentScreenIdentifier",
			"ReducedAnimationPlatformDetail",
			"Capability.Detail",
			"Error.Message",
		):
			self.assertNotIn(forbidden_token, output_source)
		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")
		for required_token in (
			"Device diagnostics",
			"64 KiB",
			"explicit allow list",
			"mark captures older than 15 minutes as stale",
			"raw error messages and native codes are not retained",
		):
			self.assertIn(required_token, readme)

	def test_build_validation_is_editor_owned_and_store_doctor_discoverable(self) -> None:
		editor_module = DEVICE_PLUGIN / "Source" / "OpenMobileDeviceEditor"
		header = (
			editor_module / "Public" / "OpenMobileDeviceBuildValidation.h"
		).read_text(encoding="utf-8")
		source = (
			editor_module / "Private" / "OpenMobileDeviceBuildValidation.cpp"
		).read_text(encoding="utf-8")
		module = (
			editor_module / "Private" / "OpenMobileDeviceEditorModule.cpp"
		).read_text(encoding="utf-8")
		for required_token in (
			"OpenMobile.StoreDoctor.ValidationContributor",
			"CaptureProjectInput",
			"StoreSubmission",
			"HasBlockingIssues",
		):
			self.assertIn(required_token, header + source)
		self.assertIn("RegisterModularFeature", module)
		self.assertIn("UnregisterModularFeature", module)

		fixtures = DEVICE_PLUGIN / "Tests" / "Fixtures" / "BuildValidation"
		for name in (
			"valid.json",
			"malformed.json",
			"conflicting.json",
			"privacy-unsafe.json",
		):
			self.assertTrue((fixtures / name).is_file())

		runtime_module = DEVICE_PLUGIN / "Source" / "OpenMobileDevice"
		for path in runtime_module.rglob("*"):
			if path.is_file() and path.suffix in {".h", ".cpp", ".cs"}:
				contents = path.read_text(encoding="utf-8")
				self.assertNotIn("StoreDoctor", contents, str(path))
				self.assertNotIn("OpenMobileDeviceBuildValidation", contents, str(path))

		readme = (DEVICE_PLUGIN / "README.md").read_text(encoding="utf-8")
		for required_token in (
			"Build-time validation",
			"development warnings",
			"Shipping and store submission",
			"Store Doctor",
		):
			self.assertIn(required_token, readme)


if __name__ == "__main__":
	unittest.main()
