import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid" / "Private"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS" / "Private"


class SensorsProximityTests(unittest.TestCase):
	def test_android_uses_type_proximity_with_distance_and_range(self):
		java = (
			ANDROID
			/ "Android"
			/ "src"
			/ "com"
			/ "openmobile"
			/ "sensors"
			/ "OpenMobileSensorsBridgeV1.java"
		).read_text(encoding="utf-8")
		bridge = (ANDROID / "OpenMobileSensorsAndroidBridge.cpp").read_text(
			encoding="utf-8"
		)
		backend = (ANDROID / "OpenMobileSensorsAndroidBackend.cpp").read_text(
			encoding="utf-8"
		)
		self.assertIn("Sensor.TYPE_PROXIMITY", java)
		self.assertIn("case 8:", bridge)
		self.assertIn("EOpenMobileSensorType::Proximity", bridge)
		self.assertIn("Sample.bHasDistanceMeters = true", backend)
		self.assertIn("Descriptor.MaximumRange", backend)
		self.assertIn("PublishProximityBatchFromBackend", backend)

	def test_ios_uses_shared_uidevice_monitoring_without_motion_permission(self):
		header = (IOS / "OpenMobileSensorsIOSBridge.h").read_text(
			encoding="utf-8"
		)
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		backend = (IOS / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		for token in (
			"bProximityApiSupported",
			"UIDeviceProximityStateDidChangeNotification",
			"proximityMonitoringEnabled",
			"proximityState",
			"FOpenMobileProximityMonitoringPolicy",
			"EService::Proximity",
		):
			self.assertIn(token, header + bridge)
		self.assertIn("PublishProximityBatchFromProximityQueue", header + backend)
		self.assertIn(
			"Request.Sensor.Type != EOpenMobileSensorType::Proximity",
			bridge,
		)

	def test_monitoring_policy_and_lifecycle_have_native_contract_coverage(self):
		policy = (
			COMMON / "Internal" / "OpenMobileProximityMonitoringPolicy.h"
		).read_text(encoding="utf-8")
		tests = (
			COMMON / "Private" / "Tests" / "OpenMobileSensorsProximityTests.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"Acquire",
			"Release",
			"SetApplicationActive",
			"Shutdown",
			"GetLeaseCount",
		):
			self.assertIn(token, policy)
		for token in (
			"Both leases are retained",
			"Pause restores monitoring disabled",
			"Shutdown preserves another owner's monitoring",
			"Stopping one subscriber keeps monitoring alive",
			"Shutdown stops the remaining physical stream once",
		):
			self.assertIn(token, tests)

	def test_public_contract_defines_state_distance_and_screen_side_effects(self):
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for token in (
			"## Proximity state",
			"near or far",
			"optional distance",
			"maximum range",
			"screen",
			"final subscriber",
			"physical-device acceptance",
		):
			self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
