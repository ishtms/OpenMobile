import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS_ROOT = ROOT / "Native" / "OpenMobileSensors"
SENSORS = SENSORS_ROOT / "Source" / "OpenMobileSensors"


class SensorsCoordinateConversionTests(unittest.TestCase):
    def test_device_basis_and_handedness_are_documented(self):
        contract = (SENSORS_ROOT / "Docs" / "PublicContract.md").read_text()

        for term in (
            "portrait-natural",
            "landscape-natural",
            "polar vector",
            "axial vector",
            "handedness",
            "determinant",
        ):
            self.assertIn(term, contract)

    def test_platform_ingress_converts_coordinates_after_units(self):
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
            units = source.index("NormalizeVectorSample")
            coordinates = source.index("ConvertVectorSample")
            publish = source.index("PublishVectorBatchFromBackend")
            self.assertLess(units, coordinates)
            self.assertLess(coordinates, publish)

    def test_conversion_handles_every_motion_representation(self):
        coordinates = (
            SENSORS / "Internal" / "OpenMobileSensorCoordinates.h"
        ).read_text()

        for conversion in (
            "ToDevicePolarVector",
            "ToDeviceAxialVector",
            "ConvertVectorSample",
            "ConvertAttitudeSample",
            "Quaternion",
            "Euler",
            "RotationMatrix",
        ):
            self.assertIn(conversion, coordinates)

    def test_golden_poses_and_device_validation_are_covered(self):
        tests = (
            SENSORS
            / "Private"
            / "Tests"
            / "OpenMobileSensorsCoordinateTests.cpp"
        ).read_text()
        contract = (SENSORS_ROOT / "Docs" / "PublicContract.md").read_text()

        for scenario in (
            "Identity",
            "QuarterTurns",
            "Gravity",
            "AngularVelocity",
            "AxisModel",
        ):
            self.assertIn(scenario, tests)
        for device in (
            "Android phone",
            "Android tablet",
            "iPhone",
            "iPad",
        ):
            self.assertIn(device, contract)

    def test_runtime_test_host_has_a_visible_axis_model(self):
        header = (
            SENSORS
            / "Public"
            / "OpenMobileSensorAxisModelActor.h"
        ).read_text()

        self.assertIn("UArrowComponent", header)
        for arrow in (
            "XAxis",
            "YAxis",
            "ZAxis",
            "Gravity",
            "AngularVelocity",
        ):
            self.assertIn(arrow, header)


if __name__ == "__main__":
    unittest.main()
