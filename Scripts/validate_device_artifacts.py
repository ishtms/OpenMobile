#!/usr/bin/env python3

import argparse
import json
import plistlib
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_HOST_ROOT = REPO_ROOT / "Tests" / "OpenMobileDeviceValidationHost"
ANDROID_NAMESPACE = "{http://schemas.android.com/apk/res/android}"

FORBIDDEN_MODULES = (
	"OpenMobileHaptics",
	"OpenMobileHapticsAndroid",
	"OpenMobileHapticsIOS",
	"OpenMobileSensors",
	"OpenMobileSensorsAndroid",
	"OpenMobileSensorsIOS",
	"OpenMobileMedia",
	"OpenMobileMediaAndroid",
	"OpenMobileMediaIOS",
	"OpenMobileAds",
	"OpenMobileAdsAdMob",
)

FORBIDDEN_BINARY_TOKENS = (
	b"OpenMobileHapticsAndroid",
	b"OpenMobileHapticsIOS",
	b"OpenMobileSensorsAndroid",
	b"OpenMobileSensorsIOS",
	b"OpenMobileMediaAndroid",
	b"OpenMobileMediaIOS",
	b"OpenMobileAds",
	b"GoogleMobileAds",
	b"GADMobileAds",
	b"AppLovinSDK",
	b"FBAudienceNetwork",
	b"OpenMobileDeviceSample",
)


def fail(message: str) -> None:
	raise RuntimeError(message)


def require_file(path: Path) -> Path:
	if not path.is_file():
		fail(f"missing artifact: {path}")
	return path


def require_directory(path: Path) -> Path:
	if not path.is_dir():
		fail(f"missing artifact directory: {path}")
	return path


def run_tool(arguments: list[str]) -> str:
	completed = subprocess.run(
		arguments,
		check=False,
		capture_output=True,
		text=True,
	)
	if completed.returncode != 0:
		fail(f"tool failed: {arguments[0]}")
	return completed.stdout + completed.stderr


def assert_forbidden_tokens_absent(data: bytes, source: str) -> None:
	for token in FORBIDDEN_BINARY_TOKENS:
		if token in data:
			fail(f"{source} contains disabled plugin or provider token {token.decode()}")


def inspect_large_binary(path: Path) -> None:
	maximum_token_length = max(len(token) for token in FORBIDDEN_BINARY_TOKENS)
	previous = b""
	with path.open("rb") as binary:
		while chunk := binary.read(4 * 1024 * 1024):
			combined = previous + chunk
			assert_forbidden_tokens_absent(combined, str(path))
			previous = combined[-maximum_token_length:]


def validate_receipt(path: Path) -> None:
	receipt = json.loads(require_file(path).read_text(encoding="utf-8"))
	serialized = json.dumps(receipt, sort_keys=True)
	for required in ("OpenMobileCore", "OpenMobileDevice"):
		if required not in serialized:
			fail(f"{path.name} is missing {required}")
	for forbidden in FORBIDDEN_MODULES:
		if f'"{forbidden}"' in serialized:
			fail(f"{path.name} contains disabled module {forbidden}")


def validate_active_upl(path: Path) -> None:
	lines = [line.strip() for line in require_file(path).read_text().splitlines()]
	openmobile_lines = [line for line in lines if "OpenMobile" in line]
	if len(openmobile_lines) != 1 or not openmobile_lines[0].endswith(
		"OpenMobileDevice_Android_UPL.xml"
	):
		fail("ActiveUPL.txt must contain only the Device OpenMobile UPL")


def validate_android_manifest(path: Path) -> None:
	root = ET.parse(require_file(path)).getroot()
	permissions = {
		element.attrib.get(f"{ANDROID_NAMESPACE}name", "")
		for element in root.findall("uses-permission")
	}
	if "android.permission.ACCESS_NETWORK_STATE" not in permissions:
		fail("AndroidManifest.xml is missing ACCESS_NETWORK_STATE")
	for forbidden in (
		"com.google.android.gms.permission.AD_ID",
		"android.permission.ACCESS_ADSERVICES_AD_ID",
		"android.permission.ACCESS_ADSERVICES_ATTRIBUTION",
		"android.permission.CAMERA",
		"android.permission.READ_MEDIA_IMAGES",
		"android.permission.BODY_SENSORS",
		"android.permission.ACTIVITY_RECOGNITION",
	):
		if forbidden in permissions:
			fail(f"AndroidManifest.xml contains disabled-plugin permission {forbidden}")


def validate_gradle(path: Path) -> None:
	gradle = require_file(path).read_text(encoding="utf-8")
	if "androidx.window:window-java:1.5.1" not in gradle:
		fail("buildAdditions.gradle is missing the Device window dependency")
	for forbidden in (
		"play-services-ads",
		"applovin",
		"audience-network",
		"user-messaging-platform",
	):
		if forbidden in gradle.lower():
			fail(f"buildAdditions.gradle contains disabled provider {forbidden}")


