import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS_ROOT = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS_ROOT / "Source" / "OpenMobileSensors"
ANDROID = SENSORS_ROOT / "Source" / "OpenMobileSensorsAndroid"
IOS = SENSORS_ROOT / "Source" / "OpenMobileSensorsIOS"


class SensorsAccuracyTests(unittest.TestCase):
	def test_public_contract_exposes_accuracy_state_and_change_events(self):
		accuracy = (
			COMMON / "Public" / "OpenMobileSensorAccuracy.h"
		).read_text(encoding="utf-8")
		samples = (
			COMMON / "Public" / "OpenMobileSensorSamples.h"
		).read_text(encoding="utf-8")
		subsystem = (
			COMMON / "Public" / "OpenMobileSensorsSubsystem.h"
		).read_text(encoding="utf-8")
		umbrella = (
			COMMON / "Public" / "OpenMobileSensors.h"
		).read_text(encoding="utf-8")

		for quality in ("Unknown", "Unreliable", "Low", "Medium", "High"):
			self.assertIn(quality, accuracy)
		for field in (
			"bCalibrationRequired",
			"bHasEstimatedError",
			"EstimatedError",
			"TimestampSeconds",
			"Sequence",
		):
			self.assertIn(field, accuracy)
		self.assertIn("OpenMobileSensorAccuracy.h", samples)
		self.assertIn("OpenMobileSensorAccuracy.h", umbrella)
		self.assertIn("bCalibrationRequired", samples)
		self.assertIn("OnAccuracyChanged", subsystem)
		self.assertIn("OnAccuracyChangedNative", subsystem)

	def test_common_service_deduplicates_material_accuracy_changes(self):
		header = (
			COMMON / "Internal" / "OpenMobileSensorsSampleService.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")

		for contract in (
			"PublishAccuracy",
			"PublishAccuracyFromBackend",
			"OnAccuracyChanged",
		):
			self.assertIn(contract, header)
		for behavior in (
			"bHasAccuracyState",
			"bMaterialChange",
			"PendingAccuracyChanges",
			"MaximumPendingAccuracyChanges",
		):
			self.assertIn(behavior, implementation)

	def test_platform_boundaries_map_only_native_accuracy_information(self):
		mapper = (
			COMMON / "Internal" / "OpenMobileSensorAccuracyMapper.h"
		).read_text(encoding="utf-8")
		android = (
			ANDROID / "Private" / "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		ios = (
			IOS / "Private" / "OpenMobileSensorsIOSBackend.mm"
		).read_text(encoding="utf-8")

		self.assertIn("FromAndroidAccuracyCallback", mapper)
		self.assertIn("FromIOSMagneticFieldAccuracy", mapper)
		self.assertIn("FromIOSHeadingAccuracy", mapper)
		self.assertIn("FromAndroidAccuracyCallback", android)
		self.assertIn("PublishAccuracyFromBackend", android)
		self.assertIn("FromIOSMagneticFieldAccuracy", ios)
		self.assertIn("FromIOSHeadingAccuracy", ios)
		self.assertIn("PublishAccuracyFromBackend", ios)

	def test_native_tests_cover_transitions_invalid_values_and_recovery(self):
		tests = (
			COMMON
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsAccuracyTests.cpp"
		).read_text(encoding="utf-8")

		for scenario in (
			"PlatformMappings",
			"QualityTransitionsAndDeduplication",
			"MissingAccuracy",
			"InvalidValuesAndRecovery",
			"BackendGeneration",
			"SubsystemEvent",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
