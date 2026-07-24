#!/usr/bin/env python3

import argparse
import json
import plistlib
import re
import subprocess
import sys
import xml.etree.ElementTree as ElementTree
import zipfile
from pathlib import Path


ANDROID_NAMESPACE = "http://schemas.android.com/apk/res/android"
BRIDGE_CLASS = b"OpenMobileHapticsBridgeV1"
JNI_PREFIX = b"Java_com_openmobile_haptics_OpenMobileHapticsBridgeV1_"
HAPTICS_MODULES = (
	"OpenMobileHaptics",
	"OpenMobileHapticsAndroid",
	"OpenMobileHapticsIOS",
)
OPTIONAL_MODULES = (
	"OpenMobileHapticsGameplayAbilities",
	"OpenMobileHapticsSequencer",
	"OpenMobileHapticsSequencerEditor",
	"OpenMobileHapticsUMG",
	"OpenMobileHapticsUMGTester",
)
STARTER_ASSETS = (
	"Combat_Impact.uasset",
	"Notification_Warning.uasset",
	"OpenMobileStarterHaptics.uasset",
	"Reward_Success.uasset",
	"UI_Confirm.uasset",
	"UI_Selection.uasset",
	"Vehicle_Bump.uasset",
)


class ArtifactValidationError(RuntimeError):
	pass


def fail(message: str) -> None:
	raise ArtifactValidationError(message)


def require_file(path: Path) -> Path:
	if not path.is_file():
		fail(f"missing artifact: {path}")
	return path


def find_one(root: Path, pattern: str) -> Path:
	matches = sorted(path for path in root.glob(pattern) if path.is_file())
	if len(matches) != 1:
		fail(f"expected one {pattern} under {root}, found {len(matches)}")
	return matches[0]


def read_evidence(project_root: Path, platform: str) -> str:
	paths = list((project_root / "Binaries" / platform).glob("*.target"))
	paths.extend(
		(project_root / "Saved" / "StagedBuilds").glob(
			f"{platform}/**/*.upluginmanifest"
		)
	)
	paths.extend(
		(project_root / "Intermediate" / "Staging" / platform).glob(
			"*.upluginmanifest"
		)
	)
	if not paths:
		fail(f"missing {platform} receipt or plugin manifest")
	return "\n".join(path.read_text(encoding="utf-8") for path in paths)


def require_modules(evidence: str, required: tuple[str, ...]) -> None:
	for module in required:
		if not re.search(rf"(?<![A-Za-z0-9_]){re.escape(module)}(?![A-Za-z0-9_])", evidence):
			fail(f"build evidence is missing module {module}")


def forbid_modules(evidence: str, forbidden: tuple[str, ...]) -> None:
	for module in forbidden:
		if re.search(rf"(?<![A-Za-z0-9_]){re.escape(module)}(?![A-Za-z0-9_])", evidence):
			fail(f"build evidence contains disabled module {module}")


def stream_contains(stream, token: bytes) -> bool:
	previous = b""
	while chunk := stream.read(4 * 1024 * 1024):
		combined = previous + chunk
		if token in combined:
			return True
		previous = combined[-len(token):]
	return False


def file_contains(path: Path, token: bytes) -> bool:
	with path.open("rb") as stream:
		return stream_contains(stream, token)


def archive_members_contain(
	archive: zipfile.ZipFile,
	names: list[str],
	token: bytes,
) -> bool:
	for name in names:
		with archive.open(name) as stream:
			if stream_contains(stream, token):
				return True
	return False


def validate_starter_assets(project_root: Path, platform: str, enabled: bool) -> None:
	manifest = require_file(
		project_root
		/ "Saved"
		/ "StagedBuilds"
		/ platform
		/ f"Manifest_UFSFiles_{platform}.txt"
	).read_text(encoding="utf-8")
	for asset in STARTER_ASSETS:
		present = f"StarterPresets/{asset}" in manifest
		if present != enabled:
			state = "missing" if enabled else "present in a disabled build"
			fail(f"starter asset {asset} is {state}")
	for suffix in (".ahap", ".aiff", ".caf", ".m4a", ".mp3", ".wav"):
		if suffix in manifest.lower():
			fail(f"loose Haptics source resource reached the staged build: {suffix}")


