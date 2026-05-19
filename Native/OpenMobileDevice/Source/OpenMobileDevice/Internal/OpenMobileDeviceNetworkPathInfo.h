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
	bool bTransportsAvailable = false;
	TArray<int32> NativeTransportTypes;
	bool bMeteredStateAvailable = false;
	bool bIsMetered = false;
	bool bConstrainedStateAvailable = false;
	bool bIsConstrained = false;
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
	static void ApplyTransports(
		FOpenMobileNetworkPathSnapshot& Snapshot,
		bool bTransportsAvailable,
		const TArray<EOpenMobileNetworkTransport>& Transports,
		const TOptional<EOpenMobileNetworkTransport>& DefaultTransport
	);
	static void ApplyPolicyHints(
		FOpenMobileNetworkPathSnapshot& Snapshot,
		const TOptional<bool>& bIsMetered,
		const TOptional<bool>& bIsExpensive,
		const TOptional<bool>& bIsConstrained
	);
};
