import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"


class SensorsBackgroundCapabilitiesTests(unittest.TestCase):
	def test_public_contract_is_operation_specific(self):
		header = (
			COMMON / "Public" / "OpenMobileSensorCapabilities.h"
		).read_text(encoding="utf-8")
		for token in (
			"EOpenMobileSensorBackgroundOperation",
			"FOpenMobileSensorBackgroundCapability",
			"PlatformBehavior",
			"ExpectedBehavior",
			"ActiveRestriction",
			"Reason",
			"BackgroundOperations",
		):
			self.assertIn(token, header)
		self.assertNotIn("bool bSupportsBackground", header)

	def test_common_service_applies_dynamic_context(self):
		source = (
			COMMON / "Private" / "OpenMobileSensorsCapabilityService.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"PopulateBackgroundCapabilities",
			"GetCapabilityFailureReason",
			"bAllowBackgroundSensorDelivery",
			"bHasBackgroundDeliverySetting",
			"PermissionDenied",
			"ApplicationWillEnterBackgroundDelegate",
		):
			self.assertIn(token, source)

	def test_platform_contracts_are_conservative(self):
		backend = (
			COMMON / "Internal" / "IOpenMobileSensorsBackend.h"
		).read_text(encoding="utf-8")
		android = (
			SENSORS
			/ "Source"
			/ "OpenMobileSensorsAndroid"
			/ "Private"
			/ "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		ios = (
			SENSORS
			/ "Source"
			/ "OpenMobileSensorsIOS"
			/ "Private"
			/ "OpenMobileSensorsIOSBackend.mm"
		).read_text(encoding="utf-8")
		self.assertIn(
			"GetNativeStepCountQueryBackgroundSupport", backend
		)
		self.assertIn(
			"EOpenMobileSensorBackgroundSupport::Unsupported", backend
		)
		self.assertIn(
			"EOpenMobileSensorBackgroundSupport::Suspended", android
		)
		self.assertIn(
			"GetNativeStepCountQueryBackgroundSupport", ios
		)
		self.assertIn(
			"EOpenMobileSensorBackgroundSupport::Limited", ios
		)

	def test_automation_covers_dynamic_background_state(self):
		tests = (
			COMMON
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsCapabilityTests.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"Background.OperationMatrix",
			"Background.DynamicState",
			"Background.DeliveryContract",
			"Project policy suspends transition delivery",
			"Denied authorization removes effective background support",
			"A provider downgrade is reflected",
			"Capability inspection starts no sensor stream",
		):
			self.assertIn(token, tests)

	def test_public_documentation_defines_background_reporting(self):
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for token in (
			"## Background capability reporting",
			"PlatformBehavior",
			"ExpectedBehavior",
			"NativeStepCountQuery",
			"does not start",
			"does not promise indefinite",
			"physical-device validation",
		):
			self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