def validate_android(project_root: Path, package_root: Path, mode: str) -> None:
	enabled = mode != "disabled"
	custom = mode == "custom"
	intermediate = project_root / "Intermediate" / "Android"
	upl = require_file(intermediate / "ActiveUPL.txt").read_text(encoding="utf-8")
	upl_present = "OpenMobileHaptics_Android_UPL.xml" in upl
	if upl_present != enabled:
		fail("Haptics Android UPL state does not match the requested mode")

	manifest = find_one(intermediate, "*AndroidManifest.xml")
	root = ElementTree.parse(manifest).getroot()
	name_attribute = f"{{{ANDROID_NAMESPACE}}}name"
	permissions = [
		element.attrib.get(name_attribute, "")
		for element in root.findall("uses-permission")
	]
	vibrate_count = permissions.count("android.permission.VIBRATE")
	if custom and vibrate_count != 1:
		fail(f"custom-vibration build has {vibrate_count} VIBRATE permissions")
	if not custom and vibrate_count:
		fail("non-custom build contains android.permission.VIBRATE")

	apk_candidates = sorted(
		path for path in package_root.glob("**/*.apk")
		if not path.name.startswith("AFS_")
	)
	if len(apk_candidates) != 1:
		fail(f"expected one game APK under {package_root}, found {len(apk_candidates)}")
	apk = apk_candidates[0]
	with zipfile.ZipFile(apk) as archive:
		names = archive.namelist()
		dex_names = [
			name for name in names
			if name.startswith("classes") and name.endswith(".dex")
		]
		library_names = [
			name for name in names if name.endswith("/libUnreal.so")
		]
		if not dex_names or len(library_names) != 1:
			fail("APK is missing DEX or arm64 Unreal library payload")
		bridge_in_dex = archive_members_contain(archive, dex_names, BRIDGE_CLASS)
		jni_in_library = archive_members_contain(
			archive,
			library_names,
			JNI_PREFIX,
		)
		if bridge_in_dex != enabled or jni_in_library != enabled:
			fail("APK bridge or JNI payload does not match the requested mode")

	evidence = read_evidence(project_root, "Android")
	if enabled:
		require_modules(evidence, ("OpenMobileHaptics", "OpenMobileHapticsAndroid"))
		forbid_modules(evidence, OPTIONAL_MODULES)
	else:
		forbid_modules(evidence, HAPTICS_MODULES + OPTIONAL_MODULES)
	validate_starter_assets(project_root, "Android", enabled)


def run_tool(arguments: list[str]) -> subprocess.CompletedProcess[str]:
	return subprocess.run(arguments, check=False, capture_output=True, text=True)


