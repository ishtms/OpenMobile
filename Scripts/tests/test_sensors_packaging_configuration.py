import configparser
import pathlib
import plistlib
import re
import tempfile
import unittest

from Scripts.inspect_sensors_package import (
	InspectionError,
	inspect_android_manifest,
	inspect_ios_plist,
	inspect_privacy_manifest,
	inspect_store_validation,
)


ROOT = pathlib.Path(__file__).resolve().parents[2]
PLUGIN = ROOT / "Native" / "OpenMobileSensors"
COMMON = PLUGIN / "Source" / "OpenMobileSensors"
ANDROID = PLUGIN / "Source" / "OpenMobileSensorsAndroid"
IOS = PLUGIN / "Source" / "OpenMobileSensorsIOS"
FIXTURES = PLUGIN / "Tests" / "Packaging"
SECTION = "/Script/OpenMobileSensors.OpenMobileSensorsSettings"
IOS_SECTION = "/Script/IOSRuntimeSettings.IOSRuntimeSettings"


class SensorsPackagingConfigurationTests(unittest.TestCase):
	def test_enabled_and_disabled_fixtures_exercise_canonical_settings(self):
		for name, expected in (
			("Enabled", ("true", "true")),
			("Disabled", ("false", "false")),
		):
			config = configparser.ConfigParser()
			config.optionxform = str
			fixture = FIXTURES / name / "DefaultEngine.ini"
			self.assertTrue(fixture.is_file())
			config.read(fixture, encoding="utf-8")
			self.assertEqual(
				config[SECTION]["bEnablePermissionSensitiveSensors"].lower(),
				expected[0],
			)
			self.assertEqual(
				config[SECTION]["bAllowHighSamplingRate"].lower(),
				expected[1],
			)
			motion_usage = config[SECTION]["IOSMotionUsageDescription"]
			fallback = config[IOS_SECTION]["AdditionalPlistData"]
			match = re.fullmatch(
				r"<key>NSMotionUsageDescription</key><string>(.*)</string>",
				fallback,
			)
			self.assertIsNotNone(match)
			self.assertEqual(match.group(1), motion_usage)

	def test_invalid_fixtures_cover_prepackage_failures(self):
		for name, token in (
			("InvalidDuplicateAndroid", "ExtraPermissions"),
			("InvalidShippingMock", "DevelopmentInputMode=Mock"),
			("InvalidIOSUsage", "IOSMotionUsageDescription="),
			("InvalidIOSMismatch", "This text does not match"),
		):
			fixture = FIXTURES / name / "DefaultEngine.ini"
			self.assertTrue(fixture.is_file())
			self.assertIn(token, fixture.read_text(encoding="utf-8"))

	def test_android_upl_uses_only_the_canonical_sensor_policy(self):
		upl = (
			ANDROID
			/ "Private"
			/ "Android"
			/ "OpenMobileSensors_Android_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertIn(f'section="{SECTION}"', upl)
		self.assertEqual(upl.count("bEnablePermissionSensitiveSensors"), 1)
		self.assertEqual(upl.count("bAllowHighSamplingRate"), 1)
		self.assertEqual(
			upl.count("android.permission.ACTIVITY_RECOGNITION"),
			1,
		)
		self.assertEqual(
			upl.count("android.permission.HIGH_SAMPLING_RATE_SENSORS"),
			1,
		)

	def test_ios_key_is_owned_by_the_ios_module_build_path(self):
		rules = (IOS / "OpenMobileSensorsIOS.Build.cs").read_text(
			encoding="utf-8"
		)
		upl = (
			IOS / "Private" / "IOS" / "OpenMobileSensors_IOS_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertIn('"IOSPlugin"', rules)
		self.assertIn("IOSMotionUsageDescription", upl)
		self.assertIn("NSMotionUsageDescription", upl)
		for unrelated in (
			"NSLocation",
			"NSFallDetectionUsageDescription",
			"UIBackgroundModes",
		):
			self.assertNotIn(unrelated, upl)

	def test_prepackage_build_validation_covers_required_failures(self):
		rules = (COMMON / "OpenMobileSensors.Build.cs").read_text(
			encoding="utf-8"
		)
		for token in (
			"ConfigCache.ReadHierarchy",
			"IOSMotionUsageDescription",
			"AndroidActivityRecognitionRationale",
			"DevelopmentInputMode",
			"ExtraPermissions",
			"AdditionalPlistData",
			"ManifestRequirementsAdditions.txt",
			"ManifestRequirementsOverride.txt",
			"Target.Version.MajorVersion",
			"Target.Version.MinorVersion",
			"modern Xcode packaging ignores iOS UPL plist updates",
			"BuildException",
		):
			self.assertIn(token, rules)

	def test_editor_revalidates_startup_and_setting_changes(self):
		editor = (
			PLUGIN
			/ "Source"
			/ "OpenMobileSensorsEditor"
			/ "Private"
			/ "OpenMobileSensorsEditorModule.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"FOpenMobileSensorsPackagingValidator",
			"LoadLocalIniFile",
			"OnSettingChanged",
			"StartupModule",
			"ShutdownModule",
			"OpenMobileSensorsPackaging",
		):
			self.assertIn(token, editor)

	def test_packaging_inspector_checks_final_platform_artifacts(self):
		inspector = ROOT / "Scripts" / "inspect_sensors_package.py"
		self.assertTrue(inspector.is_file())
		text = inspector.read_text(encoding="utf-8")
		for token in (
			"AndroidManifest.xml",
			"Info.plist",
			"PrivacyInfo.xcprivacy",
			"CoreMotion",
			"entitlements",
			"store-validation",
		):
			self.assertIn(token, text)

	def test_ue_58_build_path_exception_is_documented(self):
		docs = (PLUGIN / "Docs" / "Packaging.md").read_text(encoding="utf-8")
		self.assertIn("UE 5.8", docs)
		self.assertIn("ApplePostBuildSync", docs)
		self.assertIn("AdditionalPlistData", docs)
		self.assertIn("unsigned", docs.lower())
		self.assertIn("Validate App", docs)


class SensorsPackageInspectorTests(unittest.TestCase):
	def test_android_manifest_requires_exact_conditional_declarations(self):
		with tempfile.TemporaryDirectory() as directory:
			manifest = pathlib.Path(directory) / "AndroidManifest.xml"
			manifest.write_text(
				'<manifest xmlns:android="http://schemas.android.com/apk/res/android">'
				'<uses-permission android:name="android.permission.ACTIVITY_RECOGNITION"/>'
				'<uses-permission android:name="android.permission.HIGH_SAMPLING_RATE_SENSORS"/>'
				'</manifest>',
				encoding="utf-8",
			)
			result = inspect_android_manifest(manifest, True, True)
			self.assertTrue(result["activity_recognition"])
			self.assertTrue(result["high_sampling"])
			with self.assertRaises(InspectionError):
				inspect_android_manifest(manifest, False, True)

	def test_ios_plist_requires_the_exact_motion_text_and_no_extra_keys(self):
		with tempfile.TemporaryDirectory() as directory:
			plist = pathlib.Path(directory) / "Info.plist"
			with plist.open("wb") as stream:
				plistlib.dump(
					{"NSMotionUsageDescription": "Uses motion for steering."},
					stream,
				)
			result = inspect_ios_plist(plist, "Uses motion for steering.")
			self.assertEqual(result["motion_usage"], "Uses motion for steering.")
			with self.assertRaises(InspectionError):
				inspect_ios_plist(plist, "Different text.")
			with plist.open("wb") as stream:
				plistlib.dump(
					{
						"NSMotionUsageDescription": "Uses motion for steering.",
						"NSLocationWhenInUseUsageDescription": "Unexpected",
					},
					stream,
				)
			with self.assertRaises(InspectionError):
				inspect_ios_plist(plist, "Uses motion for steering.")

	def test_privacy_and_store_outputs_must_report_safe_success(self):
		with tempfile.TemporaryDirectory() as directory:
			root = pathlib.Path(directory)
			privacy = root / "PrivacyInfo.xcprivacy"
			with privacy.open("wb") as stream:
				plistlib.dump(
					{
						"NSPrivacyTracking": False,
						"NSPrivacyCollectedDataTypes": [],
						"NSPrivacyAccessedAPITypes": [],
					},
					stream,
				)
			self.assertFalse(inspect_privacy_manifest(privacy)["tracking"])
			store_log = root / "store-validation.log"
			store_log.write_text("Validation succeeded", encoding="utf-8")
			self.assertTrue(inspect_store_validation(store_log)["succeeded"])
			store_log.write_text("Validation failed", encoding="utf-8")
			with self.assertRaises(InspectionError):
				inspect_store_validation(store_log)


if __name__ == "__main__":
	unittest.main()
