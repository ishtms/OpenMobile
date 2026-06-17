import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SENSORS_ROOT = REPOSITORY_ROOT / "Native" / "OpenMobileSensors"
COMMON_SOURCE = SENSORS_ROOT / "Source" / "OpenMobileSensors"


class SensorsGameThreadCallbackTests(unittest.TestCase):
	def test_common_ingress_rejects_stale_platform_callbacks(self) -> None:
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
			"SetPhysicalStreamHandle",
			"PublishVectorBatchFromBackend",
			"PublishProximityBatchFromBackend",
		):
			self.assertIn(contract, header)
		for behavior in (
			"BackendGeneration",
			"PhysicalStreamHandle",
			"bShuttingDown",
			"RequiredBackendGeneration",
		):
			self.assertIn(behavior, implementation)
		rules = (
			SENSORS_ROOT
			/ "Source"
			/ "OpenMobileSensorsAndroid"
			/ "OpenMobileSensorsAndroid.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn('"Launch"', rules)

	def test_android_uses_a_dedicated_handler_thread(self) -> None:
		backend = (
			SENSORS_ROOT
			/ "Source"
			/ "OpenMobileSensorsAndroid"
			/ "Private"
			/ "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		bridge = (
			SENSORS_ROOT
			/ "Source"
			/ "OpenMobileSensorsAndroid"
			/ "Private"
			/ "Android"
			/ "src"
			/ "com"
			/ "openmobile"
			/ "sensors"
			/ "OpenMobileSensorsBridgeV1.java"
		).read_text(encoding="utf-8")
		for behavior in (
			"HandlerThread",
			"getLooper",
			"quitSafely",
		):
			self.assertIn(behavior, bridge)
		self.assertIn("PublishVectorBatchFromBackend", backend)
		rules = (
			SENSORS_ROOT
			/ "Source"
			/ "OpenMobileSensorsIOS"
			/ "OpenMobileSensorsIOS.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn('PublicFrameworks.Add("Foundation")', rules)

	def test_ios_uses_a_serial_core_motion_queue(self) -> None:
		implementation = (
			SENSORS_ROOT
			/ "Source"
			/ "OpenMobileSensorsIOS"
			/ "Private"
			/ "OpenMobileSensorsIOSBackend.mm"
		).read_text(encoding="utf-8")
		for behavior in (
			"NSOperationQueue",
			"maxConcurrentOperationCount = 1",
			"qualityOfService",
			"PublishVectorBatchFromBackend",
		):
			self.assertIn(behavior, implementation)

	def test_unreal_tests_cover_thread_and_lifecycle_races(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsGameThreadCallbackTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"ArbitraryThread",
			"DuplicateBatch",
			"StaleGeneration",
			"ControlRaces",
			"ShutdownAndOwnerTeardown",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
