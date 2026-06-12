import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS_ROOT = ROOT / "Native" / "OpenMobileSensors"
SENSORS = SENSORS_ROOT / "Source" / "OpenMobileSensors"
ANDROID = SENSORS_ROOT / "Source" / "OpenMobileSensorsAndroid"


class SensorsHighSamplingTests(unittest.TestCase):
    def test_android_permission_is_owned_by_the_sensors_packaging_path(self):
        build_rules = (ANDROID / "OpenMobileSensorsAndroid.Build.cs").read_text()
        upl = (
            ANDROID
            / "Private"
            / "Android"
            / "OpenMobileSensors_Android_UPL.xml"
        ).read_text()

        self.assertIn("AdditionalPropertiesForReceipt", build_rules)
        self.assertIn("OpenMobileSensors_Android_UPL.xml", build_rules)
        self.assertIn("bAllowHighSamplingRate", upl)
        self.assertIn("android.permission.HIGH_SAMPLING_RATE_SENSORS", upl)
        self.assertIn('<if condition="OpenMobileSensorsHighSamplingEnabled">', upl)

    def test_runtime_validates_the_packaged_declaration(self):
        backend = (
            ANDROID / "Private" / "OpenMobileSensorsAndroidBackend.cpp"
        ).read_text()
        subscription = (
            SENSORS / "Private" / "OpenMobileSensorsSubscriptionService.cpp"
        ).read_text()

        self.assertIn("HasHighSamplingRateDeclaration", backend)
        self.assertIn("AndroidThunkJava_OpenMobileSensorsHasHighSamplingRateDeclaration", backend)
        self.assertIn("MissingPlatformDeclaration", subscription)

    def test_high_sampling_scenarios_are_covered(self):
        source = (
            SENSORS
            / "Private"
            / "Tests"
            / "OpenMobileSensorsHighSamplingTests.cpp"
        ).read_text()

        for scenario in (
            "ManifestOff",
            "ManifestOn",
            "BoundaryRatesAndPresets",
            "RuntimeClamp",
            "DeviceRateBoundaries",
        ):
            self.assertIn(scenario, source)

    def test_android_device_validation_is_documented(self):
        source = (SENSORS_ROOT / "Docs" / "PublicContract.md").read_text()

        self.assertIn("API 31", source)
        self.assertIn("200 Hz", source)
        self.assertIn("physical Android device", source)


if __name__ == "__main__":
    unittest.main()
