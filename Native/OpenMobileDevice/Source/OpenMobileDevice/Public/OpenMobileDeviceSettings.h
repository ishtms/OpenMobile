#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileDeviceSettings.generated.h"

UCLASS(
	Config = Game,
	DefaultConfig,
	meta = (DisplayName = "OpenMobile Device")
)
class OPENMOBILEDEVICE_API UOpenMobileDeviceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("OpenMobile"); }
	virtual FName GetSectionName() const override { return TEXT("OpenMobile Device"); }

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Storage",
		meta = (
			DisplayName = "Use Platform Low-Storage Threshold",
			ToolTip = "Uses the platform-specific low-storage threshold instead of the custom byte threshold."
		)
	)
	bool bUsePlatformDefaultLowStorageThreshold = true;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Storage",
		meta = (
			ClampMin = "0",
			UIMin = "0",
			Units = "B",
			EditCondition = "!bUsePlatformDefaultLowStorageThreshold",
			DisplayName = "Custom Low-Storage Threshold",
			ToolTip = "Available bytes at or below this value enter low-storage state."
		)
	)
	int64 LowStorageThresholdBytes = 512ll * 1024 * 1024;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Storage",
		meta = (
			ClampMin = "0",
			UIMin = "0",
			Units = "B",
			DisplayName = "Low-Storage Recovery Hysteresis",
			ToolTip = "Additional available bytes required before leaving low-storage state."
		)
	)
	int64 LowStorageRecoveryHysteresisBytes = 64ll * 1024 * 1024;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Storage",
		meta = (
			ClampMin = "5.0",
			ClampMax = "60.0",
			UIMin = "5.0",
			UIMax = "60.0",
			Units = "s",
			DisplayName = "Low-Storage Fallback Polling Interval",
			ToolTip = "Minimum demand-driven polling interval when native notifications cannot cover the selected threshold."
		)
	)
	float LowStorageFallbackPollingIntervalSeconds = 30.0f;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Monitoring",
		meta = (
			ClampMin = "0.1",
			ClampMax = "60.0",
			UIMin = "0.1",
			UIMax = "60.0",
			Units = "s",
			DisplayName = "Default Fallback Polling Interval",
			ToolTip = "Polling interval used when a monitoring request leaves its advanced interval pin at zero and the active backend cannot provide native change notifications."
		)
	)
	float FallbackPollingIntervalSeconds = 1.0f;

	float GetValidatedFallbackPollingIntervalSeconds() const
	{
		if (!FMath::IsFinite(FallbackPollingIntervalSeconds))
		{
			return GetDefaultFallbackPollingIntervalSeconds();
		}
		return FMath::Clamp(
			FallbackPollingIntervalSeconds,
			GetMinimumFallbackPollingIntervalSeconds(),
			GetMaximumFallbackPollingIntervalSeconds()
		);
	}

	static constexpr float GetMinimumFallbackPollingIntervalSeconds()
	{
		return 0.1f;
	}

	static constexpr float GetMaximumFallbackPollingIntervalSeconds()
	{
		return 60.0f;
	}

	static constexpr float GetDefaultFallbackPollingIntervalSeconds()
	{
		return 1.0f;
	}

	int64 ResolveLowStorageThresholdBytes(
		int64 PlatformDefaultThresholdBytes
	) const
	{
		return FMath::Max<int64>(
			bUsePlatformDefaultLowStorageThreshold
				? PlatformDefaultThresholdBytes
				: LowStorageThresholdBytes,
			0
		);
	}

	int64 GetValidatedLowStorageRecoveryHysteresisBytes() const
	{
		return FMath::Max<int64>(LowStorageRecoveryHysteresisBytes, 0);
	}

	float GetValidatedLowStorageFallbackPollingIntervalSeconds() const
	{
		if (!FMath::IsFinite(LowStorageFallbackPollingIntervalSeconds))
		{
			return GetDefaultLowStorageFallbackPollingIntervalSeconds();
		}
		return FMath::Clamp(
			LowStorageFallbackPollingIntervalSeconds,
			GetMinimumLowStorageFallbackPollingIntervalSeconds(),
			GetMaximumLowStorageFallbackPollingIntervalSeconds()
		);
	}

	static constexpr float GetMinimumLowStorageFallbackPollingIntervalSeconds()
	{
		return 5.0f;
	}

	static constexpr float GetMaximumLowStorageFallbackPollingIntervalSeconds()
	{
		return 60.0f;
	}

	static constexpr float GetDefaultLowStorageFallbackPollingIntervalSeconds()
	{
		return 30.0f;
	}
};
