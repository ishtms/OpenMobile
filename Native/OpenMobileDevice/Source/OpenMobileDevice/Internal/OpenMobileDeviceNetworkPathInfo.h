#pragma once

#include "OpenMobileDeviceNetworkTypes.h"

struct OPENMOBILEDEVICE_API FOpenMobileDeviceAndroidNetworkPathTraits
{
	bool bQuerySucceeded = false;
	bool bHasActiveNetwork = false;
	bool bCapabilitiesAvailable = false;
	bool bInternetDeclared = false;
	bool bInternetValidated = false;
	bool bCaptivePortalSupported = false;
	bool bCaptivePortal = false;
	bool bLocalNetwork = false;
	bool bRestricted = false;
};

enum class EOpenMobileDeviceIOSPathStatus : uint8
{
	Invalid,
	Unsatisfied,
	Satisfiable,
	Satisfied
};

class OPENMOBILEDEVICE_API FOpenMobileDeviceNetworkPathInfo final
{
public:
	static FOpenMobileNetworkPathSnapshot BuildAndroid(
		const FOpenMobileDeviceAndroidNetworkPathTraits& Traits
	);
	static FOpenMobileNetworkPathSnapshot BuildIOS(
		EOpenMobileDeviceIOSPathStatus Status
	);
};
