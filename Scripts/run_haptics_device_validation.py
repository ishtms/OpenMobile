#!/usr/bin/env python3

import argparse
import datetime
import hashlib
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
SAMPLE_ROOT = REPOSITORY_ROOT / "Tests" / "OpenMobileHapticsSampleHost"
DEFAULT_ENGINE_ROOT = Path("/Users/Shared/Epic Games/UE_5.8")
BUNDLE_IDENTIFIER = "com.openmobile.hapticssample"
GENERATED_DIRECTORIES = {
	"Binaries",
	"DerivedDataCache",
	"Intermediate",
	"Saved",
	"__pycache__",
}
LOCAL_PLUGIN_DEPENDENCIES = {
	"OpenMobileHaptics": ("OpenMobileCore",),
}
FAILURE_PATTERN = re.compile(
	r"error:|fatal error|BUILD FAILED|AutomationTool exiting|CodeSign|Provisioning|"
	r"No space left on device|FAILED",
	re.IGNORECASE,
)
UUID_PATTERN = re.compile(
	r"\b[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-"
	r"[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}\b"
)


SCENARIOS = (
	(
		"capability_snapshot",
		"Copy the sanitized in-app capability snapshot and confirm it matches the visible tier.",
		("ios", "android"),
	),
	(
		"cold_and_warm_latency",
		"Record cold-engine and prepared warm-start latency without treating scheduler estimates as actuator measurements.",
		("ios", "android"),
	),
	(
		"duration_and_intensity",
		"Short and bounded long effects feel distinct, and intensity scaling is monotonic where the capability snapshot reports control.",
		("ios", "android"),
	),
	(
		"patterns_and_loops",
		"Prepared patterns keep event order, finite loops end, indefinite loops remain controllable, and the vehicle sample stops at its two-second cap.",
		("ios", "android"),
	),
	(
		"stop_and_controls",
		"Handle, channel, and global stops end owned playback; supported pause, resume, seek, intensity, and sharpness controls report their actual path.",
		("ios", "android"),
	),
	(
		"schedules_and_audio_sync",
		"Delayed cancellation prevents output, scheduled feedback starts once, and audio-clock feedback has no obvious duplicate or gross drift.",
		("ios", "android"),
	),
	(
		"fallback_feel",
		"Unavailable rich paths resolve to the declared semantic, primitive, or basic fallback without an unexpected stronger effect.",
		("ios", "android"),
	),
	(
		"phone_call_interruption",
		"An incoming call interrupts active work once and later requests recover without replaying stale feedback.",
		("ios", "android"),
	),
	(
		"lock_home_and_foreground",
		"Locking or leaving the app stops disallowed work; foreground return restores later playback without replay.",
		("ios", "android"),
	),
	(
		"rotation",
		"Rotation preserves the sample controls and does not duplicate the subsystem or active playback.",
		("ios", "android"),
	),
	(
		"activity_recreation",
		"Android activity recreation retains valid policy and creates no duplicate callbacks or stale presenter reference.",
		("android",),
	),
	(
		"audio_session_and_engine_reset",
		"Audio-session changes and a Core Haptics engine reset terminate stale work once and permit a later prepared request.",
		("ios",),
	),
	(
		"process_restart",
		"Force termination clears all handles and a clean process launch prepares and plays normally.",
		("ios", "android"),
	),
	(
		"global_disable_and_category_scaling",
		"Global disable suppresses ordinary feedback, and category scaling changes only later requests in that category.",
		("ios", "android"),
	),
	(
		"system_touch_feedback",
		"Changing the operating-system touch-feedback setting produces documented semantic behavior without bypassing app policy.",
		("ios", "android"),
	),
	(
		"rate_limits",
		"Rapid repeated input stays bounded and reports suppression instead of producing an excessive burst.",
		("ios", "android"),
	),
	(
		"accessibility",
		"Feedback is never the only cue, the accessibility sample respects player policy, and screen-reader use remains operable.",
		("ios", "android"),
	),
	(
		"battery",
		"A bounded repeated-use observation shows no runaway loop, retained playback, or abnormal battery warning.",
		("ios", "android"),
	),
)


