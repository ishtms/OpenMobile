import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors" / "Source" / "OpenMobileSensors"


class SensorsFlushTests(unittest.TestCase):
    def test_backend_exposes_one_shot_flush_contract(self):
        source = (SENSORS / "Internal" / "IOpenMobileSensorsBackend.h").read_text()

        self.assertIn("FOnOpenMobileSensorBackendFlushComplete", source)
        self.assertIn("FlushSensorStream", source)

    def test_subscription_service_owns_flush_serialization(self):
        source = (
            SENSORS / "Private" / "OpenMobileSensorsSubscriptionService.cpp"
        ).read_text()

        self.assertIn("FPendingFlush", source)
        self.assertIn("CompleteFlush", source)
        self.assertIn("CancelFlushesForPhysicalStream", source)
        self.assertIn("ProcessFlushTimeouts", source)

    def test_flush_uses_the_normal_plugin_sample_path(self):
        source = (
            SENSORS / "Private" / "OpenMobileSensorsSampleService.cpp"
        ).read_text()

        self.assertIn("FlushPluginSamples", source)
        self.assertIn("DrainPendingEvents", source)
        self.assertIn("GetPluginSampleCount", source)

    def test_flush_scenarios_are_covered(self):
        source = (
            SENSORS
            / "Private"
            / "Tests"
            / "OpenMobileSensorsFlushTests.cpp"
        ).read_text()

        for scenario in (
            "EmptyAndPopulated",
            "Unsupported",
            "Concurrent",
            "TimedOut",
            "LateNativeCallbacks",
            "StopAndRateChange",
        ):
            self.assertIn(scenario, source)


if __name__ == "__main__":
    unittest.main()
