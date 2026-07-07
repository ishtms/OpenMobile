import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS" / "Private"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid" / "Private"


class SensorsAbsoluteAltitudeTests(unittest.TestCase):
	def test_public_sample_reports_native_source_and_vertical_accuracy(self):
		samples = (
			COMMON / "Public" / "OpenMobileSensorSamples.h"
		).read_text(encoding="utf-8")
		for token in (
			"EOpenMobileAbsoluteAltitudeSource",
			"FOpenMobileAbsoluteAltitudeMetadata",
			"NativePlatform",
			"bHasVerticalAccuracy",
			"VerticalAccuracyMeters",
			"FOpenMobileAbsoluteAltitudeMetadata AbsoluteAltitude",
		):
			self.assertIn(token, samples)

	def test_ios_capability_checks_os_and_hardware(self):
		bridge_header = (IOS / "OpenMobileSensorsIOSBridge.h").read_text(
			encoding="utf-8"
		)
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		backend = (IOS / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		self.assertIn("bAbsoluteAltitudeApiSupported", bridge_header)
		self.assertIn("@available(iOS 15.0, *)", bridge)
		self.assertIn("Availability.bAbsoluteAltitudeApiSupported = true", bridge)
		self.assertIn("[CMAltimeter isAbsoluteAltitudeAvailable]", bridge)
		self.assertIn("MakeAbsoluteAltitudeCapability", backend)
		self.assertIn("EOpenMobileCapabilityState::NotSupported", backend)
		self.assertIn("EOpenMobileCapabilityState::Unavailable", backend)

	def test_ios_callback_returns_altitude_accuracy_and_native_source(self):
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		sources = (
			COMMON / "Private" / "OpenMobileSensorSourcePolicy.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"Data.altitude",
			"Data.accuracy",
			"Sample.AbsoluteAltitude.Source",
			"EOpenMobileAbsoluteAltitudeSource::NativePlatform",
			"Sample.AbsoluteAltitude.bHasVerticalAccuracy",
		):
			self.assertIn(token, bridge)
		self.assertIn(
			"Sensor == EOpenMobileSensorType::AbsoluteAltitude", sources
		)

	def test_ios_distinguishes_temporary_service_errors(self):
		bridge_header = (IOS / "OpenMobileSensorsIOSBridge.h").read_text(
			encoding="utf-8"
		)
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		backend = (IOS / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		self.assertIn("ServiceTemporarilyUnavailable", bridge_header)
		self.assertIn("Error.code == CMErrorNotAvailable", bridge)
		self.assertIn("ServiceTemporarilyUnavailable", bridge)
		self.assertIn("ServiceTemporarilyUnavailable", backend)
		self.assertIn(
			"EOpenMobileSensorFailureReason::TemporarilyUnavailable", backend
		)
		self.assertIn("FromNSString(Error.domain)", bridge)

	def test_android_has_no_absolute_altitude_substitute(self):
		android_sources = "\n".join(
			path.read_text(encoding="utf-8", errors="ignore")
			for path in ANDROID.rglob("*")
			if path.is_file() and path.suffix in {".cpp", ".h", ".java"}
		)
		self.assertNotIn("AbsoluteAltitude", android_sources)

	def test_public_contract_defines_absolute_altitude_acceptance(self):
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for token in (
			"## Native absolute altitude",
			"iOS 15",
			"vertical accuracy",
			"no Android substitute",
			"temporarily unavailable",
			"physical-device acceptance",
		):
			self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