class DeviceValidationError(RuntimeError):
	pass


class DeviceCommandError(DeviceValidationError):
	def __init__(self, label: str, reason: str, log: Path) -> None:
		super().__init__(f"{label} failed ({reason}), see {log}")
		self.reason = reason


@dataclass(frozen=True)
class SigningMaterial:
	team_id: str
	profile_identifier: str | None
	profile_path: Path | None
	automatic: bool


def utc_now() -> str:
	return datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat()


def write_json(path: Path, value: object) -> None:
	path.parent.mkdir(parents=True, exist_ok=True)
	temporary = path.with_suffix(path.suffix + ".tmp")
	temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
	temporary.replace(path)


def redact(text: str, secrets: set[str]) -> str:
	for secret in sorted((value for value in secrets if value), key=len, reverse=True):
		text = text.replace(secret, "<redacted>")
	return UUID_PATTERN.sub("<redacted-device-id>", text)


def plugin_path(name: str) -> Path:
	for family in ("Foundation", "Native"):
		candidate = REPOSITORY_ROOT / family / name
		if (candidate / f"{name}.uplugin").is_file():
			return candidate
	raise DeviceValidationError(f"missing local plugin {name}")


def copy_plugins(host_root: Path, names: tuple[str, ...]) -> None:
	pending = list(names)
	resolved: set[str] = set()
	while pending:
		name = pending.pop()
		if name in resolved:
			continue
		resolved.add(name)
		pending.extend(LOCAL_PLUGIN_DEPENDENCIES.get(name, ()))
	for name in sorted(resolved):
		shutil.copytree(
			plugin_path(name),
			host_root / "Plugins" / name,
			ignore=shutil.ignore_patterns(*GENERATED_DIRECTORIES),
		)


def prepare_host(output_root: Path, signing: SigningMaterial) -> Path:
	host_root = output_root / "host"
	if host_root.exists():
		raise DeviceValidationError(f"validation host already exists: {host_root}")
	shutil.copytree(
		SAMPLE_ROOT,
		host_root,
		ignore=shutil.ignore_patterns(*GENERATED_DIRECTORIES),
	)
	project = host_root / "OpenMobileHapticsSampleHost.uproject"
	descriptor = json.loads(project.read_text(encoding="utf-8"))
	descriptor.pop("AdditionalPluginDirectories", None)
	write_json(project, descriptor)
	copy_plugins(host_root, ("OpenMobileHaptics",))
	config = host_root / "Config" / "DefaultEngine.ini"
	automatic = "True" if signing.automatic else "False"
	profile_lines = ""
	if signing.profile_identifier and not signing.automatic:
		profile_lines = (
			f"MobileProvision={signing.profile_identifier}\n"
			"SigningCertificate=Apple Development\n"
		)
	xcode_profile_lines = ""
	if signing.profile_identifier and not signing.automatic:
		xcode_profile_lines = (
			"IOSSigningIdentity=Apple Development\n"
			f"IOSProvisioningProfile={signing.profile_identifier}\n"
		)
	with config.open("a", encoding="utf-8") as output:
		output.write(
			"\n[/Script/IOSRuntimeSettings.IOSRuntimeSettings]\n"
			f"bAutomaticSigning={automatic}\n"
			f"IOSTeamID={signing.team_id}\n"
			f"{profile_lines}"
			"\n[/Script/MacTargetPlatform.XcodeProjectSettings]\n"
			f"bUseAutomaticCodeSigning={automatic}\n"
			f"CodeSigningTeam={signing.team_id}\n"
			f"BundleIdentifier={BUNDLE_IDENTIFIER}\n"
			f"{xcode_profile_lines}"
		)
	return project


