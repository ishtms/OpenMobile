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


class SensorsBufferedRetrievalTests(unittest.TestCase):
	def test_sample_service_uses_preallocated_ring_buffers(self) -> None:
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
			"PublishVectorBatch",
			"DrainBufferedVector",
			"DrainBufferedProximity",
		):
			self.assertIn(contract, header)
		for behavior in (
			"TFixedSampleRingBuffer",
			"Storage.SetNum",
			"DroppedSamples",
			"HighWaterMark",
			"RejectNewest",
		):
			self.assertIn(behavior, implementation)

	def test_blueprint_buffer_reads_are_runtime_bounded(self) -> None:
		header = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorsSubsystem.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('ClampMax = "4096"', header)
		self.assertIn("DrainBufferedVector", implementation)
		self.assertIn("MaximumSamples > 4096", implementation)

	def test_runtime_capacity_limit_matches_public_limit(self) -> None:
		subscriptions = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("Requested.BufferCapacitySamples > 4096", subscriptions)

	def test_unreal_tests_cover_buffer_contract(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsBufferedRetrievalTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"OrderedBoundedDrain",
			"OverflowPoliciesAndWraparound",
			"PauseAndTeardown",
			"ConcurrentDrain",
			"AllSampleFamilies",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
