#!/usr/bin/env python3

import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ENGINE_ROOT = Path("/Users/Shared/Epic Games/UE_5.8")
MATRIX = {
	"CoreOnly": ("OpenMobileCore",),
	"HapticsOnly": ("OpenMobileHaptics",),
	"HapticsUMG": ("OpenMobileHapticsUMG",),
	"HapticsGameplayAbilities": ("OpenMobileHapticsGameplayAbilities",),
	"HapticsSequencer": ("OpenMobileHapticsSequencer",),
	"HapticsDevice": ("OpenMobileHaptics", "OpenMobileDevice"),
	"HapticsSensors": ("OpenMobileHaptics", "OpenMobileSensors"),
	"HapticsDeviceSensors": (
		"OpenMobileHaptics",
		"OpenMobileDevice",
		"OpenMobileSensors",
	),
}
LOCAL_PLUGIN_DEPENDENCIES = {
	"OpenMobileDevice": ("OpenMobileCore",),
	"OpenMobileHaptics": ("OpenMobileCore",),
	"OpenMobileHapticsGameplayAbilities": ("OpenMobileHaptics",),
	"OpenMobileHapticsSequencer": ("OpenMobileHaptics",),
	"OpenMobileHapticsUMG": ("OpenMobileHaptics",),
	"OpenMobileSensors": ("OpenMobileCore", "OpenMobilePermissions"),
}
FAILURE_PATTERN = re.compile(
	r"error:|fatal error|BUILD FAILED|AutomationTool exiting|"
	r"requires a development team|CodeSign|Tests failed|FAILED",
	re.IGNORECASE,
)


class ReleaseValidationError(RuntimeError):
	pass


def write_text(path: Path, contents: str) -> None:
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_text(contents, encoding="utf-8")


def plugin_entries(names: tuple[str, ...]) -> list[dict[str, object]]:
	return [{"Name": name, "Enabled": True} for name in names]


def local_plugin_path(name: str) -> Path:
	for family in ("Foundation", "Native"):
		candidate = REPOSITORY_ROOT / family / name
		if (candidate / f"{name}.uplugin").is_file():
			return candidate
	raise ReleaseValidationError(f"missing local plugin {name}")


def copy_local_plugins(root: Path, names: tuple[str, ...]) -> None:
	pending = list(names)
	resolved: set[str] = set()
	while pending:
		name = pending.pop()
		if name in resolved:
			continue
		resolved.add(name)
		pending.extend(LOCAL_PLUGIN_DEPENDENCIES.get(name, ()))
	plugins_root = root / "Plugins"
	plugins_root.mkdir(parents=True, exist_ok=True)
	for name in sorted(resolved):
		destination = plugins_root / name
		if destination.is_symlink():
			destination.unlink()
		shutil.copytree(
			local_plugin_path(name),
			destination,
			dirs_exist_ok=True,
			ignore=shutil.ignore_patterns(
				"Binaries",
				"DerivedDataCache",
				"Intermediate",
				"Saved",
				"__pycache__",
			),
		)


def stage_editor_modules(root: Path) -> None:
	destination = root / "Binaries" / "Mac"
	destination.mkdir(parents=True, exist_ok=True)
	for manifest in sorted((root / "Plugins").glob("*/Binaries/Mac/UnrealEditor.modules")):
		modules = json.loads(manifest.read_text(encoding="utf-8")).get("Modules", {})
		for filename in modules.values():
			source = manifest.parent / filename
			if not source.is_file():
				raise ReleaseValidationError(f"missing editor module binary {source}")
			shutil.copy2(source, destination / filename)


