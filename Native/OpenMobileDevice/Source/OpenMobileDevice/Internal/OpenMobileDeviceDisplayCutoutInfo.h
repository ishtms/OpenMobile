#pragma once

#include "OpenMobileDeviceWindowInsets.h"

struct FOpenMobileDeviceDisplayCutoutEvidence
{
	bool bCutoutsAvailable = false;
	TOptional<FVector2D> NativeWindowOrigin;
	TOptional<float> NativeUnitsPerLogicalUnit;
	TArray<FOpenMobileDeviceRect> NativeCutouts;
	TOptional<FOpenMobileDeviceInsetValues> NativeWaterfallInsets;
};

class FOpenMobileDeviceDisplayCutoutInfo final
{
public:
	static void Apply(
		FOpenMobileWindowDisplaySnapshot& Snapshot,
		const FOpenMobileDeviceDisplayCutoutEvidence& Evidence
	);
};
