#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticsBlueprintLibrary.generated.h"

class UOpenMobileHapticPlayback;
class UOpenMobileHapticsSubsystem;

UCLASS(meta = (DisplayName = "OpenMobile Haptics"))
class OPENMOBILEHAPTICS_API UOpenMobileHapticsBlueprintLibrary final :
	public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Use this for quick UI selection feedback, it picks the expected channel and still tells you when policy made the request silent. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Play", meta = (CPP_Default_Intensity = "1.0", DisplayName = "Play Selection Haptic", ExpandEnumAsExecs = "Outcome", Keywords = "haptic vibrate vibration rumble buzz tactile feedback UI select picker slider phone", ToolTip = "Plays a short selection Haptic with the recommended UI channel. Normalized intensity is clamped, and intentional silence reaches Suppressed instead of Accepted.", WorldContext = "WorldContextObject"))
	static void PlaySelectionHaptic(
		const UObject* WorldContextObject,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized strength from zero to one. Wired values are clamped too."))
		float Intensity,
		EOpenMobileHapticRequestOutcome& Outcome,
		UOpenMobileHapticPlayback*& Playback,
		UPARAM(DisplayName = "Used Fallback") bool& bUsedFallback,
		UPARAM(DisplayName = "Resolved Quality") FName& ResolvedQuality,
		FOpenMobileHapticError& Error
	);

	/** Keeps impact meaning portable while the backend chooses the closest supported native feedback. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Play", meta = (CPP_Default_Intensity = "1.0", DisplayName = "Play Impact Haptic", ExpandEnumAsExecs = "Outcome", Keywords = "haptic vibrate vibration rumble buzz tactile feedback collision hit phone", ToolTip = "Plays a portable impact Haptic with the recommended UI channel. Normalized intensity is clamped, and intentional silence reaches Suppressed instead of Accepted.", WorldContext = "WorldContextObject"))
	static void PlayImpactHaptic(
		const UObject* WorldContextObject,
		EOpenMobileHapticImpactStyle Style,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized strength from zero to one. Wired values are clamped too."))
		float Intensity,
		EOpenMobileHapticRequestOutcome& Outcome,
		UOpenMobileHapticPlayback*& Playback,
		UPARAM(DisplayName = "Used Fallback") bool& bUsedFallback,
		UPARAM(DisplayName = "Resolved Quality") FName& ResolvedQuality,
		FOpenMobileHapticError& Error
	);

	/** Plays success, warning, or error feedback only, it won't create an operating-system notification for you. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Play", meta = (CPP_Default_Intensity = "1.0", DisplayName = "Play Notification Haptic", ExpandEnumAsExecs = "Outcome", Keywords = "haptic vibrate vibration rumble buzz tactile feedback success warning error alert phone", ToolTip = "Plays portable success, warning, or error feedback on the recommended Alerts channel. It does not deliver an operating-system notification.", WorldContext = "WorldContextObject"))
	static void PlayNotificationHaptic(
		const UObject* WorldContextObject,
		EOpenMobileHapticNotificationType Type,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized strength from zero to one. Wired values are clamped too."))
		float Intensity,
		EOpenMobileHapticRequestOutcome& Outcome,
		UOpenMobileHapticPlayback*& Playback,
		UPARAM(DisplayName = "Used Fallback") bool& bUsedFallback,
		UPARAM(DisplayName = "Resolved Quality") FName& ResolvedQuality,
		FOpenMobileHapticError& Error
	);

	/** Gives gameplay a stable preset name while prepared project overrides can replace the portable fallback. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Play", meta = (CPP_Default_Intensity = "1.0", DisplayName = "Play Game Haptic", ExpandEnumAsExecs = "Outcome", Keywords = "haptic vibrate vibration rumble buzz tactile feedback confirm reject damage pickup achievement phone", ToolTip = "Plays a stable game preset. Configured prepared-pattern overrides are preferred before portable semantic fallback.", WorldContext = "WorldContextObject"))
	static void PlayGameHaptic(
		const UObject* WorldContextObject,
		EOpenMobileHapticGamePreset Preset,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized strength from zero to one. Wired values are clamped too."))
		float Intensity,
		EOpenMobileHapticRequestOutcome& Outcome,
		UOpenMobileHapticPlayback*& Playback,
		UPARAM(DisplayName = "Used Fallback") bool& bUsedFallback,
		UPARAM(DisplayName = "Resolved Quality") FName& ResolvedQuality,
		FOpenMobileHapticError& Error
	);

	/** Requests one bounded pulse when semantic meaning isn't needed, with clamping reported in the result. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Play", meta = (CPP_Default_DurationSeconds = "0.05", CPP_Default_Intensity = "1.0", DisplayName = "Vibrate Phone", ExpandEnumAsExecs = "Outcome", Keywords = "haptic vibrate vibration rumble buzz tactile feedback pulse phone", ToolTip = "Requests one bounded phone vibration on the recommended Gameplay channel. Duration and intensity are clamped to safe project limits.", WorldContext = "WorldContextObject"))
	static void VibratePhone(
		const UObject* WorldContextObject,
		UPARAM(DisplayName = "Duration", meta = (ClampMin = "0.001", ClampMax = "30.0", Units = "s", ToolTip = "Requested vibration duration in seconds. Wired values are clamped to project limits."))
		float DurationSeconds,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized strength from zero to one. Wired values are clamped too."))
		float Intensity,
		EOpenMobileHapticRequestOutcome& Outcome,
		UOpenMobileHapticPlayback*& Playback,
		UPARAM(DisplayName = "Used Fallback") bool& bUsedFallback,
		UPARAM(DisplayName = "Resolved Quality") FName& ResolvedQuality,
		FOpenMobileHapticError& Error
	);

	/** Checks only whether the raw handle has an ID, it can't promise the request is still alive. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Is Haptic Playback Handle Valid", Keywords = "haptic guid handle valid", ToolTip = "Returns true when a raw playback handle contains an identifier. Prefer the Haptic Playback object in common graphs."))
	static bool IsHapticPlaybackHandleValid(
		FOpenMobileHapticPlaybackHandle Handle
	);

	/** Compares request identity directly so Blueprint doesn't have to split the GUID. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (CompactNodeTitle = "==", DisplayName = "Equal Haptic Playback Handles", Keywords = "haptic handle compare equal", ToolTip = "Returns true when two raw playback handles identify the same request."))
	static bool EqualHapticPlaybackHandles(
		FOpenMobileHapticPlaybackHandle A,
		FOpenMobileHapticPlaybackHandle B
	);

	/** Checks whether a preload handle carries an ID, readiness still comes from its result or subsystem state. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Is Haptic Preload Handle Valid", Keywords = "haptic preparation library handle valid", ToolTip = "Returns true when a raw library preload handle contains an identifier. Prefer an owned preparation task in common graphs."))
	static bool IsHapticPreloadHandleValid(
		FOpenMobileHapticLibraryPreloadHandle Handle
	);

	/** Matches preload request identity when legacy graphs have to filter the shared preparation event. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (CompactNodeTitle = "==", DisplayName = "Equal Haptic Preload Handles", Keywords = "haptic preparation handle compare equal", ToolTip = "Returns true when two raw library preload handles identify the same request."))
	static bool EqualHapticPreloadHandles(
		FOpenMobileHapticLibraryPreloadHandle A,
		FOpenMobileHapticLibraryPreloadHandle B
	);

	/** Treats native and fallback acceptance the same, suppressed silence and rejection stay false. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Is Haptic Request Accepted", Keywords = "haptic result success fallback", ToolTip = "Returns true for accepted native or fallback requests. Suppressed and rejected requests return false."))
	static bool IsHapticRequestAccepted(
		const FOpenMobileHapticPlaybackResult& Result
	);

	/** Confirms an output path accepted the request, no phone API can prove the player physically felt it. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Did Haptic Request Produce Output", Keywords = "haptic result silent output", ToolTip = "Returns true when the immediate request was accepted for an output path. It does not prove physical actuator observation."))
	static bool DidHapticRequestProduceOutput(
		const FOpenMobileHapticPlaybackResult& Result
	);

	/** Uses the stable code instead of message text, messages are for people and can change. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Has Haptic Error", Keywords = "haptic failed error", ToolTip = "Returns true when the Haptics error contains a stable error code."))
	static bool HasHapticError(const FOpenMobileHapticError& Error);

	/** Produces a short sanitized line for debug UI without leaking platform-specific native details. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Get Haptic Result Summary", Keywords = "haptic status debug log message", ReturnDisplayName = "Summary", ToolTip = "Returns a short sanitized summary for development UI and logs."))
	static FString GetHapticResultSummary(
		const FOpenMobileHapticPlaybackResult& Result
	);

	/** Combines the stable code and suggested correction into text you can show in development tools. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Format Haptic Error", Keywords = "haptic error message correction debug", ReturnDisplayName = "Message", ToolTip = "Returns a safe developer message with the stable code and correction, without native detail."))
	static FString FormatHapticError(const FOpenMobileHapticError& Error);

	/** Maps known failures to a caller action so every Blueprint doesn't invent its own retry rules. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Get Haptic Recovery Action", Keywords = "haptic error retry fix recovery", ReturnDisplayName = "Recovery Action", ToolTip = "Maps a stable Haptics error to a suggested next step."))
	static EOpenMobileHapticRecoveryAction GetHapticRecoveryAction(
		const FOpenMobileHapticError& Error
	);

	/** Returns the current categorical state, and you shouldn't compare this enum by number. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Get Haptic Availability", Keywords = "haptic support device vibrator", ToolTip = "Returns the current categorical Haptics availability. Never compare this enum by ordinal.", WorldContext = "WorldContextObject"))
	static EOpenMobileHapticAvailability GetHapticAvailability(
		const UObject* WorldContextObject
	);

	/** Collapses the current policy and device support into an ordered tier for simple feature gating. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Get Haptic Capability Tier", Keywords = "haptic support basic semantic rich", ToolTip = "Returns an ordered portable capability tier. Policy-disabled and temporarily unavailable states resolve to None.", WorldContext = "WorldContextObject"))
	static EOpenMobileHapticCapabilityTier GetHapticCapabilityTier(
		const UObject* WorldContextObject
	);

	/** Answers whether basic output is available now, not whether the device supported it earlier. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Can Play Haptics", Keywords = "haptic available vibrator support", ToolTip = "Returns true when the current state can produce at least basic phone vibration.", WorldContext = "WorldContextObject"))
	static bool CanPlayHaptics(const UObject* WorldContextObject);

	/** Requires semantic support to be explicitly available right now, Unknown won't pass. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Can Play Semantic Haptics", Keywords = "haptic selection impact notification support", ToolTip = "Returns true when native semantic Haptics are currently available.", WorldContext = "WorldContextObject"))
	static bool CanPlaySemanticHaptics(const UObject* WorldContextObject);

	/** Checks whether prepared pattern playback is currently usable after policy and lifecycle are applied. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Can Play Rich Haptics", Keywords = "haptic pattern custom support", ToolTip = "Returns true when rich prepared-pattern Haptics are currently available.", WorldContext = "WorldContextObject"))
	static bool CanPlayRichHaptics(const UObject* WorldContextObject);

	/** Converts tri-state support to bool without accidentally treating Unknown as success. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Is Haptic Feature Supported", Keywords = "haptic capability tri state", ToolTip = "Returns true only for known support. Unknown is never treated as proof of support."))
	static bool IsHapticFeatureSupported(EOpenMobileHapticSupportState Support);

	/** Lets discovery UI separate a measured no from a value the backend hasn't resolved yet. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Is Haptic Feature Known", Keywords = "haptic capability unknown", ToolTip = "Returns false only when support has not been determined."))
	static bool IsHapticFeatureKnown(EOpenMobileHapticSupportState Support);

	/** Queries one portable feature against the latest capability snapshot and fails closed on Unknown. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Supports Haptic Feature", Keywords = "haptic capability feature device support", ToolTip = "Returns true only when the current device explicitly supports the selected portable feature. Unknown is never treated as support.", WorldContext = "WorldContextObject"))
	static bool SupportsHapticFeature(
		const UObject* WorldContextObject,
		EOpenMobileHapticFeature Feature
	);

	/** Loads library metadata for tooling discovery, don't put this query on a latency-sensitive gameplay path. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Get Configured Haptic Pattern Names", Keywords = "haptic library names discover debug", ReturnDisplayName = "Pattern Names", ToolTip = "Returns sorted pattern aliases from configured libraries for development tools and dynamic browsers. This explicit tooling query may load library metadata; do not call it on a latency-sensitive gameplay path."))
	static TArray<FName> GetConfiguredHapticPatternNames();

	/** Returns typed pattern choices for dynamic tools while literal gameplay references should still use assets. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Get Configured Haptic Pattern Identifiers", Keywords = "haptic library typed identifier picker configured", ReturnDisplayName = "Pattern Identifiers", ToolTip = "Returns sorted typed identifiers from configured libraries for development tools and dynamic browsers. Prefer a direct pattern asset for literal gameplay references."))
	static TArray<FOpenMobileHapticPatternIdentifier>
	GetConfiguredHapticPatternIdentifiers();

	/** Returns the configured library IDs so tools can offer valid preparation choices. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Get Configured Haptic Library Identifiers", Keywords = "haptic library typed identifier picker configured", ReturnDisplayName = "Library Identifiers", ToolTip = "Returns sorted typed identifiers for libraries configured in Project Settings."))
	static TArray<FOpenMobileHapticLibraryIdentifier>
	GetConfiguredHapticLibraryIdentifiers();

	/** Feeds Unreal's internal name picker, user-facing graphs should use the typed library query. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<FName> GetConfiguredHapticLibraryNames();

	/** Feeds internal channel pickers from project settings so authored names don't drift. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<FName> GetConfiguredHapticChannelNames();

	/** Feeds internal category pickers with project policy names and built-in choices. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<FName> GetConfiguredHapticCategoryNames();

	/** Feeds internal effect pickers from semantic keys, presets, and configured overrides. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<FName> GetConfiguredHapticEffectNames();

	/** Reports what's ready in this Game Instance without loading another asset. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Get Prepared Haptic Pattern Names", Keywords = "haptic library ready loaded names debug", ReturnDisplayName = "Pattern Names", ToolTip = "Returns sorted configured pattern aliases currently prepared in this Game Instance without loading assets.", WorldContext = "WorldContextObject"))
	static TArray<FName> GetPreparedHapticPatternNames(
		const UObject* WorldContextObject
	);

	/** Checks the prepared cache only, it won't trigger loading as a side effect. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Is Haptic Pattern Ready", Keywords = "haptic named prepared loaded", ToolTip = "Returns true when a configured pattern alias is prepared in this Game Instance. Prefer pattern assets in ordinary gameplay graphs.", WorldContext = "WorldContextObject"))
	static bool IsHapticPatternReady(
		const UObject* WorldContextObject,
		FName PatternName
	);

	/** Reads the current player switch for this Game Instance, saved preference handling stays with the game. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Policy", meta = (DisplayName = "Get Haptics Enabled", Keywords = "haptic policy accessibility player enabled", ToolTip = "Returns the current Game Instance player switch. Runtime changes are not saved automatically.", WorldContext = "WorldContextObject"))
	static bool GetHapticsEnabled(const UObject* WorldContextObject);

	/** Updates runtime player policy and returns validation separately, it doesn't write a save game. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Policy", meta = (DisplayName = "Set Haptics Enabled", ExpandEnumAsExecs = "Outcome", Keywords = "haptic policy accessibility player enabled", ToolTip = "Updates the current Game Instance player switch. The game remains responsible for saving this preference.", WorldContext = "WorldContextObject"))
	static void SetHapticsEnabled(
		const UObject* WorldContextObject,
		bool bEnabled,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Returns the current normalized player scale after runtime policy changes. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Policy", meta = (DisplayName = "Get Haptics Master Intensity", Keywords = "haptic policy accessibility strength volume", ToolTip = "Returns the current normalized player intensity from zero through one. Runtime changes are not saved automatically.", WorldContext = "WorldContextObject"))
	static float GetHapticsMasterIntensity(
		const UObject* WorldContextObject
	);

	/** Clamps the runtime master scale so wired Blueprint values can't send unsafe strength downstream. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Policy", meta = (CPP_Default_MasterIntensity = "1.0", DisplayName = "Set Haptics Master Intensity", ExpandEnumAsExecs = "Outcome", Keywords = "haptic policy accessibility strength volume", ToolTip = "Sets normalized player intensity for this Game Instance. Wired values are clamped, and the game remains responsible for persistence.", WorldContext = "WorldContextObject"))
	static void SetHapticsMasterIntensity(
		const UObject* WorldContextObject,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized player intensity from zero through one."))
		float MasterIntensity,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Returns the resolved scale and tells you whether it came from an explicit player override. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Policy", meta = (DisplayName = "Get Haptic Category Intensity", Keywords = "haptic policy accessibility category scale", ToolTip = "Returns the current category scale and whether this Game Instance has an explicit override. The resolved default is one.", WorldContext = "WorldContextObject"))
	static float GetHapticCategoryIntensity(
		const UObject* WorldContextObject,
		FOpenMobileHapticCategoryIdentifier Category,
		UPARAM(DisplayName = "Has Override") bool& bHasOverride
	);

	/** Changes one category without replacing unrelated player settings, persistence is still your job. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Policy", meta = (CPP_Default_Intensity = "1.0", DisplayName = "Set Haptic Category Intensity", ExpandEnumAsExecs = "Outcome", Keywords = "haptic policy accessibility category scale", ToolTip = "Sets one typed category scale without replacing unrelated player policy values. Wired values are clamped and are not saved automatically.", WorldContext = "WorldContextObject"))
	static void SetHapticCategoryIntensity(
		const UObject* WorldContextObject,
		FOpenMobileHapticCategoryIdentifier Category,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized category intensity from zero through one."))
		float Intensity,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Removes the explicit category value so resolution goes back to its default scale. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Policy", meta = (DisplayName = "Reset Haptic Category Intensity", ExpandEnumAsExecs = "Outcome", Keywords = "haptic policy accessibility category reset default", ToolTip = "Removes one typed category override so its resolved scale returns to one. Runtime changes are not saved automatically.", WorldContext = "WorldContextObject"))
	static void ResetHapticCategoryIntensity(
		const UObject* WorldContextObject,
		FOpenMobileHapticCategoryIdentifier Category,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Returns one effect's resolved player scale and whether the player actually overrode it. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Policy", meta = (DisplayName = "Get Haptic Effect Intensity", Keywords = "haptic policy accessibility effect scale", ToolTip = "Returns the current effect scale and whether this Game Instance has an explicit override. The resolved default is one.", WorldContext = "WorldContextObject"))
	static float GetHapticEffectIntensity(
		const UObject* WorldContextObject,
		FOpenMobileHapticEffectIdentifier Effect,
		UPARAM(DisplayName = "Has Override") bool& bHasOverride
	);

	/** Updates one typed effect value only, avoiding accidental replacement of the rest of the user policy. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Policy", meta = (CPP_Default_Intensity = "1.0", DisplayName = "Set Haptic Effect Intensity", ExpandEnumAsExecs = "Outcome", Keywords = "haptic policy accessibility effect scale", ToolTip = "Sets one typed effect scale without replacing unrelated player policy values. Wired values are clamped and are not saved automatically.", WorldContext = "WorldContextObject"))
	static void SetHapticEffectIntensity(
		const UObject* WorldContextObject,
		FOpenMobileHapticEffectIdentifier Effect,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized effect intensity from zero through one."))
		float Intensity,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Clears one effect override and leaves category, master, and enable settings untouched. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Policy", meta = (DisplayName = "Reset Haptic Effect Intensity", ExpandEnumAsExecs = "Outcome", Keywords = "haptic policy accessibility effect reset default", ToolTip = "Removes one typed effect override so its resolved scale returns to one. Runtime changes are not saved automatically.", WorldContext = "WorldContextObject"))
	static void ResetHapticEffectIntensity(
		const UObject* WorldContextObject,
		FOpenMobileHapticEffectIdentifier Effect,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);

	/** Anchors an external clock to the platform monotonic clock so later schedules can report their expected precision. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Timing", meta = (CPP_Default_EstimatedPrecisionSeconds = "0.005", DisplayName = "Calibrate Haptic Timing", ExpandEnumAsExecs = "Outcome", Keywords = "haptic sync game audio clock calibration", ToolTip = "Samples one Unreal clock against platform monotonic time and reports Calibrated, Clock Reset, or Rejected directly.", WorldContext = "WorldContextObject"))
	static void CalibrateHapticTiming(
		const UObject* WorldContextObject,
		EOpenMobileHapticTimingClock Clock,
		UPARAM(DisplayName = "Clock Time", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Current time from the selected Unreal clock."))
		double ClockTimeSeconds,
		UPARAM(DisplayName = "Estimated Precision", meta = (ClampMin = "0.0", ClampMax = "0.1", Units = "s", ToolTip = "Estimated uncertainty of the supplied clock sample in seconds."))
		double EstimatedPrecisionSeconds,
		EOpenMobileHapticCalibrationOutcome& Outcome,
		FString& Error
	);

	/** Checks that the clock has a live calibration for this lifecycle generation, old foreground sessions don't count. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Timing", meta = (DisplayName = "Is Haptic Clock Calibrated", Keywords = "haptic sync game audio clock ready", ToolTip = "Returns true when the selected clock has a current calibration for this application lifecycle generation.", WorldContext = "WorldContextObject"))
	static bool IsHapticClockCalibrated(
		const UObject* WorldContextObject,
		EOpenMobileHapticTimingClock Clock
	);

	/** Returns the estimated scheduling precision from calibration, zero is used when no valid estimate exists. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Timing", meta = (DisplayName = "Get Haptic Timing Accuracy", Keywords = "haptic sync game audio clock precision", ReturnDisplayName = "Estimated Precision", ToolTip = "Returns the estimated calibration precision in seconds. Returns zero when the selected clock is not currently calibrated.", WorldContext = "WorldContextObject"))
	static double GetHapticTimingAccuracy(
		const UObject* WorldContextObject,
		EOpenMobileHapticTimingClock Clock
	);

	/** Starts with the standard UI channel and policy defaults, useful when you only need a few advanced overrides. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make UI Haptic Options", Keywords = "haptic recommended channel selection", ReturnDisplayName = "Options", ToolTip = "Creates immediate UI options using the recommended UI channel and category."))
	static FOpenMobileHapticPlaybackOptions MakeUIHapticOptions();

	/** Starts with the standard Gameplay channel so manual options match the common gameplay nodes. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make Gameplay Haptic Options", Keywords = "haptic recommended channel game", ReturnDisplayName = "Options", ToolTip = "Creates immediate gameplay options using the recommended Gameplay channel and category."))
	static FOpenMobileHapticPlaybackOptions MakeGameplayHapticOptions();

	/** Starts with the Alerts channel and its expected priority for user-visible feedback. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make Alert Haptic Options", Keywords = "haptic recommended channel warning", ReturnDisplayName = "Options", ToolTip = "Creates immediate high-priority alert options using the recommended Alerts channel and category."))
	static FOpenMobileHapticPlaybackOptions MakeAlertHapticOptions();

	/** Explicitly asks project settings to choose the channel, which is different from the common node defaults. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make Project-Default Haptic Options", Keywords = "haptic settings default channel", ReturnDisplayName = "Options", ToolTip = "Creates immediate options using the configured project default channel and category. This is distinct from effect-recommended defaults."))
	static FOpenMobileHapticPlaybackOptions MakeProjectDefaultHapticOptions();

	/** Converts a built-in channel choice to its stable typed name, avoiding hand-written strings in graphs. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make Standard Haptic Channel", Keywords = "haptic typed channel UI gameplay alerts accessibility cinematic", ReturnDisplayName = "Channel", ToolTip = "Creates a typed identifier for one standard project Haptics channel."))
	static FOpenMobileHapticChannelIdentifier MakeStandardHapticChannel(
		EOpenMobileHapticStandardChannel Channel
	);

	/** Converts a built-in policy category to its stable typed name for request overrides. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make Standard Haptic Category", Keywords = "haptic typed category UI gameplay alerts accessibility cinematic", ReturnDisplayName = "Category", ToolTip = "Creates a typed player-policy category matching one standard Haptics channel."))
	static FOpenMobileHapticCategoryIdentifier MakeStandardHapticCategory(
		EOpenMobileHapticStandardChannel Category
	);

	/** Builds advanced options in one node so channel, overlap, fallback, schedule, and loop values stay together. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (AdvancedDisplay = "Schedule,Loop,InterruptionPolicy", CPP_Default_IntensityScale = "1.0", DisplayName = "Make Haptic Playback Options", Keywords = "haptic typed channel category priority overlap fallback schedule loop", ReturnDisplayName = "Options", ToolTip = "Creates intentional advanced playback options from distinct typed channel and category identifiers. Empty identifiers resolve to project defaults."))
	static FOpenMobileHapticPlaybackOptions MakeHapticPlaybackOptions(
		FOpenMobileHapticChannelIdentifier Channel,
		FOpenMobileHapticCategoryIdentifier Category,
		EOpenMobileHapticChannelPriority Priority,
		EOpenMobileHapticOverlapPolicy OverlapPolicy,
		EOpenMobileHapticFallbackPolicy FallbackPolicy,
		UPARAM(DisplayName = "Intensity Scale", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Additional normalized request scale from zero through one."))
		float IntensityScale,
		FOpenMobileHapticSchedule Schedule,
		FOpenMobileHapticLoopOptions Loop,
		EOpenMobileHapticInterruptionPolicy InterruptionPolicy
	);

	/** Wraps a configured pattern name in its own Blueprint type so it can't be wired into an unrelated name input. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Make Haptic Pattern Identifier", Keywords = "haptic typed pattern alias name advanced", NativeMakeFunc, ReturnDisplayName = "Pattern Identifier", ToolTip = "Creates a typed configured-pattern identifier from a raw alias. Prefer direct pattern assets or configured identifier lists in common graphs."))
	static FOpenMobileHapticPatternIdentifier MakeHapticPatternIdentifier(
		UPARAM(meta = (GetOptions = "GetConfiguredHapticPatternNames"))
		FName Name
	);

	/** Unwraps the stable name when diagnostics or dynamic tooling genuinely needs the raw value. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Break Haptic Pattern Identifier", Keywords = "haptic typed pattern alias name advanced", NativeBreakFunc, ToolTip = "Returns the raw configured alias for advanced diagnostics and migration."))
	static void BreakHapticPatternIdentifier(
		FOpenMobileHapticPatternIdentifier Identifier,
		FName& Name
	);

	/** Creates a typed library ID so preparation inputs can't be confused with pattern names. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Make Haptic Library Identifier", Keywords = "haptic typed library name advanced", NativeMakeFunc, ReturnDisplayName = "Library Identifier", ToolTip = "Creates a typed configured-library identifier from the Project Settings picker."))
	static FOpenMobileHapticLibraryIdentifier MakeHapticLibraryIdentifier(
		UPARAM(meta = (GetOptions = "GetConfiguredHapticLibraryNames"))
		FName Name
	);

	/** Exposes the raw library name for tools while ordinary gameplay can keep the typed value intact. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Break Haptic Library Identifier", Keywords = "haptic typed library name advanced", NativeBreakFunc, ToolTip = "Returns the raw configured library name for advanced diagnostics and migration."))
	static void BreakHapticLibraryIdentifier(
		FOpenMobileHapticLibraryIdentifier Identifier,
		FName& Name
	);

	/** Creates a typed project channel value and prevents accidental wiring from other name-based settings. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Make Haptic Channel Identifier", Keywords = "haptic typed channel name advanced", NativeMakeFunc, ReturnDisplayName = "Channel Identifier", ToolTip = "Creates a typed Haptics channel identifier from a raw project name. Prefer standard channel constructors in common graphs."))
	static FOpenMobileHapticChannelIdentifier MakeHapticChannelIdentifier(
		UPARAM(meta = (GetOptions = "GetConfiguredHapticChannelNames"))
		FName Name
	);

	/** Returns the underlying channel name for logs or data-driven tooling. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Break Haptic Channel Identifier", Keywords = "haptic typed channel name advanced", NativeBreakFunc, ToolTip = "Returns the raw project channel name for advanced diagnostics and migration."))
	static void BreakHapticChannelIdentifier(
		FOpenMobileHapticChannelIdentifier Identifier,
		FName& Name
	);

	/** Wraps a project policy category so Blueprint can enforce the right input type. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Make Haptic Category Identifier", Keywords = "haptic typed policy category name advanced", NativeMakeFunc, ReturnDisplayName = "Category Identifier", ToolTip = "Creates a typed player-policy category identifier from a raw project name."))
	static FOpenMobileHapticCategoryIdentifier MakeHapticCategoryIdentifier(
		UPARAM(meta = (GetOptions = "GetConfiguredHapticCategoryNames"))
		FName Name
	);

	/** Returns the raw category name when a generic settings or diagnostics UI needs it. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Break Haptic Category Identifier", Keywords = "haptic typed policy category name advanced", NativeBreakFunc, ToolTip = "Returns the raw player-policy category name for advanced diagnostics and migration."))
	static void BreakHapticCategoryIdentifier(
		FOpenMobileHapticCategoryIdentifier Identifier,
		FName& Name
	);

	/** Wraps an effect key for player policy nodes, keeping it separate from channels and patterns. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Make Haptic Effect Identifier", Keywords = "haptic typed policy effect name advanced", NativeMakeFunc, ReturnDisplayName = "Effect Identifier", ToolTip = "Creates a typed effect-scale identifier from a raw stable effect or configured alias."))
	static FOpenMobileHapticEffectIdentifier MakeHapticEffectIdentifier(
		UPARAM(meta = (GetOptions = "GetConfiguredHapticEffectNames"))
		FName Name
	);

	/** Unwraps an effect key for tooling that stores generic names. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Break Haptic Effect Identifier", Keywords = "haptic typed policy effect name advanced", NativeBreakFunc, ToolTip = "Returns the raw effect-scale key for advanced diagnostics and migration."))
	static void BreakHapticEffectIdentifier(
		FOpenMobileHapticEffectIdentifier Identifier,
		FName& Name
	);

	/** Builds a bounded repeat request and clamps it later against project safety limits during submission. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (CPP_Default_TotalPlayCount = "2", CPP_Default_RepeatStartTimeSeconds = "0.0", CPP_Default_MaximumDurationSeconds = "30.0", DisplayName = "Make Finite Haptic Loop", Keywords = "haptic repeat bounded", ReturnDisplayName = "Loop", ToolTip = "Creates a bounded loop with an explicit total play count. Total play count includes the first play."))
	static FOpenMobileHapticLoopOptions MakeFiniteHapticLoop(
		UPARAM(meta = (ClampMin = "2", ToolTip = "Total number of plays, including the first play."))
		int32 TotalPlayCount,
		UPARAM(DisplayName = "Repeat Start", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Timeline position used when each repeat begins."))
		double RepeatStartTimeSeconds,
		UPARAM(DisplayName = "Maximum Duration", meta = (ClampMin = "0.1", Units = "s", ToolTip = "Safety limit for the entire repeated request."))
		double MaximumDurationSeconds
	);

	/** Creates an explicit owner-stopped loop with a maximum duration, so forgotten controls can't vibrate forever. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (CPP_Default_RepeatStartTimeSeconds = "0.0", CPP_Default_MaximumDurationSeconds = "30.0", DisplayName = "Make Haptic Loop Until Stopped", Keywords = "haptic repeat continuous bounded", ReturnDisplayName = "Loop", ToolTip = "Creates repeat-until-stopped behavior with a mandatory finite safety duration."))
	static FOpenMobileHapticLoopOptions MakeHapticLoopUntilStopped(
		UPARAM(DisplayName = "Repeat Start", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Timeline position used when each repeat begins."))
		double RepeatStartTimeSeconds,
		UPARAM(DisplayName = "Maximum Duration", meta = (ClampMin = "0.1", Units = "s", ToolTip = "Safety limit after which playback ends even when Stop was not called."))
		double MaximumDurationSeconds
	);

	/** Schedules relative to submission time, useful when no external clock owns the cue. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Timing", meta = (DisplayName = "Make Haptic Delay Schedule", Keywords = "haptic after delay relative", ReturnDisplayName = "Schedule", ToolTip = "Creates a relative schedule that starts after a non-negative delay."))
	static FOpenMobileHapticSchedule MakeHapticDelaySchedule(
		UPARAM(DisplayName = "Delay", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Delay in seconds from submission."))
		double DelaySeconds
	);

	/** Schedules against calibrated game time and carries a latency offset for measured device output delay. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Timing", meta = (CPP_Default_LatencyOffsetSeconds = "0.0", DisplayName = "Make Haptic Game-Time Schedule", Keywords = "haptic absolute game clock", ReturnDisplayName = "Schedule", ToolTip = "Creates an absolute Unreal game-time schedule. Calibrate the game clock before submission."))
	static FOpenMobileHapticSchedule MakeHapticGameTimeSchedule(
		UPARAM(DisplayName = "Game Time", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Absolute time sampled from the same Unreal game clock used for calibration."))
		double GameTimeSeconds,
		UPARAM(DisplayName = "Latency Offset", meta = (Units = "s", ToolTip = "Optional signed device-latency correction."))
		double LatencyOffsetSeconds
	);

	/** Schedules against calibrated audio time when haptics have to align with an audio transport. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Timing", meta = (CPP_Default_LatencyOffsetSeconds = "0.0", DisplayName = "Make Haptic Audio-Time Schedule", Keywords = "haptic absolute audio clock sync", ReturnDisplayName = "Schedule", ToolTip = "Creates an absolute Unreal audio-time schedule. Calibrate the audio clock before submission."))
	static FOpenMobileHapticSchedule MakeHapticAudioTimeSchedule(
		UPARAM(DisplayName = "Audio Time", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Absolute time sampled from the same Unreal audio clock used for calibration."))
		double AudioTimeSeconds,
		UPARAM(DisplayName = "Latency Offset", meta = (Units = "s", ToolTip = "Optional signed device-latency correction."))
		double LatencyOffsetSeconds
	);

	/** Copies bounded runtime diagnostics for development UI without exposing mutable subsystem state. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Get Haptics Diagnostics Snapshot", Keywords = "haptic debug state performance", ReturnDisplayName = "Diagnostics", ToolTip = "Captures the full diagnostics snapshot once for this execution. Use small pure capability nodes for ordinary gameplay.", WorldContext = "WorldContextObject"))
	static FOpenMobileHapticsDiagnostics GetHapticsDiagnosticsSnapshot(
		const UObject* WorldContextObject
	);

	/** Attaches a task to an accepted raw handle so legacy code can receive one typed final branch. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Control", meta = (DisplayName = "Wait For Haptic Playback", ExpandEnumAsExecs = "Outcome", Keywords = "haptic raw handle adapt lifecycle wait migrate", ToolTip = "Adapts one active raw handle into the request-scoped Haptic Playback object. Prefer the playback object returned by common play nodes for new graphs.", WorldContext = "WorldContextObject"))
	static void WaitForHapticPlayback(
		const UObject* WorldContextObject,
		FOpenMobileHapticPlaybackHandle Handle,
		EOpenMobileHapticControlBranch& Outcome,
		UOpenMobileHapticPlayback*& Playback,
		FOpenMobileHapticError& Error
	);

	/** Expands the immediate result for diagnostics-heavy graphs that need more than the common play-node outputs. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (AdvancedDisplay = "Handle,SuppressionReason,ErrorCode", DisplayName = "Break Haptic Playback Result", Keywords = "haptic result compact accepted suppressed fallback error", NativeBreakFunc, ToolTip = "Breaks the broad advanced result into the compact fields used for ordinary request decisions."))
	static void BreakHapticPlaybackResult(
		const FOpenMobileHapticPlaybackResult& Result,
		bool& bAccepted,
		bool& bSuppressed,
		UPARAM(DisplayName = "Used Fallback") bool& bUsedFallback,
		FOpenMobileHapticPlaybackHandle& Handle,
		EOpenMobileHapticSuppressionReason& SuppressionReason,
		EOpenMobileHapticErrorCode& ErrorCode,
		FString& Message
	);

private:
	/** Converts an immediate native result into common Blueprint branches while preserving fallback and error detail. */
	static void ResolveCommonResult(
		UOpenMobileHapticsSubsystem* Subsystem,
		const FOpenMobileHapticPlaybackResult& Result,
		FName PatternOrEffect,
		EOpenMobileHapticRequestOutcome& Outcome,
		UOpenMobileHapticPlayback*& Playback,
		bool& bUsedFallback,
		FName& ResolvedQuality,
		FOpenMobileHapticError& Error
	);
	/** Maps a control response to Succeeded or Failed and always returns the typed error for logging or recovery. */
	static void ResolveControlResult(
		const FOpenMobileHapticControlResult& Result,
		EOpenMobileHapticControlBranch& Outcome,
		FOpenMobileHapticError& Error
	);
};
