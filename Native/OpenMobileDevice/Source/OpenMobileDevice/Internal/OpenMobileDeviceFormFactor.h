#pragma once

#include "OpenMobileDeviceIdentityTypes.h"

enum class EOpenMobileDeviceWindowSizeClass : uint8
{
	Unknown,
	Compact,
	Large
};

struct FOpenMobileDeviceFormFactorTraits
{
	bool bPhoneIdiom = false;
	bool bTabletIdiom = false;
	bool bSimulator = false;
	bool bHasFoldableHardware = false;
	bool bSeparatingPosture = false;
	int32 SmallestWindowWidthDp = 0;
	EOpenMobileDeviceWindowSizeClass WindowSizeClass =
		EOpenMobileDeviceWindowSizeClass::Unknown;
};

class OPENMOBILEDEVICE_API FOpenMobileDeviceFormFactor final
{
public:
	static EOpenMobileDeviceFormFactor Classify(
		const FOpenMobileDeviceFormFactorTraits& Traits
	);
};
