import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS" / "Private"


class SensorsMotionActivityTests(unittest.TestCase):
	def test_ios_uses_core_motion_activity_with_normalized_state(self):
		backend = (IOS / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		for token in (
			"MakeMotionActivityCapability",
			"PublishActivityBatchFromMotionQueue",
			"EOpenMobileSensorPermission::MotionActivity",
		):
			self.assertIn(token, backend)
		self.assertNotIn("MotionActivityTrackers", backend)
		common_samples = (
			SENSORS
			/ "Source"
			/ "OpenMobileSensors"
			/ "Private"
			/ "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FOpenMobileActivitySampleFilter ActivityFilter", common_samples)
		for token in (
			"CMMotionActivityManager",
			"isActivityAvailable",
			"startActivityUpdatesToQueue",
			"stopActivityUpdates",
			"HandleMotionActivity",
			"FOpenMobileMotionActivityClassifier::Classify",
			"Activity.stationary",
			"Activity.walking",
			"Activity.running",
			"Activity.cycling",
			"Activity.automotive",
			"Activity.confidence",
		):
			self.assertIn(token, bridge)

	def test_android_routes_activity_only_through_versioned_provider_spi(self):
		backend = (
			ANDROID / "Private" / "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		header = (
			ANDROID / "Private" / "OpenMobileSensorsAndroidBackend.h"
		).read_text(encoding="utf-8")
		for token in (
			"IOpenMobileMotionActivityProvider",
			"FOpenMobileMotionActivityProviderResolver::GetCapability",
			"StartMotionActivityProviderStream",
			"ReconfigureMotionActivityProviderStream",
			"StopMotionActivityProviderStream",
			"HandleMotionActivityProviderUnregistered",
		):
			self.assertIn(token, backend + header)
		base_source = "\n".join(
			path.read_text(encoding="utf-8", errors="ignore")
			for path in ANDROID.rglob("*")
			if path.is_file()
		)
		self.assertNotIn("com.google.android.gms", base_source)
		self.assertNotIn("play-services-activity-recognition", base_source)
		spi = (
			SENSORS
			/ "Source"
			/ "OpenMobileSensors"
			/ "Public"
			/ "IOpenMobileMotionActivityProvider.h"
		).read_text(encoding="utf-8")
		umbrella = (
			SENSORS
			/ "Source"
			/ "OpenMobileSensors"
			/ "Public"
			/ "OpenMobileSensors.h"
		).read_text(encoding="utf-8")
		self.assertIn("InterfaceVersion = 1", spi)
		self.assertIn("IOpenMobileMotionActivityProvider.h", umbrella)


if __name__ == "__main__":
	unittest.main()