def development_identity_fingerprints() -> set[str]:
	completed = subprocess.run(
		["security", "find-identity", "-v", "-p", "codesigning"],
		capture_output=True,
		text=True,
		check=False,
	)
	if completed.returncode != 0:
		raise DeviceValidationError("could not inspect installed code-signing identities")
	fingerprints = {
		match.group(1)
		for line in completed.stdout.splitlines()
		if "Apple Development:" in line
		if (match := re.search(r"\)\s+([0-9A-F]{40})\s+\"", line))
	}
	if not fingerprints:
		raise DeviceValidationError(
			"no Apple Development code-signing identity with a private key is installed"
		)
	return fingerprints


def provisioning_profiles() -> list[tuple[Path, dict[str, object]]]:
	roots = (
		Path.home() / "Library" / "MobileDevice" / "Provisioning Profiles",
		Path.home() / "Library" / "Developer" / "Xcode" / "UserData" / "Provisioning Profiles",
	)
	profiles: list[tuple[Path, dict[str, object]]] = []
	seen: set[str] = set()
	for root in roots:
		if not root.is_dir():
			continue
		for path in sorted(root.iterdir()):
			if not path.is_file():
				continue
			decoded = subprocess.run(
				["security", "cms", "-D", "-i", str(path)],
				capture_output=True,
				check=False,
			)
			if decoded.returncode != 0:
				continue
			try:
				profile = plistlib.loads(decoded.stdout)
			except plistlib.InvalidFileException:
				continue
			identifier = str(profile.get("UUID", ""))
			if not identifier or identifier in seen:
				continue
			seen.add(identifier)
			profiles.append((path, profile))
	return profiles


def profile_bundle_matches(profile: dict[str, object], bundle_identifier: str) -> bool:
	entitlements = profile.get("Entitlements", {})
	application_identifier = str(entitlements.get("application-identifier", ""))
	profile_bundle = application_identifier.split(".", 1)[1] if "." in application_identifier else ""
	return (
		profile_bundle == bundle_identifier
		or profile_bundle == "*"
		or profile_bundle.endswith("*")
		and bundle_identifier.startswith(profile_bundle[:-1])
	)


def discover_signing_material(
	explicit_team_id: str | None,
	allow_provisioning_download: bool,
) -> SigningMaterial:
	requested_team = explicit_team_id or os.environ.get("OPENMOBILE_IOS_TEAM_ID")
	if requested_team and not re.fullmatch(r"[A-Z0-9]{10}", requested_team):
		raise DeviceValidationError("the iOS team identifier must be 10 characters")
	identity_fingerprints = development_identity_fingerprints()
	now = datetime.datetime.now(datetime.timezone.utc)
	matching: list[tuple[datetime.datetime, Path, dict[str, object], str]] = []
	identity_teams: set[str] = set()
	for path, profile in provisioning_profiles():
		certificate_fingerprints = {
			hashlib.sha1(certificate).hexdigest().upper()
			for certificate in profile.get("DeveloperCertificates", [])
		}
		if not identity_fingerprints.intersection(certificate_fingerprints):
			continue
		teams = {
			str(team)
			for team in profile.get("TeamIdentifier", [])
			if re.fullmatch(r"[A-Z0-9]{10}", str(team))
		}
		identity_teams.update(teams)
		if requested_team and requested_team not in teams:
			continue
		entitlements = profile.get("Entitlements", {})
		expiration = profile.get("ExpirationDate")
		if not isinstance(expiration, datetime.datetime):
			continue
		if expiration.tzinfo is None:
			expiration = expiration.replace(tzinfo=datetime.timezone.utc)
		if (
			expiration <= now
			or not entitlements.get("get-task-allow")
			or not profile.get("ProvisionedDevices")
			or not profile_bundle_matches(profile, BUNDLE_IDENTIFIER)
		):
			continue
		for team in teams:
			matching.append((expiration, path, profile, team))
	if matching:
		_expiration, path, profile, team = max(matching, key=lambda candidate: candidate[0])
		return SigningMaterial(
			team_id=team,
			profile_identifier=str(profile["UUID"]),
			profile_path=path,
			automatic=bool(profile.get("IsXcodeManaged")),
		)
	if not allow_provisioning_download:
		raise DeviceValidationError(
			"no valid development profile matches the sample bundle and an installed Apple Development identity"
		)
	if requested_team:
		team = requested_team
	elif len(identity_teams) == 1:
		team = identity_teams.pop()
	else:
		raise DeviceValidationError(
			"set OPENMOBILE_IOS_TEAM_ID before requesting an automatic profile download"
		)
	return SigningMaterial(team, None, None, True)


