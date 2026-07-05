import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS"


class SensorsTrueHeadingTests(unittest.TestCase):
	def test_location_input_is_narrow_and_caller_owned(self):
		header = (
			COMMON / "Public" / "OpenMobileSensorPermissions.h"
		).read_text(encoding="utf-8")
		match = re.search(
			r"struct OPENMOBILESENSORS_API FOpenMobileSensorLocationInput"
			r"\s*\{(?P<body>.*?)\n\};",
			header,
			re.DOTALL,
		)
		self.assertIsNotNone(match)
		fields = set(re.findall(r"double\s+(\w+)\s*=", match.group("body")))
		self.assertEqual(
			fields,
			{
				"LatitudeDegrees",
				"LongitudeDegrees",
				"AltitudeMeters",
				"HorizontalAccuracyMeters",
				"TimestampSeconds",
			},
		)
		self.assertIn("Unix time", match.group("body"))

	def test_sensors_does_not_acquire_location_or_request_location_permission(self):
		contents = "\n".join(
			path.read_text(encoding="utf-8", errors="ignore")
			for path in SENSORS.rglob("*")
			if path.is_file()
			and path.suffix in {".Build.cs", ".cpp", ".h", ".java", ".mm", ".xml"}
		)
		for forbidden in (
			"CLLocationManager",
			"CoreLocation.framework",
			"android.location",
			"ACCESS_FINE_LOCATION",
			"ACCESS_COARSE_LOCATION",
			"FusedLocationProviderClient",
			"OpenMobileLocation",
		):
			self.assertNotIn(forbidden, contents)
		subsystem = (
			COMMON / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
		request = subsystem[
			subsystem.index("UOpenMobileSensorsSubsystem::RequestPermissionNative"):
			subsystem.index("UOpenMobileSensorsSubsystem::CancelPermissionRequestNative")
		]
		self.assertIn(
			"Permission == EOpenMobileSensorPermission::TrueHeadingLocation",
			request,
		)
		self.assertLess(
			request.index("TrueHeadingLocation"),
			request.index("FOpenMobilePermissions::RequestPermission"),
		)
		self.assertIn("Request location permission through its owning provider", request)

	def test_shared_fallback_provides_platform_parity(self):
		android = "\n".join(
			path.read_text(encoding="utf-8", errors="ignore")
			for path in ANDROID.rglob("*")
			if path.is_file() and path.suffix in {".cpp", ".h"}
		)
		ios = "\n".join(
			path.read_text(encoding="utf-8", errors="ignore")
			for path in IOS.rglob("*")
			if path.is_file() and path.suffix in {".mm", ".h"}
		)
		subscriptions = (
			COMMON / "Private" / "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		conversion = (
			COMMON / "Private" / "OpenMobileSensorsTrueHeadingService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("EOpenMobileSensorType::MagneticHeading", android)
		self.assertIn("EOpenMobileSensorType::MagneticHeading", ios)
		self.assertIn("OutPhysicalSensor = MagneticHeading->Sensor", subscriptions)
		self.assertIn("CalculateWMM2025", conversion)
		self.assertIn("WorldMagneticModel2025", conversion)

	def test_public_contract_defines_true_heading_validity_and_metadata(self):
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for token in (
			"## True heading",
			"WMM2025",
			"2025 through 2029",
			"60 seconds",
			"100 metres",
			"declination source",
			"location age",
			"location acquisition",
			"physical-device acceptance",
		):
			self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
