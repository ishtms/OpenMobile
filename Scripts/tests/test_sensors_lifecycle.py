import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"


class SensorsLifecycleTests(unittest.TestCase):
	def test_subscription_service_owns_all_application_lifecycle_signals(self):
		source = (
			COMMON / "Private" / "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"ApplicationWillDeactivateDelegate",
			"ApplicationHasReactivatedDelegate",
			"ApplicationWillEnterBackgroundDelegate",
			"ApplicationHasEnteredForegroundDelegate",
			"bPausedForLifecycle",
			"ReconcilePhysicalStream",
			"CompleteSubscriptionUpdate",
		):
			self.assertIn(token, source)

	def test_lifecycle_policies_cover_operation_specific_behavior(self):
		tests = (
			COMMON
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsLifecycleTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"SuspendResumeOwnership",
			"OperationPolicies",
			"StartInterruptionRace",
			"CapabilitySignals",
			"Event-driven steps continue",
			"Activity transitions continue",
			"Unsupported proximity continuation suspends",
			"Supported altitude continuation remains active",
		):
			self.assertIn(scenario, tests)

	def test_recording_policy_reaches_hidden_subscriptions(self):
		header = (
			COMMON / "Public" / "OpenMobileSensorRecording.h"
		).read_text(encoding="utf-8")
		source = (
			COMMON / "Private" / "OpenMobileSensorsRecordingService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("LifecyclePolicy", header)
		self.assertIn(
			"Request.Options.LifecyclePolicy = Entry.Options.LifecyclePolicy",
			source,
		)
		self.assertIn("HandleSubscriptionStateChanged", source)
		self.assertIn("BackgroundRestricted", source)

	def test_public_contract_documents_lifecycle_ownership(self):
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for token in (
			"## Foreground lifecycle",
			"SuspendInBackground",
			"StopInBackground",
			"ContinueWhenSupported",
			"process-level physical stream",
			"control is disabled or interrupted",
			"recording finalizes",
		):
			self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
