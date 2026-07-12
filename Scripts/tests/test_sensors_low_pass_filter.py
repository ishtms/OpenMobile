import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"


class SensorsLowPassFilterTests(unittest.TestCase):
	def test_options_default_to_disabled_time_based_low_pass(self):
		options = (
			COMMON / "Public" / "OpenMobileSensorStreamOptions.h"
		).read_text(encoding="utf-8")
		self.assertIn("bool bEnableLowPass = false", options)
		self.assertIn("double LowPassTimeConstantSeconds = 0.1", options)
		self.assertIn('Units = "s"', options)

	def test_common_path_uses_elapsed_time_and_resets_long_gaps(self):
		filter_source = (
			COMMON / "Private" / "OpenMobileSensorVectorFilter.cpp"
		).read_text(encoding="utf-8")
		samples = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"LowPassTimeConstantSeconds + DeltaSeconds",
			"LowPassState += Alpha * (Value - LowPassState)",
		):
			self.assertIn(token, filter_source)
		for token in (
			"ApplyLongGapReset",
			"Slot.VectorFilter.Apply(Slot.FilterOptions, Sample)",
			"Header.bStatefulProcessingReset = true",
		):
			self.assertIn(token, samples)


if __name__ == "__main__":
	unittest.main()
