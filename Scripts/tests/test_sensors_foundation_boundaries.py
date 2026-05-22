import json
import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CORE_PLUGIN = REPOSITORY_ROOT / "Foundation" / "OpenMobileCore"
PERMISSIONS_PLUGIN = REPOSITORY_ROOT / "Foundation" / "OpenMobilePermissions"
SENSORS_PLUGIN = REPOSITORY_ROOT / "Native" / "OpenMobileSensors"


def load_descriptor(path: Path) -> dict:
	with path.open(encoding="utf-8") as file:
		return json.load(file)


class SensorsFoundationBoundaryTests(unittest.TestCase):
	def test_permissions_is_a_narrow_core_dependent_foundation_plugin(self) -> None:
		descriptor = load_descriptor(
			PERMISSIONS_PLUGIN / "OpenMobilePermissions.uplugin"
		)
		self.assertEqual(
			["OpenMobilePermissions"],
			[module["Name"] for module in descriptor["Modules"]],
		)
		self.assertEqual("Runtime", descriptor["Modules"][0]["Type"])
		self.assertEqual(
			{"OpenMobileCore"},
			{plugin["Name"] for plugin in descriptor.get("Plugins", [])},
		)

		build_rules = (
			PERMISSIONS_PLUGIN
			/ "Source"
			/ "OpenMobilePermissions"
			/ "OpenMobilePermissions.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn('"OpenMobileCore"', build_rules)

	def test_sensors_declares_both_foundation_dependencies(self) -> None:
		descriptor = load_descriptor(SENSORS_PLUGIN / "OpenMobileSensors.uplugin")
		self.assertEqual(
			{"OpenMobileCore", "OpenMobilePermissions"},
			{plugin["Name"] for plugin in descriptor.get("Plugins", [])},
		)

		build_rules = (
			SENSORS_PLUGIN
			/ "Source"
			/ "OpenMobileSensors"
			/ "OpenMobileSensors.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn('"OpenMobileCore"', build_rules)
		self.assertIn('"OpenMobilePermissions"', build_rules)

	def test_permissions_owns_only_normalized_status_and_request_mechanics(self) -> None:
		types = (
			PERMISSIONS_PLUGIN
			/ "Source"
			/ "OpenMobilePermissions"
			/ "Public"
			/ "OpenMobilePermissionTypes.h"
		).read_text(encoding="utf-8")
		status_body = re.search(
			r"enum class EOpenMobilePermissionStatus[^\{]*\{(?P<body>.*?)\}",
			types,
			re.DOTALL,
		)
		self.assertIsNotNone(status_body)
		for status in (
			"NotDetermined",
			"Granted",
			"Denied",
			"Restricted",
			"PermanentlyDenied",
		):
			self.assertIn(status, status_body.group("body"))

		api = (
			PERMISSIONS_PLUGIN
			/ "Source"
			/ "OpenMobilePermissions"
			/ "Public"
			/ "OpenMobilePermissions.h"
		).read_text(encoding="utf-8")
		for operation in ("GetStatus", "RequestPermission", "CancelRequest"):
			self.assertIn(operation, api)

		all_foundation_source = "\n".join(
			path.read_text(encoding="utf-8")
			for path in PERMISSIONS_PLUGIN.rglob("*")
			if path.is_file()
			and not {"Binaries", "Intermediate"}.intersection(path.parts)
			and path.suffix in {".cpp", ".h", ".md", ".uplugin"}
		)
		for sensor_owned_term in (
			"ActivityRecognition",
			"MotionActivity",
			"TrueHeading",
			"NSMotionUsageDescription",
		):
			self.assertNotIn(sensor_owned_term, all_foundation_source)

	def test_sensor_permission_policy_stays_in_sensors(self) -> None:
		policy = (
			SENSORS_PLUGIN
			/ "Source"
			/ "OpenMobileSensors"
			/ "Internal"
			/ "OpenMobileSensorsPermissionPolicy.h"
		).read_text(encoding="utf-8")
		for sensor_owned_term in (
			"ActivityRecognition",
			"MotionActivity",
			"TrueHeading",
			"GetExplanation",
		):
			self.assertIn(sensor_owned_term, policy)

	def test_foundation_dependencies_remain_one_way(self) -> None:
		for plugin_root in (CORE_PLUGIN, PERMISSIONS_PLUGIN):
			for path in plugin_root.rglob("*"):
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

	def test_sensors_reuses_core_primitives_without_private_replacements(self) -> None:
		sensor_sources = "\n".join(
			path.read_text(encoding="utf-8")
			for path in (SENSORS_PLUGIN / "Source").rglob("*")
			if path.is_file() and path.suffix in {".cpp", ".h"}
		)
		self.assertIn("FOpenMobileError", sensor_sources)
		self.assertIn("FOpenMobileCapability", sensor_sources)
		self.assertNotIn("DECLARE_LOG_CATEGORY", sensor_sources)
		self.assertNotIn("AsyncTask(", sensor_sources)

		readme = (SENSORS_PLUGIN / "README.md").read_text(encoding="utf-8")
		for primitive in (
			"FOpenMobileError",
			"FOpenMobileCapability",
			"OpenMobile::DispatchToGameThread",
			"LogOpenMobile",
			"second concrete consumer",
			"focused Core tests",
		):
			self.assertIn(primitive, readme)


if __name__ == "__main__":
	unittest.main()
