#pragma once

#include "OpenMobileDeviceDisplayTypes.h"

struct FOpenMobileDeviceRefreshModeEvidence
{
	FIntPoint PixelSize = FIntPoint::ZeroValue;
	TArray<float> RefreshRatesHz;
};

struct FOpenMobileDeviceRefreshRateEvidence
{
	TOptional<float> CurrentRefreshRateHz;
	TOptional<float> MaximumRefreshRateHz;
	TOptional<bool> bVariableRefreshRateSupported;
	bool bSupportedModesAvailable = false;
	TArray<FOpenMobileDeviceRefreshModeEvidence> SupportedModes;
};

class FOpenMobileDeviceRefreshRateInfo final
{
public:
	static void Apply(
		FOpenMobileWindowDisplaySnapshot& Snapshot,
		const FOpenMobileDeviceRefreshRateEvidence& Evidence
	);
};
