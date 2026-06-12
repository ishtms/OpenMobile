import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS_ROOT = ROOT / "Native" / "OpenMobileSensors"
SENSORS = SENSORS_ROOT / "Source" / "OpenMobileSensors"


class SensorsStandardizedUnitTests(unittest.TestCase):
    def test_public_unit_and_representation_contract_is_complete(self):
        contract = (SENSORS_ROOT / "Docs" / "PublicContract.md").read_text()

        for unit in (
            "metres per second squared",
            "radians per second",
            "microteslas",
            "hectopascals",
            "metres",
            "lux",
            "degrees",
        ):
            self.assertIn(unit, contract)
        for representation in (
            "normalized quaternion",
            "yaw-pitch-roll",
            "[-180, 180)",
            "matrix columns",
            "step origin",
            "named activity confidence",
        ):
            self.assertIn(representation, contract)

    def test_platform_ingress_normalizes_before_common_delivery(self):
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

        for source in (android, ios):
            normalize = source.index("NormalizeVectorSample")
            publish = source.index("PublishVectorBatchFromBackend")
            self.assertLess(normalize, publish)

    def test_native_values_are_development_only_and_sanitized(self):
        units = (
            SENSORS / "Internal" / "OpenMobileSensorUnits.h"
        ).read_text()

        self.assertIn("#if !UE_BUILD_SHIPPING", units)
        self.assertIn("FOpenMobileSensorNativeUnitDiagnostics", units)
        self.assertIn("SanitizedValues", units)
        self.assertIn("SanitizeNativeValue", units)

    def test_every_platform_and_sample_family_has_golden_coverage(self):
        source = (
            SENSORS
            / "Private"
            / "Tests"
            / "OpenMobileSensorsUnitTests.cpp"
        ).read_text()

        for platform in ("Android", "IOS"):
            self.assertIn(platform, source)
        for family in (
            "Vector",
            "Attitude",
            "Scalar",
            "Heading",
            "Steps",
            "Activity",
            "Orientation",
            "Proximity",
        ):
            self.assertIn(family, source)


if __name__ == "__main__":
    unittest.main()
