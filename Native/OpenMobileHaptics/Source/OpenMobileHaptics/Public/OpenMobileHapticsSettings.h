#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticsSettings.generated.h"

UENUM(BlueprintType, meta = (ToolTip = "Project policy for Haptics while the application is not active."))
enum class EOpenMobileHapticBackgroundPolicy : uint8
{
	StopAll UMETA(DisplayName = "Stop All", ToolTip = "Suppresses every Haptics request outside the active foreground state."),
	CriticalOnly UMETA(DisplayName = "Critical Alerts Only", ToolTip = "Allows only Critical-priority Alerts or Accessibility feedback when the platform explicitly supports background output."),
	AllowAll UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticChannelSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Channel", meta = (ToolTip = "Stable project-defined channel name used by playback options and player policy."))
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Channel", meta = (ToolTip = "Base channel priority used for overlap and interruption decisions."))
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Channel", meta = (ClampMin = "0", ClampMax = "128", ToolTip = "Maximum simultaneous active requests owned by this channel."))
	int32 MaximumActiveHandles = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Channel", meta = (ClampMin = "0", ClampMax = "64", ToolTip = "Maximum requests waiting behind active work on this channel."))
	int32 MaximumQueueDepth = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Channel", meta = (ToolTip = "Overlap behavior used when native mixing is unavailable."))
	EOpenMobileHapticOverlapPolicy UnsupportedMixFallbackPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Channel", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s", ToolTip = "Minimum seconds between accepted submissions on this channel."))
	float MinimumIntervalSeconds = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Channel", meta = (ClampMin = "1", ClampMax = "30", ToolTip = "Maximum accepted submissions per second on this channel."))
	int32 MaximumSubmissionsPerSecond = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Channel", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized scale multiplied into every request on this channel."))
	float IntensityScale = 1.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticEffectSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Effect", meta = (ToolTip = "Stable semantic effect or configured pattern key receiving this override."))
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Effect", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized project scale multiplied into the matching effect."))
	float IntensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Effect", meta = (ClampMin = "0.0", ClampMax = "10.0", Units = "s", ToolTip = "Minimum seconds between matching effect submissions."))
	float MinimumIntervalSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticNamedLibrarySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Library", meta = (ToolTip = "Stable configured library identifier shown by typed library pickers."))
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Library", meta = (AllowedClasses = "/Script/OpenMobileHaptics.OpenMobileHapticLibrary", ToolTip = "Soft reference to the Haptic Library asset prepared for this identifier."))
	FSoftObjectPath Asset;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticAndroidSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Android", meta = (ToolTip = "Allows portable semantic feedback on Android when the device supports it."))
	bool bEnableSemanticFeedback = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Android", meta = (ToolTip = "Prefers Android predefined effects before portable waveform fallback."))
	bool bPreferPredefinedEffects = true;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticIOSSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|iOS", meta = (ToolTip = "Allows UIKit semantic feedback generators on iOS."))
	bool bEnableSemanticFeedback = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|iOS", meta = (ToolTip = "Allows Core Haptics pattern playback on supported iOS devices."))
	bool bEnableCoreHaptics = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|iOS", meta = (DisplayName = "Package AHAP Resources", ToolTip = "Includes imported AHAP and referenced audio resources in supported iOS builds."))
	bool bPackageAHAPResources = true;
};

