import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid" / "Private"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS" / "Private"


class SensorsAmbientLightTests(unittest.TestCase):
	def test_android_discovers_and_streams_type_light_in_lux(self):
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
		self.assertIn("Sensor.TYPE_LIGHT", java)
		self.assertIn("case 5:", bridge)
		self.assertIn("EOpenMobileSensorType::AmbientLight", bridge)
		self.assertIn("EOpenMobileSensorType::AmbientLight", backend)
		self.assertIn("PublishScalarBatchFromBackend", backend)
		self.assertIn("Descriptor.MaximumRange", backend)

	def test_common_policy_uses_low_rates_and_event_thresholds(self):
		options = (
			COMMON / "Public" / "OpenMobileSensorStreamOptions.h"
		).read_text(encoding="utf-8")
		subscriptions = (
			COMMON / "Private" / "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		samples = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		units = (
			COMMON / "Private" / "OpenMobileSensorUnits.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("MinimumScalarEventChange", options)
		self.assertIn("Sensor.Type == EOpenMobileSensorType::AmbientLight", subscriptions)
		self.assertIn("OutApplied.CustomFrequencyHz = 1.0", subscriptions)
		self.assertIn("MinimumScalarEventChange", samples)
		self.assertIn("MeetsEventThreshold", samples)
		self.assertIn("EOpenMobileSensorType::AmbientLight", units)

	def test_ios_is_explicitly_unsupported_without_sensorkit(self):
		backend = (IOS / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		plugin = (SENSORS / "OpenMobileSensors.uplugin").read_text(
			encoding="utf-8"
		)
		self.assertIn("MakeUnsupportedAmbientLightCapability", backend)
		self.assertIn("EOpenMobileCapabilityState::NotSupported", backend)
		self.assertNotIn("EOpenMobileSensorType::AmbientLight", bridge)
		for text in (backend, bridge, plugin):
			self.assertNotIn("SensorKit", text)
			self.assertNotIn("SRSensorReader", text)
			self.assertNotIn("com.apple.developer.sensorkit", text)

	def test_public_contract_defines_ambient_light_behavior(self):
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for token in (
			"## Ambient-light level",
			"lux",
			"one hertz",
			"five hertz",
			"MinimumScalarEventChange",
			"Ordinary iOS applications",
			"SensorKit",
			"physical-device acceptance",
		):
			self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
