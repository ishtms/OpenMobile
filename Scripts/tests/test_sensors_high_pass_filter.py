import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"


class SensorsHighPassFilterTests(unittest.TestCase):
	def test_vector_samples_expose_high_pass_state(self):
		samples = (
			COMMON / "Public" / "OpenMobileSensorSamples.h"
		).read_text(encoding="utf-8")
		self.assertIn("bool bHighPassFiltered = false", samples)
		self.assertIn("bool bHighPassFilterWarmingUp = false", samples)

	def test_filter_uses_elapsed_time_and_time_based_warmup(self):
		options = (
			COMMON / "Public" / "OpenMobileSensorStreamOptions.h"
		).read_text(encoding="utf-8")
		filter_source = (
			COMMON / "Private" / "OpenMobileSensorVectorFilter.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("bool bEnableHighPass = false", options)
		self.assertIn("double HighPassTimeConstantSeconds = 0.1", options)
		for token in (
			"HighPassTimeConstantSeconds + DeltaSeconds",
			"HighPassState + Value - HighPassPreviousInput",
			"HighPassWarmupElapsedSeconds += DeltaSeconds",
			"Sample.bHighPassFilterWarmingUp",
		):
			self.assertIn(token, filter_source)


if __name__ == "__main__":
	unittest.main()
