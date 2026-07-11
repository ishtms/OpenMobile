import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid" / "Private"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS" / "Private"


class SensorsActivityTransitionTests(unittest.TestCase):
	def test_common_layer_derives_ordered_debounced_transitions(self):
		capabilities = (
			COMMON / "Private" / "OpenMobileSensorsCapabilityService.cpp"
		).read_text(encoding="utf-8")
		samples = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		tracker = (
			COMMON / "Private" / "OpenMobileActivityTransitionTracker.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ApplyActivityTransitionFallback", capabilities)
		for token in (
			"BuildDerivedActivityTransitions",
			"EOpenMobileSensorType::ActivityTransition",
			"ActivityTransitionFilter",
		):
			self.assertIn(token, samples)
		for token in (
			"EOpenMobileActivityTransition::Stopped",
			"EOpenMobileActivityTransition::Started",
			"DebounceSeconds",
			"EOpenMobileActivityTransitionOrigin::Derived",
		):
			self.assertIn(token, tracker)

	def test_android_provider_can_supply_native_transitions(self):
		spi = (
			COMMON / "Public" / "IOpenMobileMotionActivityProvider.h"
		).read_text(encoding="utf-8")
		resolver = (
			COMMON / "Private" / "OpenMobileMotionActivityProviderResolver.cpp"
		).read_text(encoding="utf-8")
		backend = (
			ANDROID / "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("GetTransitionCapability", spi)
		self.assertIn("FOpenMobileSensorIdentifier Sensor", spi)
		self.assertIn("InterfaceVersion = 2", spi)
		self.assertIn("GetTransitionCapability", resolver)
		for token in (
			"FOpenMobileMotionActivityProviderResolver::GetTransitionCapability",
			"ProviderRequest.Sensor = InOutRequest.Sensor",
			"EOpenMobileActivityTransitionOrigin::Native",
			"EOpenMobileSensorType::ActivityTransition",
		):
			self.assertIn(token, backend)

	def test_ios_classification_identifies_core_motion_provider(self):
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		self.assertIn('Sample.ActivityProvider = TEXT("CoreMotion")', bridge)


if __name__ == "__main__":
	unittest.main()
