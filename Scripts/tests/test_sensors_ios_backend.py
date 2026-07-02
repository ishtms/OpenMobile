import pathlib
import plistlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS"
PRIVATE = IOS / "Private"
BRIDGE = PRIVATE / "OpenMobileSensorsIOSBridge.mm"
UPL = PRIVATE / "IOS" / "OpenMobileSensors_IOS_UPL.xml"
PRIVACY = (
	SENSORS
	/ "Resources"
	/ "IOS"
	/ "OpenMobileSensorsPrivacy.bundle"
	/ "PrivacyInfo.xcprivacy"
)


class SensorsIOSBackendTests(unittest.TestCase):
	def test_module_owns_apple_frameworks_and_packaging(self):
		rules = (IOS / "OpenMobileSensorsIOS.Build.cs").read_text(
			encoding="utf-8"
		)

		for token in (
			'bEnableObjCAutomaticReferenceCounting = true',
			'"CoreMotion"',
			'"Foundation"',
			'"UIKit"',
			'"IOSPlugin"',
			"AdditionalBundleResources",
			"OpenMobileSensorsPrivacy.bundle",
		):
			self.assertIn(token, rules)
		for forbidden in ("CoreLocation", "Google", "AdSupport"):
			self.assertNotIn(forbidden, rules)

	def test_plist_always_contains_motion_usage_text(self):
		self.assertTrue(UPL.is_file())
		upl = UPL.read_text(encoding="utf-8")

		for token in (
			"iosPListUpdates",
			"IOSMotionUsageDescription",
			"NSMotionUsageDescription",
			"setStringFromProperty",
		):
			self.assertIn(token, upl)
		self.assertNotIn("NSLocation", upl)

	def test_privacy_manifest_declares_no_collection_or_tracking(self):
		self.assertTrue(PRIVACY.is_file())
		with PRIVACY.open("rb") as stream:
			manifest = plistlib.load(stream)

		self.assertFalse(manifest["NSPrivacyTracking"])
		self.assertEqual(manifest["NSPrivacyCollectedDataTypes"], [])
		self.assertEqual(manifest["NSPrivacyAccessedAPITypes"], [])

	def test_backend_implements_discovery_metadata_and_stream_operations(self):
		header = (PRIVATE / "OpenMobileSensorsIOSBackend.h").read_text(
			encoding="utf-8"
		)
		source = (PRIVATE / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)

		for token in (
			"GetSensorCapabilities() const override",
			"GetSensorMetadata() const override",
			"StartSensorStream(",
			"ReconfigureSensorStream(",
			"StopSensorStream(",
			"FlushSensorStream(",
			"FOpenMobileSensorsIOSBridge",
			"FOpenMobileSensorsErrorMapper::Map",
		):
			self.assertIn(token, header + source)

	def test_bridge_uses_one_manager_and_named_serial_queues(self):
		self.assertTrue(BRIDGE.is_file())
		bridge = BRIDGE.read_text(encoding="utf-8")

		self.assertEqual(bridge.count("[[CMMotionManager alloc] init]"), 2)
		for token in (
			"OpenMobileSensorsAccelerometerQueue",
			"OpenMobileSensorsGyroscopeQueue",
			"OpenMobileSensorsMagnetometerQueue",
			"OpenMobileSensorsDeviceMotionQueue",
			"OpenMobileSensorsAltimeterQueue",
			"maxConcurrentOperationCount = 1",
		):
			self.assertIn(token, bridge)

	def test_availability_guards_and_unsupported_metadata_are_explicit(self):
		bridge = BRIDGE.read_text(encoding="utf-8")
		backend = (PRIVATE / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)

		for token in (
			"isAccelerometerAvailable",
			"isGyroAvailable",
			"isMagnetometerAvailable",
			"isDeviceMotionAvailable",
			"isRelativeAltitudeAvailable",
			"isAbsoluteAltitudeAvailable",
			"availableAttitudeReferenceFrames",
			"@available(iOS",
		):
			self.assertIn(token, bridge)
		for token in (
			"bReportingModeAvailable = false",
			"EOpenMobileSensorReportingMode::Unknown",
			"bSupportsNativeBatching = false",
		):
			self.assertIn(token, backend)

	def test_streams_apply_intervals_and_emit_bounded_common_batches(self):
		bridge = BRIDGE.read_text(encoding="utf-8")

		for token in (
			"MaximumCallbackBatchSamples = 1",
			"accelerometerUpdateInterval",
			"gyroUpdateInterval",
			"magnetometerUpdateInterval",
			"deviceMotionUpdateInterval",
			"startAccelerometerUpdatesToQueue",
			"startGyroUpdatesToQueue",
			"startMagnetometerUpdatesToQueue",
			"startDeviceMotionUpdatesUsingReferenceFrame",
			"stopAccelerometerUpdates",
			"stopGyroUpdates",
			"stopMagnetometerUpdates",
			"stopDeviceMotionUpdates",
		):
			self.assertIn(token, bridge)

	def test_attitude_frames_report_native_selection_and_dependencies(self):
		backend = (PRIVATE / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		bridge = BRIDGE.read_text(encoding="utf-8")

		for token in (
			"AttitudeReferenceFrames",
			"AppliedReferenceFrame",
			"bFallbackApplied",
			"bHeadingDependent",
			"bLocationDependent",
			"bCalibrationRequired",
			"bExpectedToDrift",
		):
			self.assertIn(token, backend)
		self.assertIn("CMAttitudeReferenceFrameXArbitraryZVertical", bridge)
		self.assertIn("CMAttitudeReferenceFrameXMagneticNorthZVertical", bridge)
		self.assertIn("ReferenceFrameUnavailable", bridge)

	def test_calibrated_magnetic_quality_precedes_its_sample(self):
		bridge = BRIDGE.read_text(encoding="utf-8")
		device_motion = bridge[
			bridge.index("void HandleDeviceMotion("):
			bridge.index("void HandleRelativeAltitude(")
		]

		self.assertLess(
			device_motion.index(
				"PublishMagneticFieldAccuracyFromMotionQueue"
			),
			device_motion.index("PublishVectorBatchFromMotionQueue"),
		)

	def test_magnetic_heading_declares_reference_tilt_and_quality(self):
		bridge = BRIDGE.read_text(encoding="utf-8")
		device_motion = bridge[
			bridge.index("void HandleDeviceMotion("):
			bridge.index("void HandleRelativeAltitude(")
		]
		heading = device_motion[
			device_motion.index(
				"Type == EOpenMobileSensorType::MagneticHeading"
			):
		]

		self.assertIn(
			"EOpenMobileHeadingReference::MagneticNorth",
			heading,
		)
		self.assertIn("bTiltCompensated = true", heading)
		self.assertLess(
			heading.index("PublishMagneticFieldAccuracyFromMotionQueue"),
			heading.index("PublishHeadingBatchFromMotionQueue"),
		)

	def test_lifecycle_errors_permissions_and_late_blocks_are_guarded(self):
		bridge = BRIDGE.read_text(encoding="utf-8")

		for token in (
			"UIApplicationWillResignActiveNotification",
			"UIApplicationDidBecomeActiveNotification",
			"RegistrationGeneration",
			"FOpenMobileSensorsBackendRegistry::IsTokenCurrent",
			"CMErrorNotAuthorized",
			"CMErrorMotionActivityNotAuthorized",
			"NSMotionUsageDescription",
			"FailPhysicalStreamFromBackend",
			"removeObserver",
			"waitUntilAllOperationsAreFinished",
		):
			self.assertIn(token, bridge)


if __name__ == "__main__":
	unittest.main()
