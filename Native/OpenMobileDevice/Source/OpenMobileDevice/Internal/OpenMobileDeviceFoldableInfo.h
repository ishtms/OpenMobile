#pragma once

#include "OpenMobileDeviceDisplayTypes.h"

enum class EOpenMobileDeviceNativeFoldState : uint8
{
	Unknown,
	Flat,
	HalfOpened
};

enum class EOpenMobileDeviceNativeFoldOrientation : uint8
{
	Unknown,
	Horizontal,
	Vertical
};

struct FOpenMobileDeviceFoldableEvidence
{
	bool bFeatureAvailable = false;
	EOpenMobileDeviceNativeFoldState State =
		EOpenMobileDeviceNativeFoldState::Unknown;
	EOpenMobileDeviceNativeFoldOrientation Orientation =
		EOpenMobileDeviceNativeFoldOrientation::Unknown;
	TOptional<FOpenMobileDeviceRect> NativeBounds;
	float NativeUnitsPerLogicalUnit = 0.0f;
	TOptional<bool> bSeparating;
};

class FOpenMobileDeviceFoldableInfo final
{
public:
	static void Apply(
		FOpenMobileWindowDisplaySnapshot& Snapshot,
		const FOpenMobileDeviceFoldableEvidence& Evidence
	);
};
