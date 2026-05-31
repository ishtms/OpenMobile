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


class SensorsSettingsTests(unittest.TestCase):
	def test_settings_have_required_unreal_identity(self) -> None:
		header = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorsSettings.h"
		).read_text(encoding="utf-8")
		self.assertIn("public UDeveloperSettings", header)
		self.assertIn("Config = Engine", header)
		self.assertIn("DefaultConfig", header)
		self.assertIn('DisplayName = "OpenMobile Sensors"', header)
		self.assertIn('return TEXT("OpenMobile")', header)
		self.assertIn('return TEXT("OpenMobile Sensors")', header)

	def test_settings_cover_runtime_and_packaging_policy(self) -> None:
		header = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorsSettings.h"
		).read_text(encoding="utf-8")
		for setting in (
			"FOpenMobileSensorRatePresetSettings",
			"UIPreset",
			"GamePreset",
			"FastPreset",
			"DefaultStreamOptions",
			"bAllowHighSamplingRate",
			"bAllowBackgroundSensorDelivery",
			"MaximumRecordingDurationSeconds",
			"MaximumRecordingBytes",
			"bEnablePermissionSensitiveSensors",
			"IOSMotionUsageDescription",
			"AndroidActivityRecognitionRationale",
			"DevelopmentInputMode",
			"DevelopmentReplayFile",
		):
			self.assertIn(setting, header)
		self.assertNotIn("bEnablePlugin", header)
		self.assertNotIn("bPluginEnabled", header)

	def test_validation_covers_unsafe_and_shipping_values(self) -> None:
		implementation = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsSettings.cpp"
		).read_text(encoding="utf-8")
		for validation in (
			"CustomFrequencyHz",
			"MaximumCallbackFrequencyHz",
			"BufferCapacitySamples",
			"MaximumDeliveryLatencySeconds",
			"MaximumRecordingDurationSeconds",
			"MaximumRecordingBytes",
			"IOSMotionUsageDescription",
			"AndroidActivityRecognitionRationale",
			"ContinueWhenSupported",
			"DevelopmentInputMode",
			"DevelopmentReplayFile",
			"bShipping",
		):
			self.assertIn(validation, implementation)

	def test_unreal_tests_cover_metadata_defaults_and_validation(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsSettingsTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"Settings.Metadata",
			"Settings.Defaults",
			"Settings.Validation",
			"OpenMobile Sensors",
			"CLASS_DefaultConfig",
			"NAME_Engine",
			"CPF_Config",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