def run_logged(
	label: str,
	arguments: list[str],
	log: Path,
	secrets: set[str],
	*,
	environment: dict[str, str] | None = None,
) -> None:
	print(f"[{label}] running", flush=True)
	log.parent.mkdir(parents=True, exist_ok=True)
	with log.open("w", encoding="utf-8") as output:
		completed = subprocess.run(
			arguments,
			cwd=REPOSITORY_ROOT,
			env=environment,
			stdout=output,
			stderr=subprocess.STDOUT,
			text=True,
			check=False,
		)
	if completed.returncode == 0:
		print(f"[{label}] passed", flush=True)
		return
	contents = log.read_text(encoding="utf-8", errors="replace")
	matches = [line for line in contents.splitlines() if FAILURE_PATTERN.search(line)]
	if not matches:
		matches = contents.splitlines()[-16:]
	for line in matches[-16:]:
		print(redact(line, secrets), file=sys.stderr)
	raise DeviceValidationError(f"{label} failed, see {log}")


def package_ios(
	engine_root: Path,
	output_root: Path,
	project: Path,
	signing: SigningMaterial,
) -> Path:
	run_uat = engine_root / "Engine" / "Build" / "BatchFiles" / "RunUAT.sh"
	if not run_uat.is_file():
		raise DeviceValidationError(f"missing Unreal Automation Tool: {run_uat}")
	archive = output_root / "packages" / "ios"
	run_logged(
		"package-ios-development",
		[
			str(run_uat),
			"BuildCookRun",
			"-WaitForUATMutex",
			f"-project={project}",
			"-noP4",
			"-platform=IOS",
			"-clientconfig=Development",
			"-build",
			"-cook",
			"-stage",
			"-pak",
			"-package",
			"-archive",
			f"-archivedirectory={archive}",
			"-utf8output",
			"-ubtargs=-NoUBA",
		],
		output_root / "logs" / "package-ios-development.log",
		{signing.team_id, signing.profile_identifier or ""},
	)
	run_logged(
		"inspect-ios-development",
		[
			sys.executable,
			str(REPOSITORY_ROOT / "Scripts" / "validate_haptics_artifacts.py"),
			"ios",
			"--project-root",
			str(project.parent),
			"--configuration",
			"Development",
			"--mode",
			"enabled",
			"--signature",
			"either",
		],
		output_root / "logs" / "inspect-ios-development.log",
		{signing.team_id, signing.profile_identifier or ""},
	)
	return extract_signed_app(output_root, signing.team_id)


def safely_extract_ipa(ipa: Path, destination: Path) -> None:
	destination.mkdir(parents=True, exist_ok=True)
	resolved_destination = destination.resolve()
	with zipfile.ZipFile(ipa) as archive:
		for member in archive.infolist():
			target = (destination / member.filename).resolve()
			if resolved_destination not in target.parents and target != resolved_destination:
				raise DeviceValidationError(f"unsafe path in iOS package: {member.filename}")
		archive.extractall(destination)


