import json
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CORE_PLUGIN = REPOSITORY_ROOT / "Foundation" / "OpenMobileCore"
HAPTICS_PLUGIN = REPOSITORY_ROOT / "Native" / "OpenMobileHaptics"


def load_descriptor() -> dict:
	with (HAPTICS_PLUGIN / "OpenMobileHaptics.uplugin").open(
		encoding="utf-8"
	) as file:
		return json.load(file)


def load_android_bridge() -> str:
	return (
		HAPTICS_PLUGIN
		/ "Source"
		/ "OpenMobileHapticsAndroid"
		/ "Private"
		/ "Android"
		/ "src"
		/ "com"
		/ "openmobile"
		/ "haptics"
		/ "OpenMobileHapticsBridgeV1.java"
	).read_text(encoding="utf-8")


def load_ios_bridge() -> str:
	return (
		HAPTICS_PLUGIN
		/ "Source"
		/ "OpenMobileHapticsIOS"
		/ "Private"
		/ "OpenMobileHapticsIOSBridge.mm"
	).read_text(encoding="utf-8")


class HapticsPluginBoundaryTests(unittest.TestCase):
	def test_plugin_is_independently_enabled(self) -> None:
		self.assertTrue((HAPTICS_PLUGIN / "OpenMobileHaptics.uplugin").is_file())

		parent = HAPTICS_PLUGIN.parent
		while parent != REPOSITORY_ROOT:
			self.assertEqual([], list(parent.glob("*.uplugin")), str(parent))
			parent = parent.parent

	def test_modules_match_runtime_platform_and_editor_boundaries(self) -> None:
		modules = {module["Name"]: module for module in load_descriptor()["Modules"]}

		self.assertEqual(
			{
				"OpenMobileHaptics",
				"OpenMobileHapticsAndroid",
				"OpenMobileHapticsIOS",
				"OpenMobileHapticsEditor",
			},
			set(modules),
		)
		self.assertEqual("Runtime", modules["OpenMobileHaptics"]["Type"])
		self.assertNotIn("PlatformAllowList", modules["OpenMobileHaptics"])
		self.assertEqual(
			["Android"],
			modules["OpenMobileHapticsAndroid"]["PlatformAllowList"],
		)
		self.assertEqual(
			["IOS"],
			modules["OpenMobileHapticsIOS"]["PlatformAllowList"],
		)
		self.assertEqual("Editor", modules["OpenMobileHapticsEditor"]["Type"])

		for module_name in modules:
			module_root = HAPTICS_PLUGIN / "Source" / module_name
			self.assertTrue((module_root / f"{module_name}.Build.cs").is_file())
			self.assertTrue(
				(module_root / "Private" / f"{module_name}Module.cpp").is_file()
			)

	def test_core_dependency_is_declared_and_remains_one_way(self) -> None:
		descriptor_dependencies = {
			plugin["Name"] for plugin in load_descriptor().get("Plugins", [])
		}
		self.assertEqual({"OpenMobileCore"}, descriptor_dependencies)

		build_rules = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHaptics"
			/ "OpenMobileHaptics.Build.cs"
		).read_text(encoding="utf-8")
		self.assertIn('"OpenMobileCore"', build_rules)

		core_inputs = list((CORE_PLUGIN / "Source").rglob("*"))
		core_inputs.append(CORE_PLUGIN / "OpenMobileCore.uplugin")
		for path in core_inputs:
			if not path.is_file() or path.suffix not in {
				".cs",
				".cpp",
				".h",
				".uplugin",
			}:
				continue
			self.assertNotIn(
				"OpenMobileHaptics",
				path.read_text(encoding="utf-8"),
				str(path),
			)

	def test_core_contains_no_haptics_specific_contract(self) -> None:
		for path in (CORE_PLUGIN / "Source").rglob("*"):
			if not path.is_file() or path.suffix not in {".cs", ".cpp", ".h"}:
				continue
			contents = path.read_text(encoding="utf-8")
			for forbidden_token in (
				"AHAP",
				"Haptic",
				"PlaybackHandle",
				"Vibration",
			):
				self.assertNotIn(forbidden_token, contents, str(path))

	def test_common_runtime_is_platform_neutral_and_headless(self) -> None:
		common_module = HAPTICS_PLUGIN / "Source" / "OpenMobileHaptics"
		for path in common_module.rglob("*"):
			if not path.is_file() or path.suffix not in {".cs", ".cpp", ".h"}:
				continue
			contents = path.read_text(encoding="utf-8")
			for forbidden_token in (
				"Android/",
				"AndroidJNI",
				"JNIEnv",
				"jni.h",
				"CoreHaptics/",
				"UIKit/",
				"CHHaptic",
				"UMG",
				"GameplayAbilities",
				"MovieScene",
				"Slate",
				"UnrealEd",
			):
				self.assertNotIn(forbidden_token, contents, str(path))

	def test_platform_and_gamepad_code_cannot_cross_boundaries(self) -> None:
		android_tokens = (
			"AndroidJNI",
			"JNIEnv",
			"jni.h",
			"GameActivity",
			"VibrationEffect",
		)
		ios_tokens = (
			"CoreHaptics/",
			"UIKit/",
			"CHHaptic",
			"UIFeedbackGenerator",
		)
		gamepad_tokens = (
			"FForceFeedback",
			"IInputInterface",
			"SetForceFeedbackChannelValue",
			"SetForceFeedbackChannelValues",
		)

		for path in (HAPTICS_PLUGIN / "Source").rglob("*"):
			if not path.is_file() or path.suffix not in {
				".cs",
				".cpp",
				".h",
				".java",
				".kt",
				".mm",
				".xml",
			}:
				continue
			contents = path.read_text(encoding="utf-8")
			relative_path = path.relative_to(HAPTICS_PLUGIN / "Source")
			if path.suffix in {".java", ".kt", ".xml"}:
				self.assertEqual("OpenMobileHapticsAndroid", relative_path.parts[0])
			if path.suffix == ".mm":
				self.assertEqual("OpenMobileHapticsIOS", relative_path.parts[0])
			if any(token in contents for token in android_tokens):
				self.assertEqual("OpenMobileHapticsAndroid", relative_path.parts[0])
			if any(token in contents for token in ios_tokens):
				self.assertEqual("OpenMobileHapticsIOS", relative_path.parts[0])
			for token in gamepad_tokens:
				self.assertNotIn(token, contents, str(path))

	def test_base_plugin_has_no_optional_integration_dependencies(self) -> None:
		for build_rules in (HAPTICS_PLUGIN / "Source").glob("*/*.Build.cs"):
			contents = build_rules.read_text(encoding="utf-8")
			for forbidden_dependency in (
				"EnhancedInput",
				"GameplayAbilities",
				"InputCore",
				"InputDevice",
				"MovieScene",
				"OpenMobileAds",
				"OpenMobileDevice",
				"OpenMobileMedia",
				"OpenMobileSensors",
				"SlateCore",
				"UMG",
			):
				self.assertNotIn(forbidden_dependency, contents, str(build_rules))

	def test_public_consumer_uses_only_the_documented_haptics_header(self) -> None:
		consumer = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsEditor"
			/ "Private"
			/ "Tests"
			/ "OpenMobileHapticsPublicConsumerTests.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('#include "OpenMobileHaptics.h"', consumer)
		for forbidden_path in ("/Internal/", "/Private/"):
			self.assertNotIn(forbidden_path, consumer)

		umbrella = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHaptics"
			/ "Public"
			/ "OpenMobileHaptics.h"
		).read_text(encoding="utf-8")
		for public_contract in (
			"OpenMobileHapticLibrary.h",
			"OpenMobileHapticsAsyncAction.h",
			"OpenMobileHapticsNative.h",
			"OpenMobileHapticsSettings.h",
			"OpenMobileHapticsSubsystem.h",
			"OpenMobileHapticsTypes.h",
		):
			self.assertIn(public_contract, umbrella)

	def test_native_backend_seam_stays_internal(self) -> None:
		internal_module = HAPTICS_PLUGIN / "Source" / "OpenMobileHaptics"
		self.assertTrue(
			(internal_module / "Internal" / "IOpenMobileHapticsBackend.h").is_file()
		)
		self.assertTrue(
			(
				internal_module
				/ "Internal"
				/ "OpenMobileHapticsBackendRegistry.h"
			).is_file()
		)
		for public_header in (internal_module / "Public").glob("*.h"):
			self.assertNotIn(
				'#include "IOpenMobileHapticsBackend.h"',
				public_header.read_text(encoding="utf-8"),
				str(public_header),
			)

		module = (
			internal_module / "Private" / "OpenMobileHapticsModule.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FOpenMobileHapticsBackendRegistry::Start()", module)
		self.assertIn(
			"FOpenMobileHapticsBackendRegistry::BeginShutdown()",
			module,
		)

	def test_application_lifecycle_binding_is_process_owned(self) -> None:
		common_module = HAPTICS_PLUGIN / "Source" / "OpenMobileHaptics"
		module = (
			common_module / "Private" / "OpenMobileHapticsModule.cpp"
		).read_text(encoding="utf-8")
		subsystem = (
			common_module / "Private" / "OpenMobileHapticsSubsystem.cpp"
		).read_text(encoding="utf-8")

		for binding in (
			"ApplicationWillDeactivateDelegate.AddLambda",
			"ApplicationWillEnterBackgroundDelegate.AddLambda",
			"ApplicationHasEnteredForegroundDelegate.AddLambda",
			"ApplicationHasReactivatedDelegate.AddLambda",
			"GetApplicationWillTerminateDelegate().AddLambda",
		):
			self.assertEqual(1, module.count(binding), binding)
		for event in (
			"WillDeactivate",
			"WillEnterBackground",
			"HasEnteredForeground",
			"HasReactivated",
			"WillTerminate",
		):
			self.assertIn(f"EOpenMobileHapticsLifecycleEvent::{event}", module)
		self.assertNotIn("FCoreDelegates", subsystem)

	def test_channel_admission_covers_controllable_submission_paths(self) -> None:
		common_module = HAPTICS_PLUGIN / "Source" / "OpenMobileHaptics"
		policy_header = (
			common_module / "Internal" / "OpenMobileHapticsChannelPolicy.h"
		)
		policy_source = (
			common_module / "Private" / "OpenMobileHapticsChannelPolicy.cpp"
		)
		settings = (
			common_module / "Private" / "OpenMobileHapticsSettings.cpp"
		).read_text(encoding="utf-8")
		subsystem = (
			common_module / "Private" / "OpenMobileHapticsSubsystem.cpp"
		).read_text(encoding="utf-8")

		self.assertTrue(policy_header.is_file())
		self.assertTrue(policy_source.is_file())
		for channel in ("UI", "Gameplay", "Alerts", "Cinematic", "Critical"):
			self.assertIn(f'TEXT("{channel}")', settings)
		self.assertEqual(6, subsystem.count("AdmitChannelRequest("))
		self.assertIn("State.ChannelArbiter.Release(RequestId)", subsystem)
		self.assertNotIn(
			"IOpenMobileHapticsBackend",
			policy_header.read_text(encoding="utf-8"),
		)

	def test_rate_limits_are_bounded_and_cover_all_submission_paths(self) -> None:
		common_module = HAPTICS_PLUGIN / "Source" / "OpenMobileHaptics"
		limiter_header = (
			common_module / "Internal" / "OpenMobileHapticsRateLimiter.h"
		).read_text(encoding="utf-8")
		limiter_source = (
			common_module / "Private" / "OpenMobileHapticsRateLimiter.cpp"
		).read_text(encoding="utf-8")
		settings = (
			common_module / "Public" / "OpenMobileHapticsSettings.h"
		).read_text(encoding="utf-8")
		types = (
			common_module / "Public" / "OpenMobileHapticsTypes.h"
		).read_text(encoding="utf-8")
		subsystem = (
			common_module / "Private" / "OpenMobileHapticsSubsystem.cpp"
		).read_text(encoding="utf-8")

		for contract in (
			"HardMaximumChannelSubmissionsPerSecond = 30",
			"HardMaximumGlobalSubmissionsPerSecond = 60",
			"MaximumTrackedChannels = 128",
			"MaximumTrackedEffects = 1024",
			"MaximumTrackedEquivalentRequests = 256",
			"using FClock = TFunction<double()>;",
			"FCriticalSection Mutex;",
			"RecentGlobalSubmissionTimes",
			"RecentNonCriticalSubmissionTimes",
		):
			self.assertIn(contract, limiter_header)
		for bounded_state in (
			"Channels.Num() >= MaximumTrackedChannels",
			"Effects.Num() >= MaximumTrackedEffects",
			"EquivalentRequests.Num() >= MaximumTrackedEquivalentRequests",
		):
			self.assertIn(bounded_state, limiter_source)

		self.assertIn("MaximumSubmissionsPerSecond", settings)
		self.assertIn("UIRequestDebounceSeconds", settings)
		self.assertIn("bRetainRateLimitStateAcrossForeground", settings)
		self.assertIn("EOpenMobileHapticSuppressionReason", types)
		self.assertIn("SuppressionReason", types)
		self.assertIn("State->RateLimiter.Reset()", subsystem)
		self.assertNotIn("RateLimiter.ShouldSuppress", subsystem)

		path_boundaries = (
			(
				"UOpenMobileHapticsSubsystem::SubmitSemanticOrOverride(",
				"UOpenMobileHapticsSubsystem::SubmitOneShot(",
			),
			(
				"UOpenMobileHapticsSubsystem::SubmitOneShotInternal(",
				"UOpenMobileHapticsSubsystem::SubmitNamedPattern(",
			),
			(
				"UOpenMobileHapticsSubsystem::SubmitNamedPatternInternal(",
				"UOpenMobileHapticsSubsystem::CompleteControlledRequest(",
			),
		)
		for start, end in path_boundaries:
			path = subsystem[subsystem.index(start):subsystem.index(end)]
			self.assertEqual(1, path.count("RateLimiter.Evaluate("), start)
			self.assertIn("MakeRateLimitedPlaybackResult", path)

	def test_performance_budgets_and_hot_paths_are_explicit(self) -> None:
		common = HAPTICS_PLUGIN / "Source" / "OpenMobileHaptics"
		budget = (
			common / "Internal" / "OpenMobileHapticsBudgetPolicy.h"
		).read_text(encoding="utf-8")
		for contract in (
			"HardMaximumActiveHandles = 128",
			"HardMaximumQueuedHandles = 256",
			"HardMaximumQueueDepthPerChannel = 64",
			"HardMaximumPreparedPatterns = 128",
			"HardMaximumPreparedPatternBytes =",
			"64 * 1024 * 1024",
			"HardMaximumPatternEvents = 4096",
			"HardMaximumDiagnosticEvents = 512",
		):
			self.assertIn(contract, budget)

		dynamic_policy = (
			common / "Internal" / "OpenMobileHapticsDynamicParameterPolicy.h"
		).read_text(encoding="utf-8")
		self.assertIn("TArray<uint64> ScratchRequestIds;", dynamic_policy)
		dynamic_policy_source = (
			common / "Private" / "OpenMobileHapticsDynamicParameterPolicy.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ScratchRequestIds.Reset()", dynamic_policy_source)
		self.assertNotIn("Playbacks.GetKeys", dynamic_policy_source)

		types = (
			common / "Public" / "OpenMobileHapticsTypes.h"
		).read_text(encoding="utf-8")
		for metric in (
			"DroppedRequestCount",
			"PeakQueuedPlaybackCount",
			"TimelineCacheHitCount",
			"TimelineCacheMissCount",
			"TimelineCacheEvictionCount",
			"TimelineCacheMemoryBytes",
			"PreparationLatencyMilliseconds",
			"NativeSubmissionLatencyMilliseconds",
		):
			self.assertIn(metric, types)

		subsystem = (
			common / "Private" / "OpenMobileHapticsSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("DynamicParameterBatch", subsystem)
		self.assertNotIn(
			"TArray<FOpenMobileHapticsScheduledDynamicParameterUpdate> Updates;",
			subsystem,
		)
		for trace_scope in (
			"OpenMobileHaptics_SubmitSemantic",
			"OpenMobileHaptics_SubmitOneShot",
			"OpenMobileHaptics_SubmitNamedPattern",
			"OpenMobileHaptics_ResolveOverlap",
			"OpenMobileHaptics_PreloadNamedLibraries",
			"OpenMobileHaptics_ApplicationLifecycle",
			"OpenMobileHaptics_Shutdown",
		):
			self.assertIn(
				f"TRACE_CPUPROFILER_EVENT_SCOPE({trace_scope})",
				subsystem,
			)
		timeline_manager = (
			common / "Private" / "OpenMobileHapticsTimelineManager.cpp"
		).read_text(encoding="utf-8")
		self.assertIn(
			"TRACE_CPUPROFILER_EVENT_SCOPE(OpenMobileHaptics_ResolveTimeline)",
			timeline_manager,
		)

		path_boundaries = (
			(
				"UOpenMobileHapticsSubsystem::SubmitSemanticOrOverride(",
				"UOpenMobileHapticsSubsystem::SubmitOneShot(",
			),
			(
				"UOpenMobileHapticsSubsystem::SubmitOneShotInternal(",
				"UOpenMobileHapticsSubsystem::SubmitNamedPattern(",
			),
			(
				"UOpenMobileHapticsSubsystem::SubmitNamedPatternInternal(",
				"UOpenMobileHapticsSubsystem::CompleteControlledRequest(",
			),
		)
		for start, end in path_boundaries:
			path = subsystem[subsystem.index(start):subsystem.index(end)]
			for forbidden_operation in (
				"LoadSynchronous",
				"TryLoad",
				"RequestAsyncLoad",
				"StaticLoadObject",
				"LoadObject<",
				"FFileHelper",
				"FJsonSerializer",
			):
				self.assertNotIn(forbidden_operation, path, start)

	def test_background_alert_support_matches_platform_contracts(self) -> None:
		android_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsAndroid"
			/ "Private"
			/ "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		ios_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsIOS"
			/ "Private"
			/ "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		android_bridge = load_android_bridge()

		self.assertIn("Capabilities.BackgroundAlerts", android_backend)
		self.assertIn("EOpenMobileHapticSupportState::Supported", android_backend)
		self.assertIn("VibrationAttributes.USAGE_NOTIFICATION", android_bridge)
		self.assertIn("AudioAttributes.USAGE_NOTIFICATION_EVENT", android_bridge)
		self.assertIn("Capabilities.BackgroundAlerts", ios_backend)
		self.assertIn("EOpenMobileHapticSupportState::Unsupported", ios_backend)

	def test_mixing_capability_matches_plugin_owned_platform_paths(self) -> None:
		android_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsAndroid"
			/ "Private"
			/ "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		ios_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsIOS"
			/ "Private"
			/ "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")

		self.assertIn(
			"Capabilities.Mixing = EOpenMobileHapticSupportState::Unsupported",
			android_backend,
		)
		self.assertIn("Capabilities.Mixing = bCoreHapticsEnabled", ios_backend)
		for path in HAPTICS_PLUGIN.rglob("*"):
			if not path.is_file() or path.suffix not in {".cpp", ".h", ".mm"}:
				continue
			contents = path.read_text(encoding="utf-8")
			self.assertNotIn("IForceFeedbackSystem", contents, str(path))
			self.assertNotIn("SetForceFeedbackChannelValues", contents, str(path))

	def test_native_availability_probes_are_side_effect_free(self) -> None:
		android_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsAndroid"
		android_probe = (
			android_root
			/ "Private"
			/ "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		android_bridge = load_android_bridge()
		self.assertIn("hasVibrator", android_bridge)
		self.assertIn("hasAmplitudeControl", android_bridge)
		self.assertIn("areEffectsSupported", android_bridge)
		self.assertIn("arePrimitivesSupported", android_bridge)
		self.assertIn("getEnvelopeEffectInfo", android_bridge)
		self.assertIn("getMaxControlPointDurationMillis", android_bridge)
		self.assertIn("getMinFrequencyHz", android_bridge)
		self.assertIn("getMaxFrequencyHz", android_bridge)
		self.assertIn("MaximumControlPointDurationMillis", android_probe)
		self.assertIn("FrequencyRange", android_probe)
		android_query = android_bridge.split(
			"static long[] queryCapabilities", 1
		)[1].split("static int playSemantic", 1)[0]
		self.assertNotIn(".vibrate(", android_query)
		self.assertNotIn("requestPermissions", android_bridge)
		self.assertIn(
			"FOpenMobileHapticsBackendRegistry::RegisterBackend",
			(android_probe + (
				android_root
				/ "Private"
				/ "OpenMobileHapticsAndroidModule.cpp"
			).read_text(encoding="utf-8")),
		)

		ios_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsIOS"
		ios_probe = (
			ios_root / "Private" / "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		ios_bridge = load_ios_bridge()
		ios_hardware_query = ios_bridge.split(
			"QueryHardware() override", 1
		)[1].split("CreateEngine() override", 1)[0]
		self.assertIn("capabilitiesForHardware", ios_hardware_query)
		self.assertIn("supportsHaptics", ios_hardware_query)
		self.assertIn("supportsAudio", ios_hardware_query)
		self.assertNotIn("initAndReturnError", ios_hardware_query)
		self.assertNotIn("startAndReturnError", ios_hardware_query)
		self.assertIn(
			"FOpenMobileHapticsBackendRegistry::RegisterBackend",
			(ios_probe + (
				ios_root
				/ "Private"
				/ "OpenMobileHapticsIOSModule.cpp"
			).read_text(encoding="utf-8")),
		)

	def test_one_shot_native_paths_are_platform_owned(self) -> None:
		android_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsAndroid"
		android_backend = (
			android_root / "Private" / "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		android_bridge = load_android_bridge()
		self.assertIn(
			"Bridge.PlayOneShot",
			android_backend,
		)
		self.assertIn("bNativeDurationKnown", android_backend)
		self.assertIn("bNativeClamped", android_backend)
		self.assertIn("bNativeIntensityKnown", android_backend)
		self.assertIn("Bridge.StopAll", android_backend)
		one_shot_bridge = android_bridge.split(
			"static int playOneShot", 1
		)[1].split("static boolean stopAll", 1)[0]
		self.assertIn("VibrationEffect.createOneShot", one_shot_bridge)
		self.assertIn("VibrationEffect.createPredefined", one_shot_bridge)
		self.assertIn("systemHapticsEnabled", one_shot_bridge)
		self.assertIn("HAPTIC_FEEDBACK_ENABLED", android_bridge)
		self.assertIn("vibrate(vibrator", one_shot_bridge)
		self.assertIn("usedDefaultAmplitude", one_shot_bridge)
		self.assertIn("static boolean stopAll", android_bridge)
		self.assertIn("vibrator.cancel()", android_bridge)
		self.assertNotIn("FLAG_IGNORE", one_shot_bridge)

		ios_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsIOS"
			/ "Private"
			/ "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		self.assertIn("FOpenMobileHapticsIOSBackend::SubmitOneShot", ios_backend)
		self.assertIn("PlaySystemVibration", ios_backend)
		self.assertIn("kSystemSoundID_Vibrate", load_ios_bridge())
		self.assertIn(
			"EOpenMobileHapticsSemanticBehavior::ImpactMedium",
			ios_backend,
		)

	def test_semantic_playback_stays_in_platform_backends(self) -> None:
		android_bridge = load_android_bridge()
		android_upl = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsAndroid"
			/ "Private"
			/ "Android"
			/ "OpenMobileHaptics_Android_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertIn("performHapticFeedback", android_bridge)
		self.assertIn("HapticFeedbackConstants", android_bridge)
		self.assertIn("HAPTIC_FEEDBACK_ENABLED", android_bridge)
		self.assertIn("VibrationAttributes.USAGE_TOUCH", android_bridge)
		self.assertIn("VibrationAttributes.USAGE_MEDIA", android_bridge)
		self.assertIn("VibrationAttributes.USAGE_NOTIFICATION", android_bridge)
		self.assertIn("android.permission.VIBRATE", android_upl)
		for bypass_token in (
			"FLAG_IGNORE_GLOBAL_SETTING",
			"FLAG_IGNORE_VIEW_SETTING",
			"FLAG_BYPASS_INTERRUPTION_POLICY",
		):
			self.assertNotIn(bypass_token, android_bridge)
			for public_header in (
				HAPTICS_PLUGIN / "Source" / "OpenMobileHaptics" / "Public"
			).glob("*.h"):
				self.assertNotIn(
					bypass_token,
					public_header.read_text(encoding="utf-8"),
					str(public_header),
				)

		ios_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsIOS"
			/ "Private"
			/ "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		ios_bridge = load_ios_bridge()
		self.assertIn("UISelectionFeedbackGenerator", ios_bridge)
		self.assertIn("UIImpactFeedbackGenerator", ios_bridge)
		self.assertIn("UINotificationFeedbackGenerator", ios_bridge)
		self.assertIn("ImpactGenerators", ios_bridge)
		self.assertIn("[ImpactGenerators[Index] prepare]", ios_bridge)
		self.assertIn("dispatch_after", ios_bridge)
		self.assertIn("RunOnMainQueue", ios_bridge)
		self.assertIn("releaseObjects", ios_bridge)
		self.assertIn("initAndReturnError", ios_bridge)
		self.assertEqual(1, ios_bridge.count("[[CHHapticEngine alloc]"))
		self.assertIn("stoppedHandler", ios_bridge)
		self.assertIn("resetHandler", ios_bridge)
		apple_service = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHaptics"
			/ "Private"
			/ "OpenMobileHapticsAppleBridgeService.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("EnsureEngine", apple_service)
		self.assertIn("BeginShutdown", ios_backend)
		self.assertNotIn("#import <CoreHaptics", ios_backend)

	def test_android_custom_vibration_configuration_is_canonical(self) -> None:
		settings = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHaptics"
			/ "Public"
			/ "OpenMobileHapticsSettings.h"
		).read_text(encoding="utf-8")
		self.assertIn("bEnableAndroidCustomVibration", settings)
		self.assertNotIn("bPackageCustomVibration", settings)

		android_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsAndroid"
		upl = (
			android_root
			/ "Private"
			/ "Android"
			/ "OpenMobileHaptics_Android_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertIn("<setBoolFromProperty", upl)
		self.assertIn('ini="Game"', upl)
		self.assertIn(
			'property="bEnableAndroidCustomVibration"',
			upl,
		)
		self.assertIn(
			'<if condition="OpenMobileHapticsCustomVibrationEnabled">',
			upl,
		)
		self.assertIn("<addPermission", upl)
		self.assertIn("<removePermission", upl)
		self.assertEqual(2, upl.count("android.permission.VIBRATE"))

		backend = (
			android_root / "Private" / "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ApplyCapabilityMask", backend)
		self.assertIn("IsCustomPlaybackConfigured", backend)
		self.assertIn("bEnableAndroidCustomVibration", backend)
		self.assertIn("EOpenMobileErrorCode::NotConfigured", backend)

		common_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHaptics"
			/ "Internal"
			/ "IOpenMobileHapticsBackend.h"
		).read_text(encoding="utf-8")
		self.assertIn("IsCustomPlaybackConfigured", common_backend)

	def test_apple_transient_patterns_use_one_guarded_native_pattern(self) -> None:
		ios_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsIOS"
		bridge = load_ios_bridge()
		backend = (
			ios_root / "Private" / "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		manager = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHaptics"
			/ "Private"
			/ "OpenMobileHapticsTimelineManager.cpp"
		).read_text(encoding="utf-8")
		policy = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHaptics"
			/ "Private"
			/ "OpenMobileHapticsAppleTransientPolicy.cpp"
		).read_text(encoding="utf-8")

		self.assertIn("CHHapticEventTypeHapticTransient", bridge)
		self.assertIn("CHHapticEventParameterIDHapticIntensity", bridge)
		self.assertIn("CHHapticEventParameterIDHapticSharpness", bridge)
		self.assertIn("relativeTime:Pattern.StartTimesSeconds[Index]", bridge)
		self.assertIn("initWithEvents:Events", bridge)
		self.assertIn("createAdvancedPlayerWithPattern", bridge)
		self.assertIn("startAndReturnError", bridge)
		self.assertIn("completionHandler", bridge)
		self.assertIn("PlaybackCallbacks", bridge)
		self.assertIn("FMath::IsFinite", bridge)
		stop_pattern = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)stopPattern:"
			"(uint64)RequestId\n{",
			1,
		)[1].split("\n}\n\n- (void)releaseGenerators", 1)[0]
		self.assertIn("if (!bStopped || Error)", stop_pattern)
		self.assertLess(
			stop_pattern.index("if (!bStopped || Error)"),
			stop_pattern.index("ClearCompletionHandler(Player)"),
		)

		self.assertIn("FOpenMobileHapticsAppleTransientPolicy::Resolve", manager)
		self.assertIn("BridgeService->EnsureEngine", backend)
		self.assertIn("BridgeService->PlayTransientPattern", backend)
		self.assertIn("AppleSemanticFallback", backend)
		self.assertIn("AppleSystemVibrationFallback", backend)
		self.assertIn("bCreatesControllablePlayback = true", backend)
		self.assertIn("bExpectsCallbacks = true", backend)
		self.assertIn("DecodeNormalized", policy)
		self.assertIn("RequestIntensity", policy)

	def test_apple_continuous_patterns_keep_owned_players_and_safety(self) -> None:
		ios_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsIOS"
		bridge = load_ios_bridge()
		backend = (
			ios_root / "Private" / "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		manager = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHaptics"
			/ "Private"
			/ "OpenMobileHapticsTimelineManager.cpp"
		).read_text(encoding="utf-8")

		self.assertIn("CHHapticEventTypeHapticContinuous", bridge)
		self.assertIn("Engine.autoShutdownEnabled = YES", bridge)
		self.assertIn("duration:NativeEvent.DurationSeconds", bridge)
		self.assertIn("CHHapticParameterCurveControlPoint", bridge)
		self.assertIn(
			"CHHapticDynamicParameterIDHapticIntensityControl",
			bridge,
		)
		self.assertIn(
			"CHHapticDynamicParameterIDHapticSharpnessControl",
			bridge,
		)
		self.assertIn("parameterCurves:Curves", bridge)
		self.assertIn("Player.loopEnabled = Pattern.bLoop", bridge)
		self.assertIn("Player.loopEnd = Pattern.LoopEndSeconds", bridge)
		self.assertIn("SafetyTimers", bridge)
		self.assertIn("NSRunLoopCommonModes", bridge)
		self.assertIn("[Timer invalidate]", bridge)
		self.assertIn("cancelAndReturnError", bridge)
		semantic_prepare = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)"
			"prepareSemanticGenerators:",
			1,
		)[1].split(
			"- (EOpenMobileHapticsAppleSubmissionResult)"
			"prepareTransientPattern:",
			1,
		)[0]
		self.assertNotIn("selectionChanged", semantic_prepare)
		self.assertNotIn("impactOccurred", semantic_prepare)
		self.assertNotIn("notificationOccurred", semantic_prepare)
		continuous_native = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)playContinuousPattern:\n"
			"\t(uint64)RequestId\n"
			"\tpattern:(const FOpenMobileHapticsAppleContinuousPattern&)Pattern\n"
			"\tinitialParameters:\n"
			"\t\t(const FOpenMobileHapticDynamicParameterUpdate*)"
			"InitialParameters\n"
			"\tpreparedResourceId:(uint64)PreparedResourceId\n"
			"\tcallback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback\n{",
			1,
		)[1].split("\n}\n\n- (void)cancelSafetyTimerForKey", 1)[0]
		self.assertLess(
			continuous_native.index("timerWithTimeInterval"),
			continuous_native.index("startAtTime:CHHapticTimeImmediate"),
		)
		stop_pattern = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)stopPattern:"
			"(uint64)RequestId\n{",
			1,
		)[1].split("\n}\n\n- (void)releaseGenerators", 1)[0]
		self.assertLess(
			stop_pattern.index("if (!bStopped || Error)"),
			stop_pattern.index("cancelSafetyTimerForKey"),
		)
		reset_handler = bridge.split("Engine.resetHandler = ^\n\t\t{", 1)[1].split(
			"\n\t\t};",
			1,
		)[0]
		self.assertLess(
			reset_handler.index("dispatch_async(dispatch_get_main_queue()"),
			reset_handler.index("interruptAllPatterns"),
		)
		self.assertLess(
			reset_handler.index("interruptAllPatterns"),
			reset_handler.index("EngineReset"),
		)
		self.assertIn("AVAudioSessionInterruptionNotification", bridge)
		self.assertIn("AVAudioSessionRouteChangeNotification", bridge)
		self.assertIn("AVAudioSessionMediaServicesWereResetNotification", bridge)

		self.assertIn("PlayContinuousPattern", backend)
		self.assertIn("FOpenMobileHapticsAppleContinuousPolicy::Resolve", manager)
		self.assertIn("Capabilities.ContinuousEvents", manager)
		self.assertIn(
			"FOpenMobileHapticLoopOptions EffectiveLoop = Pattern->Loop",
			backend,
		)
		self.assertIn("EffectiveLoop = Request.Options.Loop", backend)
		self.assertIn("Loop.bLoop || !Pattern.ParameterCurves.IsEmpty()", manager)

	def test_apple_dynamic_parameters_are_request_scoped_and_immediate(self) -> None:
		ios_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsIOS"
		bridge = load_ios_bridge()
		backend = (
			ios_root / "Private" / "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		android_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsAndroid"
			/ "Private"
			/ "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")

		self.assertIn("CHHapticDynamicParameter", bridge)
		self.assertIn("sendParameters:Parameters", bridge)
		self.assertIn("atTime:CHHapticTimeImmediate", bridge)
		self.assertIn(
			"Update.Sharpness * 2.0f - 1.0f",
			bridge,
		)
		update_pattern = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)updatePattern:",
			1,
		)[1].split("\n}\n\n- (void)releaseGenerators", 1)[0]
		self.assertLess(
			update_pattern.index("[Players objectForKey:Key]"),
			update_pattern.index("SendDynamicParameters(Player, Update)"),
		)
		self.assertIn("EOpenMobileHapticsAppleSubmissionResult::StaleRequest", update_pattern)
		self.assertIn("Pattern.bHasInitialDynamicParameters", bridge)
		self.assertLess(
			bridge.index("Pattern.bHasInitialDynamicParameters"),
			bridge.index("startAtTime:CHHapticTimeImmediate"),
		)
		self.assertIn("Capabilities.DynamicParameters = bCoreHapticsEnabled", backend)
		self.assertIn("Support.bDynamicParameters", backend)
		self.assertIn("Parameters.bHasInitialDynamicParameters", backend)
		self.assertIn("BridgeService->UpdatePattern", backend)
		self.assertIn(
			"FOpenMobileHapticNamedPatternRequest FallbackRequest = Request",
			backend,
		)
		self.assertIn("Parameters.InitialDynamicParameters.Intensity", backend)
		self.assertGreaterEqual(backend.count("FallbackRequest,"), 3)
		self.assertIn(
			"const FOpenMobileHapticsBackendPlaybackParameters& Parameters",
			android_backend,
		)
		self.assertIn("MakeScheduledPlayback", android_backend)
		self.assertNotIn("static_cast<void>(Parameters)", android_backend)

	def test_apple_ahap_import_and_native_playback_are_bounded(self) -> None:
		editor_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsEditor"
		factory = (
			editor_root / "Private" / "OpenMobileHapticAHAPFactory.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FactoryCanImport", factory)
		self.assertIn("SourceSize > 256 * 1024", factory)
		self.assertIn("SetAHAPSource", factory)
		self.assertIn("ImportData->Update(Filename)", factory)
		for native_token in (
			"CoreHaptics",
			"CHHaptic",
			"OpenMobileHapticsIOS",
		):
			self.assertNotIn(native_token, factory)

		bridge = load_ios_bridge()
		backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsIOS"
			/ "Private"
			/ "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		self.assertIn("playAHAPPattern", bridge)
		self.assertIn("NSJSONSerialization", bridge)
		self.assertIn("initWithDictionary", bridge)
		self.assertIn("createPlayerWithPattern", bridge)
		self.assertIn("createAdvancedPlayerWithPattern", bridge)
		ahap_native = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)playAHAPPattern:\n"
			"\t(uint64)RequestId\n"
			"\tpattern:(const FOpenMobileHapticsAppleAHAPPattern&)Pattern\n"
			"\tcallback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback\n{",
			1,
		)[1].split("\n}\n\n- (void)cancelSafetyTimerForKey", 1)[0]
		self.assertLess(
			ahap_native.index("NSJSONSerialization"),
			ahap_native.index("startAndReturnError"),
		)
		self.assertLess(
			ahap_native.index("bRequiresAdvancedPlayer"),
			ahap_native.index("startAtTime:StartTime"),
		)
		self.assertIn("BridgeService->PlayAHAPPattern", backend)
		self.assertIn("FOpenMobileHapticsAppleAHAPPlaybackPolicy::Resolve", backend)
		self.assertIn("Capabilities.AHAP = bAHAPEnabled", backend)
		self.assertIn("Capabilities.AudioEvents =", backend)
		self.assertIn(
			"AHAP.Pattern.InitialDynamicParameters.Intensity =",
			backend,
		)
		self.assertIn("AHAPStaticIntensityScales", backend)
		self.assertIn("ComposeDynamicUpdate", backend)
		self.assertGreaterEqual(backend.count("ForgetAHAPIntensityScale"), 3)
		self.assertNotIn("AHAPPlaybackPending", backend)

	def test_apple_custom_audio_preparation_is_async_and_bounded(self) -> None:
		bridge = load_ios_bridge()
		backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsIOS"
			/ "Private"
			/ "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		for token in (
			"MaximumActiveAudioResourceBytes",
			"MaximumActiveAudioPatternCount",
			"Pattern.AudioResources",
			"AudioPreparationQueue",
			"dispatch_queue_create",
			"DISPATCH_QUEUE_SERIAL",
			"initWithContentsOfURL",
			"PendingAHAPRequests",
			"ResourceDirectories",
			"removeItemAtPath",
			"Engine.currentTime",
			"FPlatformTime::Seconds()",
		):
			self.assertIn(token, bridge)
		self.assertLess(
			bridge.index("dispatch_async(AudioPreparationQueue"),
			bridge.index("initWithContentsOfURL"),
		)
		self.assertIn("Pattern.MaximumLatenessSeconds", bridge)
		self.assertIn("Pattern.ScheduledPlatformTimeSeconds", bridge)
		self.assertIn("NativeAudioAndHaptics", backend)
		self.assertIn("IOSAsset->ContainsHapticEvents()", backend)

		factory = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsEditor"
			/ "Private"
			/ "OpenMobileHapticAHAPFactory.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("MaximumResourceBytes", factory)
		self.assertIn("MaximumResourceCount", factory)
		self.assertIn("MaximumTotalBytes", factory)
		self.assertIn("SetAHAPSourceWithAudioResources", factory)

	def test_android_scheduled_patterns_use_best_effort_main_looper(self) -> None:
		android_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsAndroid"
		bridge = load_android_bridge()
		for token in (
			"MAXIMUM_SCHEDULED_DELAY_MILLIS",
			"Handler",
			"SystemClock.uptimeMillis()",
			"postAtTime",
			"SCHEDULED_REQUESTS",
			"removeCallbacks",
			"cancelScheduledRequests",
			"cancelScheduledRequest",
			"nativeCanStart",
			"RESULT_PENDING",
			"RESULT_STALE",
		):
			self.assertIn(token, bridge)
		scheduler = bridge[
			bridge.index("private static int schedulePlayback"):
			bridge.index("private static void cancelScheduledRequests")
		]
		self.assertLess(
			scheduler.index("nativeCanStart(requestId)"),
			scheduler.index("playback.play(current)"),
		)

		native_bridge = (
			android_root
			/ "Private"
			/ "OpenMobileHapticsAndroidBridge.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("StartDelayMilliseconds", native_bridge)
		self.assertIn("PendingCallbacks.Add", native_bridge)
		self.assertIn("ResultPending", native_bridge)
		self.assertIn("HandleCanStart", native_bridge)
		self.assertIn("CancelScheduled", native_bridge)

		backend = (
			android_root
			/ "Private"
			/ "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("Parameters.Timing.StartDelaySeconds", backend)
		self.assertIn("EOpenMobileHapticSynchronizationMode::BestEffort", backend)
		self.assertIn(
			"Capabilities.AudioEvents = EOpenMobileHapticSupportState::Unsupported",
			backend,
		)
		self.assertNotIn("NativeAudioAndHaptics", backend)
		self.assertIn("EstimatedPrecisionSeconds = FMath::Max", backend)
		self.assertIn("0.010", backend)
		self.assertIn(
			"Capabilities.Scheduling = EOpenMobileHapticSupportState::Supported",
			backend,
		)

	def test_android_portable_controls_are_request_owned_and_emulated(self) -> None:
		bridge = load_android_bridge()
		android_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsAndroid"
		native_bridge = (
			android_root / "Private" / "OpenMobileHapticsAndroidBridge.cpp"
		).read_text(encoding="utf-8")
		backend = (
			android_root / "Private" / "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"ControlledWaveform",
			"CONTROLLED_WAVEFORM_LOCK",
			"playControlledWaveform",
			"pauseControlledWaveform",
			"resumeControlledWaveform",
			"seekControlledWaveform",
			"stopControlledWaveform",
			"nativeOnControlledWaveformEvent",
			"controlRevision",
			"completionRunnable",
			"SystemClock.uptimeMillis()",
			"nativeCanStart(requestId)",
			):
				self.assertIn(token, bridge)
		self.assertNotIn("Thread.sleep", bridge)
		controlled_event_dispatch = bridge.split(
			"private static void emitControlledWaveformEvent(", 1
		)[1].split("private static void removeControlledCallbacks", 1)[0]
		self.assertIn("SCHEDULED_HANDLER.post", controlled_event_dispatch)
		controlled_play = bridge.split(
			"static int playControlledWaveform(", 1
		)[1].split("static int pauseControlledWaveform(", 1)[0]
		self.assertIn("preparedResourceId", controlled_play)
		controlled_start = bridge.split(
			"private static int startControlledOutputLocked(", 1
		)[1].split("static int playControlledWaveform(", 1)[0]
		self.assertIn(
			"PREPARED_WAVEFORMS.get(preparedResourceId)",
			controlled_start,
		)
		controlled_seek = bridge.split(
			"static int seekControlledWaveform(", 1
		)[1].split("static boolean stopControlledWaveform(", 1)[0]
		seek_restart_failure = controlled_seek.split(
			"if (result != RESULT_ACCEPTED", 1
		)[1].split("return result;", 1)[0]
		self.assertIn("CONTROLLED_EVENT_FAILED", seek_restart_failure)

		for token in (
			"PlayControlledWaveformMethod",
			"PauseControlledWaveformMethod",
			"ResumeControlledWaveformMethod",
			"SeekControlledWaveformMethod",
			"StopControlledWaveformMethod",
			"HandleControlledWaveformEvent",
		):
			self.assertIn(token, native_bridge)
		for token in (
			"FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve",
			"EOpenMobileHapticControlImplementation::Emulated",
			"Support.bPause = IsCustomPlaybackConfigured()",
			"Support.bResume = IsCustomPlaybackConfigured()",
			"Support.bSeek = IsCustomPlaybackConfigured()",
			"PausePlayback(",
			"ResumePlayback(",
			"SeekPlayback(",
		):
			self.assertIn(token, backend)

	def test_apple_scheduled_playback_revalidates_on_native_scheduler(self) -> None:
		bridge = load_ios_bridge()
		backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsIOS"
			/ "Private"
			/ "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		for token in (
			"ScheduledStartTimers",
			"ScheduledStartActions",
			"handleScheduledStartTimer",
			"PlayScheduledSemantic",
			"PlayScheduledSystemVibration",
			"PlayScheduledTransientPattern",
			"PlayScheduledContinuousPattern",
			"PlayScheduledAHAPPattern",
			"EOpenMobileHapticsApplePlaybackEvent::Interrupted",
		):
			self.assertIn(token, bridge)
		scheduled_start = bridge.split(
			"- (void)handleScheduledStartTimer:(NSTimer*)Timer\n{", 1
		)[1].split("\n}\n", 1)[0]
		self.assertLess(
			scheduled_start.index("Guard->CanStart"),
			scheduled_start.index("(*Action)"),
		)
		self.assertIn(
			"FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()",
			scheduled_start,
		)
		self.assertIn("releaseAudioResourcesForRequest:RequestId", scheduled_start)
		stop_pattern = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)stopPattern:(uint64)RequestId\n{",
			1,
		)[1].split("\n}\n", 1)[0]
		self.assertLess(
			stop_pattern.index("cancelScheduledStart:RequestId"),
			stop_pattern.index("[Players objectForKey:Key]"),
		)
		ahap_start = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)startAHAPPattern:\n"
			"\t(uint64)RequestId\n"
			"\tpattern:(const FOpenMobileHapticsAppleAHAPPattern&)Pattern\n"
			"\tnativePattern:(CHHapticPattern*)NativePattern\n{",
			1,
		)[1].split("\n}\n", 1)[0]
		self.assertIn("ScheduleResult", ahap_start)
		self.assertIn(
			"EOpenMobileHapticsApplePlaybackEvent::Interrupted",
			ahap_start,
		)
		self.assertIn("MakeAppleSchedule(Parameters)", backend)
		self.assertIn("ApplyAppleTimingDiagnostics", backend)
		self.assertIn("EstimatedPrecisionSeconds = FMath::Max", backend)
		self.assertIn("0.002", backend)
		self.assertIn(
			"Capabilities.Scheduling = bSemanticEnabled || bCoreHapticsEnabled",
			backend,
		)
		self.assertNotIn("static_cast<void>(Parameters)", backend)
		self.assertNotIn(
			"Scheduled Apple playback requires an AHAP platform override.",
			backend,
		)

	def test_apple_playback_controls_require_advanced_players(self) -> None:
		bridge = load_ios_bridge()
		backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsIOS"
			/ "Private"
			/ "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		for token in (
			"pausePattern:",
			"resumePattern:",
			"seekPattern:",
			"pauseAtTime:CHHapticTimeImmediate",
			"resumeAtTime:CHHapticTimeImmediate",
			"seekToOffset:PositionSeconds",
			"PausedSafetyRemainingByRequest",
			"SafetyDeadlineByRequest",
		):
			self.assertIn(token, bridge)

		pause_pattern = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)pausePattern:(uint64)RequestId\n{",
			1,
		)[1].split("\n}\n", 1)[0]
		self.assertLess(
			pause_pattern.index("AsAdvancedPlayer"),
			pause_pattern.index("pauseAtTime:CHHapticTimeImmediate"),
		)
		self.assertIn("pauseSafetyTimerForKey", pause_pattern)

		resume_pattern = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)resumePattern:(uint64)RequestId\n{",
			1,
		)[1].split("\n}\n", 1)[0]
		self.assertLess(
			resume_pattern.index("AsAdvancedPlayer"),
			resume_pattern.index("resumeAtTime:CHHapticTimeImmediate"),
		)
		self.assertIn("resumeSafetyTimerForKey", resume_pattern)

		seek_pattern = bridge.split(
			"- (EOpenMobileHapticsAppleSubmissionResult)seekPattern:\n"
			"\t(uint64)RequestId\n"
			"\tpositionSeconds:(double)PositionSeconds\n{",
			1,
		)[1].split("\n}\n", 1)[0]
		self.assertIn("FMath::IsFinite(PositionSeconds)", seek_pattern)
		self.assertLess(
			seek_pattern.index("AsAdvancedPlayer"),
			seek_pattern.index("seekToOffset:PositionSeconds"),
		)
		for token in (
			"Support.bPause = bCoreHapticsEnabled",
			"Support.bResume = bCoreHapticsEnabled",
			"Support.bSeek = bCoreHapticsEnabled",
			"Capabilities.Pause = bCoreHapticsEnabled",
			"Capabilities.Resume = bCoreHapticsEnabled",
			"Capabilities.Seek = bCoreHapticsEnabled",
			"MakeNativePlaybackControlSupport",
			"PausePlayback(",
			"ResumePlayback(",
			"SeekPlayback(",
		):
			self.assertIn(token, backend)
		self.assertIn("AHAP.Pattern.bRequiresAdvancedPlayer", backend)
		self.assertIn("EOpenMobileHapticControlImplementation::Native", backend)

	def test_android_bridge_is_versioned_and_lifecycle_safe(self) -> None:
		android_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsAndroid"
		bridge_path = (
			android_root
			/ "Private"
			/ "Android"
			/ "src"
			/ "com"
			/ "openmobile"
			/ "haptics"
			/ "OpenMobileHapticsBridgeV1.java"
		)
		self.assertTrue(bridge_path.is_file())
		bridge = bridge_path.read_text(encoding="utf-8")
		self.assertIn("final class OpenMobileHapticsBridgeV1", bridge)
		self.assertIn("BRIDGE_VERSION = 1", bridge)
		self.assertIn("WeakReference<Activity>", bridge)
		self.assertIn("getApplicationContext()", bridge)
		self.assertIn("getDefaultVibrator()", bridge)
		self.assertNotIn("InputDevice", bridge)
		self.assertIn("private static native void nativeOnBridgeResult", bridge)
		self.assertIn("private static native void nativeOnInterruption", bridge)
		self.assertIn("trackActivity", bridge)
		self.assertIn("ActivityLifecycleCallbacks", bridge)
		self.assertIn("INTERRUPTION_ACTIVITY_REPLACED", bridge)
		self.assertIn("INTERRUPTION_NATIVE_SERVICE_LOST", bridge)
		self.assertIn("AtomicLong", bridge)
		self.assertIn("snapshotInstrumentation", bridge)

		upl = (
			android_root
			/ "Private"
			/ "Android"
			/ "OpenMobileHaptics_Android_UPL.xml"
		).read_text(encoding="utf-8")
		self.assertIn("OpenMobileHapticsBridgeV1.java", upl)
		self.assertNotIn("gameActivityClassAdditions", upl)

		native_bridge = (
			android_root
			/ "Private"
			/ "OpenMobileHapticsAndroidBridge.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FindJavaClassGlobalRef", native_bridge)
		self.assertIn("GetStaticMethodID", native_bridge)
		self.assertIn("DeleteGlobalRef", native_bridge)
		self.assertIn("Token.RequestId", native_bridge)
		self.assertIn("nativeOnBridgeResult", native_bridge)
		self.assertIn("nativeOnInterruption", native_bridge)
		self.assertIn("NotifyInterruption", native_bridge)
		self.assertIn("PendingCallbacks", native_bridge)

		common_callback = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHaptics"
			/ "Private"
			/ "OpenMobileHapticsSubsystem.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ENamedThreads::GameThread", common_callback)

	def test_android_primitive_composition_is_guarded(self) -> None:
		android_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsAndroid"
		bridge = load_android_bridge()
		self.assertIn("static int playPrimitives", bridge)
		self.assertIn("arePrimitivesSupported", bridge)
		self.assertIn("VibrationEffect.startComposition()", bridge)
		self.assertIn("composition.addPrimitive", bridge)
		self.assertIn("private static int primitiveId", bridge)
		self.assertIn("supported == null", bridge)
		playback = bridge[
			bridge.index("static int playPrimitives"):
			bridge.index("static boolean stopAll")
		]
		self.assertLess(
			playback.index("arePrimitivesSupported"),
			playback.index("VibrationEffect.startComposition()"),
		)

		native_bridge = (
			android_root
			/ "Private"
			/ "OpenMobileHapticsAndroidBridge.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("PlayPrimitives", native_bridge)
		self.assertIn("FScopedJavaObject<jintArray>", native_bridge)
		self.assertIn("FScopedJavaObject<jfloatArray>", native_bridge)

		backend = (
			android_root
			/ "Private"
			/ "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("SubmitNamedPattern", backend)
		self.assertIn("PrimitiveCompositionPolicy::Resolve", backend)
		self.assertIn("Bridge.PlayPrimitives", backend)

	def test_android_envelope_builders_are_guarded(self) -> None:
		android_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsAndroid"
		bridge = load_android_bridge()
		self.assertIn("static int playEnvelope", bridge)
		self.assertIn("areEnvelopeEffectsSupported", bridge)
		self.assertIn("getEnvelopeEffectInfo", bridge)
		self.assertIn("getFrequencyProfile", bridge)
		self.assertIn(
			"android.os.VibrationEffect$BasicEnvelopeBuilder",
			bridge,
		)
		self.assertIn(
			"android.os.VibrationEffect$WaveformEnvelopeBuilder",
			bridge,
		)
		self.assertIn(
			"private static volatile EnvelopeApi36 envelopeApi36",
			bridge,
		)
		self.assertIn("synchronized (OpenMobileHapticsBridgeV1.class)", bridge)
		self.assertGreaterEqual(bridge.count('"addControlPoint"'), 2)
		playback = bridge[
			bridge.index("static int playEnvelope"):
			bridge.index("static boolean stopAll")
		]
		self.assertLess(
			playback.index("areEnvelopeEffectsSupported.invoke"),
			playback.index("basicConstructor.newInstance"),
		)
		self.assertIn("catch (Exception exception)", playback)
		self.assertIn("return RESULT_FAILED", playback)

		native_bridge = (
			android_root
			/ "Private"
			/ "OpenMobileHapticsAndroidBridge.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("PlayEnvelope", native_bridge)
		self.assertIn("FScopedJavaObject<jlongArray>", native_bridge)
		self.assertGreaterEqual(
			native_bridge.count("FScopedJavaObject<jfloatArray>"),
			2,
		)

		backend = (
			android_root
			/ "Private"
			/ "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("FOpenMobileHapticsEnvelopePolicy::Resolve", backend)
		self.assertIn("Bridge.PlayEnvelope", backend)
		self.assertIn("Bridge.PlayPrimitives", backend)

	def test_android_waveforms_and_predefined_effects_are_guarded(self) -> None:
		android_root = HAPTICS_PLUGIN / "Source" / "OpenMobileHapticsAndroid"
		bridge = load_android_bridge()
		self.assertIn("static int playWaveform", bridge)
		self.assertIn("VibrationEffect.createWaveform", bridge)
		self.assertIn("repeatIndex < -1", bridge)
		self.assertIn("repeatIndex >= timingsMilliseconds.length", bridge)
		self.assertIn("amplitude != VibrationEffect.DEFAULT_AMPLITUDE", bridge)
		self.assertIn("amplitude == 0", bridge)
		self.assertIn("nativeAmplitudes[index] =", bridge)
		waveform = bridge[
			bridge.index("static int playWaveform"):
			bridge.index("static int playPredefined")
		]
		self.assertIn("Build.VERSION.SDK_INT < 26", waveform)
		self.assertIn("PREPARED_WAVEFORMS.get(preparedResourceId)", waveform)
		self.assertIn("static int prepareWaveform", bridge)
		self.assertIn("static void releasePreparedResources", bridge)
		prepare_waveform = bridge[
			bridge.index("static int prepareWaveform"):
			bridge.index("static void releasePreparedResources")
		]
		self.assertNotIn("vibrate(", prepare_waveform)
		self.assertIn("catch (SecurityException exception)", waveform)
		self.assertIn("catch (Exception exception)", waveform)
		self.assertIn("static int playPredefined", bridge)
		self.assertIn("vibrator.areEffectsSupported", bridge)
		self.assertIn("Vibrator.VIBRATION_EFFECT_SUPPORT_YES", bridge)
		self.assertIn("VibrationEffect.createPredefined", bridge)
		one_shot = bridge[
			bridge.index("static int playOneShot"):
			bridge.index("static int playWaveform")
		]
		self.assertIn("vibrator.areEffectsSupported", one_shot)
		self.assertLess(
			one_shot.index("vibrator.areEffectsSupported"),
			one_shot.index("VibrationEffect.createPredefined"),
		)
		predefined = bridge[
			bridge.index("static int playPredefined"):
			bridge.index("static int playPrimitives")
		]
		self.assertLess(
			predefined.index("vibrator.areEffectsSupported"),
			predefined.index("VibrationEffect.createPredefined"),
		)
		self.assertIn("Build.VERSION.SDK_INT < 29", predefined)
		self.assertIn("catch (SecurityException exception)", predefined)
		self.assertIn("catch (Exception exception)", predefined)
		semantic_vibration = bridge[
			bridge.index("private static int playVibration"):
			bridge.index("private static int performSemantic")
		]
		self.assertIn("vibrator.areEffectsSupported", semantic_vibration)
		self.assertLess(
			semantic_vibration.index("vibrator.areEffectsSupported"),
			semantic_vibration.index("VibrationEffect.createPredefined"),
		)
		self.assertIn("else if (path != 3)", semantic_vibration)
		self.assertIn("VibrationAttributes.USAGE_MEDIA", bridge)
		self.assertIn("VibrationAttributes.USAGE_NOTIFICATION", bridge)
		self.assertIn("VibrationAttributes.USAGE_TOUCH", bridge)
		self.assertNotIn("VibrationAttributes.USAGE_ALARM", bridge)

		native_header = (
			android_root / "Private" / "OpenMobileHapticsAndroidBridge.h"
		).read_text(encoding="utf-8")
		self.assertIn("PlayWaveform", native_header)
		self.assertIn("PlayPredefined", native_header)
		self.assertIn("PlayWaveformMethod", native_header)
		self.assertIn("PlayPredefinedMethod", native_header)

		native_bridge = (
			android_root / "Private" / "OpenMobileHapticsAndroidBridge.cpp"
		).read_text(encoding="utf-8")
		self.assertIn('"(Landroid/app/Activity;JJ[J[IIIJ)I"', native_bridge)
		self.assertIn('"(Landroid/app/Activity;J[J[IIJIJJ)I"', native_bridge)
		self.assertIn('"releasePreparedResources"', native_bridge)
		self.assertIn('"(Landroid/app/Activity;JIIJ)I"', native_bridge)
		self.assertIn("FScopedJavaObject<jlongArray>", native_bridge)
		self.assertIn("FScopedJavaObject<jintArray>", native_bridge)

		backend = (
			android_root / "Private" / "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("AndroidWaveformPolicy::ResolveOverride", backend)
		self.assertIn("Parameters.PortableTimeline", backend)
		self.assertIn("Bridge.PlayWaveform", backend)
		self.assertIn("Bridge.PlayPredefined", backend)
		self.assertIn(
			"Resolution.Path == EOpenMobileHapticsOneShotPath::PredefinedEffect",
			backend,
		)
		self.assertIn("SupportsPredefined", backend)

	def test_portable_timelines_use_the_shared_manager(self) -> None:
		common = HAPTICS_PLUGIN / "Source" / "OpenMobileHaptics"
		manager_header = (
			common / "Internal" / "OpenMobileHapticsTimelineManager.h"
		)
		manager_source = (
			common / "Private" / "OpenMobileHapticsTimelineManager.cpp"
		)
		self.assertTrue(manager_header.is_file())
		self.assertTrue(manager_source.is_file())

		backend_parameters = (
			common / "Internal" / "IOpenMobileHapticsBackend.h"
		).read_text(encoding="utf-8")
		self.assertIn("PortableTimeline", backend_parameters)
		registry = (
			common / "Private" / "OpenMobileHapticsBackendRegistry.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("TimelineManager().Clear()", registry)

		android_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsAndroid"
			/ "Private"
			/ "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("Parameters.PortableTimeline", android_backend)
		self.assertNotIn(
			"AndroidWaveformPolicy::ResolvePortable",
			android_backend,
		)

		apple_backend = (
			HAPTICS_PLUGIN
			/ "Source"
			/ "OpenMobileHapticsIOS"
			/ "Private"
			/ "OpenMobileHapticsIOSBackend.mm"
		).read_text(encoding="utf-8")
		self.assertIn("Parameters.PortableTimeline", apple_backend)

if __name__ == "__main__":
	unittest.main()
