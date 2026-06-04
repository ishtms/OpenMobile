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


class SensorsLatestValueTests(unittest.TestCase):
	def test_read_result_reports_sample_context(self) -> None:
		results = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorResults.h"
		).read_text(encoding="utf-8")
		for field in (
			"SampleAgeSeconds",
			"bHasNewerSample",
			"Sequence",
			"bSampleValid",
			"Accuracy",
			"SourceFlags",
		):
			self.assertIn(field, results)

	def test_sample_service_is_thread_safe_and_per_handle(self) -> None:
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
			"RegisterSubscription",
			"SetSubscriptionState",
			"PublishVector",
			"ReadLatestVector",
			"ReadLatestProximity",
		):
			self.assertIn(contract, header)
		for behavior in (
			"FRWLock",
			"FCriticalSection",
			"NextSequence",
			"StaleAfterSeconds",
			"bHasNewerSample",
		):
			self.assertIn(behavior, implementation)
		self.assertNotIn("UObject", implementation)

	def test_subscriptions_own_sample_cache_lifetime(self) -> None:
		subscriptions = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		for operation in (
			"RegisterSubscription",
			"SetSubscriptionState",
			"UpdateSubscriptionOptions",
			"UnregisterSubscription",
		):
			self.assertIn(operation, subscriptions)

	def test_subsystem_reads_every_sample_family(self) -> None:
		subsystem = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
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
			self.assertIn(f"ReadLatest{family}", subsystem)

	def test_unreal_tests_cover_latest_value_states_and_races(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsLatestValueTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"BeforeFirstSample",
			"ExactSequenceTransitions",
			"StaleAndPaused",
			"StoppedAndPermissionLoss",
			"AllSampleFamilies",
			"ConcurrentPublishAndRead",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
