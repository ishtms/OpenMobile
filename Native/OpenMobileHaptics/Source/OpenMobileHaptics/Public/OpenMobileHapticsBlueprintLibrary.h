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

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Is Haptic Playback Handle Valid", Keywords = "haptic guid handle valid", ToolTip = "Returns true when a raw playback handle contains an identifier. Prefer the Haptic Playback object in common graphs."))
	static bool IsHapticPlaybackHandleValid(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (CompactNodeTitle = "==", DisplayName = "Equal Haptic Playback Handles", Keywords = "haptic handle compare equal", ToolTip = "Returns true when two raw playback handles identify the same request."))
	static bool EqualHapticPlaybackHandles(
		FOpenMobileHapticPlaybackHandle A,
		FOpenMobileHapticPlaybackHandle B
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Is Haptic Preload Handle Valid", Keywords = "haptic preparation library handle valid", ToolTip = "Returns true when a raw library preload handle contains an identifier. Prefer an owned preparation task in common graphs."))
	static bool IsHapticPreloadHandleValid(
		FOpenMobileHapticLibraryPreloadHandle Handle
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (CompactNodeTitle = "==", DisplayName = "Equal Haptic Preload Handles", Keywords = "haptic preparation handle compare equal", ToolTip = "Returns true when two raw library preload handles identify the same request."))
	static bool EqualHapticPreloadHandles(
		FOpenMobileHapticLibraryPreloadHandle A,
		FOpenMobileHapticLibraryPreloadHandle B
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Is Haptic Request Accepted", Keywords = "haptic result success fallback", ToolTip = "Returns true for accepted native or fallback requests. Suppressed and rejected requests return false."))
	static bool IsHapticRequestAccepted(
		const FOpenMobileHapticPlaybackResult& Result
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Did Haptic Request Produce Output", Keywords = "haptic result silent output", ToolTip = "Returns true when the immediate request was accepted for an output path. It does not prove physical actuator observation."))
	static bool DidHapticRequestProduceOutput(
		const FOpenMobileHapticPlaybackResult& Result
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Has Haptic Error", Keywords = "haptic failed error", ToolTip = "Returns true when the Haptics error contains a stable error code."))
	static bool HasHapticError(const FOpenMobileHapticError& Error);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Get Haptic Result Summary", Keywords = "haptic status debug log message", ReturnDisplayName = "Summary", ToolTip = "Returns a short sanitized summary for development UI and logs."))
	static FString GetHapticResultSummary(
		const FOpenMobileHapticPlaybackResult& Result
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Format Haptic Error", Keywords = "haptic error message correction debug", ReturnDisplayName = "Message", ToolTip = "Returns a safe developer message with the stable code and correction, without native detail."))
	static FString FormatHapticError(const FOpenMobileHapticError& Error);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Get Haptic Recovery Action", Keywords = "haptic error retry fix recovery", ReturnDisplayName = "Recovery Action", ToolTip = "Maps a stable Haptics error to a suggested next step."))
	static EOpenMobileHapticRecoveryAction GetHapticRecoveryAction(
		const FOpenMobileHapticError& Error
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Get Haptic Availability", Keywords = "haptic support device vibrator", ToolTip = "Returns the current categorical Haptics availability. Never compare this enum by ordinal.", WorldContext = "WorldContextObject"))
	static EOpenMobileHapticAvailability GetHapticAvailability(
		const UObject* WorldContextObject
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Get Haptic Capability Tier", Keywords = "haptic support basic semantic rich", ToolTip = "Returns an ordered portable capability tier. Policy-disabled and temporarily unavailable states resolve to None.", WorldContext = "WorldContextObject"))
	static EOpenMobileHapticCapabilityTier GetHapticCapabilityTier(
		const UObject* WorldContextObject
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Can Play Haptics", Keywords = "haptic available vibrator support", ToolTip = "Returns true when the current state can produce at least basic phone vibration.", WorldContext = "WorldContextObject"))
	static bool CanPlayHaptics(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Can Play Semantic Haptics", Keywords = "haptic selection impact notification support", ToolTip = "Returns true when native semantic Haptics are currently available.", WorldContext = "WorldContextObject"))
	static bool CanPlaySemanticHaptics(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Can Play Rich Haptics", Keywords = "haptic pattern custom support", ToolTip = "Returns true when rich prepared-pattern Haptics are currently available.", WorldContext = "WorldContextObject"))
	static bool CanPlayRichHaptics(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Is Haptic Feature Supported", Keywords = "haptic capability tri state", ToolTip = "Returns true only for known support. Unknown is never treated as proof of support."))
	static bool IsHapticFeatureSupported(EOpenMobileHapticSupportState Support);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "Is Haptic Feature Known", Keywords = "haptic capability unknown", ToolTip = "Returns false only when support has not been determined."))
	static bool IsHapticFeatureKnown(EOpenMobileHapticSupportState Support);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Get Configured Haptic Pattern Names", Keywords = "haptic library names discover debug", ReturnDisplayName = "Pattern Names", ToolTip = "Returns sorted pattern aliases from configured libraries for development tools and dynamic browsers. This explicit tooling query may load library metadata; do not call it on a latency-sensitive gameplay path."))
	static TArray<FName> GetConfiguredHapticPatternNames();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Get Prepared Haptic Pattern Names", Keywords = "haptic library ready loaded names debug", ReturnDisplayName = "Pattern Names", ToolTip = "Returns sorted configured pattern aliases currently prepared in this Game Instance without loading assets.", WorldContext = "WorldContextObject"))
	static TArray<FName> GetPreparedHapticPatternNames(
		const UObject* WorldContextObject
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Is Haptic Pattern Ready", Keywords = "haptic named prepared loaded", ToolTip = "Returns true when a configured pattern alias is prepared in this Game Instance. Prefer pattern assets in ordinary gameplay graphs.", WorldContext = "WorldContextObject"))
	static bool IsHapticPatternReady(
		const UObject* WorldContextObject,
		FName PatternName
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make UI Haptic Options", Keywords = "haptic recommended channel selection", ReturnDisplayName = "Options", ToolTip = "Creates immediate UI options using the recommended UI channel and category."))
	static FOpenMobileHapticPlaybackOptions MakeUIHapticOptions();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make Gameplay Haptic Options", Keywords = "haptic recommended channel game", ReturnDisplayName = "Options", ToolTip = "Creates immediate gameplay options using the recommended Gameplay channel and category."))
	static FOpenMobileHapticPlaybackOptions MakeGameplayHapticOptions();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make Alert Haptic Options", Keywords = "haptic recommended channel warning", ReturnDisplayName = "Options", ToolTip = "Creates immediate high-priority alert options using the recommended Alerts channel and category."))
	static FOpenMobileHapticPlaybackOptions MakeAlertHapticOptions();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (DisplayName = "Make Project-Default Haptic Options", Keywords = "haptic settings default channel", ReturnDisplayName = "Options", ToolTip = "Creates immediate options using the configured project default channel and category. This is distinct from effect-recommended defaults."))
	static FOpenMobileHapticPlaybackOptions MakeProjectDefaultHapticOptions();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (CPP_Default_TotalPlayCount = "2", CPP_Default_RepeatStartTimeSeconds = "0.0", CPP_Default_MaximumDurationSeconds = "30.0", DisplayName = "Make Finite Haptic Loop", Keywords = "haptic repeat bounded", ReturnDisplayName = "Loop", ToolTip = "Creates a bounded loop with an explicit total play count. Total play count includes the first play."))
	static FOpenMobileHapticLoopOptions MakeFiniteHapticLoop(
		UPARAM(meta = (ClampMin = "2", ToolTip = "Total number of plays, including the first play."))
		int32 TotalPlayCount,
		UPARAM(DisplayName = "Repeat Start", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Timeline position used when each repeat begins."))
		double RepeatStartTimeSeconds,
		UPARAM(DisplayName = "Maximum Duration", meta = (ClampMin = "0.1", Units = "s", ToolTip = "Safety limit for the entire repeated request."))
		double MaximumDurationSeconds
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Options", meta = (CPP_Default_RepeatStartTimeSeconds = "0.0", CPP_Default_MaximumDurationSeconds = "30.0", DisplayName = "Make Haptic Loop Until Stopped", Keywords = "haptic repeat continuous bounded", ReturnDisplayName = "Loop", ToolTip = "Creates repeat-until-stopped behavior with a mandatory finite safety duration."))
	static FOpenMobileHapticLoopOptions MakeHapticLoopUntilStopped(
		UPARAM(DisplayName = "Repeat Start", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Timeline position used when each repeat begins."))
		double RepeatStartTimeSeconds,
		UPARAM(DisplayName = "Maximum Duration", meta = (ClampMin = "0.1", Units = "s", ToolTip = "Safety limit after which playback ends even when Stop was not called."))
		double MaximumDurationSeconds
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Timing", meta = (DisplayName = "Make Haptic Delay Schedule", Keywords = "haptic after delay relative", ReturnDisplayName = "Schedule", ToolTip = "Creates a relative schedule that starts after a non-negative delay."))
	static FOpenMobileHapticSchedule MakeHapticDelaySchedule(
		UPARAM(DisplayName = "Delay", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Delay in seconds from submission."))
		double DelaySeconds
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Timing", meta = (CPP_Default_LatencyOffsetSeconds = "0.0", DisplayName = "Make Haptic Game-Time Schedule", Keywords = "haptic absolute game clock", ReturnDisplayName = "Schedule", ToolTip = "Creates an absolute Unreal game-time schedule. Calibrate the game clock before submission."))
	static FOpenMobileHapticSchedule MakeHapticGameTimeSchedule(
		UPARAM(DisplayName = "Game Time", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Absolute time sampled from the same Unreal game clock used for calibration."))
		double GameTimeSeconds,
		UPARAM(DisplayName = "Latency Offset", meta = (Units = "s", ToolTip = "Optional signed device-latency correction."))
		double LatencyOffsetSeconds
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Timing", meta = (CPP_Default_LatencyOffsetSeconds = "0.0", DisplayName = "Make Haptic Audio-Time Schedule", Keywords = "haptic absolute audio clock sync", ReturnDisplayName = "Schedule", ToolTip = "Creates an absolute Unreal audio-time schedule. Calibrate the audio clock before submission."))
	static FOpenMobileHapticSchedule MakeHapticAudioTimeSchedule(
		UPARAM(DisplayName = "Audio Time", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Absolute time sampled from the same Unreal audio clock used for calibration."))
		double AudioTimeSeconds,
		UPARAM(DisplayName = "Latency Offset", meta = (Units = "s", ToolTip = "Optional signed device-latency correction."))
		double LatencyOffsetSeconds
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Diagnostics", meta = (DisplayName = "Get Haptics Diagnostics Snapshot", Keywords = "haptic debug state performance", ReturnDisplayName = "Diagnostics", ToolTip = "Captures the full diagnostics snapshot once for this execution. Use small pure capability nodes for ordinary gameplay.", WorldContext = "WorldContextObject"))
	static FOpenMobileHapticsDiagnostics GetHapticsDiagnosticsSnapshot(
		const UObject* WorldContextObject
	);

private:
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
};
