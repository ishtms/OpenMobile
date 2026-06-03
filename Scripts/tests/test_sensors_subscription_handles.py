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


class SensorsSubscriptionHandleTests(unittest.TestCase):
	def test_backend_contract_owns_physical_streams(self) -> None:
		backend_types = (
			COMMON_SOURCE
			/ "Internal"
			/ "OpenMobileSensorsBackendTypes.h"
		).read_text(encoding="utf-8")
		backend = (
			COMMON_SOURCE / "Internal" / "IOpenMobileSensorsBackend.h"
		).read_text(encoding="utf-8")
		for contract in (
			"FOpenMobileSensorBackendStreamHandle",
			"FOpenMobileSensorPhysicalStreamRequest",
			"RequestedFrequencyHz",
			"MaximumDeliveryLatencySeconds",
		):
			self.assertIn(contract, backend_types)
		for operation in (
			"StartSensorStream",
			"ReconfigureSensorStream",
			"StopSensorStream",
		):
			self.assertIn(operation, backend)

	def test_service_validates_negotiates_and_fans_out(self) -> None:
		header = (
			COMMON_SOURCE
			/ "Internal"
			/ "OpenMobileSensorsSubscriptionService.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		for contract in (
			"OnStateChanged",
			"SelectSubscribersForSample",
			"ProcessPendingBackendOperationsForTests",
		):
			self.assertIn(contract, header)
		for behavior in (
			"ValidateAndResolveOptions",
			"PhysicalStreams",
			"BuildPhysicalRequest",
			"MaximumCallbackFrequencyHz",
			"ReconfigureSensorStream",
		):
			self.assertIn(behavior, implementation)

	def test_subsystem_forwards_owned_state_changes(self) -> None:
		header = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorsSubsystem.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("SubscriptionServiceChangedHandle", header)
		self.assertIn("HandleSubscriptionStateChanged", header)
		self.assertIn("OnSubscriptionStateChanged.Broadcast", implementation)
		self.assertIn("SubscriptionStateChangedEvent.Broadcast", implementation)

	def test_unreal_tests_cover_subscription_stream_contract(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsSubscriptionTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"RequestValidation",
			"StartStateEvents",
			"StartFailure",
			"RateNegotiation",
			"IncompatibleOptions",
			"UpdateInPlace",
			"IndependentFanout",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
