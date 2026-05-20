#pragma once

#include "OpenMobileDeviceDisplayTypes.h"

struct FOpenMobileDeviceWindowMetricsEvidence
{
	TOptional<FVector2D> LogicalWindowSize;
	TOptional<FIntPoint> DrawablePixelSize;
	TOptional<float> ScaleFactor;
	TOptional<float> DensityDpi;
	TOptional<FString> ScreenIdentifier;
	TOptional<bool> bIsWindowed;
};

class FOpenMobileDeviceWindowMetrics final
{
public:
	static FOpenMobileWindowDisplaySnapshot Build(
		const FOpenMobileDeviceWindowMetricsEvidence& Evidence
	);
};