def app_metadata(app: Path, expected_team_id: str) -> dict[str, object]:
	info_path = app / "Info.plist"
	if not info_path.is_file():
		raise DeviceValidationError(f"missing app Info.plist: {info_path}")
	with info_path.open("rb") as input_file:
		info = plistlib.load(input_file)
	bundle_identifier = info.get("CFBundleIdentifier")
	if bundle_identifier != BUNDLE_IDENTIFIER:
		raise DeviceValidationError(
			f"unexpected bundle identifier {bundle_identifier!r} in signed app"
		)
	executable_name = info.get("CFBundleExecutable")
	if not executable_name or not (app / executable_name).is_file():
		raise DeviceValidationError("signed app has no declared executable")
	verification = subprocess.run(
		["codesign", "--verify", "--deep", "--strict", str(app)],
		capture_output=True,
		text=True,
		check=False,
	)
	if verification.returncode != 0:
		raise DeviceValidationError("packaged iOS app does not have a valid code signature")
	details = subprocess.run(
		["codesign", "--display", "--verbose=2", str(app)],
		capture_output=True,
		text=True,
		check=False,
	)
	if details.returncode != 0 or f"TeamIdentifier={expected_team_id}" not in details.stderr:
		raise DeviceValidationError("packaged iOS app is not signed by the selected team")
	architectures = subprocess.run(
		["lipo", "-archs", str(app / executable_name)],
		capture_output=True,
		text=True,
		check=False,
	)
	if architectures.returncode != 0:
		raise DeviceValidationError("could not inspect the packaged iOS architecture")
	architecture_names = architectures.stdout.split()
	if "arm64" not in architecture_names:
		raise DeviceValidationError("packaged iOS app does not contain arm64")
	return {
		"architecture": architecture_names,
		"bundleIdentifier": bundle_identifier,
		"configuration": "Development",
		"signed": True,
	}


def extract_signed_app(output_root: Path, team_id: str) -> Path:
	archived_apps = sorted((output_root / "packages" / "ios").glob("*.app"))
	if len(archived_apps) == 1:
		app_metadata(archived_apps[0], team_id)
		return archived_apps[0]
	if archived_apps:
		raise DeviceValidationError(
			f"expected one archived iOS app, found {len(archived_apps)}"
		)
	ipas = sorted((output_root / "packages" / "ios").rglob("*.ipa"))
	if len(ipas) != 1:
		raise DeviceValidationError(
			f"expected one archived iOS package, found {len(ipas)}"
		)
	destination = output_root / "install"
	if destination.exists():
		existing_apps = sorted(destination.glob("Payload/*.app"))
		if len(existing_apps) == 1:
			app_metadata(existing_apps[0], team_id)
			return existing_apps[0]
		raise DeviceValidationError(f"existing iOS extraction is incomplete: {destination}")
	safely_extract_ipa(ipas[0], destination)
	apps = sorted(destination.glob("Payload/*.app"))
	if len(apps) != 1:
		raise DeviceValidationError(f"expected one app in iOS package, found {len(apps)}")
	app_metadata(apps[0], team_id)
	return apps[0]


def run_devicectl(
	label: str,
	arguments: list[str],
	output_root: Path,
	secrets: set[str],
) -> dict[str, object]:
	json_path = output_root / "private" / f"{label}.json"
	log_path = output_root / "private" / f"{label}.log"
	json_path.parent.mkdir(parents=True, exist_ok=True)
	completed = subprocess.run(
		[
			"xcrun",
			"devicectl",
			*arguments,
			"--timeout",
			"60",
			"--json-output",
			str(json_path),
			"--log-output",
			str(log_path),
		],
		stdout=subprocess.DEVNULL,
		stderr=subprocess.PIPE,
		text=True,
		check=False,
	)
	if completed.returncode != 0:
		log_contents = log_path.read_text(encoding="utf-8", errors="replace") if log_path.is_file() else ""
		context = "\n".join((log_contents + "\n" + completed.stderr).splitlines()[-16:])
		if context:
			print(redact(context, secrets), file=sys.stderr)
		reason = (
			"device_locked"
			if "could not be, unlocked" in log_contents
			or "reason: Locked" in log_contents
			else "command_rejected"
		)
		raise DeviceCommandError(label, reason, log_path)
	try:
		return json.loads(json_path.read_text(encoding="utf-8"))
	except (OSError, json.JSONDecodeError) as error:
		raise DeviceValidationError(f"{label} returned invalid JSON") from error