def generate_code_host(root: Path, variant: str, plugins: tuple[str, ...]) -> Path:
	project_name = f"OpenMobileHaptics{variant}"
	project = root / f"{project_name}.uproject"
	descriptor = {
		"FileVersion": 3,
		"EngineAssociation": "5.8",
		"Category": "Mobile",
		"Description": f"Temporary OpenMobile Haptics {variant} validation host.",
		"Modules": [
			{
				"Name": project_name,
				"Type": "Runtime",
				"LoadingPhase": "Default",
			}
		],
		"Plugins": plugin_entries(plugins),
	}
	write_text(project, json.dumps(descriptor, indent="\t") + "\n")
	copy_local_plugins(root, plugins)
	write_text(
		root / "Config" / "DefaultEngine.ini",
		"""[/Script/EngineSettings.GameMapsSettings]
EditorStartupMap=/Engine/Maps/Entry.Entry
GameDefaultMap=/Engine/Maps/Entry.Entry

[/Script/Engine.RendererSettings]
r.Mobile.ShadingPath=0

[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]
PackageName=com.openmobile.hapticsvalidation
MinSDKVersion=26
TargetSDKVersion=36
SDKAPILevelOverride=android-36
bBuildForArm64=True
bSupportsVulkan=True
bBuildForES31=True

[/Script/IOSRuntimeSettings.IOSRuntimeSettings]
BundleIdentifier=com.openmobile.hapticsvalidation
BundleDisplayName=OpenMobile Haptics Validation
BundleName=OpenMobile Haptics Validation
MinimumiOSVersion=IOS_17
bSupportsIPhone=True
bSupportsIPad=True
""",
	)
	haptics_enabled = any(name.startswith("OpenMobileHaptics") for name in plugins)
	game_config = """[/Script/OpenMobileHaptics.OpenMobileHapticsSettings]
bEnableAndroidCustomVibration=True

[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/OpenMobileHaptics/StarterPresets")
""" if haptics_enabled else ""
	write_text(root / "Config" / "DefaultGame.ini", game_config)
	write_text(
		root / "Source" / f"{project_name}.Target.cs",
		f"""using UnrealBuildTool;

public class {project_name}Target : TargetRules
{{
	public {project_name}Target(TargetInfo Target) : base(Target)
	{{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("{project_name}");
	}}
}}
""",
	)
	write_text(
		root / "Source" / f"{project_name}Editor.Target.cs",
		f"""using UnrealBuildTool;

public class {project_name}EditorTarget : TargetRules
{{
	public {project_name}EditorTarget(TargetInfo Target) : base(Target)
	{{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("{project_name}");
	}}
}}
""",
	)
	module_root = root / "Source" / project_name
	write_text(
		module_root / f"{project_name}.Build.cs",
		f"""using UnrealBuildTool;

public class {project_name} : ModuleRules
{{
	public {project_name}(ReadOnlyTargetRules Target) : base(Target)
	{{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] {{ "Core", "CoreUObject", "Engine" }});
	}}
}}
""",
	)
	write_text(
		module_root / f"{project_name}.cpp",
		f"""#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, {project_name}, "{project_name}");
""",
	)
	return project


def generate_blueprint_host(root: Path, mode: str) -> Path:
	project_name = f"OpenMobileHapticsBlueprint{mode.title()}"
	plugins = () if mode == "disabled" else ("OpenMobileHaptics",)
	project = root / f"{project_name}.uproject"
	descriptor = {
		"FileVersion": 3,
		"EngineAssociation": "5.8",
		"Category": "Mobile",
		"Description": f"Temporary Blueprint-only Haptics {mode} package.",
		"Plugins": plugin_entries(plugins),
	}
	write_text(project, json.dumps(descriptor, indent="\t") + "\n")
	copy_local_plugins(root, plugins)
	write_text(
		root / "Config" / "DefaultEngine.ini",
		f"""[/Script/EngineSettings.GameMapsSettings]
EditorStartupMap=/Engine/Maps/Entry.Entry
GameDefaultMap=/Engine/Maps/Entry.Entry

[/Script/Engine.RendererSettings]
r.Mobile.ShadingPath=0

[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]
PackageName=com.openmobile.hapticsblueprint{mode}
MinSDKVersion=26
TargetSDKVersion=36
SDKAPILevelOverride=android-36
bBuildForArm64=True
bSupportsVulkan=True
bBuildForES31=True

[/Script/IOSRuntimeSettings.IOSRuntimeSettings]
BundleIdentifier=com.openmobile.hapticsblueprint{mode}
BundleDisplayName=OpenMobile Haptics Blueprint {mode.title()}
BundleName=OpenMobile Haptics Blueprint {mode.title()}
MinimumiOSVersion=IOS_17
bSupportsIPhone=True
bSupportsIPad=True
""",
	)
	custom = "True" if mode == "custom" else "False"
	packaging = (
		'\n[/Script/UnrealEd.ProjectPackagingSettings]\n'
		'+DirectoriesToAlwaysCook=(Path="/OpenMobileHaptics/StarterPresets")\n'
		if mode != "disabled"
		else ""
	)
	write_text(
		root / "Config" / "DefaultGame.ini",
		f"""[/Script/OpenMobileHaptics.OpenMobileHapticsSettings]
bEnableAndroidCustomVibration={custom}
{packaging}""",
	)
	if mode == "disabled":
		write_text(
			root / "Build" / "Android" / "ManifestRequirementsOverride.txt",
			"""<uses-feature android:glEsVersion="0x00030001" android:required="true" />
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.WAKE_LOCK" />
<uses-permission android:name="android.permission.MODIFY_AUDIO_SETTINGS" />
""",
		)
	return project


