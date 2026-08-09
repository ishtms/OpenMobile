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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel")
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel", meta = (ClampMin = "0", ClampMax = "128"))
	int32 MaximumActiveHandles = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaximumQueueDepth = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel")
	EOpenMobileHapticOverlapPolicy UnsupportedMixFallbackPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float MinimumIntervalSeconds = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel", meta = (ClampMin = "1", ClampMax = "30"))
	int32 MaximumSubmissionsPerSecond = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityScale = 1.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticEffectSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.0", ClampMax = "10.0", Units = "s"))
	float MinimumIntervalSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticNamedLibrarySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Library")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Library", meta = (AllowedClasses = "/Script/OpenMobileHaptics.OpenMobileHapticLibrary"))
	FSoftObjectPath Asset;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticAndroidSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android")
	bool bEnableSemanticFeedback = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android")
	bool bPreferPredefinedEffects = true;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticIOSSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "iOS")
	bool bEnableSemanticFeedback = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "iOS")
	bool bEnableCoreHaptics = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "iOS", meta = (DisplayName = "Package AHAP Resources"))
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

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Defaults", meta = (DisplayName = "Enabled by Default", ToolTip = "Initial per-player Haptics state. Games remain responsible for saving player preferences."))
	bool bEnabledByDefault = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Defaults", meta = (DisplayName = "Allow Critical Feedback When Disabled by Default", ToolTip = "Initial opt-in for Critical-priority Alerts or Accessibility feedback while the global Haptics switch is off. Games remain responsible for saving player preferences."))
	bool bAllowCriticalFeedbackWhenDisabledByDefault = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Defaults", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Default Master Intensity", ToolTip = "Initial per-player master intensity. Games remain responsible for saving later changes."))
	float DefaultMasterIntensity = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
	FName DefaultChannel = TEXT("Gameplay");

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
	FName DefaultCategory = TEXT("Gameplay");

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Libraries")
	TArray<FOpenMobileHapticNamedLibrarySettings> NamedLibraries;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Channels")
	TArray<FOpenMobileHapticChannelSettings> Channels;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TArray<FOpenMobileHapticEffectSettings> EffectOverrides;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (ToolTip = "StopAll blocks background playback. CriticalOnly permits explicit Critical Alerts only when the active platform reports background alert support. AllowAll is rejected by validation and fails closed at runtime."))
	EOpenMobileHapticBackgroundPolicy BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::StopAll;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lifecycle")
	bool bResumeEligiblePlaybackAfterForeground = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (ToolTip = "Retains recent rate-limit history over foreground transitions so a rapid resume cannot produce a comfort-breaking burst. Disable only when a fresh foreground session must start with an empty limiter."))
	bool bRetainRateLimitStateAcrossForeground = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (ClampMin = "0", ClampMax = "8"))
	int32 MaximumRecoveryAttempts = 2;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaximumActiveHandles = 16;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaximumQueuedHandles = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaximumQueueDepthPerChannel = 8;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "0.01", ClampMax = "30.0", Units = "s"))
	float MaximumQueuedRequestAgeSeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaximumPreparedPatterns = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Preparation Limits", meta = (ClampMin = "64", ClampMax = "65536", Units = "KB"))
	int32 MaximumPreparedPatternMemoryKilobytes = 4096;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Preparation Limits", meta = (ClampMin = "1.0", ClampMax = "300.0", Units = "s"))
	float PreparedPatternIdleLifetimeSeconds = 30.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "512"))
	int32 MaximumDiagnosticEvents = 64;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loop Limits", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 MaximumFiniteRepeatCount = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loop Limits", meta = (ClampMin = "0.1", ClampMax = "300.0", Units = "s"))
	float MaximumContinuousDurationSeconds = 30.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pattern Limits", meta = (ClampMin = "0.001", ClampMax = "300.0", Units = "s"))
	float MaximumPatternEventDurationSeconds = 10.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pattern Limits", meta = (ClampMin = "1", ClampMax = "4096"))
	int32 MaximumPatternEventCount = 128;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pattern Limits", meta = (ClampMin = "0", ClampMax = "128"))
	int32 MaximumPatternCurveCount = 16;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pattern Limits", meta = (ClampMin = "1", ClampMax = "4096"))
	int32 MaximumPatternCurvePointCount = 256;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pattern Limits", meta = (ClampMin = "0.0001", ClampMax = "1.0", Units = "s"))
	float MinimumPatternGranularitySeconds = 0.001f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "One Shot", meta = (ClampMin = "0.001", ClampMax = "1.0", Units = "s"))
	float MinimumOneShotDurationSeconds = 0.001f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "One Shot", meta = (ClampMin = "0.001", ClampMax = "30.0", Units = "s"))
	float MaximumOneShotDurationSeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rate Limits", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float DefaultMinimumIntervalSeconds = 0.02f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rate Limits", meta = (ClampMin = "1", ClampMax = "60"))
	int32 MaximumSubmissionsPerSecond = 30;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rate Limits", meta = (ClampMin = "1", ClampMax = "240", DisplayName = "Maximum Dynamic Parameter Updates Per Second", ToolTip = "Maximum native dynamic-parameter submissions per active playback handle. Newer values replace pending values within the interval."))
	int32 MaximumDynamicParameterUpdatesPerSecond = 60;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rate Limits", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float SelectionDebounceSeconds = 0.04f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rate Limits", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s", ToolTip = "Coalesces otherwise equivalent immediate semantic UI requests without delaying the first request."))
	float UIRequestDebounceSeconds = 0.02f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Platform", meta = (ToolTip = "Enables portable custom pattern playback. Platform packaging options must agree with this setting."))
	bool bEnableCustomPlayback = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Platform|Android", meta = (DisplayName = "Enable Android Custom Vibration", ToolTip = "Packages android.permission.VIBRATE and enables custom Android vibration. Rebuild and repackage after changing this setting."))
	bool bEnableAndroidCustomVibration = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Platform")
	FOpenMobileHapticAndroidSettings Android;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Platform")
	FOpenMobileHapticIOSSettings IOS;

	bool Validate(TArray<FString>& OutErrors) const;
};
