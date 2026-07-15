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


class SensorsErrorModelTests(unittest.TestCase):
	def test_public_failure_reasons_cover_sensor_contract(self) -> None:
		header = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorErrors.h"
		).read_text(encoding="utf-8")
		for reason in (
			"UnsupportedPlatform",
			"MissingHardware",
			"DerivedInputUnavailable",
			"PermissionRequired",
			"PermissionDenied",
			"PermissionRestricted",
			"RateLimited",
			"InvalidFrequency",
			"InvalidReferenceFrame",
			"PoorCalibration",
			"BackgroundRestricted",
			"BufferOverflow",
			"MissingLocationInput",
			"StaleLocationInput",
			"PoorLocationAccuracy",
			"TemporarilyUnavailable",
			"ConfigurationBlocked",
			"OperationalFailure",
		):
			self.assertIn(reason, header)

	def test_mapper_sanitizes_native_details_and_redacts_shipping_logs(self) -> None:
		header = (
			COMMON_SOURCE / "Internal" / "OpenMobileSensorsErrorMapper.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsErrorMapper.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FOpenMobileSensorsErrorMapper", header)
		self.assertIn("SanitizeNativeIdentifier", implementation)
		self.assertIn("FormatForLog", implementation)
		self.assertIn("bShipping", implementation)
		self.assertIn('TEXT("redacted")', implementation)

	def test_capability_and_latest_queries_remain_side_effect_free(self) -> None:
		subsystem = (
			COMMON_SOURCE / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
		capability_body = subsystem.split(
			"UOpenMobileSensorsSubsystem::GetCapabilitySnapshotNative() const",
			1,
		)[1].split(
			"UOpenMobileSensorsSubsystem::GetMetadataNative() const",
			1,
		)[0]
		for side_effect in (
			"RequestPermission",
			"StartSubscription",
			"Initialize",
			"LoadModule",
			"RegisterBackend",
		):
			self.assertNotIn(side_effect, capability_body)
		latest_body = subsystem.split(
			"#define OPENMOBILE_IMPLEMENT_LATEST_SAMPLE",
			1,
		)[1].split("#undef OPENMOBILE_IMPLEMENT_LATEST_SAMPLE", 1)[0]
		self.assertIn("FOpenMobileSensorsSampleService::ServiceMethod", latest_body)
		for side_effect in (
			"RequestPermission",
			"StartSubscription",
			"Initialize",
			"LoadModule",
			"RegisterBackend",
		):
			self.assertNotIn(side_effect, latest_body)

	def test_unreal_tests_cover_mapping_and_query_contracts(self) -> None:
		tests = (
			COMMON_SOURCE
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsErrorTests.cpp"
		).read_text(encoding="utf-8")
		for scenario in (
			"NormalizedMapping",
			"UnknownFutureReason",
			"MissingNativeDetails",
			"ShippingRedaction",
			"SideEffectFreeQueries",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
