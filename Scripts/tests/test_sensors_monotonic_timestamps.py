import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS_ROOT = ROOT / "Native" / "OpenMobileSensors"
SENSORS = SENSORS_ROOT / "Source" / "OpenMobileSensors"


class SensorsMonotonicTimestampTests(unittest.TestCase):
    def test_public_timeline_contract_is_explicit(self):
        contract = (SENSORS_ROOT / "Docs" / "PublicContract.md").read_text()

        self.assertIn("boot-relative", contract)
        self.assertIn("monotonic", contract)
        self.assertIn("wall-clock", contract)
        self.assertIn("double-precision", contract)

    def test_platform_adapters_use_native_sensor_timestamps(self):
        android = (
            SENSORS_ROOT
            / "Source"
            / "OpenMobileSensorsAndroid"
            / "Private"
            / "OpenMobileSensorsAndroidBackend.cpp"
        ).read_text()
        ios = (
            SENSORS_ROOT
            / "Source"
            / "OpenMobileSensorsIOS"
            / "Private"
            / "OpenMobileSensorsIOSBackend.mm"
        ).read_text()

        self.assertIn("FromAndroidSensorEventNanoseconds", android)
        self.assertIn("FromIOSCoreMotionSeconds", ios)
        self.assertIn("FOpenMobileSensorTimestampConverter", android)
        self.assertIn("FOpenMobileSensorTimestampConverter", ios)

    def test_timestamp_issues_and_receipt_time_are_public(self):
        samples = (
            SENSORS / "Public" / "OpenMobileSensorSamples.h"
        ).read_text()
        service = (
            SENSORS / "Private" / "OpenMobileSensorsSampleService.cpp"
        ).read_text()

        self.assertIn("TimestampIssueFlags", samples)
        self.assertIn("bStatefulProcessingReset", samples)
        self.assertIn("GameThreadReceiptSeconds", service)
        for issue in ("Invalid", "Duplicate", "Backward"):
            self.assertIn(issue, service)

    def test_required_timestamp_scenarios_are_covered(self):
        source = (
            SENSORS
            / "Private"
            / "Tests"
            / "OpenMobileSensorsTimestampTests.cpp"
        ).read_text()

        for scenario in (
            "ConversionPrecision",
            "LargeUptime",
            "BatchSequenceAndReceipt",
            "InvalidDuplicateBackward",
            "WallClockIndependence",
        ):
            self.assertIn(scenario, source)


if __name__ == "__main__":
    unittest.main()
