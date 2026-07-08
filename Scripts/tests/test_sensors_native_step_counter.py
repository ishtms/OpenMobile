import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid" / "Private"
ANDROID_BACKEND = ANDROID / "OpenMobileSensorsAndroidBackend.cpp"
ANDROID_HEADER = ANDROID / "OpenMobileSensorsAndroidBackend.h"
ANDROID_JAVA = (
	ANDROID
	/ "Android"
	/ "src"
	/ "com"
	/ "openmobile"
	/ "sensors"
	/ "OpenMobileSensorsBridgeV1.java"
)
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS" / "Private"
IOS_BACKEND = IOS / "OpenMobileSensorsIOSBackend.mm"
IOS_HEADER = IOS / "OpenMobileSensorsIOSBackend.h"
IOS_BRIDGE = IOS / "OpenMobileSensorsIOSBridge.mm"
IOS_BRIDGE_HEADER = IOS / "OpenMobileSensorsIOSBridge.h"
SUBSYSTEM_HEADER = COMMON / "Public" / "OpenMobileSensorsSubsystem.h"
SUBSYSTEM_SOURCE = COMMON / "Private" / "OpenMobileSensorsSubsystem.cpp"
UMBRELLA = COMMON / "Public" / "OpenMobileSensors.h"


class SensorsNativeStepCounterTests(unittest.TestCase):
	def test_android_tracks_wide_since_boot_totals_and_permission(self):
		backend = ANDROID_BACKEND.read_text(encoding="utf-8")
		header = ANDROID_HEADER.read_text(encoding="utf-8")
		java = ANDROID_JAVA.read_text(encoding="utf-8")

		for token in (
			"FOpenMobileNativeStepCounterTracker",
			"NativeStepCounters",
			"TryConvertNativeTotal",
			"Tracker.Apply(Sample)",
			"EOpenMobileStepCountOrigin::DeviceBoot",
			"EOpenMobileSensorPermission::ActivityRecognition",
		):
			self.assertIn(token, backend + header)
		step_block = backend[
			backend.index(
				"if (Type == EOpenMobileSensorType::StepCounter"
			):
			backend.index(
				"if (Type == EOpenMobileSensorType::Proximity"
			)
		]
		self.assertNotIn("FMath::RoundToInt64", step_block)
		start_stream = backend[
			backend.index("FOpenMobileSensorsAndroidBackend::StartSensorStream"):
			backend.index("FOpenMobileSensorsAndroidBackend::ReconfigureSensorStream")
		]
		self.assertLess(
			start_stream.index("NativeStepCounters.Add"),
			start_stream.index("GetBridge().StartStream"),
		)
		self.assertIn("NativeStepCounters.Remove", start_stream)
		for token in (
			"Sensor.TYPE_STEP_COUNTER",
			"Manifest.permission.ACTIVITY_RECOGNITION",
			"checkSelfPermission",
		):
			self.assertIn(token, java)

	def test_apple_live_totals_expose_query_origin_and_authorization(self):
		backend = IOS_BACKEND.read_text(encoding="utf-8")
		header = IOS_HEADER.read_text(encoding="utf-8")
		bridge = IOS_BRIDGE.read_text(encoding="utf-8")
		bridge_header = IOS_BRIDGE_HEADER.read_text(encoding="utf-8")

		for token in (
			"bStepCounting",
			"EOpenMobileSensorType::StepCounter",
			"EOpenMobileSensorPermission::MotionActivity",
			"EOpenMobileCapabilityState::PermissionRequired",
			"EOpenMobileCapabilityState::Restricted",
			"EOpenMobileCapabilityState::Denied",
			"FOpenMobileNativeStepCounterTracker",
			"NativeStepCounters",
			"PublishStepsBatchFromPedometerQueue",
		):
			self.assertIn(token, backend + header + bridge_header)
		for token in (
			"CMPedometer",
			"isStepCountingAvailable",
			"authorizationStatus",
			"startPedometerUpdatesFromDate",
			"stopPedometerUpdates",
			"numberOfSteps.longLongValue",
			"EOpenMobileStepCountOrigin::QueryInterval",
			"bHasQueryInterval = true",
			"QueryStartUnixTimeSeconds",
			"QueryEndUnixTimeSeconds",
		):
			self.assertIn(token, bridge)
		self.assertNotIn("SensorKit", backend + bridge)

	def test_historical_query_is_available_to_cpp_and_blueprints(self):
		subsystem_header = SUBSYSTEM_HEADER.read_text(encoding="utf-8")
		subsystem_source = SUBSYSTEM_SOURCE.read_text(encoding="utf-8")
		umbrella = UMBRELLA.read_text(encoding="utf-8")
		async_header = (
			COMMON / "Public" / "OpenMobileNativeStepCountAsyncAction.h"
		)
		async_source = (
			COMMON / "Private" / "OpenMobileNativeStepCountAsyncAction.cpp"
		)

		self.assertTrue(async_header.is_file())
		self.assertTrue(async_source.is_file())
		for token in (
			"QueryNativeStepCountNative",
			"CancelNativeStepCountQueryNative",
			"FOnOpenMobileNativeStepCountQueryComplete",
			"FOpenMobileNativeStepQueryService::Query",
			"FOpenMobileNativeStepQueryService::CancelOwner",
		):
			self.assertIn(token, subsystem_header + subsystem_source)
		for token in (
			"UOpenMobileNativeStepCountAsyncAction",
			"Query Native Step Count",
			"Completed",
			"Cancelled",
			"Failed",
			"CancelNativeStepCountQueryNative",
		):
			self.assertIn(
				token,
				async_header.read_text(encoding="utf-8")
				+ async_source.read_text(encoding="utf-8"),
			)
		self.assertIn("OpenMobileNativeStepCount.h", umbrella)
		self.assertIn("OpenMobileNativeStepCountAsyncAction.h", umbrella)

	def test_apple_historical_queries_use_native_range_and_cancel_safely(self):
		backend = IOS_BACKEND.read_text(encoding="utf-8")
		header = IOS_HEADER.read_text(encoding="utf-8")
		bridge = IOS_BRIDGE.read_text(encoding="utf-8")
		bridge_header = IOS_BRIDGE_HEADER.read_text(encoding="utf-8")

		for token in (
			"QueryNativeStepCount",
			"CancelNativeStepCountQuery",
			"FOnOpenMobileNativeStepCountBackendQueryComplete",
		):
			self.assertIn(token, backend + header + bridge_header)
		for token in (
			"queryPedometerDataFromDate",
			"PendingStepQueries",
			"HandleHistoricalStepQuery",
			"CancelNativeStepCountQuery",
			"Data.startDate.timeIntervalSince1970",
			"Data.endDate.timeIntervalSince1970",
			"Data.numberOfSteps.longLongValue",
		):
			self.assertIn(token, bridge)


if __name__ == "__main__":
	unittest.main()