def validate_ios(
	project_root: Path,
	configuration: str,
	mode: str,
	signature: str,
) -> None:
	enabled = mode == "enabled"
	binaries = project_root / "Binaries" / "IOS"
	binary_candidates = [
		path for path in binaries.glob("*")
		if path.is_file()
		and not path.name.endswith((".target", ".dSYM", ".stub"))
	]
	if configuration == "Shipping":
		binary_candidates = [
			path for path in binary_candidates if path.name.endswith("-IOS-Shipping")
		]
	else:
		binary_candidates = [
			path for path in binary_candidates if not path.name.endswith("-IOS-Shipping")
		]
	if len(binary_candidates) != 1:
		fail(f"expected one raw iOS executable, found {len(binary_candidates)}")
	binary = binary_candidates[0]

	architectures = run_tool(["lipo", "-archs", str(binary)])
	if architectures.returncode or architectures.stdout.split() != ["arm64"]:
		fail("iOS executable is not arm64-only")
	linked = run_tool(["otool", "-L", str(binary)])
	if linked.returncode:
		fail("otool could not inspect the iOS executable")
	for framework in (
		"AudioToolbox.framework",
		"AVFoundation.framework",
		"CoreHaptics.framework",
		"Foundation.framework",
		"UIKit.framework",
	):
		present = framework in linked.stdout
		if enabled and not present:
			fail(f"iOS framework state does not match Haptics mode: {framework}")

	try:
		evidence = read_evidence(project_root, "IOS")
	except ArtifactValidationError:
		evidence = ""
	if evidence:
		if enabled:
			require_modules(evidence, ("OpenMobileHaptics", "OpenMobileHapticsIOS"))
			forbid_modules(evidence, OPTIONAL_MODULES)
		else:
			forbid_modules(evidence, HAPTICS_MODULES + OPTIONAL_MODULES)
	else:
		base_module_present = file_contains(binary, b"/Script/OpenMobileHaptics")
		if base_module_present != enabled:
			fail("raw iOS executable module payload does not match Haptics mode")
		if not enabled and file_contains(binary, b"OpenMobileHapticsIOSBridge"):
			fail("disabled iOS executable contains the Haptics native bridge")
		for module in OPTIONAL_MODULES:
			if file_contains(binary, module.encode("utf-8")):
				fail(f"raw iOS executable contains optional module {module}")

	plist = find_one(project_root / "Intermediate" / "IOS", "*-Info.plist")
	with plist.open("rb") as plist_file:
		info = plistlib.load(plist_file)
	if not info.get("CFBundleDisplayName") or "arm64" not in info.get(
		"UIRequiredDeviceCapabilities", []
	):
		fail("iOS Info.plist is missing display or arm64 capability metadata")
	for unrelated_usage_key in (
		"NSCameraUsageDescription",
		"NSLocationWhenInUseUsageDescription",
		"NSMicrophoneUsageDescription",
		"NSMotionUsageDescription",
		"NSPhotoLibraryUsageDescription",
	):
		if unrelated_usage_key in info:
			fail(f"Haptics fixture contains unrelated plist key {unrelated_usage_key}")

	signature_result = run_tool(["codesign", "--verify", "--strict", str(binary)])
	is_signed = signature_result.returncode == 0
	if signature == "required" and not is_signed:
		fail("iOS executable is not signed")
	if signature == "unsigned" and is_signed:
		fail("iOS executable is unexpectedly signed")

	staged = project_root / "Saved" / "StagedBuilds" / "IOS"
	if staged.is_dir():
		validate_starter_assets(project_root, "IOS", enabled)
	app_candidates = sorted(binaries.glob("*.app"))
	if app_candidates and any(app_candidates[0].iterdir()):
		app_signature = run_tool(
			["codesign", "--verify", "--deep", "--strict", str(app_candidates[0])]
		)
		if signature == "required" and app_signature.returncode:
			fail("iOS app bundle signature is invalid")

	print(
		f"iOS {configuration} artifact is arm64 and "
		f"{'signed' if is_signed else 'unsigned'} with the expected "
		f"{'enabled' if enabled else 'disabled'} Haptics payload."
	)


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Inspect OpenMobile Haptics mobile build artifacts."
	)
	subparsers = parser.add_subparsers(dest="platform", required=True)
	android = subparsers.add_parser("android")
	android.add_argument("--project-root", type=Path, required=True)
	android.add_argument("--package-root", type=Path, required=True)
	android.add_argument(
		"--mode",
		choices=("custom", "semantic", "disabled"),
		required=True,
	)
	ios = subparsers.add_parser("ios")
	ios.add_argument("--project-root", type=Path, required=True)
	ios.add_argument("--configuration", choices=("Development", "Shipping"), required=True)
	ios.add_argument("--mode", choices=("enabled", "disabled"), required=True)
	ios.add_argument(
		"--signature",
		choices=("required", "unsigned", "either"),
		default="either",
	)
	return parser.parse_args()


def main() -> int:
	arguments = parse_arguments()
	try:
		if arguments.platform == "android":
			validate_android(
				arguments.project_root.resolve(),
				arguments.package_root.resolve(),
				arguments.mode,
			)
		else:
			validate_ios(
				arguments.project_root.resolve(),
				arguments.configuration,
				arguments.mode,
				arguments.signature,
			)
	except (
		ArtifactValidationError,
		ElementTree.ParseError,
		OSError,
		ValueError,
		zipfile.BadZipFile,
	) as error:
		print(f"Haptics artifact validation failed: {error}", file=sys.stderr)
		return 1
	if arguments.platform == "android":
		print(f"Android {arguments.mode} Haptics artifacts passed.")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
