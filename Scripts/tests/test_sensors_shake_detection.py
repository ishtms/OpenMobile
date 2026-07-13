import csv
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"


class SensorsShakeDetectionTests(unittest.TestCase):
	def test_public_contract_exposes_configuration_and_event_metadata(self):
		identifiers = (COMMON / "Public" / "OpenMobileSensorIdentifiers.h").read_text(
			encoding="utf-8"
		)
		options = (COMMON / "Public" / "OpenMobileSensorStreamOptions.h").read_text(
			encoding="utf-8"
		)
		samples = (COMMON / "Public" / "OpenMobileSensorSamples.h").read_text(
			encoding="utf-8"
		)
		self.assertIn("EOpenMobileSensorType::Shake", (
			COMMON / "Private" / "OpenMobileSensorIdentifiers.cpp"
		).read_text(encoding="utf-8"))
		self.assertIn("Shake", identifiers)
		for token in (
			"FOpenMobileShakeDetectionOptions",
			"StrengthThresholdMetresPerSecondSquared",
			"MinimumImpulses",
			"DurationWindowSeconds",
			"QuietResetSeconds",
			"CooldownSeconds",
			"ShakeDetection",
		):
			self.assertIn(token, options)
		for token in (
			"FOpenMobileShakeEventData",
			"StrengthMetresPerSecondSquared",
			"DurationSeconds",
			"TimestampSeconds",
			"ImpulseCount",
			"SourceSensor",
			"bHasShakeEvent",
		):
			self.assertIn(token, samples)

	def test_common_path_uses_bounded_state_and_shared_stream_selection(self):
		detector = (
			COMMON / "Private" / "OpenMobileSensorShakeDetector.cpp"
		).read_text(encoding="utf-8")
		capabilities = (
			COMMON / "Private" / "OpenMobileSensorsCapabilityService.cpp"
		).read_text(encoding="utf-8")
		subscriptions = (
			COMMON / "Private" / "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		samples = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"Strength < Options.StrengthThresholdMetresPerSecondSquared",
			"Options.DurationWindowSeconds",
			"Options.QuietResetSeconds",
			"CooldownUntilSeconds",
			"Impulses.RemoveAt",
			"EOpenMobileSensorSourceFlags::PluginDerived",
		):
			self.assertIn(token, detector)
		self.assertIn("ApplyShakeFallback", capabilities)
		self.assertIn("bNativeLinearAvailable", capabilities)
		self.assertIn("LogicalSensor.Type == EOpenMobileSensorType::Shake", subscriptions)
		self.assertIn("Shake->Fallback.RequiredInputs[0]", subscriptions)
		self.assertIn("Slot.ShakeDetector.Process(Motion, Event)", samples)
		self.assertIn("Slot.ShakeDetector.Reset()", samples)
		self.assertIn("Slot->ShakeDetector.Configure", samples)

	def test_recorded_fixture_covers_non_shakes_and_one_shake(self):
		fixture = SENSORS / "Tests" / "Fixtures" / "ShakeLinearAcceleration.csv"
		with fixture.open(newline="", encoding="utf-8") as fixture_file:
			rows = list(csv.DictReader(fixture_file))
		self.assertGreater(len(rows), 12)
		self.assertEqual({"bump", "sustained", "shake"}, {
			row["trace"] for row in rows
		})
		self.assertEqual(1, sum(int(row["expected_event"]) for row in rows))
		for row in rows:
			for field in (
				"timestamp_seconds",
				"x_mps2",
				"y_mps2",
				"z_mps2",
			):
				self.assertTrue(float(row[field]) == float(row[field]))


if __name__ == "__main__":
	unittest.main()