class Runner:
	def __init__(self, engine_root: Path, output_root: Path) -> None:
		self.engine_root = engine_root
		self.output_root = output_root
		self.logs = output_root / "logs"
		self.hosts = output_root / "hosts"
		self.packages = output_root / "packages"
		self.logs.mkdir(parents=True, exist_ok=True)
		self.hosts.mkdir(parents=True, exist_ok=True)
		self.packages.mkdir(parents=True, exist_ok=True)
		self.build = engine_root / "Engine" / "Build" / "BatchFiles" / "Mac" / "Build.sh"
		self.run_uat = engine_root / "Engine" / "Build" / "BatchFiles" / "RunUAT.sh"
		self.editor = (
			engine_root
			/ "Engine"
			/ "Binaries"
			/ "Mac"
			/ "UnrealEditor.app"
			/ "Contents"
			/ "MacOS"
			/ "UnrealEditor"
		)
		for tool in (self.build, self.run_uat, self.editor):
			if not tool.is_file():
				raise ReleaseValidationError(f"missing Unreal tool: {tool}")

	def environment(self) -> dict[str, str]:
		environment = os.environ.copy()
		ndk = environment.get(
			"OPENMOBILE_ANDROID_NDK_ROOT",
			"/opt/homebrew/share/android-commandlinetools/ndk/27.2.12479018",
		)
		environment["ANDROID_NDK_ROOT"] = ndk
		environment["NDKROOT"] = ndk
		environment["JAVA_HOME"] = environment.get(
			"OPENMOBILE_JAVA_HOME",
			"/opt/homebrew/opt/openjdk@17",
		)
		return environment

	def run(
		self,
		label: str,
		arguments: list[str],
		*,
		allow_ios_signing_failure: bool = False,
	) -> None:
		log = self.logs / f"{label}.log"
		print(f"[{label}] running")
		with log.open("w", encoding="utf-8") as output:
			completed = subprocess.run(
				arguments,
				cwd=REPOSITORY_ROOT,
				env=self.environment(),
				stdout=output,
				stderr=subprocess.STDOUT,
				text=True,
				check=False,
			)
		if completed.returncode == 0:
			print(f"[{label}] passed")
			return
		contents = log.read_text(encoding="utf-8", errors="replace")
		if allow_ios_signing_failure and (
			"requires a development team" in contents
			or "Signing for" in contents and "requires" in contents
		):
			print(f"[{label}] compiled; local signing identity is unavailable")
			return
		matches = [line for line in contents.splitlines() if FAILURE_PATTERN.search(line)]
		for line in matches[-20:]:
			print(line, file=sys.stderr)
		raise ReleaseValidationError(f"{label} failed, see {log}")

	def build_target(
		self,
		project: Path,
		target: str,
		platform: str,
		configuration: str,
		label: str,
		*,
		allow_ios_signing_failure: bool = False,
	) -> None:
		arguments = [
			str(self.build),
			target,
			platform,
			configuration,
			f"-Project={project}",
			"-NoUBA",
			"-WaitMutex",
		]
		if platform == "Android":
			arguments.append("-Architecture=arm64")
		self.run(
			label,
			arguments,
			allow_ios_signing_failure=allow_ios_signing_failure,
		)

	def static(self) -> None:
		self.run("architecture", [str(REPOSITORY_ROOT / "Scripts" / "validate_architecture.sh")])
		self.run(
			"haptics-python",
			[
				sys.executable,
				"-m",
				"unittest",
				"Scripts.tests.test_haptics_plugin_boundaries",
				"Scripts.tests.test_haptics_android_manifest",
				"Scripts.tests.test_haptics_artifacts",
				"Scripts.tests.test_haptics_device_validation",
			],
		)

	def matrix(self) -> None:
		for variant, plugins in MATRIX.items():
			root = self.hosts / variant
			project = generate_code_host(root, variant, plugins)
			self.build_target(
				project,
				f"OpenMobileHaptics{variant}",
				"Mac",
				"Development",
				f"matrix-{variant.lower()}",
			)

	def compile(self) -> None:
		variant = "HapticsOnly"
		root = self.hosts / variant
		project = generate_code_host(root, variant, MATRIX[variant])
		target = f"OpenMobileHaptics{variant}"
		self.build_target(
			project,
			f"{target}Editor",
			"Mac",
			"Development",
			"compile-mac-editor",
		)
		for platform in ("Android", "IOS"):
			for configuration in ("Development", "Shipping"):
				self.build_target(
					project,
					target,
					platform,
					configuration,
					f"compile-{platform.lower()}-{configuration.lower()}",
					allow_ios_signing_failure=platform == "IOS",
				)
				if platform == "IOS":
					self.run(
						f"inspect-ios-enabled-{configuration.lower()}",
						[
							sys.executable,
							str(
								REPOSITORY_ROOT
								/ "Scripts"
								/ "validate_haptics_artifacts.py"
							),
							"ios",
							"--project-root",
							str(root),
							"--configuration",
							configuration,
							"--mode",
							"enabled",
							"--signature",
							"unsigned",
						],
					)

	def android(self) -> None:
		for mode in ("custom", "semantic", "disabled"):
			root = self.hosts / f"Blueprint{mode.title()}"
			project = generate_blueprint_host(root, mode)
			archive = self.packages / f"android-{mode}"
			self.run(
				f"package-android-{mode}",
				[
					str(self.run_uat),
					"BuildCookRun",
					"-WaitForUATMutex",
					f"-project={project}",
					"-noP4",
					"-platform=Android",
					"-clientconfig=Shipping",
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
			)
			self.run(
				f"inspect-android-{mode}",
				[
					sys.executable,
					str(REPOSITORY_ROOT / "Scripts" / "validate_haptics_artifacts.py"),
					"android",
					"--project-root",
					str(root),
					"--package-root",
					str(archive),
					"--mode",
					mode,
				],
			)

	def inspect_ios_cook(self, project_root: Path, enabled: bool) -> None:
		project = next(project_root.glob("*.uproject"))
		registry = (
			project_root
			/ "Saved"
			/ "Cooked"
			/ "IOS"
			/ project.stem
			/ "Metadata"
			/ "DevelopmentAssetRegistry.bin"
		)
		if not registry.is_file():
			raise ReleaseValidationError(f"missing cooked iOS asset registry: {registry}")
		report = self.output_root / "reports" / f"ios-{project_root.name}"
		self.run(
			f"dump-ios-assets-{project_root.name.lower()}",
			[
				str(self.editor),
				str(project),
				"-run=DumpAssetRegistry",
				f"-Path={registry}",
				f"-OutDir={report}",
				"-unattended",
				"-nop4",
				"-nosplash",
				"-NullRHI",
				"-stdout",
				"-FullStdOutLogOutput",
			],
		)
		dump = "\n".join(
			path.read_text(encoding="utf-8", errors="replace")
			for path in report.glob("*.txt")
		)
		for asset in (
			"Combat_Impact",
			"Notification_Warning",
			"OpenMobileStarterHaptics",
			"Reward_Success",
			"UI_Confirm",
			"UI_Selection",
			"Vehicle_Bump",
		):
			present = f"/OpenMobileHaptics/StarterPresets/{asset}" in dump
			if present != enabled:
				raise ReleaseValidationError(
					f"cooked iOS asset state does not match Haptics mode: {asset}"
				)

	def ios(self) -> None:
		variant = "CoreOnly"
		root = self.hosts / variant
		project = generate_code_host(root, variant, MATRIX[variant])
		target = f"OpenMobileHaptics{variant}"
		for configuration in ("Development", "Shipping"):
			self.build_target(
				project,
				target,
				"IOS",
				configuration,
				f"compile-ios-disabled-{configuration.lower()}",
				allow_ios_signing_failure=True,
			)
			self.run(
				f"inspect-ios-disabled-{configuration.lower()}",
				[
					sys.executable,
					str(REPOSITORY_ROOT / "Scripts" / "validate_haptics_artifacts.py"),
					"ios",
					"--project-root",
					str(root),
					"--configuration",
					configuration,
					"--mode",
					"disabled",
					"--signature",
					"unsigned",
				],
			)

		cook_hosts = (
			("custom", "IOSCookEnabled", ("OpenMobileHaptics",)),
			("disabled", "IOSCookDisabled", ("OpenMobileCore",)),
		)
		for mode, variant, plugins in cook_hosts:
			cook_root = self.hosts / variant
			cook_project = generate_code_host(cook_root, variant, plugins)
			self.build_target(
				cook_project,
				f"OpenMobileHaptics{variant}Editor",
				"Mac",
				"Development",
				f"compile-ios-cook-editor-{mode}",
			)
			self.run(
				f"cook-ios-{mode}",
				[
					str(self.editor),
					str(cook_project),
					"-run=Cook",
					"-TargetPlatform=IOS",
					"-CookAll",
					"-unversioned",
					"-unattended",
					"-nop4",
					"-nosplash",
					"-NullRHI",
					"-stdout",
					"-FullStdOutLogOutput",
				],
			)
			self.inspect_ios_cook(cook_root, mode != "disabled")

	def automation(self, project: Path | None) -> None:
		if project is None:
			root = self.hosts / "Automation"
			project = generate_code_host(
				root,
				"Automation",
				(
					"OpenMobileHapticsUMG",
					"OpenMobileHapticsGameplayAbilities",
					"OpenMobileHapticsSequencer",
				),
			)
			self.build_target(
				project,
				"OpenMobileHapticsAutomationEditor",
				"Mac",
				"Development",
				"automation-editor",
			)
			stage_editor_modules(root)
		log = self.logs / "haptics-automation.log"
		self.run(
			"automation-process",
			[
				str(self.editor),
				str(project),
				"-unattended",
				"-nop4",
				"-nosplash",
				"-NullRHI",
				f"-abslog={log}",
				"-ExecCmds=Automation RunTests OpenMobile.Haptics; Quit",
				"-TestExit=Automation Test Queue Empty",
			],
		)
		contents = log.read_text(encoding="utf-8", errors="replace")
		if "Automation Test Queue Empty" not in contents or "Automation Test Failed" in contents:
			raise ReleaseValidationError(f"Haptics automation failed, see {log}")


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Build and inspect the OpenMobile Haptics release matrix."
	)
	parser.add_argument("--engine-root", type=Path, default=DEFAULT_ENGINE_ROOT)
	parser.add_argument("--output-root", type=Path)
	parser.add_argument(
		"--phase",
		action="append",
		choices=("static", "matrix", "compile", "android", "ios", "automation", "all"),
	)
	parser.add_argument(
		"--automation-project",
		type=Path,
		help="Use an existing built project instead of a fresh validation host.",
	)
	return parser.parse_args()


def main() -> int:
	arguments = parse_arguments()
	stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
	output_root = arguments.output_root or Path(f"/tmp/openmobile-haptics-release-{stamp}")
	phases = arguments.phase or ["all"]
	if "all" in phases:
		phases = ["static", "matrix", "compile", "android", "ios", "automation"]
	try:
		runner = Runner(arguments.engine_root.resolve(), output_root.resolve())
		for phase in phases:
			if phase == "static":
				runner.static()
			elif phase == "matrix":
				runner.matrix()
			elif phase == "compile":
				runner.compile()
			elif phase == "android":
				runner.android()
			elif phase == "ios":
				runner.ios()
			elif phase == "automation":
				runner.automation(
					arguments.automation_project.resolve()
					if arguments.automation_project
					else None
				)
	except (OSError, ReleaseValidationError) as error:
		print(f"Haptics release validation failed: {error}", file=sys.stderr)
		return 1
	print(f"Haptics release validation passed. Logs: {output_root / 'logs'}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
