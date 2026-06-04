#pragma once

#include "OpenMobileDeviceDisplayTypes.h"

struct FOpenMobileDeviceHdrEvidence
{
	TOptional<bool> bHdrAvailable;
	bool bSupportedHdrTypesAvailable = false;
	TArray<EOpenMobileHdrType> SupportedHdrTypes;
	TOptional<bool> bWideColorAvailable;
	TOptional<bool> bHdrOutputActive;
};

class FOpenMobileDeviceHdrInfo final
{
public:
	static EOpenMobileHdrType FromAndroidType(int32 NativeType);

	static void Apply(
		FOpenMobileWindowDisplaySnapshot& Snapshot,
		const FOpenMobileDeviceHdrEvidence& Evidence
	);
};