def select_device(output_root: Path, requested: str | None) -> str:
	if requested:
		return requested
	data = run_devicectl("list-devices", ["list", "devices"], output_root, set())
	devices = data.get("result", {}).get("devices", [])
	candidates = []
	for device in devices:
		connection = device.get("connectionProperties", {})
		hardware = device.get("hardwareProperties", {})
		if (
			hardware.get("platform") == "iOS"
			and connection.get("pairingState") == "paired"
			and connection.get("tunnelState") != "unavailable"
		):
			candidates.append(device.get("identifier"))
	if len(candidates) != 1 or not candidates[0]:
		raise DeviceValidationError(
			"pass --device when exactly one paired and reachable iOS device is not available"
		)
	return candidates[0]


def device_summary(details: dict[str, object]) -> tuple[dict[str, object], set[str]]:
	result = details.get("result", {})
	device = result if isinstance(result, dict) else {}
	hardware = device.get("hardwareProperties", {})
	properties = device.get("deviceProperties", {})
	connection = device.get("connectionProperties", {})
	if hardware.get("platform") != "iOS":
		raise DeviceValidationError("selected device is not an iOS device")
	if connection.get("pairingState") != "paired":
		raise DeviceValidationError("selected iOS device is not paired")
	if properties.get("developerModeStatus") != "enabled":
		raise DeviceValidationError("selected iOS device does not have Developer Mode enabled")
	secrets = {
		str(value)
		for value in (
			device.get("identifier"),
			properties.get("name"),
			hardware.get("ecid"),
			hardware.get("serialNumber"),
			hardware.get("udid"),
		)
		if value
	}
	return (
		{
			"developerMode": "enabled",
			"model": hardware.get("marketingName", "Unknown"),
			"osVersion": properties.get("osVersionNumber", "Unknown"),
			"platform": "iOS",
		},
		secrets,
	)


def new_report() -> dict[str, object]:
	return {
		"schemaVersion": 1,
		"generatedAt": utc_now(),
		"plugin": "OpenMobileHaptics",
		"platform": "iOS",
		"device": {
			"model": "Pending",
			"osVersion": "Pending",
			"platform": "iOS",
			"developerMode": "Pending",
		},
		"build": {
			"architecture": [],
			"bundleIdentifier": BUNDLE_IDENTIFIER,
			"configuration": "Development",
			"signed": False,
		},
		"deployment": {
			"installed": False,
			"installStatus": "pending",
			"installActual": "Installation has not been attempted.",
			"launched": False,
			"launchStatus": "pending",
			"launchActual": "Foreground launch has not been attempted.",
		},
		"capabilitySnapshot": {
			"status": "pending",
			"actual": "Copy the sanitized snapshot from the sample after launch.",
		},
		"scenarios": [
			{
				"id": identifier,
				"expected": expected,
				"actual": (
					"Requires observation on this device."
					if "ios" in platforms
					else "Android-only scenario."
				),
				"status": "pending" if "ios" in platforms else "not_applicable",
			}
			for identifier, expected, platforms in SCENARIOS
		],
		"limitations": [
			"Technical launch evidence does not prove actuator feel or hardware timing.",
			"This record covers one Apple device only and does not satisfy the Android or iPad matrix.",
		],
		"redaction": {
			"deviceIdentifiersIncluded": False,
			"personalDeviceNameIncluded": False,
			"signingTeamIncluded": False,
		},
	}


def render_report(report: dict[str, object]) -> str:
	device = report["device"]
	build = report["build"]
	deployment = report["deployment"]
	lines = [
		"# OpenMobile Haptics device validation",
		"",
		f"Generated: {report['generatedAt']}",
		f"Device: {device['model']}, iOS {device['osVersion']}",
		f"Build: {build['configuration']}, {', '.join(build['architecture']) or 'pending'}, signed: {str(build['signed']).lower()}",
		f"Deployment: installed: {str(deployment['installed']).lower()}, launched: {str(deployment['launched']).lower()}",
		"",
		"Copy the sanitized capability snapshot into `report.json`. For each applicable scenario, replace `pending` with `pass` or `fail` and record the observed result. Do not add a device name, UDID, serial number, signing team, or personal data.",
		"",
		"| Scenario | Status | Expected | Actual |",
		"| --- | --- | --- | --- |",
	]
	for scenario in report["scenarios"]:
		lines.append(
			f"| {markdown_cell(scenario['id'])} | {markdown_cell(scenario['status'])} | "
			f"{markdown_cell(scenario['expected'])} | {markdown_cell(scenario['actual'])} |"
		)
	lines.extend(("", "## Known limitations", ""))
	lines.extend(f"- {limitation}" for limitation in report["limitations"])
	return "\n".join(lines) + "\n"


