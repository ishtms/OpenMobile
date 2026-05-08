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
};
