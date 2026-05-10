#pragma once

#include "OpenMobileDeviceIdentityTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceArchitecture final
{
public:
	static void Apply(
		FOpenMobileDeviceInformationSnapshot& Snapshot,
		const FString& ProcessArchitecture,
		TConstArrayView<FString> SupportedAbis,
		bool bSupportedAbisAvailable
	);
};
