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


class SensorsSubscriptionOwnershipTests(unittest.TestCase):
	def test_handle_identity_is_opaque_and_generation_checked(self) -> None:
		handle = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorSubscription.h"
		).read_text(encoding="utf-8")
		self.assertIn("FGuid Identifier", handle)
		self.assertIn("uint32 Generation", handle)
		self.assertIn("private:", handle)
		self.assertIn("GetIdentifier()", handle)
		self.assertIn("GetGeneration()", handle)

	def test_process_service_owns_logical_subscriptions(self) -> None:
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
		for operation in (
			"StartSubscription",
			"UpdateSubscription",
			"StopSubscription",
			"StopAllSubscriptions",
			"GetSubscriptionState",
			"IsHandleCurrent",
			"InvalidateForUnrecoverablePermissionLoss",
		):
			self.assertIn(operation, header)
		self.assertIn("TMap<FGuid, FSubscriptionEntry>", implementation)
		self.assertIn("OwnerIdentifier", implementation)
		self.assertIn("BackendToken", implementation)
		self.assertNotIn("UObject", implementation)

	def test_module_and_subsystem_enforce_lifetimes(self) -> None:
		module = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsModule.cpp"
		).read_text(encoding="utf-8")
		subsystem = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
		registry = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsBackendRegistry.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FOpenMobileSensorsSubscriptionService::Start", module)
		self.assertIn("FOpenMobileSensorsSubscriptionService::BeginShutdown", module)
		self.assertIn("SubscriptionOwnerIdentifier", subsystem)
		self.assertIn("StopAllSubscriptions", subsystem)
		self.assertIn("HandleBackendGenerationChanged", registry)

	def test_unreal_contract_covers_owner_and_generation_races(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsArchitectureTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"SubscriptionOwnership",
			"Cross-owner stop",
			"Duplicate stop",
			"PIE teardown",
			"permission loss",
			"Backend loss",
			"Late batches",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
