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


class SensorsEventCallbackTests(unittest.TestCase):
	def test_sample_service_batches_on_the_game_thread(self) -> None:
		header = (
			COMMON_SOURCE
			/ "Internal"
			/ "OpenMobileSensorsSampleService.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		for contract in (
			"OnVectorBatch",
			"OnAttitudeBatch",
			"OnProximityBatch",
			"DrainPendingEventsForTests",
		):
			self.assertIn(contract, header)
		for behavior in (
			"PendingVector",
			"MaximumCallbackFrequencyHz",
			"DrainPendingEvents",
			"FTSTicker",
			"IsInGameThread",
		):
			self.assertIn(behavior, implementation)
		self.assertNotIn("UObject", implementation)

	def test_subsystem_forwards_typed_batches(self) -> None:
		header = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorsSubsystem.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("EnsureSampleListeners", header)
		for family in (
			"Vector",
			"Attitude",
			"Scalar",
			"Heading",
			"Steps",
			"Activity",
			"Orientation",
			"Proximity",
		):
			self.assertIn(f"Handle{family}Batch", header)
		self.assertIn("OPENMOBILE_IMPLEMENT_BATCH_HANDLER", implementation)
		self.assertIn("On##FamilyName##Samples.Broadcast", implementation)
		self.assertIn("FamilyName##SamplesEvent.Broadcast", implementation)

	def test_unreal_tests_cover_event_callback_contract(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsEventCallbackTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"OffThreadBatching",
			"CallbackThrottling",
			"ReentrantStopAndUpdate",
			"OwnerDestruction",
			"LateCallback",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
