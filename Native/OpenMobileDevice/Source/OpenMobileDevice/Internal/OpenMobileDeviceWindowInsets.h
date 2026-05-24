#pragma once

#include "OpenMobileDeviceDisplayTypes.h"

struct FOpenMobileDeviceInsetValues
{
	float Left = 0.0f;
	float Top = 0.0f;
	float Right = 0.0f;
	float Bottom = 0.0f;
};

struct FOpenMobileDeviceWindowInsetsEvidence
{
	TOptional<FOpenMobileDeviceInsetValues> SafeArea;
	TOptional<FOpenMobileDeviceInsetValues> SystemBars;
	TOptional<FOpenMobileDeviceInsetValues> HomeIndicator;
	TOptional<FOpenMobileDeviceInsetValues> SystemGestures;
};

class FOpenMobileDeviceWindowInsets final
{
public:
	static void Apply(
		FOpenMobileWindowDisplaySnapshot& Snapshot,
		const FOpenMobileDeviceWindowInsetsEvidence& Evidence
	);
};
