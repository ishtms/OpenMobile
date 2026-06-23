import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors" / "Source"
COMMON = SENSORS / "OpenMobileSensors"
ANDROID = SENSORS / "OpenMobileSensorsAndroid" / "Private"
IOS = SENSORS / "OpenMobileSensorsIOS" / "Private"


class SensorsRawVariantTests(unittest.TestCase):
	def test_android_maps_uncalibrated_native_types_and_optional_bias(self):
		bridge = (ANDROID / "OpenMobileSensorsAndroidBridge.cpp").read_text(
			encoding="utf-8"
		)
		backend = (ANDROID / "OpenMobileSensorsAndroidBackend.cpp").read_text(
			encoding="utf-8"
		)

		for native_type, sensor_type in (
			("case 35:", "AccelerometerUncalibrated"),
			("case 16:", "GyroscopeUncalibrated"),
			("case 14:", "MagnetometerUncalibrated"),
		):
			start = bridge.index(native_type)
			self.assertIn(sensor_type, bridge[start:start + 120])
		self.assertIn("ValuesPerSample >= 6", backend)
		for sensor_type in (
			"AccelerometerUncalibrated",
			"GyroscopeUncalibrated",
			"MagnetometerUncalibrated",
		):
			self.assertIn(f"Type == EOpenMobileSensorType::{sensor_type}", backend)

	def test_ios_labels_only_the_directly_available_raw_streams(self):
		backend = (IOS / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		supported = backend[
			backend.index("TArray<FSupportedSensor> GetSupportedSensors"):
			backend.index("FString FailureCode")
		]

		self.assertIn("EOpenMobileSensorType::Accelerometer", supported)
		self.assertIn("EOpenMobileSensorType::Gyroscope", supported)
		self.assertIn("EOpenMobileSensorType::MagnetometerUncalibrated", supported)
		self.assertNotIn("AccelerometerUncalibrated", supported)
		self.assertNotIn("GyroscopeUncalibrated", supported)

	def test_raw_variants_use_unfiltered_recording_subscriptions(self):
		recording = (
			COMMON / "Private" / "OpenMobileSensorsRecordingService.cpp"
		).read_text(encoding="utf-8")

		for sensor_type in (
			"AccelerometerUncalibrated",
			"GyroscopeUncalibrated",
			"MagnetometerUncalibrated",
		):
			self.assertIn(f"case EOpenMobileSensorType::{sensor_type}:", recording)
		self.assertIn("Request.Options.Filters = {};", recording)


if __name__ == "__main__":
	unittest.main()
