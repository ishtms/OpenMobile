import json
import unittest
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
			self.assertIn("FPlatformMisc::GetBatteryLevel", backend)
			self.assertIn("FPlatformMisc::GetDeviceVolume", backend)

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


if __name__ == "__main__":
	unittest.main()
