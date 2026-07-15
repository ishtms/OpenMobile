import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
ANDROID = SENSORS / "Source" / "OpenMobileSensorsAndroid"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS"


class SensorsPermissionFlowTests(unittest.TestCase):
	def test_platform_modules_register_sensor_owned_permission_providers(self):
		for platform in (ANDROID, IOS):
			rules = (platform / f"{platform.name}.Build.cs").read_text(
				encoding="utf-8"
			)
			module = next((platform / "Private").glob("*Module.cpp")).read_text(
				encoding="utf-8"
			)
			header = next((platform / "Private").glob("*Backend.h")).read_text(
				encoding="utf-8"
			)

			self.assertIn('"OpenMobilePermissions"', rules)
			self.assertIn("IOpenMobilePermissionProvider", header)
			self.assertIn("SupportsPermission", header)
			self.assertIn("GetStatus", header)
			self.assertIn("RequestPermission", header)
			self.assertIn("CancelRequest", header)
			self.assertIn("RegisterProvider", module)
			self.assertIn("UnregisterProvider", module)
			shutdown = module[
				module.index("virtual void ShutdownModule"):
				module.index("private:", module.index("virtual void ShutdownModule"))
			]
			self.assertLess(
				shutdown.index("UnregisterProvider"),
				shutdown.index("UnregisterBackend"),
			)

	def test_android_maps_api_levels_denial_history_and_request_results(self):
		java = (
			ANDROID
			/ "Private"
			/ "Android"
			/ "src"
			/ "com"
			/ "openmobile"
			/ "sensors"
			/ "OpenMobileSensorsBridgeV1.java"
		).read_text(encoding="utf-8")
		upl = (
			ANDROID
			/ "Private"
			/ "Android"
			/ "OpenMobileSensors_Android_UPL.xml"
		).read_text(encoding="utf-8")
		bridge = (
			ANDROID / "Private" / "OpenMobileSensorsAndroidBridge.cpp"
		).read_text(encoding="utf-8")

		for token in (
			"getActivityRecognitionPermissionStatus",
			"requestActivityRecognitionPermission",
			"cancelPermissionRequest",
			"Build.VERSION.SDK_INT < 29",
			"shouldShowRequestPermissionRationale",
			"getSharedPreferences",
			"requestPermissions",
			"nativeOnPermissionResult",
			"PERMISSION_STATUS_PERMANENTLY_DENIED",
		):
			self.assertIn(token, java)
		self.assertIn("requestCode, permissions, grantResults", upl)
		self.assertIn("nativeOnPermissionResult", bridge)

	def test_android_cancellation_keeps_the_os_prompt_reserved(self):
		java = (
			ANDROID
			/ "Private"
			/ "Android"
			/ "src"
			/ "com"
			/ "openmobile"
			/ "sensors"
			/ "OpenMobileSensorsBridgeV1.java"
		).read_text(encoding="utf-8")
		start = java.index("public void cancelPermissionRequest")
		end = java.index("public int startStream", start)

		self.assertNotIn("pendingPermissionRequestId = null", java[start:end])

	def test_ios_queries_without_prompting_and_requests_on_a_dedicated_queue(self):
		bridge_header = (
			IOS / "Private" / "OpenMobileSensorsIOSBridge.h"
		).read_text(encoding="utf-8")
		bridge = (
			IOS / "Private" / "OpenMobileSensorsIOSBridge.mm"
		).read_text(encoding="utf-8")

		for token in (
			"GetMotionActivityPermissionStatus",
			"RequestMotionActivityPermission",
			"CancelMotionActivityPermission",
		):
			self.assertIn(token, bridge_header)
		for token in (
			"[CMMotionActivityManager authorizationStatus]",
			"queryActivityStartingFromDate",
			"OpenMobileSensorsPermissionQueue",
			"PendingPermissionRequests",
		):
			self.assertIn(token, bridge)

	def test_permission_contract_documents_explicit_subscription_retry(self):
		readme = (SENSORS / "README.md").read_text(encoding="utf-8")
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for text in (readme, contract):
			self.assertIn("explicit retry", text.lower())
			self.assertIn("OpenMobilePermissions", text)


if __name__ == "__main__":
	unittest.main()