def markdown_cell(value: object) -> str:
	return str(value).replace("\n", " ").replace("|", "\\|")


def save_report(output_root: Path, report: dict[str, object], secrets: set[str]) -> None:
	report["generatedAt"] = utc_now()
	serialized = json.dumps(report, indent=2, sort_keys=True) + "\n"
	for secret in secrets:
		if secret and secret in serialized:
			raise DeviceValidationError("a private device or signing value reached the report")
	if UUID_PATTERN.search(serialized):
		raise DeviceValidationError("a device identifier reached the report")
	write_json(output_root / "report.json", report)
	(output_root / "report.md").write_text(render_report(report), encoding="utf-8")


def load_report(output_root: Path) -> dict[str, object]:
	path = output_root / "report.json"
	if not path.is_file():
		raise DeviceValidationError(f"missing validation report: {path}")
	try:
		report = json.loads(path.read_text(encoding="utf-8"))
	except json.JSONDecodeError as error:
		raise DeviceValidationError(f"invalid validation report: {path}") from error
	deployment = report.get("deployment", {})
	if not isinstance(deployment, dict):
		raise DeviceValidationError(f"invalid deployment record: {path}")
	deployment.setdefault(
		"installStatus",
		"pass" if deployment.get("installed") else "pending",
	)
	deployment.setdefault(
		"installActual",
		"CoreDevice accepted the signed app installation."
		if deployment.get("installed")
		else "Installation has not been attempted.",
	)
	deployment.setdefault(
		"launchStatus",
		"pass" if deployment.get("launched") else "pending",
	)
	deployment.setdefault(
		"launchActual",
		"CoreDevice launched the app in the foreground."
		if deployment.get("launched")
		else "Foreground launch has not been attempted.",
	)
	return report


def deploy(
	output_root: Path,
	app: Path | None,
	device: str,
	report: dict[str, object],
	signing: SigningMaterial | None,
	*,
	install: bool,
	launch: bool,
) -> set[str]:
	base_secrets = {device}
	if signing:
		base_secrets.update((signing.team_id, signing.profile_identifier or ""))
	details = run_devicectl(
		"device-details",
		["device", "info", "details", "--device", device],
		output_root,
		base_secrets,
	)
	summary, device_secrets = device_summary(details)
	secrets = base_secrets | device_secrets
	if signing and signing.profile_path:
		selected_udid = str(details.get("result", {}).get("hardwareProperties", {}).get("udid", ""))
		profile = next(
			(
				value
				for path, value in provisioning_profiles()
				if path == signing.profile_path
			),
			None,
		)
		if not profile or selected_udid not in profile.get("ProvisionedDevices", []):
			raise DeviceValidationError(
				"the selected iOS device is not included in the chosen development profile"
			)
	report["device"] = summary
	save_report(output_root, report, secrets)
	if install:
		if app is None:
			raise DeviceValidationError("install requires a packaged iOS app")
		print("[install-ios-app] running", flush=True)
		try:
			run_devicectl(
				"install-ios-app",
				["device", "install", "app", "--device", device, str(app)],
				output_root,
				secrets,
			)
		except DeviceCommandError:
			report["deployment"]["installStatus"] = "fail"
			report["deployment"]["installActual"] = "CoreDevice rejected installation. Review the private command log."
			save_report(output_root, report, secrets)
			raise
		report["deployment"]["installed"] = True
		report["deployment"]["installStatus"] = "pass"
		report["deployment"]["installActual"] = "CoreDevice accepted the signed app installation."
		save_report(output_root, report, secrets)
		print("[install-ios-app] passed", flush=True)
	if launch:
		print("[launch-ios-app] running", flush=True)
		try:
			run_devicectl(
				"launch-ios-app",
				[
					"device",
					"process",
					"launch",
					"--device",
					device,
					"--terminate-existing",
					"--activate",
					BUNDLE_IDENTIFIER,
				],
				output_root,
				secrets,
			)
		except DeviceCommandError as error:
			report["deployment"]["launchStatus"] = "fail"
			report["deployment"]["launchActual"] = (
				"CoreDevice rejected foreground launch because the device was locked."
				if error.reason == "device_locked"
				else "CoreDevice rejected foreground launch. Review the private command log."
			)
			save_report(output_root, report, secrets)
			raise
		report["deployment"]["launched"] = True
		report["deployment"]["launchStatus"] = "pass"
		report["deployment"]["launchActual"] = "CoreDevice launched the app in the foreground."
		save_report(output_root, report, secrets)
		print("[launch-ios-app] passed", flush=True)
	return secrets


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Package, install, launch, and record OpenMobile Haptics device validation."
	)
	parser.add_argument("--engine-root", type=Path, default=DEFAULT_ENGINE_ROOT)
	parser.add_argument("--output-root", type=Path)
	parser.add_argument("--device", help="CoreDevice identifier or unambiguous device name.")
	parser.add_argument("--team-id", help="iOS development team. Prefer OPENMOBILE_IOS_TEAM_ID.")
	parser.add_argument(
		"--allow-provisioning-download",
		action="store_true",
		help="Allow Xcode to fetch a profile when none is installed.",
	)
	parser.add_argument(
		"--phase",
		action="append",
		choices=("prepare", "package", "install", "launch", "deploy", "all"),
		help="Run one or more phases. The default is all.",
	)
	return parser.parse_args()


