import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid" / "Private"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS" / "Private"


class SensorsPressureTests(unittest.TestCase):
	def test_android_pressure_uses_the_native_scalar_path(self):
		bridge = (ANDROID / "OpenMobileSensorsAndroidBridge.cpp").read_text(
			encoding="utf-8"
		)
		java = (
			ANDROID
			/ "Android"
			/ "src"
			/ "com"
			/ "openmobile"
			/ "sensors"
			/ "OpenMobileSensorsBridgeV1.java"
		).read_text(encoding="utf-8")
		backend = (ANDROID / "OpenMobileSensorsAndroidBackend.cpp").read_text(
			encoding="utf-8"
		)
		self.assertIn("case 6:", bridge)
		self.assertIn("EOpenMobileSensorType::BarometricPressure", bridge)
		self.assertIn("Sensor.TYPE_PRESSURE", java)
		self.assertIn("FOpenMobileScalarSensorSample", backend)
		self.assertIn("NormalizeScalarSample", backend)
		self.assertIn("FromAndroidSensorEventNanoseconds", backend)

	def test_ios_pressure_converts_altimeter_kilopascals(self):
		backend = (IOS / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		units = (COMMON / "Private" / "OpenMobileSensorUnits.cpp").read_text(
			encoding="utf-8"
		)
		self.assertIn("Availability.bRelativeAltitude", backend)
		self.assertIn("IOS-BarometricPressure", backend)
		self.assertIn("HandleRelativeAltitude", bridge)
		self.assertIn("Data.pressure.doubleValue", bridge)
		self.assertIn("KilopascalsToHectopascals", units)
		self.assertIn("SuccessWithAppliedInterval(1.0)", bridge)
		self.assertIn("Capability.MaximumFrequencyHz = 1.0", backend)

	def test_common_policy_is_low_rate_and_rejects_invalid_pressure(self):
		units = (COMMON / "Private" / "OpenMobileSensorUnits.cpp").read_text(
			encoding="utf-8"
		)
		subscriptions = (
			COMMON / "Private" / "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("Sample.Value > 0.0", units)
		self.assertIn("Sample.Value <= 2000.0", units)
		self.assertIn("ApplyLowPowerDefaults", subscriptions)
		self.assertIn("IsPressureRateSupported", subscriptions)
		self.assertIn("EOpenMobileSensorFailureReason::MissingHardware", subscriptions)

	def test_public_contract_defines_pressure_behavior(self):
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for token in (
			"## Barometric pressure",
			"hectopascals",
			"1 Hz",
			"2000 hPa",
			"sea-level default",
			"slow changes",
			"physical-device acceptance",
		):
			self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
