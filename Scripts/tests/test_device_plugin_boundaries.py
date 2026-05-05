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


if __name__ == "__main__":
	unittest.main()
