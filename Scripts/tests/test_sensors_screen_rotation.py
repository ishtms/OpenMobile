import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS_ROOT = ROOT / "Native" / "OpenMobileSensors"
SENSORS = SENSORS_ROOT / "Source" / "OpenMobileSensors"


class SensorsScreenRotationTests(unittest.TestCase):
    def test_rotation_contract_is_public_and_timestamped(self):
        header = (
            SENSORS / "Public" / "OpenMobileSensorScreenRotation.h"
        ).read_text()

        for token in (
            "Rotation0",
            "Rotation90",
            "Rotation180",
            "Rotation270",
            "TimestampSeconds",
            "Sequence",
            "bNaturalOrientationLandscape",
        ):
            self.assertIn(token, header)

        contract = (SENSORS_ROOT / "Docs" / "PublicContract.md").read_text()
        for term in (
            "owner-scoped",
            "application-window rotation",
            "sample timestamp",
            "ScreenRotationSequence",
            "physical orientation",
        ):
            self.assertIn(term, contract)

    def test_rotation_history_is_thread_safe_and_owner_scoped(self):
        source = (
            SENSORS / "Private" / "OpenMobileSensorScreenRotation.cpp"
        ).read_text()

        self.assertIn("FRWLock", source)
        self.assertIn("RotationHistories", source)
        self.assertIn("OwnerIdentifier", source)
        self.assertIn("TimestampSeconds", source)
        self.assertIn("MaximumRotationHistory", source)

    def test_platform_capture_boundaries_are_explicit(self):
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

        self.assertIn("CaptureApplicationWindowRotationFromUIThread", android)
        self.assertIn("CaptureApplicationWindowRotationFromMainThread", ios)

        subsystem = (
            SENSORS / "Public" / "OpenMobileSensorsSubsystem.h"
        ).read_text()
        self.assertIn("UpdateApplicationWindowRotationNative", subsystem)

    def test_required_rotation_scenarios_are_covered(self):
        source = (
            SENSORS
            / "Private"
            / "Tests"
            / "OpenMobileSensorsScreenRotationTests.cpp"
        ).read_text()

        for scenario in (
            "FourRotations",
            "SharedPhysicalStream",
            "BufferedBoundary",
            "NaturalOrientation",
            "AttitudeAndHeading",
            "SplitScreenAndOrientationLock",
            "PhysicalOrientationSeparation",
        ):
            self.assertIn(scenario, source)


if __name__ == "__main__":
    unittest.main()
