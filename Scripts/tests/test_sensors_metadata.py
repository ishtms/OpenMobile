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


class SensorsMetadataTests(unittest.TestCase):
	def test_public_metadata_is_portable_and_presence_aware(self) -> None:
		metadata = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorMetadata.h"
		).read_text(encoding="utf-8")
		for optional_type in (
			"FOpenMobileSensorOptionalNumber",
			"FOpenMobileSensorOptionalInteger",
			"FOpenMobileSensorOptionalText",
			"FOpenMobileSensorOptionalBoolean",
		):
			self.assertIn(optional_type, metadata)
		for field in (
			"Vendor",
			"NativeName",
			"Version",
			"MaximumRange",
			"Resolution",
			"EstimatedPowerMilliwatts",
			"MinimumIntervalSeconds",
			"MaximumIntervalSeconds",
			"FifoCapacitySamples",
			"WakeUpBehavior",
			"bReportingModeAvailable",
			"ReportingMode",
			"bPreferred",
		):
			self.assertIn(field, metadata)
		self.assertNotIn("void*", metadata)
		self.assertNotIn("NativeSensorType", metadata)

	def test_backend_metadata_carries_units_and_mutability(self) -> None:
		backend_types = (
			COMMON_SOURCE
			/ "Internal"
			/ "OpenMobileSensorsBackendTypes.h"
		).read_text(encoding="utf-8")
		backend = (
			COMMON_SOURCE / "Internal" / "IOpenMobileSensorsBackend.h"
		).read_text(encoding="utf-8")
		for contract in (
			"EOpenMobileSensorMetadataUnit",
			"EOpenMobileSensorMetadataTimeUnit",
			"FOpenMobileSensorBackendMetadata",
			"NativeIdentifier",
			"bMutable",
		):
			self.assertIn(contract, backend_types)
		self.assertIn("GetSensorMetadata", backend)
		self.assertIn("RefreshMutableSensorMetadata", backend)

	def test_service_caches_normalizes_and_refreshes_metadata(self) -> None:
		header = (
			COMMON_SOURCE
			/ "Internal"
			/ "OpenMobileSensorsMetadataService.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON_SOURCE
			/ "Private"
			/ "OpenMobileSensorsMetadataService.cpp"
		).read_text(encoding="utf-8")
		for operation in (
			"GetMetadata",
			"HandleBackendGenerationChanged",
			"GetVerboseNativeMetadataForDiagnostics",
		):
			self.assertIn(operation, header)
		for behavior in (
			"NormalizeMeasurement",
			"NormalizeInterval",
			"RefreshMutableSensorMetadata",
			"bPreferred",
			"CachedBackendMetadata",
		):
			self.assertIn(behavior, implementation)

	def test_unreal_tests_cover_metadata_edge_cases(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsMetadataTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"MissingFields",
			"UnitNormalization",
			"ExtremeValues",
			"DuplicateSelection",
			"MutableRefresh",
			"BackendGenerationRefresh",
			"NativeMetadataDiagnostics",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
