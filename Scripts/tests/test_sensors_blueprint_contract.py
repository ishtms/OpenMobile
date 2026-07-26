import json
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SENSORS_PLUGIN = REPOSITORY_ROOT / "Native" / "OpenMobileSensors"
COMMON_SOURCE = SENSORS_PLUGIN / "Source" / "OpenMobileSensors"


class SensorsBlueprintContractTests(unittest.TestCase):
	def test_subsystem_exposes_long_lived_blueprint_operations(self) -> None:
		header = (COMMON_SOURCE / "Public" / "OpenMobileSensorsSubsystem.h").read_text(
			encoding="utf-8"
		)
		for operation in (
			"GetCapabilitySnapshotNative",
			"GetMetadataNative",
			"StartSubscriptionNative",
			"UpdateSubscriptionNative",
			"StopSubscriptionNative",
			"GetLatestVectorSampleNative",
			"GetBufferedVectorSamplesNative",
			"RecenterSubscription",
			"GetPermissionStatusNative",
			"GetDiagnosticsSnapshotNative",
		):
			self.assertIn(operation, header)
		self.assertGreaterEqual(header.count("UFUNCTION("), 25)
		self.assertIn('Category = "Open Mobile|Sensors"', header)
		self.assertIn("BlueprintAssignable", header)

	def test_terminal_operations_use_async_action_classes(self) -> None:
		for class_name, header_name in (
			("UOpenMobileSensorPermissionAsyncAction", "OpenMobileSensorPermissionAsyncAction.h"),
			("UOpenMobileSensorFlushAsyncAction", "OpenMobileSensorFlushAsyncAction.h"),
			("UOpenMobileSensorRecordingAsyncAction", "OpenMobileSensorRecordingAsyncAction.h"),
			("UOpenMobileSensorReplayAsyncAction", "OpenMobileSensorReplayAsyncAction.h"),
		):
			header = (COMMON_SOURCE / "Public" / header_name).read_text(
				encoding="utf-8"
			)
			self.assertIn(class_name, header)
			self.assertIn("UOpenMobileSensorAsyncActionBase", header)
			self.assertIn("BlueprintInternalUseOnly", header)
			self.assertIn('Category = "Open Mobile|Sensors"', header)

	def test_runtime_defaults_bound_blueprint_work(self) -> None:
		options = (
			COMMON_SOURCE / "Public" / "OpenMobileSensorStreamOptions.h"
		).read_text(encoding="utf-8")
		self.assertIn("EOpenMobileSensorRatePreset::UI", options)
		self.assertIn("MaximumCallbackFrequencyHz = 15.0", options)
		self.assertIn("CustomFrequencyHz = 15.0", options)
		self.assertIn("BufferCapacitySamples = 128", options)

		documentation = (
			SENSORS_PLUGIN / "Docs" / "PublicContract.md"
		).read_text(encoding="utf-8")
		self.assertIn("Blueprint defaults", documentation)
		self.assertIn("latest-value polling", documentation)
		self.assertIn("callback cap", documentation)

	def test_editor_module_validates_reflection_and_representative_nodes(self) -> None:
		descriptor = json.loads(
			(SENSORS_PLUGIN / "OpenMobileSensors.uplugin").read_text(encoding="utf-8")
		)
		modules = {module["Name"]: module for module in descriptor["Modules"]}
		self.assertEqual("Editor", modules["OpenMobileSensorsEditor"]["Type"])

		test_source = (
			SENSORS_PLUGIN
			/ "Source"
			/ "OpenMobileSensorsEditor"
			/ "Private"
			/ "Tests"
			/ "OpenMobileSensorsBlueprintContractTests.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"TFieldIterator<UFunction>",
			"BlueprintCallable",
			"Open Mobile|Sensors",
			"UK2Node_CallFunction",
			"AllocateDefaultPins",
			"StartSubscriptionNative",
			"GetLatestVectorSampleNative",
		):
			self.assertIn(token, test_source)

	def test_runtime_module_has_no_editor_dependencies(self) -> None:
		build_rules = (COMMON_SOURCE / "OpenMobileSensors.Build.cs").read_text(
			encoding="utf-8"
		)
		for editor_module in ("BlueprintGraph", "Kismet", "UnrealEd"):
			self.assertNotIn(f'"{editor_module}"', build_rules)

	def test_raw_sample_events_state_their_delivery_requirement(self) -> None:
		header = (COMMON_SOURCE / "Public" / "OpenMobileSensorsSubsystem.h").read_text(
			encoding="utf-8"
		)
		for event_name in (
			"OnVectorSamples",
			"OnAttitudeSamples",
			"OnScalarSamples",
			"OnHeadingSamples",
			"OnStepsSamples",
			"OnActivitySamples",
			"OnOrientationSamples",
			"OnProximitySamples",
		):
			event_offset = header.index(event_name)
			property_start = header.rfind("\tUPROPERTY", 0, event_offset)
			declaration = header[property_start : event_offset + len(event_name)]
			self.assertIn(
				"Requires Delivery Mode = Event Batches",
				declaration,
				msg=f"{event_name} must explain why a default raw stream is silent",
			)


if __name__ == "__main__":
	unittest.main()
