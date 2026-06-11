import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors" / "Source" / "OpenMobileSensors"


class SensorsLowLatencyTests(unittest.TestCase):
    def test_low_latency_is_an_explicit_applied_mode(self):
        source = (
            SENSORS / "Private" / "OpenMobileSensorsSubscriptionService.cpp"
        ).read_text()

        self.assertIn("OutApplied.bLowLatency", source)
        self.assertIn("OutApplied.MaximumDeliveryLatencySeconds = 0.0", source)

    def test_development_diagnostics_cover_queue_costs(self):
        diagnostics = (
            SENSORS / "Public" / "OpenMobileSensorDiagnostics.h"
        ).read_text()
        sample_service = (
            SENSORS / "Private" / "OpenMobileSensorsSampleService.cpp"
        ).read_text()

        self.assertIn("QueueDelaySeconds", diagnostics)
        self.assertIn("GameThreadProcessingSeconds", diagnostics)
        self.assertIn("GetDeliveryDiagnostics", sample_service)
        self.assertIn("EventDroppedSamples", sample_service)

    def test_low_latency_scenarios_are_covered(self):
        source = (
            SENSORS
            / "Private"
            / "Tests"
            / "OpenMobileSensorsLowLatencyTests.cpp"
        ).read_text()

        for scenario in (
            "ExplicitMode",
            "PhysicalStreamIsolation",
            "CallbackCap",
            "DevelopmentDiagnostics",
        ):
            self.assertIn(scenario, source)

    def test_device_acceptance_targets_are_measurable(self):
        source = (
            ROOT / "Native" / "OpenMobileSensors" / "Docs" / "PublicContract.md"
        ).read_text()

        for target in ("33 ms", "16.7 ms", "0.25 ms", "10 minutes"):
            self.assertIn(target, source)


if __name__ == "__main__":
    unittest.main()
