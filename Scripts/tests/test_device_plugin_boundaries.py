import json
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


if __name__ == "__main__":
	unittest.main()
