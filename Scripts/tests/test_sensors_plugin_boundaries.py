import json
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CORE_PLUGIN = REPOSITORY_ROOT / "Foundation" / "OpenMobileCore"
SENSORS_PLUGIN = REPOSITORY_ROOT / "Native" / "OpenMobileSensors"


def load_descriptor() -> dict:
	with (SENSORS_PLUGIN / "OpenMobileSensors.uplugin").open(encoding="utf-8") as file:
		return json.load(file)


class SensorsPluginBoundaryTests(unittest.TestCase):
	def test_modules_match_runtime_platform_boundaries(self) -> None:
		modules = {module["Name"]: module for module in load_descriptor()["Modules"]}

		self.assertEqual(
			{
				"OpenMobileSensors",
				"OpenMobileSensorsAndroid",
				"OpenMobileSensorsIOS",
				"OpenMobileSensorsConsumerTests",
				"OpenMobileSensorsEditor",
			},
			set(modules),
		)
		self.assertEqual("Runtime", modules["OpenMobileSensors"]["Type"])
		self.assertNotIn("PlatformAllowList", modules["OpenMobileSensors"])
		self.assertEqual(
			["Android"],
			modules["OpenMobileSensorsAndroid"]["PlatformAllowList"],
		)
		self.assertEqual(
			["IOS"],
			modules["OpenMobileSensorsIOS"]["PlatformAllowList"],
		)
		self.assertEqual(
			"DeveloperTool",
			modules["OpenMobileSensorsConsumerTests"]["Type"],
		)
		self.assertEqual("Editor", modules["OpenMobileSensorsEditor"]["Type"])

		for module_name in modules:
			module_root = SENSORS_PLUGIN / "Source" / module_name
			self.assertTrue((module_root / f"{module_name}.Build.cs").is_file())
			self.assertTrue(
				(module_root / "Private" / f"{module_name}Module.cpp").is_file()
			)

	def test_core_dependency_is_explicit_and_one_way(self) -> None:
		descriptor_dependencies = {
			plugin["Name"] for plugin in load_descriptor().get("Plugins", [])
		}
		self.assertEqual(
			{"OpenMobileCore", "OpenMobilePermissions"},
			descriptor_dependencies,
		)

		common_rules = (
			SENSORS_PLUGIN
			/ "Source"
			/ "OpenMobileSensors"
			/ "OpenMobileSensors.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn('"OpenMobileCore"', common_rules)

		for path in CORE_PLUGIN.rglob("*"):
			if (
				not path.is_file()
				or {"Binaries", "Intermediate"}.intersection(path.parts)
				or path.suffix not in {
				".cs",
				".cpp",
				".h",
				".uplugin",
				}
			):
				continue
			self.assertNotIn(
				"OpenMobileSensors",
				path.read_text(encoding="utf-8"),
				str(path),
			)

	def test_backend_seam_is_internal_lazy_and_modular(self) -> None:
		interface = (
			SENSORS_PLUGIN
			/ "Source"
			/ "OpenMobileSensors"
			/ "Internal"
			/ "IOpenMobileSensorsBackend.h"
		).read_text(encoding="utf-8")
		registry = (
			SENSORS_PLUGIN
			/ "Source"
			/ "OpenMobileSensors"
			/ "Private"
			/ "OpenMobileSensorsBackendRegistry.cpp"
		).read_text(encoding="utf-8")

		self.assertIn("public IModularFeature", interface)
		self.assertIn("OpenMobile.Sensors.Backend", interface)
		self.assertIn("GetModularFeatureImplementations", registry)
		self.assertIn("RegisterModularFeature", registry)
		self.assertIn("UnregisterModularFeature", registry)
		self.assertNotIn("LoadModule", registry)
		self.assertNotIn("GetModuleChecked", registry)

	def test_common_module_defines_no_backend_result(self) -> None:
		module = (
			SENSORS_PLUGIN
			/ "Source"
			/ "OpenMobileSensors"
			/ "Private"
			/ "OpenMobileSensorsModule.cpp"
		).read_text(encoding="utf-8")

		self.assertIn("EOpenMobileCapabilityState::NotSupported", module)
		self.assertIn("No OpenMobile Sensors backend is registered.", module)

	def test_scripted_mock_is_development_only_and_covers_backend_events(self) -> None:
		mock = (
			SENSORS_PLUGIN
			/ "Source"
			/ "OpenMobileSensors"
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsMockBackend.h"
		).read_text(encoding="utf-8")

		self.assertIn("#if WITH_DEV_AUTOMATION_TESTS", mock)
		self.assertIn("IOpenMobileSensorsBackend", mock)
		for event in (
			"Capability",
			"SampleBatch",
			"Permission",
			"Lifecycle",
			"Error",
			"Delay",
		):
			self.assertIn(event, mock)

	def test_plugin_has_no_unrelated_or_third_party_payload(self) -> None:
		for build_rules in (SENSORS_PLUGIN / "Source").glob("*/*.Build.cs"):
			contents = build_rules.read_text(encoding="utf-8")
			for forbidden_dependency in (
				"OpenMobileAds",
				"OpenMobileDevice",
				"OpenMobileHaptics",
				"OpenMobileMedia",
			):
				self.assertNotIn(forbidden_dependency, contents, str(build_rules))

		for path in SENSORS_PLUGIN.rglob("*"):
			if (
				not path.is_file()
				or {"Binaries", "Intermediate"}.intersection(path.parts)
				or path.suffix not in {
				".cs",
				".cpp",
				".h",
				".ini",
				".md",
				".uplugin",
				".xml",
				}
			):
				continue
			contents = path.read_text(encoding="utf-8")
			for forbidden_payload in (
				"GoogleMobileAds",
				"play-services-ads",
				"play-services-location",
				"ActivityRecognitionClient",
				"CoreHaptics",
				"MediaPlayer",
			):
				self.assertNotIn(forbidden_payload, contents, str(path))

		self.assertFalse((SENSORS_PLUGIN / "ThirdParty").exists())


if __name__ == "__main__":
	unittest.main()
