import json
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SENSORS_PLUGIN = REPOSITORY_ROOT / "Native" / "OpenMobileSensors"
PUBLIC_HEADERS = (
	SENSORS_PLUGIN / "Source" / "OpenMobileSensors" / "Public"
)


class SensorsPublicContractTests(unittest.TestCase):
	def test_public_contract_is_split_by_stable_responsibility(self) -> None:
		for header in (
			"OpenMobileSensorIdentifiers.h",
			"OpenMobileSensorCapabilities.h",
			"OpenMobileSensorMetadata.h",
			"OpenMobileSensorStreamOptions.h",
			"OpenMobileSensorSubscription.h",
			"OpenMobileSensorSamples.h",
			"OpenMobileSensorPermissions.h",
			"OpenMobileSensorDiagnostics.h",
			"OpenMobileSensorErrors.h",
			"OpenMobileSensorRecording.h",
			"OpenMobileSensorResults.h",
			"OpenMobileSensorsSubsystem.h",
			"OpenMobileSensors.h",
		):
			self.assertTrue((PUBLIC_HEADERS / header).is_file(), header)

	def test_sample_families_are_strongly_typed(self) -> None:
		samples = (PUBLIC_HEADERS / "OpenMobileSensorSamples.h").read_text(
			encoding="utf-8"
		)
		for sample_type in (
			"FOpenMobileVectorSensorSample",
			"FOpenMobileAttitudeSensorSample",
			"FOpenMobileScalarSensorSample",
			"FOpenMobileHeadingSensorSample",
			"FOpenMobileStepsSensorSample",
			"FOpenMobileActivitySensorSample",
			"FOpenMobileOrientationSensorSample",
			"FOpenMobileProximitySensorSample",
		):
			self.assertIn(sample_type, samples)
		self.assertNotIn("TArray<uint8>", samples)
		self.assertNotIn("void*", samples)

	def test_subsystem_has_direct_native_operations(self) -> None:
		subsystem = (PUBLIC_HEADERS / "OpenMobileSensorsSubsystem.h").read_text(
			encoding="utf-8"
		)
		for operation in (
			"GetCapabilitySnapshotNative",
			"GetMetadataNative",
			"StartSubscriptionNative",
			"UpdateSubscriptionNative",
			"StopSubscriptionNative",
			"StopAllSubscriptionsNative",
			"GetLatestVectorSampleNative",
			"GetBufferedVectorSamplesNative",
			"FlushNative",
			"RecenterNative",
			"GetPermissionStatusNative",
			"RequestPermissionNative",
			"GetDiagnosticsSnapshotNative",
		):
			self.assertIn(operation, subsystem)
		for delegate in (
			"FOnOpenMobileSensorSubscriptionStateChanged",
			"FOnOpenMobileVectorSensorBatch",
			"FOnOpenMobileAttitudeSensorBatch",
			"FOnOpenMobileScalarSensorBatch",
		):
			self.assertIn(delegate, subsystem)

	def test_consumer_module_uses_only_public_headers(self) -> None:
		descriptor = json.loads(
			(SENSORS_PLUGIN / "OpenMobileSensors.uplugin").read_text(encoding="utf-8")
		)
		modules = {module["Name"]: module for module in descriptor["Modules"]}
		self.assertEqual(
			"DeveloperTool",
			modules["OpenMobileSensorsConsumerTests"]["Type"],
		)

		consumer = (
			SENSORS_PLUGIN
			/ "Source"
			/ "OpenMobileSensorsConsumerTests"
			/ "Private"
			/ "OpenMobileSensorsConsumerTestsModule.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('#include "OpenMobileSensors.h"', consumer)
		self.assertNotIn("/Internal/", consumer)
		self.assertNotIn("/Private/", consumer)
		self.assertNotIn("OpenMobileSensorsBackendRegistry", consumer)
		self.assertNotIn("IOpenMobileSensorsBackend", consumer)

	def test_contract_documentation_covers_runtime_invariants(self) -> None:
		documentation = (
			SENSORS_PLUGIN / "Docs" / "PublicContract.md"
		).read_text(encoding="utf-8")
		for invariant in (
			"Game Instance ownership",
			"Game-thread affinity",
			"Sample lifetime",
			"Coordinate spaces",
			"Standard units",
			"Monotonic timestamps",
			"Callback order",
			"Buffer overflow",
			"Handle invalidation",
		):
			self.assertIn(invariant, documentation)


if __name__ == "__main__":
	unittest.main()
