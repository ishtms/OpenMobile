import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"


class SensorsCalibrationTests(unittest.TestCase):
	def test_blueprint_contract_exposes_localized_lifecycle_events(self):
		calibration = (
			COMMON / "Public" / "OpenMobileSensorCalibration.h"
		).read_text(encoding="utf-8")
		subsystem = (
			COMMON / "Public" / "OpenMobileSensorsSubsystem.h"
		).read_text(encoding="utf-8")
		for token in (
			"EOpenMobileSensorCalibrationState",
			"Required",
			"Resolved",
			"EOpenMobileSensorCalibrationReason",
			"FOpenMobileSensorCalibrationEvent",
			"FText Guidance",
		):
			self.assertIn(token, calibration)
		self.assertIn("FOpenMobileSensorCalibrationChangedDynamic", subsystem)
		self.assertIn("OnCalibrationChanged", subsystem)

	def test_service_deduplicates_guidance_with_a_fixed_cooldown(self):
		service = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("CalibrationEventCooldownSeconds = 30.0", service)
		self.assertIn("bCalibrationRequiredEventEmitted", service)
		self.assertIn("LastCalibrationRequiredEventSeconds", service)
		self.assertIn("QualityRecovered", service)
		self.assertGreaterEqual(service.count("NSLOCTEXT("), 3)

	def test_native_prompt_requires_an_explicit_caller_operation(self):
		backend = (
			COMMON / "Internal" / "IOpenMobileSensorsBackend.h"
		).read_text(encoding="utf-8")
		subsystem_header = (
			COMMON / "Public" / "OpenMobileSensorsSubsystem.h"
		).read_text(encoding="utf-8")
		subsystem_cpp = (
			COMMON / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
		service = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("RequestCalibrationPrompt", backend)
		self.assertIn("EOpenMobileSensorResultCode::NotSupported", backend)
		self.assertIn("RequestNativeCalibrationPrompt", subsystem_header)
		self.assertIn("RequestNativeCalibrationPrompt", subsystem_cpp)
		self.assertNotIn("RequestCalibrationPrompt", service)

	def test_public_contract_defines_informational_guidance(self):
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for token in (
			"## Calibration guidance",
			"30-second cooldown",
			"informational",
			"does not launch native UI",
			"RequestNativeCalibrationPrompt",
			"UnsupportedOperation",
			"localization-ready",
			"resolution event",
		):
			self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
