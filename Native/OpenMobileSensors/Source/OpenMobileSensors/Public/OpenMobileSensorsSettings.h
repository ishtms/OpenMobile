#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorsSettings.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorsDevelopmentInputMode : uint8
{
	Disabled,
	Mock,
	Replay
};

UENUM(BlueprintType)
enum class EOpenMobileSensorPowerIntent : uint8
{
	LowPower,
	Balanced,
	Performance
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "1.0", ClampMax = "1000.0", Units = "Hz"))
	double RequestedFrequencyHz = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0.0", ClampMax = "10.0", Units = "s"))
	double MaximumDeliveryLatencySeconds = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "1.0", ClampMax = "120.0", Units = "Hz"))
	double MaximumCallbackFrequencyHz = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
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

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sampling Presets", meta = (DisplayName = "UI Preset"))
	FOpenMobileSensorRatePresetSettings UIPreset;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sampling Presets", meta = (DisplayName = "Game Preset"))
	FOpenMobileSensorRatePresetSettings GamePreset =
		FOpenMobileSensorRatePresetSettings(
			60.0,
			0.02,
			30.0,
			EOpenMobileSensorPowerIntent::Balanced
		);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sampling Presets", meta = (DisplayName = "Fast Preset"))
	FOpenMobileSensorRatePresetSettings FastPreset =
		FOpenMobileSensorRatePresetSettings(
			200.0,
			0.0,
			60.0,
			EOpenMobileSensorPowerIntent::Performance
		);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Streaming", meta = (DisplayName = "Default Stream Options", ToolTip = "Safe defaults copied into new sensor requests before caller overrides."))
	FOpenMobileSensorStreamOptions DefaultStreamOptions;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Streaming", meta = (DisplayName = "Allow High Sampling Rate", ToolTip = "Allows explicit high-rate requests after platform and project validation."))
	bool bAllowHighSamplingRate = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (DisplayName = "Allow Background Sensor Delivery", ToolTip = "Allows operations to request background continuation only where the platform supports it."))
	bool bAllowBackgroundSensorDelivery = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Recording", meta = (ClampMin = "1.0", ClampMax = "86400.0", Units = "s", DisplayName = "Maximum Recording Duration"))
	double MaximumRecordingDurationSeconds = 300.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Recording", meta = (ClampMin = "1048576", ClampMax = "4294967296", Units = "B", DisplayName = "Maximum Recording Size"))
	int64 MaximumRecordingBytes = 64ll * 1024 * 1024;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Recording", meta = (ClampMin = "1", ClampMax = "4096", DisplayName = "Maximum Buffered Recording Batches"))
	int32 MaximumRecordingBufferedBatches = 32;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Permissions", meta = (DisplayName = "Enable Permission-Sensitive Sensors", ToolTip = "Enables motion activity and other sensor paths that require runtime permission."))
	bool bEnablePermissionSensitiveSensors = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Permissions", meta = (MultiLine = "true", DisplayName = "iOS Motion Usage Description"))
	FString IOSMotionUsageDescription;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Permissions", meta = (MultiLine = "true", DisplayName = "Android Activity Recognition Rationale"))
	FString AndroidActivityRecognitionRationale;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Development", meta = (DisplayName = "Development Input Mode", ToolTip = "Selects explicit mock or replay input for development builds."))
	EOpenMobileSensorsDevelopmentInputMode DevelopmentInputMode =
		EOpenMobileSensorsDevelopmentInputMode::Disabled;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Development", meta = (DisplayName = "Development Replay File", EditCondition = "DevelopmentInputMode == EOpenMobileSensorsDevelopmentInputMode::Replay"))
	FString DevelopmentReplayFile;

	bool Validate(
		TArray<FString>& OutErrors,
		bool bShipping = UE_BUILD_SHIPPING
	) const;
};
