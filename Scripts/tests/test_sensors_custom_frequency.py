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


class SensorsCustomFrequencyTests(unittest.TestCase):
	def test_public_contract_reports_rate_resolution(self) -> None:
		options = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorStreamOptions.h"
		).read_text(encoding="utf-8")
		results = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorResults.h"
		).read_text(encoding="utf-8")
		subscription = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorSubscription.h"
		).read_text(encoding="utf-8")
		for contract in (
			"EOpenMobileSensorRateAdjustmentReason",
			"FOpenMobileSensorRateResolution",
			"RequestedFrequencyHz",
			"ClampedFrequencyHz",
			"AppliedNativeFrequencyHz",
		):
			self.assertIn(contract, options)
		self.assertIn("RateResolution", results)
		self.assertIn("RateResolution", subscription)

	def test_public_frequency_conversion_helpers_exist(self) -> None:
		header = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorStreamOptions.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorStreamOptions.cpp"
		).read_text(encoding="utf-8")
		for helper in (
			"HertzToIntervalSeconds",
			"IntervalSecondsToHertz",
		):
			self.assertIn(helper, header)
			self.assertIn(helper, implementation)

	def test_runtime_tracks_clamped_and_backend_applied_rates(self) -> None:
		implementation = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		for behavior in (
			"InvalidFrequency",
			"UpdateAppliedNativeRate",
			"BackendLimit",
			"RateResolution",
		):
			self.assertIn(behavior, implementation)

	def test_unreal_tests_cover_custom_frequency_contract(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsCustomFrequencyTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"ConversionPrecision",
			"NumericBoundaries",
			"RateReporting",
			"ActiveRateChange",
			"UnsupportedCustomRate",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
