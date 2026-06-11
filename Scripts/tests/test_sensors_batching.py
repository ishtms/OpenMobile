import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors" / "Source" / "OpenMobileSensors"


class SensorsBatchingTests(unittest.TestCase):
    def test_backend_contract_tracks_requested_and_applied_batching(self):
        source = (SENSORS / "Internal" / "OpenMobileSensorsBackendTypes.h").read_text()

        self.assertIn("bNativeBatchingRequested", source)
        self.assertIn("bNativeBatchingApplied", source)
        self.assertIn("MaximumDeliveryLatencySeconds", source)

    def test_shared_stream_negotiates_latency_and_reports_batching(self):
        source = (
            SENSORS / "Private" / "OpenMobileSensorsSubscriptionService.cpp"
        ).read_text()

        self.assertIn("SupportsNativeBatching", source)
        self.assertIn("MaximumDeliveryLatencySeconds = 0.0", source)
        self.assertIn("GetBatchingMode", source)
        self.assertIn("EOpenMobileSensorBatchingMode::Native", source)
        self.assertIn("EOpenMobileSensorBatchingMode::Plugin", source)

    def test_fallback_reuses_the_bounded_sample_buffer(self):
        source = (
            SENSORS / "Private" / "OpenMobileSensorsSampleService.cpp"
        ).read_text()

        self.assertIn("TFixedSampleRingBuffer", source)
        self.assertIn("EnqueueBufferedSample", source)
        self.assertIn("PublishVectorBatchFromBackend", source)

    def test_native_batching_scenarios_are_covered(self):
        source = (
            SENSORS
            / "Private"
            / "Tests"
            / "OpenMobileSensorsBatchingTests.cpp"
        ).read_text()

        for scenario in (
            "MixedLatencySubscribers",
            "NativeAndFallbackDiagnostics",
            "PartialNativeBatches",
            "PluginBufferOverflow",
            "ForegroundTransitions",
        ):
            self.assertIn(scenario, source)


if __name__ == "__main__":
    unittest.main()
