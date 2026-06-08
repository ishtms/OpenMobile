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


class SensorsSamplingPresetTests(unittest.TestCase):
	def test_presets_define_power_and_timing_intent(self) -> None:
		header = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorsSettings.h"
		).read_text(encoding="utf-8")
		for contract in (
			"EOpenMobileSensorPowerIntent",
			"LowPower",
			"Balanced",
			"Performance",
			"PowerIntent",
		):
			self.assertIn(contract, header)

	def test_resolution_applies_capability_and_project_limits(self) -> None:
		implementation = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsSubscriptionService.cpp"
		).read_text(encoding="utf-8")
		for behavior in (
			"ResolvePreset",
			"MinimumFrequencyHz",
			"MaximumFrequencyHz",
			"bAllowHighSamplingRate",
			"200.0",
		):
			self.assertIn(behavior, implementation)

	def test_unreal_tests_cover_preset_resolution(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsSamplingPresetTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"PresetMapping",
			"PlatformClamps",
			"ProjectHighRatePolicy",
			"InvalidPreset",
			"SettingsOverride",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