UCLASS(
	Config = Game,
	DefaultConfig,
	meta = (DisplayName = "OpenMobile Haptics")
)
class OPENMOBILEHAPTICS_API UOpenMobileHapticsSettings final :
	public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UOpenMobileHapticsSettings();

	virtual FName GetCategoryName() const override { return TEXT("OpenMobile"); }
	virtual FName GetSectionName() const override
	{
		return TEXT("OpenMobile Haptics");
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Defaults", meta = (DisplayName = "Enabled by Default", ToolTip = "Initial per-player Haptics state. Games remain responsible for saving player preferences."))
	bool bEnabledByDefault = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Defaults", meta = (DisplayName = "Allow Critical Feedback When Disabled by Default", ToolTip = "Initial opt-in for Critical-priority Alerts or Accessibility feedback while the global Haptics switch is off. Games remain responsible for saving player preferences."))
	bool bAllowCriticalFeedbackWhenDisabledByDefault = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Defaults", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Default Master Intensity", ToolTip = "Initial per-player master intensity. Games remain responsible for saving later changes."))
	float DefaultMasterIntensity = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Defaults", meta = (ToolTip = "Project channel used only when playback explicitly requests Project Default options."))
	FName DefaultChannel = TEXT("Gameplay");

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Defaults", meta = (ToolTip = "Project policy category used when no effect, asset, or request category is supplied."))
	FName DefaultCategory = TEXT("Gameplay");

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Libraries", meta = (TitleProperty = "Name", ToolTip = "Libraries available to preparation tasks and configured pattern pickers."))
	TArray<FOpenMobileHapticNamedLibrarySettings> NamedLibraries;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Channels", meta = (TitleProperty = "Name", ToolTip = "Project channel limits, priorities, rate limits, and intensity scales."))
	TArray<FOpenMobileHapticChannelSettings> Channels;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Effects", meta = (TitleProperty = "Name", ToolTip = "Project intensity and rate overrides for stable effect keys."))
	TArray<FOpenMobileHapticEffectSettings> EffectOverrides;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Lifecycle", meta = (ToolTip = "StopAll blocks background playback. CriticalOnly permits explicit Critical Alerts only when the active platform reports background alert support. AllowAll is rejected by validation and fails closed at runtime."))
	EOpenMobileHapticBackgroundPolicy BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::StopAll;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Lifecycle", meta = (ToolTip = "Attempts to resume eligible interrupted playback after the app returns to the foreground."))
	bool bResumeEligiblePlaybackAfterForeground = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Lifecycle", meta = (ToolTip = "Retains recent rate-limit history over foreground transitions so a rapid resume cannot produce a comfort-breaking burst. Disable only when a fresh foreground session must start with an empty limiter."))
	bool bRetainRateLimitStateAcrossForeground = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Lifecycle", meta = (ClampMin = "0", ClampMax = "8", ToolTip = "Maximum backend recovery attempts before requests remain unavailable."))
	int32 MaximumRecoveryAttempts = 2;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Queue Limits", meta = (ClampMin = "1", ClampMax = "128", ToolTip = "Maximum active plugin-owned playback handles across all channels."))
	int32 MaximumActiveHandles = 16;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Queue Limits", meta = (ClampMin = "1", ClampMax = "256", ToolTip = "Maximum queued plugin-owned requests across all channels."))
	int32 MaximumQueuedHandles = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Queue Limits", meta = (ClampMin = "1", ClampMax = "64", ToolTip = "Default maximum queued requests for one channel."))
	int32 MaximumQueueDepthPerChannel = 8;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Queue Limits", meta = (ClampMin = "0.01", ClampMax = "30.0", Units = "s", ToolTip = "Maximum seconds a queued request may wait before expiring."))
	float MaximumQueuedRequestAgeSeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Queue Limits", meta = (ClampMin = "1", ClampMax = "128", ToolTip = "Maximum prepared pattern entries retained by the runtime cache."))
	int32 MaximumPreparedPatterns = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Preparation Limits", meta = (ClampMin = "64", ClampMax = "65536", Units = "KB", ToolTip = "Approximate memory budget in kilobytes for prepared pattern data."))
	int32 MaximumPreparedPatternMemoryKilobytes = 4096;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Preparation Limits", meta = (ClampMin = "1.0", ClampMax = "300.0", Units = "s", ToolTip = "Seconds unused prepared data may remain cached without an owner."))
	float PreparedPatternIdleLifetimeSeconds = 30.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Queue Limits", meta = (ClampMin = "1", ClampMax = "512", ToolTip = "Maximum recent playback events retained in diagnostics."))
	int32 MaximumDiagnosticEvents = 64;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Loop Limits", meta = (ClampMin = "1", ClampMax = "1000", ToolTip = "Maximum allowed finite repeat count after the first play."))
	int32 MaximumFiniteRepeatCount = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Loop Limits", meta = (ClampMin = "0.1", ClampMax = "300.0", Units = "s", ToolTip = "Safety duration in seconds applied to repeat-until-stopped playback."))
	float MaximumContinuousDurationSeconds = 30.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Pattern Limits", meta = (ClampMin = "0.001", ClampMax = "300.0", Units = "s", ToolTip = "Maximum duration in seconds for one authored continuous event."))
	float MaximumPatternEventDurationSeconds = 10.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Pattern Limits", meta = (ClampMin = "1", ClampMax = "4096", ToolTip = "Maximum authored event count in one portable pattern."))
	int32 MaximumPatternEventCount = 128;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Pattern Limits", meta = (ClampMin = "0", ClampMax = "128", ToolTip = "Maximum parameter-curve count in one portable pattern."))
	int32 MaximumPatternCurveCount = 16;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Pattern Limits", meta = (ClampMin = "1", ClampMax = "4096", ToolTip = "Maximum total control-point count in one portable pattern."))
	int32 MaximumPatternCurvePointCount = 256;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Pattern Limits", meta = (ClampMin = "0.0001", ClampMax = "1.0", Units = "s", ToolTip = "Smallest supported authored timing interval in seconds."))
	float MinimumPatternGranularitySeconds = 0.001f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|One Shot", meta = (ClampMin = "0.001", ClampMax = "1.0", Units = "s", ToolTip = "Shortest one-shot phone vibration accepted by common nodes."))
	float MinimumOneShotDurationSeconds = 0.001f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|One Shot", meta = (ClampMin = "0.001", ClampMax = "30.0", Units = "s", ToolTip = "Longest one-shot phone vibration accepted by common nodes."))
	float MaximumOneShotDurationSeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Rate Limits", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s", ToolTip = "Default minimum interval in seconds when a channel has no override."))
	float DefaultMinimumIntervalSeconds = 0.02f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Rate Limits", meta = (ClampMin = "1", ClampMax = "60", ToolTip = "Default maximum accepted submissions per second across channels without overrides."))
	int32 MaximumSubmissionsPerSecond = 30;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Rate Limits", meta = (ClampMin = "1", ClampMax = "240", DisplayName = "Maximum Dynamic Parameter Updates Per Second", ToolTip = "Maximum native dynamic-parameter submissions per active playback handle. Newer values replace pending values within the interval."))
	int32 MaximumDynamicParameterUpdatesPerSecond = 60;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Rate Limits", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s", ToolTip = "Seconds used to coalesce rapid selection feedback requests."))
	float SelectionDebounceSeconds = 0.04f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Rate Limits", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s", ToolTip = "Coalesces otherwise equivalent immediate semantic UI requests without delaying the first request."))
	float UIRequestDebounceSeconds = 0.02f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Platform", meta = (ToolTip = "Enables portable custom pattern playback. Platform packaging options must agree with this setting."))
	bool bEnableCustomPlayback = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Platform|Android", meta = (DisplayName = "Enable Android Custom Vibration", ToolTip = "Packages android.permission.VIBRATE and enables custom Android vibration. Rebuild and repackage after changing this setting."))
	bool bEnableAndroidCustomVibration = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Platform", meta = (ToolTip = "Android-specific semantic and predefined-effect preferences."))
	FOpenMobileHapticAndroidSettings Android;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Settings|Platform", meta = (ToolTip = "iOS-specific semantic, Core Haptics, and packaging preferences."))
	FOpenMobileHapticIOSSettings IOS;

	bool Validate(TArray<FString>& OutErrors) const;
};
