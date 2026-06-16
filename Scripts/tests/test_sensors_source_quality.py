import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS_ROOT = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS_ROOT / "Source" / "OpenMobileSensors"
ANDROID = SENSORS_ROOT / "Source" / "OpenMobileSensorsAndroid"
IOS = SENSORS_ROOT / "Source" / "OpenMobileSensorsIOS"


class SensorsSourceQualityTests(unittest.TestCase):
	def test_public_contract_exposes_source_and_fusion_context(self):
		quality = (
			COMMON / "Public" / "OpenMobileSensorQuality.h"
		).read_text(encoding="utf-8")
		samples = (
			COMMON / "Public" / "OpenMobileSensorSamples.h"
		).read_text(encoding="utf-8")

		for source in (
			"Raw",
			"CalibratedNative",
			"NativeFused",
			"PluginDerived",
			"MagneticNorthReferenced",
			"TrueNorthReferenced",
			"Mock",
			"Replay",
		):
			self.assertIn(source, quality)
		for field in (
			"Quality",
			"bHasNativeQualityReport",
			"NativeQuality",
			"ExpectedInputMask",
			"ContributingInputMask",
			"MissingInputMask",
			"DegradedInputMask",
		):
			self.assertIn(field, quality)
		self.assertIn("UOpenMobileSensorQualityLibrary", quality)
		self.assertIn("OpenMobileSensorQuality.h", samples)
		self.assertIn("bSourceChanged", samples)
		self.assertIn("Fusion", samples)

	def test_common_policy_validates_sources_and_observable_fusion_inputs(self):
		source_policy = (
			COMMON / "Internal" / "OpenMobileSensorSourcePolicy.h"
		).read_text(encoding="utf-8")
		fusion = (
			COMMON / "Internal" / "OpenMobileSensorFusionQuality.h"
		).read_text(encoding="utf-8")

		for contract in (
			"ValidateSourceFlags",
			"GetAndroidNativeSourceFlags",
			"GetIOSNativeSourceFlags",
			"MarkReplayed",
		):
			self.assertIn(contract, source_policy)
		for contract in (
			"FOpenMobileSensorFusionInputObservation",
			"Evaluate",
			"bCalibrationRequired",
			"Accuracy",
		):
			self.assertIn(contract, fusion)

	def test_transport_tracks_source_changes_and_recording_provenance(self):
		service = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		codec = (
			COMMON / "Internal" / "OpenMobileSensorProvenanceCodec.h"
		).read_text(encoding="utf-8")

		for behavior in (
			"bHasSourceState",
			"LastSourceFlags",
			"bSourceChanged",
			"ValidateSourceFlags",
		):
			self.assertIn(behavior, service)
		self.assertIn("Encode", codec)
		self.assertIn("Decode", codec)
		self.assertIn("MarkReplayed", codec)

	def test_platform_vector_ingress_assigns_platform_specific_sources(self):
		android = (
			ANDROID / "Private" / "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		ios = (
			IOS / "Private" / "OpenMobileSensorsIOSBackend.mm"
		).read_text(encoding="utf-8")

		self.assertIn("GetAndroidNativeSourceFlags", android)
		self.assertIn("GetIOSNativeSourceFlags", ios)
		for source in (android, ios):
			self.assertLess(source.index("SourceFlags"), source.index("PublishVectorBatchFromBackend"))

	def test_native_tests_cover_fallback_mixed_quality_and_round_trips(self):
		tests = (
			COMMON
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsSourceQualityTests.cpp"
		).read_text(encoding="utf-8")

		for scenario in (
			"PlatformSourceMappings",
			"MixedQualityInputs",
			"NativeToDerivedFallback",
			"TransportPreservation",
			"RecordingReplayRoundTrip",
		):
			self.assertIn(scenario, tests)


if __name__ == "__main__":
	unittest.main()
