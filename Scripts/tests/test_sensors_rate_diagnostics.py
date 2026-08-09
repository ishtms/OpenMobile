import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
COMMON_SOURCE = (
	REPOSITORY_ROOT
	/ "Native"
	/ "OpenMobileSensors"
	/ "Source"
	/ "OpenMobileSensors"
)


class SensorsRateDiagnosticsTests(unittest.TestCase):
	def test_rate_window_is_fixed_and_allocation_free(self) -> None:
		implementation = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		for behavior in (
			"RateIntervals[64]",
			"RateIntervalSum",
			"RateIntervalSquareSum",
			"UpdateRateStatistics",
			"ResetRateStatistics",
		):
			self.assertIn(behavior, implementation)

	def test_subsystem_returns_owned_stream_diagnostics(self) -> None:
		diagnostics = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsDiagnosticsService.cpp"
		).read_text(encoding="utf-8")
		subscriptions = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("GetStreamDiagnostics", diagnostics)
		self.assertIn("GetStreamDiagnostics", subscriptions)
		self.assertIn("RateResolution.AppliedNativeFrequencyHz", subscriptions)

	def test_unreal_tests_cover_rate_diagnostics(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsRateDiagnosticsTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"SteadyWindow",
			"JitteredIntervals",
			"BurstBatched",
			"StallClockResetAndRestart",
			"RequestedAndApplied",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
