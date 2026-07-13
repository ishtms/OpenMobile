import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"


class SensorsSmoothingDeadZoneTests(unittest.TestCase):
	def test_vector_and_heading_filters_use_family_specific_dead_zones(self):
		vector = (
			COMMON / "Private" / "OpenMobileSensorVectorFilter.cpp"
		).read_text(encoding="utf-8")
		heading = (
			COMMON / "Private" / "OpenMobileSensorHeadingFilter.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("const double MagnitudeSquared = Value.SizeSquared()", vector)
		self.assertIn("MagnitudeSquared <= DeadZone * DeadZone", vector)
		self.assertIn("FMath::FindDeltaAngleDegrees", heading)
		self.assertIn("DistanceFromNorth <= Options.DeadZone", heading)

	def test_samples_and_diagnostics_expose_filter_results(self):
		samples = (
			COMMON / "Public" / "OpenMobileSensorSamples.h"
		).read_text(encoding="utf-8")
		diagnostics = (
			COMMON / "Public" / "OpenMobileSensorDiagnostics.h"
		).read_text(encoding="utf-8")
		service = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		self.assertGreaterEqual(samples.count("bool bExponentiallySmoothed"), 2)
		self.assertGreaterEqual(samples.count("bool bDeadZoneSuppressed"), 2)
		self.assertIn("int64 FilteredSamples = 0", diagnostics)
		self.assertIn("int64 SuppressedSamples = 0", diagnostics)
		self.assertIn("RecordFilterDiagnostics(Slot, Sample)", service)
		self.assertIn("Slot.HeadingFilter.Apply(Slot.FilterOptions, Sample)", service)


if __name__ == "__main__":
	unittest.main()
