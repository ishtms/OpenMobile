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


class SensorsCapabilityMatrixTests(unittest.TestCase):
	def test_sensor_type_contract_covers_every_planned_family(self) -> None:
		identifiers = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorIdentifiers.h"
		).read_text(encoding="utf-8")
		for sensor_type in (
			"Accelerometer",
			"AccelerometerUncalibrated",
			"Gyroscope",
			"GyroscopeUncalibrated",
			"Magnetometer",
			"MagnetometerUncalibrated",
			"Gravity",
			"LinearAcceleration",
			"Attitude",
			"MagneticHeading",
			"TrueHeading",
			"BarometricPressure",
			"RelativeAltitude",
			"AbsoluteAltitude",
			"AmbientLight",
			"Proximity",
			"StepCounter",
			"StepDetector",
			"Pedometer",
			"MotionActivity",
			"ActivityTransition",
			"PhysicalOrientation",
		):
			self.assertIn(sensor_type, identifiers)
		self.assertIn("FOpenMobileSensorTypes", identifiers)
		self.assertIn("GetAll", identifiers)
		self.assertIn("GetStableName", identifiers)

	def test_backend_seam_returns_portable_capabilities(self) -> None:
		backend = (
			COMMON_SOURCE / "Internal" / "IOpenMobileSensorsBackend.h"
		).read_text(encoding="utf-8")
		capabilities = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorCapabilities.h"
		).read_text(encoding="utf-8")
		self.assertIn("GetSensorCapabilities", backend)
		for field in (
			"Availability",
			"Source",
			"RequiredPermission",
			"ActiveRestriction",
			"MinimumFrequencyHz",
			"MaximumFrequencyHz",
			"bSupportsNativeBatching",
			"BackgroundSupport",
		):
			self.assertIn(field, capabilities)

	def test_service_reevaluates_all_dynamic_inputs(self) -> None:
		header = (
			COMMON_SOURCE
			/ "Internal"
			/ "OpenMobileSensorsCapabilityService.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsCapabilityService.cpp"
		).read_text(encoding="utf-8")
		for operation in (
			"HandleBackendGenerationChanged",
			"NotifyPermissionStatusChanged",
			"SetApplicationActive",
			"SetLocationInputAvailable",
			"OnChanged",
		):
			self.assertIn(operation, header)
		self.assertIn("ApplicationWillEnterBackgroundDelegate", implementation)
		self.assertIn("ApplicationHasEnteredForegroundDelegate", implementation)
		self.assertIn("CachedSnapshot", implementation)
		self.assertIn("IsMateriallyEqual", implementation)

	def test_subsystem_forwards_material_changes(self) -> None:
		subsystem = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("HandleCapabilitySnapshotChanged", subsystem)
		self.assertIn("OnCapabilitiesChanged.Broadcast", subsystem)
		self.assertIn("CapabilitiesChangedEvent.Broadcast", subsystem)

	def test_unreal_tests_cover_required_matrix_scenarios(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsCapabilityTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"FullySupported",
			"PartiallySupported",
			"PermissionBlocked",
			"DerivedOnly",
			"RateLimited",
			"MockAndReplay",
			"UnsupportedEditor",
			"MaterialChangeEvents",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
