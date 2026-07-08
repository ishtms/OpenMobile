import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid" / "Private"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS" / "Private"


class SensorsStepDetectionTests(unittest.TestCase):
	def test_public_contract_tracks_source_quality_delta_and_native_total(self):
		samples = (COMMON / "Public" / "OpenMobileSensorSamples.h").read_text(
			encoding="utf-8"
		)
		service = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"EOpenMobileStepDetectionSource",
			"EOpenMobileStepDetectionQuality",
			"DetectedStepDelta",
			"bHasNativeTotal",
			"NativeTotal",
		):
			self.assertIn(token, samples)
		for token in (
			"FOpenMobileStepDetectionTracker",
			"StepDetectionTracker.Initialize",
			"StepDetectionTracker.Process",
		):
			self.assertIn(token, service)

	def test_android_step_detector_marks_direct_hardware_events(self):
		backend = (
			ANDROID / "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		step_block = backend[
			backend.index("if (Type == EOpenMobileSensorType::StepCounter"):
			backend.index("if (Type == EOpenMobileSensorType::Proximity")
		]
		compact = "".join(step_block.split())
		for token in (
			"DetectedStepDelta=Sample.Count",
			"EOpenMobileStepDetectionSource::AndroidStepDetector",
			"EOpenMobileStepDetectionQuality::DirectHardwareEvent",
		):
			self.assertIn(token, compact)

	def test_ios_step_detector_uses_pedometer_deltas(self):
		backend = (IOS / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		compact_backend = "".join(backend.split())
		for token in (
			"MakeStepDetectorCapability",
			"EOpenMobileStepDetectionSource::IOSPedometerDelta",
			"EOpenMobileStepDetectionQuality::InferredFromPedometerDelta",
			"Sample.bHasNativeTotal=true",
			"Sample.NativeTotal=Sample.Count",
		):
			self.assertIn(token, compact_backend)
		for token in (
			"case EOpenMobileSensorType::StepDetector:",
			"return EService::Pedometer",
			"Availability.bStepCounting",
		):
			self.assertIn(token, bridge)


if __name__ == "__main__":
	unittest.main()