def main() -> int:
	arguments = parse_arguments()
	stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
	output_root = (
		arguments.output_root or Path(f"/tmp/openmobile-haptics-device-{stamp}")
	).resolve()
	phases = arguments.phase or ["all"]
	if "all" in phases:
		phases = ["prepare", "package", "deploy"]
	try:
		needs_signing = any(
			phase in phases for phase in ("prepare", "package", "install", "deploy")
		)
		signing = (
			discover_signing_material(
				arguments.team_id,
				arguments.allow_provisioning_download,
			)
			if needs_signing
			else None
		)
		secrets = (
			{signing.team_id, signing.profile_identifier or ""}
			if signing
			else set()
		)
		project = output_root / "host" / "OpenMobileHapticsSampleHost.uproject"
		if "prepare" in phases:
			if signing is None:
				raise DeviceValidationError("prepare requires iOS signing material")
			project = prepare_host(output_root, signing)
			report = new_report()
			save_report(output_root, report, secrets)
		else:
			report = load_report(output_root)
		app: Path | None = None
		if "package" in phases:
			if signing is None:
				raise DeviceValidationError("package requires iOS signing material")
			if not project.is_file():
				raise DeviceValidationError(f"missing prepared sample project: {project}")
			app = package_ios(arguments.engine_root.resolve(), output_root, project, signing)
			report["build"] = app_metadata(app, signing.team_id)
			save_report(output_root, report, secrets)
		elif "install" in phases or "deploy" in phases:
			if signing is None:
				raise DeviceValidationError("install requires iOS signing material")
			app = extract_signed_app(output_root, signing.team_id)
			report["build"] = app_metadata(app, signing.team_id)
			save_report(output_root, report, secrets)
		if "install" in phases or "launch" in phases or "deploy" in phases:
			device = select_device(output_root, arguments.device)
			secrets |= deploy(
				output_root,
				app,
				device,
				report,
				signing,
				install="install" in phases or "deploy" in phases,
				launch="launch" in phases or "deploy" in phases,
			)
		save_report(output_root, report, secrets)
	except (OSError, DeviceValidationError, zipfile.BadZipFile) as error:
		print(f"Haptics device validation failed: {redact(str(error), locals().get('secrets', set()))}", file=sys.stderr)
		return 1
	print(f"Haptics device validation is ready for observation: {output_root / 'report.md'}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
