#!/usr/bin/env python3

import argparse
import json
import pathlib
import plistlib
import subprocess
import sys
import xml.etree.ElementTree as element_tree


ANDROID_NAMESPACE = "{http://schemas.android.com/apk/res/android}"
ACTIVITY_PERMISSION = "android.permission.ACTIVITY_RECOGNITION"
HIGH_SAMPLING_PERMISSION = "android.permission.HIGH_SAMPLING_RATE_SENSORS"


class InspectionError(RuntimeError):
	pass


def inspect_android_manifest(
	path: pathlib.Path,
	expect_activity: bool,
	expect_high_sampling: bool,
) -> dict[str, object]:
	root = element_tree.parse(path).getroot()
	permissions = [
		node.attrib.get(f"{ANDROID_NAMESPACE}name", "")
		for node in root.findall("uses-permission")
	]
	for permission, expected in (
		(ACTIVITY_PERMISSION, expect_activity),
		(HIGH_SAMPLING_PERMISSION, expect_high_sampling),
	):
		count = permissions.count(permission)
		if expected and count != 1:
			raise InspectionError(
				f"AndroidManifest.xml expected one {permission}, found {count}."
			)
		if not expected and count:
			raise InspectionError(
				f"AndroidManifest.xml contains disabled {permission}."
			)
	return {
		"path": str(path),
		"activity_recognition": ACTIVITY_PERMISSION in permissions,
		"high_sampling": HIGH_SAMPLING_PERMISSION in permissions,
	}


def inspect_ios_plist(
	path: pathlib.Path,
	expected_motion_usage: str | None,
) -> dict[str, object]:
	with path.open("rb") as stream:
		values = plistlib.load(stream)
	motion_usage = values.get("NSMotionUsageDescription")
	if not isinstance(motion_usage, str) or not motion_usage.strip():
		raise InspectionError(
			"Info.plist is missing a nonempty NSMotionUsageDescription."
		)
	if expected_motion_usage is not None and motion_usage != expected_motion_usage:
		raise InspectionError(
			"Info.plist NSMotionUsageDescription does not match the Sensors setting."
		)
	for unrelated in (
		"NSLocationAlwaysUsageDescription",
		"NSLocationWhenInUseUsageDescription",
		"NSFallDetectionUsageDescription",
	):
		if unrelated in values:
			raise InspectionError(
				f"Info.plist contains unexpected Sensors key {unrelated}."
			)
	return {"path": str(path), "motion_usage": motion_usage}


def inspect_privacy_manifest(path: pathlib.Path) -> dict[str, object]:
	with path.open("rb") as stream:
		values = plistlib.load(stream)
	for key in (
		"NSPrivacyTracking",
		"NSPrivacyCollectedDataTypes",
		"NSPrivacyAccessedAPITypes",
	):
		if key not in values:
			raise InspectionError(
				f"PrivacyInfo.xcprivacy is missing {key}."
			)
	if values["NSPrivacyTracking"] is not False:
		raise InspectionError("PrivacyInfo.xcprivacy unexpectedly enables tracking.")
	return {
		"path": str(path),
		"tracking": values["NSPrivacyTracking"],
		"collected_data_types": len(values["NSPrivacyCollectedDataTypes"]),
		"accessed_api_types": len(values["NSPrivacyAccessedAPITypes"]),
	}


def inspect_ios_binary(path: pathlib.Path) -> dict[str, object]:
	result = subprocess.run(
		["otool", "-L", str(path)],
		check=False,
		capture_output=True,
		text=True,
	)
	if result.returncode != 0:
		raise InspectionError(result.stderr.strip() or "otool failed.")
	if "/CoreMotion.framework/CoreMotion" not in result.stdout:
		raise InspectionError("The iOS binary is not linked with CoreMotion.")
	return {"path": str(path), "core_motion": True}


def inspect_entitlements(path: pathlib.Path) -> dict[str, object]:
	with path.open("rb") as stream:
		values = plistlib.load(stream)
	return {"path": str(path), "keys": sorted(values)}


def inspect_store_validation(path: pathlib.Path) -> dict[str, object]:
	text = path.read_text(encoding="utf-8", errors="replace")
	lower = text.lower()
	if "validation succeeded" not in lower and "no errors" not in lower:
		raise InspectionError(
			"store-validation output does not contain a success result."
		)
	return {"path": str(path), "succeeded": True}


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Inspect final OpenMobile Sensors package artifacts."
	)
	parser.add_argument("--android-manifest", type=pathlib.Path)
	parser.add_argument("--expect-activity", action="store_true")
	parser.add_argument("--expect-high-sampling", action="store_true")
	parser.add_argument("--ios-plist", type=pathlib.Path)
	parser.add_argument("--expected-motion-usage")
	parser.add_argument("--ios-binary", type=pathlib.Path)
	parser.add_argument("--privacy-manifest", type=pathlib.Path)
	parser.add_argument("--entitlements", type=pathlib.Path)
	parser.add_argument("--store-validation-log", type=pathlib.Path)
	return parser.parse_args()


def main() -> int:
	arguments = parse_arguments()
	results: dict[str, object] = {}
	try:
		if arguments.android_manifest:
			results["android"] = inspect_android_manifest(
				arguments.android_manifest,
				arguments.expect_activity,
				arguments.expect_high_sampling,
			)
		if arguments.ios_plist:
			results["ios_plist"] = inspect_ios_plist(
				arguments.ios_plist,
				arguments.expected_motion_usage,
			)
		if arguments.ios_binary:
			results["ios_binary"] = inspect_ios_binary(arguments.ios_binary)
		if arguments.privacy_manifest:
			results["privacy"] = inspect_privacy_manifest(
				arguments.privacy_manifest
			)
		if arguments.entitlements:
			results["entitlements"] = inspect_entitlements(
				arguments.entitlements
			)
		if arguments.store_validation_log:
			results["store_validation"] = inspect_store_validation(
				arguments.store_validation_log
			)
		if not results:
			raise InspectionError("No package artifacts were provided.")
	except (InspectionError, OSError, element_tree.ParseError, plistlib.InvalidFileException) as error:
		print(str(error), file=sys.stderr)
		return 1
	print(json.dumps(results, indent=2, sort_keys=True))
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
