import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS_ROOT = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS_ROOT / "Source" / "OpenMobileSensors"
ANDROID = SENSORS_ROOT / "Source" / "OpenMobileSensorsAndroid"
PRIVATE = ANDROID / "Private"
JAVA = (
	PRIVATE
	/ "Android"
	/ "src"
	/ "com"
	/ "openmobile"
	/ "sensors"
	/ "OpenMobileSensorsBridgeV1.java"
)


class SensorsAndroidBackendTests(unittest.TestCase):
	def test_versioned_bridge_is_packaged_from_the_android_module(self):
		self.assertTrue(JAVA.is_file())
		bridge = JAVA.read_text(encoding="utf-8")
		upl = (
			PRIVATE / "Android" / "OpenMobileSensors_Android_UPL.xml"
		).read_text(encoding="utf-8")

		self.assertIn("package com.openmobile.sensors;", bridge)
		self.assertIn("final class OpenMobileSensorsBridgeV1", bridge)
		self.assertIn("BRIDGE_VERSION = 1", bridge)
		self.assertIn("<resourceCopies>", upl)
		self.assertIn("OpenMobileSensorsBridgeV1.java", upl)

	def test_bridge_owns_sensor_manager_discovery_and_metadata(self):
		bridge = JAVA.read_text(encoding="utf-8")

		for token in (
			"SensorManager",
			"HandlerThread",
			"OpenMobileSensorsHandler",
			"SensorEventListener2",
			"getSensorList(Sensor.TYPE_ALL)",
			"getMinDelay()",
			"getMaxDelay()",
			"getFifoMaxEventCount()",
			"getReportingMode()",
			"isWakeUpSensor()",
			"getMaximumRange()",
			"getResolution()",
			"getPower()",
		):
			self.assertIn(token, bridge)

	def test_bridge_batches_bounded_primitive_payloads(self):
		bridge = JAVA.read_text(encoding="utf-8")

		self.assertIn("MAX_BATCH_SAMPLES = 64", bridge)
		self.assertIn("MAX_VALUES_PER_SAMPLE = 6", bridge)
		self.assertIn("long[] timestamps", bridge)
		self.assertIn("float[] values", bridge)
		self.assertIn("handler.postDelayed", bridge)
		self.assertIn("nativeOnSampleBatch", bridge)
		self.assertNotIn("nativeOnSensorEvent", bridge)

	def test_shared_physical_stream_operations_use_the_handler(self):
		bridge = JAVA.read_text(encoding="utf-8")

		for token in (
			"startStream(",
			"reconfigureStream(",
			"stopStream(",
			"flushStream(",
			"registerListener(",
			"unregisterListener(",
			"sensorManager.flush(",
			"maxReportLatencyUs",
			"handler",
		):
			self.assertIn(token, bridge)
		unregister = bridge[
			bridge.index("void unregister(boolean discardPendingBatch)"):
			bridge.index("void failPendingFlushes", bridge.index("void unregister"))
		]
		self.assertIn("try {", unregister)
		self.assertIn("catch (RuntimeException exception)", unregister)
		self.assertIn("finally {", unregister)
		shutdown = bridge[
			bridge.index("public void shutdown()"):
			bridge.index("private static OpenMobileSensorsBridgeV1 getActiveBridge")
		]
		self.assertIn("unregisterDynamicSensorCallback", shutdown)
		self.assertIn("catch (RuntimeException exception)", shutdown)
		native_bridge = (
			PRIVATE / "OpenMobileSensorsAndroidBridge.cpp"
		).read_text(encoding="utf-8")
		backend = (
			PRIVATE / "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("GetActiveSensorDescriptor", native_bridge)
		self.assertIn("GetActiveSensorDescriptor", backend)

	def test_lifecycle_permission_disconnect_and_generation_are_guarded(self):
		bridge = JAVA.read_text(encoding="utf-8")
		upl = (
			PRIVATE / "Android" / "OpenMobileSensors_Android_UPL.xml"
		).read_text(encoding="utf-8")

		for token in (
			"onActivityResumed",
			"onActivityPaused",
			"onActivityDestroyed",
			"onPermissionsChanged",
			"DynamicSensorCallback",
			"registerDynamicSensorCallback",
			"unregisterDynamicSensorCallback",
			"nativeOnSensorDisconnected",
			"backendGeneration",
			"failPendingFlushes(RESULT_PAUSED)",
		):
			self.assertIn(token, bridge)
		for node in (
			"gameActivityOnResumeAdditions",
			"gameActivityOnPauseAdditions",
			"gameActivityOnDestroyAdditions",
			"gameActivityOnRequestPermissionsResultAdditions",
		):
			self.assertIn(node, upl)

	def test_native_bridge_releases_jni_references_and_maps_failures(self):
		header = (
			PRIVATE / "OpenMobileSensorsAndroidBridge.h"
		).read_text(encoding="utf-8")
		source = (
			PRIVATE / "OpenMobileSensorsAndroidBridge.cpp"
		).read_text(encoding="utf-8")

		for token in (
			"BridgeClassMissing",
			"BridgeMethodMissing",
			"ActivityUnavailable",
			"SensorMissing",
			"PermissionDenied",
			"RegisterFailed",
			"FlushFailed",
		):
			self.assertIn(token, header + source)
		for token in (
			"FindJavaClassGlobalRef",
			"GetMethodID",
			"DeleteGlobalRef",
			"CallVoidMethod(LocalBridgeObject, LocalShutdownMethod)",
			"FScopedJavaObject<jstring>",
			"GetLongArrayRegion",
			"GetFloatArrayRegion",
			"ExceptionCheck",
			"if (IntegerCount > 0)",
			"if (NumberCount > 0)",
		):
			self.assertIn(token, source)

	def test_backend_implements_discovery_and_streaming_contract(self):
		header = (
			PRIVATE / "OpenMobileSensorsAndroidBackend.h"
		).read_text(encoding="utf-8")
		source = (
			PRIVATE / "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")

		for token in (
			"GetSensorCapabilities() const override",
			"GetSensorMetadata() const override",
			"StartSensorStream(",
			"ReconfigureSensorStream(",
			"StopSensorStream(",
			"FlushSensorStream(",
			"FOpenMobileSensorsAndroidBridge",
		):
			self.assertIn(token, header + source)
		self.assertIn("FOpenMobileSensorsErrorMapper::Map", source)
		self.assertIn("|| ValueStride > 6", source)
		count_read = source.index("const double Count = ValueAt(Index, 0);")
		finite_check = source.index("FMath::IsFinite(Count)", count_read)
		count_round = source.index("FMath::RoundToInt64(Count)", count_read)
		self.assertLess(finite_check, count_round)

	def test_attitude_frames_report_native_selection_and_dependencies(self):
		source = (
			PRIVATE / "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")

		for token in (
			"AttitudeReferenceFrames",
			"AppliedReferenceFrame",
			"bFallbackApplied",
			"bHeadingDependent",
			"bLocationDependent",
			"bCalibrationRequired",
			"bExpectedToDrift",
			"NativeType == 15",
			"NativeType == 11",
			"NativeType == 20",
			"TrueNorthUnavailable",
		):
			self.assertIn(token, source)

	def test_magnetic_heading_uses_magnetic_rotation_vectors(self):
		backend = (
			PRIVATE / "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		bridge = (
			PRIVATE / "OpenMobileSensorsAndroidBridge.cpp"
		).read_text(encoding="utf-8")

		type_42 = bridge[
			bridge.index("case 42:"):
			bridge.index("case 6:", bridge.index("case 42:"))
		]
		self.assertIn("EOpenMobileSensorType::TrueHeading", type_42)
		for token in (
			"AddMagneticHeadingDescriptors",
			"FOpenMobileSensorHeading::FromAndroidRotationVector",
			"EOpenMobileHeadingReference::MagneticNorth",
			"bTiltCompensated = true",
			"FMath::RadiansToDegrees",
		):
			self.assertIn(token, backend)

	def test_packaging_is_permission_scoped_and_has_no_unrelated_sdk(self):
		upl = (
			PRIVATE / "Android" / "OpenMobileSensors_Android_UPL.xml"
		).read_text(encoding="utf-8")
		module_contents = "\n".join(
			path.read_text(encoding="utf-8")
			for path in ANDROID.rglob("*")
			if path.is_file() and path.suffix in {".cs", ".cpp", ".h", ".java", ".xml"}
		)

		self.assertIn("bEnablePermissionSensitiveSensors", upl)
		self.assertIn("android.permission.ACTIVITY_RECOGNITION", upl)
		for forbidden in (
			"play-services",
			"ActivityRecognitionClient",
			"GoogleMobileAds",
			"com.facebook",
		):
			self.assertNotIn(forbidden, module_contents)


if __name__ == "__main__":
	unittest.main()
