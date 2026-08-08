#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorsSettings.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorsDevelopmentInputMode : uint8
{
	Disabled UMETA(DisplayName = "Disabled", ToolTip = "Uses native sensor providers and disables development input substitution."),
	Mock UMETA(DisplayName = "Mock Input", ToolTip = "Uses deterministic development sensor values instead of native device input."),
	Replay UMETA(DisplayName = "Recording Replay", ToolTip = "Uses a recorded sensor stream as development input.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorPowerIntent : uint8
{
	LowPower UMETA(DisplayName = "Low Power", ToolTip = "Prefers lower sampling and callback rates to reduce sensor and CPU use."),
	Balanced UMETA(DisplayName = "Balanced", ToolTip = "Balances responsiveness with sensor, CPU, and battery cost."),
	Performance UMETA(DisplayName = "Performance", ToolTip = "Prefers responsiveness and higher rates when the platform supports them.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorRatePresetSettings
{
	GENERATED_BODY()

	FOpenMobileSensorRatePresetSettings() = default;

	FOpenMobileSensorRatePresetSettings(
		double InRequestedFrequencyHz,
		double InMaximumDeliveryLatencySeconds,
		double InMaximumCallbackFrequencyHz,
		EOpenMobileSensorPowerIntent InPowerIntent
	)
		: RequestedFrequencyHz(InRequestedFrequencyHz)
		, MaximumDeliveryLatencySeconds(InMaximumDeliveryLatencySeconds)
		, MaximumCallbackFrequencyHz(InMaximumCallbackFrequencyHz)
		, PowerIntent(InPowerIntent)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Preset", meta = (ToolTip = "Physical sampling frequency in hertz requested by this preset before project and platform clamping.", ClampMin = "1.0", ClampMax = "1000.0", Units = "Hz"))
	double RequestedFrequencyHz = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Preset", meta = (ToolTip = "Maximum batching latency in seconds requested by this preset. Zero requests immediate delivery.", ClampMin = "0.0", ClampMax = "10.0", Units = "s"))
	double MaximumDeliveryLatencySeconds = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Preset", meta = (ToolTip = "Maximum game-thread event callback frequency in hertz. This does not reduce physical sampling.", ClampMin = "1.0", ClampMax = "120.0", Units = "Hz"))
	double MaximumCallbackFrequencyHz = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Preset", meta = (ToolTip = "Communicates the expected power and responsiveness tradeoff at the point this preset is chosen."))
	EOpenMobileSensorPowerIntent PowerIntent =
		EOpenMobileSensorPowerIntent::LowPower;
};

UCLASS(
	Config = Engine,
	DefaultConfig,
	meta = (DisplayName = "OpenMobile Sensors")
)
class OPENMOBILESENSORS_API UOpenMobileSensorsSettings final
	: public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("OpenMobile");
	}

	virtual FName GetSectionName() const override
	{
		return TEXT("OpenMobile Sensors");
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Sampling Presets", meta = (ToolTip = "Low-power preset used by UI and Interface listener choices.", DisplayName = "UI Preset"))
	FOpenMobileSensorRatePresetSettings UIPreset;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Sampling Presets", meta = (ToolTip = "Balanced preset used by Game and Gameplay listener choices.", DisplayName = "Game Preset"))
	FOpenMobileSensorRatePresetSettings GamePreset =
		FOpenMobileSensorRatePresetSettings(
			60.0,
			0.02,
			30.0,
			EOpenMobileSensorPowerIntent::Balanced
		);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Sampling Presets", meta = (ToolTip = "Performance preset used by Fast and High Responsiveness listener choices.", DisplayName = "Fast Preset"))
	FOpenMobileSensorRatePresetSettings FastPreset =
		FOpenMobileSensorRatePresetSettings(
			200.0,
			0.0,
			60.0,
			EOpenMobileSensorPowerIntent::Performance
		);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Streaming", meta = (DisplayName = "Default Stream Options", ToolTip = "Safe defaults copied into new sensor requests before caller overrides."))
	FOpenMobileSensorStreamOptions DefaultStreamOptions;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Streaming", meta = (DisplayName = "Allow High Sampling Rate", ToolTip = "Allows explicit high-rate requests after platform and project validation."))
	bool bAllowHighSamplingRate = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Lifecycle", meta = (DisplayName = "Allow Background Sensor Delivery", ToolTip = "Allows operations to request background continuation only where the platform supports it."))
	bool bAllowBackgroundSensorDelivery = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Physical Orientation", meta = (ClampMin = "5.0", ClampMax = "40.0", Units = "deg", DisplayName = "Face Up Angle", ToolTip = "Maximum angle from display-normal gravity used to classify Face Up or Face Down."))
	double PhysicalOrientationFaceAngleDegrees = 25.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Physical Orientation", meta = (ClampMin = "0.0", ClampMax = "15.0", Units = "deg", DisplayName = "Orientation Hysteresis", ToolTip = "Creates an unknown band around edge-orientation boundaries to prevent chatter."))
	double PhysicalOrientationHysteresisDegrees = 5.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Physical Orientation", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s", DisplayName = "Transition Debounce", ToolTip = "Requires a candidate physical orientation to remain stable before committing a transition."))
	double PhysicalOrientationTransitionDebounceSeconds = 0.15;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Recording", meta = (ToolTip = "Project-wide safety ceiling in seconds. Each recording request is clamped to this duration.", ClampMin = "1.0", ClampMax = "86400.0", Units = "s", DisplayName = "Maximum Recording Duration"))
	double MaximumRecordingDurationSeconds = 300.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Recording", meta = (ClampMin = "1048576", ClampMax = "268435456", Units = "B", DisplayName = "Maximum Recording Size", ToolTip = "Maximum complete recording size. The current recording and replay format supports files up to 256 MiB."))
	int64 MaximumRecordingBytes = 64ll * 1024 * 1024;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Recording", meta = (ToolTip = "Maximum number of accepted sample batches queued for the recording writer before overflow is reported.", ClampMin = "1", ClampMax = "4096", DisplayName = "Maximum Buffered Recording Batches"))
	int32 MaximumRecordingBufferedBatches = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Development", meta = (DisplayName = "Allow Sensitive Location in Recordings", ToolTip = "Allows an explicit non-Shipping recording request to store caller-supplied location context."))
	bool bAllowSensitiveLocationContextInDevelopmentRecordings = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Permissions", meta = (DisplayName = "Enable Permission-Sensitive Sensors", ToolTip = "Enables motion activity and other sensor paths that require runtime permission."))
	bool bEnablePermissionSensitiveSensors = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Permissions", meta = (ToolTip = "User-facing reason for iOS motion access. The exact same text must be present as NSMotionUsageDescription in Additional Plist Data for modern Xcode packaging.", MultiLine = "true", DisplayName = "iOS Motion Usage Description"))
	FString IOSMotionUsageDescription =
		TEXT("This app uses motion sensors for gameplay features.");

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Permissions", meta = (ToolTip = "Optional user-facing explanation shown before requesting Android activity-recognition permission.", MultiLine = "true", DisplayName = "Android Activity Recognition Rationale"))
	FString AndroidActivityRecognitionRationale;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Development", meta = (DisplayName = "Development Input Mode", ToolTip = "Selects explicit mock or replay input for development builds."))
	EOpenMobileSensorsDevelopmentInputMode DevelopmentInputMode =
		EOpenMobileSensorsDevelopmentInputMode::Disabled;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Settings|Development", meta = (ToolTip = "Recording file loaded as sensor input when Development Input Mode is Recording Replay. Ignored in Shipping builds.", DisplayName = "Development Replay File", EditCondition = "DevelopmentInputMode == EOpenMobileSensorsDevelopmentInputMode::Replay"))
	FString DevelopmentReplayFile;

	bool Validate(
		TArray<FString>& OutErrors,
		bool bShipping = UE_BUILD_SHIPPING
	) const;
};