def validate_apk(path: Path) -> None:
	with zipfile.ZipFile(require_file(path)) as archive:
		names = archive.namelist()
		for required in (
			"AndroidManifest.xml",
			"classes.dex",
			"lib/arm64-v8a/libUnreal.so",
		):
			if required not in names:
				fail(f"APK is missing {required}")
		for name in names:
			lower_name = name.lower()
			if any(
				forbidden in lower_name
				for forbidden in (
					"openmobilehaptics",
					"openmobilesensors",
					"openmobilemedia",
					"openmobileads",
					"googlemobileads",
					"applovin",
				)
			):
				fail(f"APK contains disabled plugin entry {name}")
		for name in names:
			if name.startswith("classes") and name.endswith(".dex"):
				assert_forbidden_tokens_absent(archive.read(name), name)


def validate_android_symbols(path: Path) -> None:
	required = {
		"nativeOpenMobileDeviceBatteryChanged",
		"nativeOpenMobileDeviceNetworkChanged",
		"nativeOpenMobileDeviceWindowChanged",
	}
	found: set[str] = set()
	process = subprocess.Popen(
		["nm", "-gU", str(require_file(path))],
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
		text=True,
	)
	assert process.stdout is not None
	for line in process.stdout:
		for symbol in required:
			if symbol in line:
				found.add(symbol)
		for forbidden in FORBIDDEN_MODULES:
			if forbidden in line:
				process.kill()
				fail(f"Android JNI symbols contain disabled module {forbidden}")
	stderr = process.stderr.read() if process.stderr else ""
	if process.wait() != 0:
		fail(f"nm failed for Android binary: {stderr.strip()}")
	if found != required:
		fail("Android JNI symbols are missing Device callbacks")


def flatten_plist(value: object) -> str:
	if isinstance(value, dict):
		return " ".join(
			str(key) + " " + flatten_plist(item)
			for key, item in value.items()
		)
	if isinstance(value, list):
		return " ".join(flatten_plist(item) for item in value)
	return str(value)


def validate_ios_app(path: Path) -> None:
	app = require_directory(path)
	with require_file(app / "Info.plist").open("rb") as plist_file:
		info = plistlib.load(plist_file)
	if not info.get("CFBundleIdentifier"):
		fail("Info.plist is missing CFBundleIdentifier")
	for forbidden_key in (
		"GADApplicationIdentifier",
		"SKAdNetworkItems",
		"NSCameraUsageDescription",
		"NSMicrophoneUsageDescription",
		"NSPhotoLibraryUsageDescription",
	):
		if forbidden_key in info:
			fail(f"Info.plist contains disabled-plugin key {forbidden_key}")

	privacy_path = require_file(app / "UEMetadata" / "PrivacyInfo.xcprivacy")
	with privacy_path.open("rb") as privacy_file:
		privacy = flatten_plist(plistlib.load(privacy_file))
	for required in ("NSPrivacyAccessedAPICategoryDiskSpace", "E174.1"):
		if required not in privacy:
			fail(f"PrivacyInfo.xcprivacy is missing {required}")

	binary = require_file(app / "OpenMobileDeviceValidationHost")
	architectures = run_tool(["lipo", "-archs", str(binary)]).split()
	if architectures != ["arm64"]:
		fail(f"iOS binary has unexpected architectures: {architectures}")
	linked = run_tool(["otool", "-L", str(binary)])
	for framework in (
		"AVFoundation.framework",
		"Foundation.framework",
		"SystemConfiguration.framework",
		"UIKit.framework",
	):
		if framework not in linked:
			fail(f"iOS binary is missing {framework}")
	for forbidden in ("GoogleMobileAds", "AppLovinSDK", "FBAudienceNetwork"):
		if forbidden in linked:
			fail(f"iOS binary links disabled provider {forbidden}")
	inspect_large_binary(binary)
	run_tool(["codesign", "--verify", "--deep", "--strict", str(app)])


def validate_mac_binary(path: Path) -> None:
	inspect_large_binary(require_file(path))


def validate(host_root: Path) -> None:
	android_binaries = host_root / "Binaries" / "Android"
	ios_binaries = host_root / "Binaries" / "IOS"
	mac_binaries = host_root / "Binaries" / "Mac"
	android_intermediate = host_root / "Intermediate" / "Android"

	validate_receipt(android_binaries / "OpenMobileDeviceValidationHost.target")
	validate_active_upl(android_intermediate / "ActiveUPL.txt")
	validate_android_manifest(android_intermediate / "arm64_AndroidManifest.xml")
	validate_gradle(
		android_intermediate / "arm64" / "gradle" / "app" / "buildAdditions.gradle"
	)
	validate_apk(android_binaries / "OpenMobileDeviceValidationHost-arm64.apk")
	validate_android_symbols(
		android_binaries / "OpenMobileDeviceValidationHost-arm64.so"
	)

	validate_receipt(ios_binaries / "OpenMobileDeviceValidationHost.target")
	validate_ios_app(ios_binaries / "OpenMobileDeviceValidationHost.app")

	validate_receipt(
		mac_binaries / "OpenMobileDeviceValidationHost-Mac-Shipping.target"
	)
	validate_mac_binary(
		mac_binaries / "OpenMobileDeviceValidationHost-Mac-Shipping"
	)


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--host-root", type=Path, default=DEFAULT_HOST_ROOT)
	arguments = parser.parse_args()
	try:
		validate(arguments.host_root.resolve())
	except (OSError, RuntimeError, ValueError, zipfile.BadZipFile) as error:
		print(f"Device artifact validation failed: {error}", file=sys.stderr)
		return 1
	print("OpenMobile Device artifacts passed Android, iOS, and Mac isolation checks.")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
