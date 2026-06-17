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
		self.assertIn("capabilitiesForHardware", ios_probe)
		self.assertIn("supportsHaptics", ios_probe)
		self.assertIn("supportsAudio", ios_probe)
		self.assertNotIn("CHHapticEngine alloc", ios_probe)
		self.assertNotIn("startAndReturnError", ios_probe)
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
		self.assertIn("kSystemSoundID_Vibrate", ios_backend)
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
		self.assertIn("UISelectionFeedbackGenerator", ios_backend)
		self.assertIn("UIImpactFeedbackGenerator", ios_backend)
		self.assertIn("UINotificationFeedbackGenerator", ios_backend)
		self.assertIn("ImpactGenerators", ios_backend)
		self.assertIn("[ImpactGenerators[Index] prepare]", ios_backend)
		self.assertIn("dispatch_after", ios_backend)
		self.assertIn("releaseGenerators", ios_backend)
		self.assertIn("BeginShutdown", ios_backend)
		self.assertNotIn("CHHapticEngine alloc", ios_backend)

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
		self.assertIn('"(Landroid/app/Activity;J[J[III)I"', native_bridge)
		self.assertIn('"(Landroid/app/Activity;JII)I"', native_bridge)
		self.assertIn("FScopedJavaObject<jlongArray>", native_bridge)
		self.assertIn("FScopedJavaObject<jintArray>", native_bridge)

		backend = (
			android_root / "Private" / "OpenMobileHapticsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("AndroidWaveformPolicy::ResolveOverride", backend)
		self.assertIn("AndroidWaveformPolicy::ResolvePortable", backend)
		self.assertIn("Bridge.PlayWaveform", backend)
		self.assertIn("Bridge.PlayPredefined", backend)
		self.assertIn(
			"Resolution.Path == EOpenMobileHapticsOneShotPath::PredefinedEffect",
			backend,
		)
		self.assertIn("SupportsPredefined", backend)

if __name__ == "__main__":
	unittest.main()
