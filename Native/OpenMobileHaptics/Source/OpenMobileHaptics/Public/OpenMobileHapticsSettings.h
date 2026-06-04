#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticsSettings.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileHapticBackgroundPolicy : uint8
{
	StopAll,
	CriticalOnly,
	AllowAll
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaximumQueueDepth = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channel", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float MinimumIntervalSeconds = 0.02f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android", meta = (DisplayName = "Package Custom Vibration"))
	bool bPackageCustomVibration = true;

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

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lifecycle")
	EOpenMobileHapticBackgroundPolicy BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::StopAll;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lifecycle")
	bool bResumeEligiblePlaybackAfterForeground = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (ClampMin = "0", ClampMax = "8"))
	int32 MaximumRecoveryAttempts = 2;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaximumActiveHandles = 16;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaximumQueuedHandles = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaximumQueueDepthPerChannel = 8;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaximumPreparedPatterns = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Queue Limits", meta = (ClampMin = "1", ClampMax = "512"))
	int32 MaximumDiagnosticEvents = 64;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loop Limits", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 MaximumFiniteRepeatCount = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loop Limits", meta = (ClampMin = "0.1", ClampMax = "300.0", Units = "s"))
	float MaximumContinuousDurationSeconds = 30.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "One Shot", meta = (ClampMin = "0.001", ClampMax = "1.0", Units = "s"))
	float MinimumOneShotDurationSeconds = 0.001f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "One Shot", meta = (ClampMin = "0.001", ClampMax = "30.0", Units = "s"))
	float MaximumOneShotDurationSeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rate Limits", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float DefaultMinimumIntervalSeconds = 0.02f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rate Limits", meta = (ClampMin = "1", ClampMax = "100"))
	int32 MaximumSubmissionsPerSecond = 30;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rate Limits", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float SelectionDebounceSeconds = 0.04f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Platform", meta = (ToolTip = "Enables portable custom pattern playback. Platform packaging options must agree with this setting."))
	bool bEnableCustomPlayback = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Platform")
	FOpenMobileHapticAndroidSettings Android;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Platform")
	FOpenMobileHapticIOSSettings IOS;

	bool Validate(TArray<FString>& OutErrors) const;
};
