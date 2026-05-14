#pragma once

#include "Misc/Build.h"
#include "OpenMobileDeviceIdentityTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceApplicationInfo final
{
public:
	static FOpenMobileApplicationMetadataSnapshot Build(
		const FString& DisplayName,
		const FString& PackageIdentifier,
		const FString& VersionName,
		const FString& BuildNumber,
		EBuildConfiguration BuildConfiguration
	);
};
