#include "OpenMobileDeviceIOSNetwork.h"

#include "OpenMobileDeviceNetworkPathInfo.h"

#import <SystemConfiguration/SystemConfiguration.h>

#include <netinet/in.h>

FOpenMobileNetworkPathSnapshot GetOpenMobileDeviceIOSNetworkPathSnapshot()
{
	sockaddr_in RouteAddress = {};
	RouteAddress.sin_len = sizeof(RouteAddress);
	RouteAddress.sin_family = AF_INET;
	SCNetworkReachabilityRef Reachability =
		SCNetworkReachabilityCreateWithAddress(
			nullptr,
			reinterpret_cast<const sockaddr*>(&RouteAddress)
		);
	if (!Reachability)
	{
		return FOpenMobileDeviceNetworkPathInfo::BuildIOS(
			EOpenMobileDeviceIOSPathStatus::Invalid
		);
	}
	SCNetworkReachabilityFlags Flags = 0;
	const bool bReadSucceeded = SCNetworkReachabilityGetFlags(
		Reachability,
		&Flags
	);
	CFRelease(Reachability);
	if (!bReadSucceeded)
	{
		return FOpenMobileDeviceNetworkPathInfo::BuildIOS(
			EOpenMobileDeviceIOSPathStatus::Invalid
		);
	}
	if ((Flags & kSCNetworkReachabilityFlagsReachable) == 0)
	{
		return FOpenMobileDeviceNetworkPathInfo::BuildIOS(
			EOpenMobileDeviceIOSPathStatus::Unsatisfied
		);
	}
	const bool bConnectionRequired =
		(Flags & kSCNetworkReachabilityFlagsConnectionRequired) != 0;
	return FOpenMobileDeviceNetworkPathInfo::BuildIOS(
		bConnectionRequired
			? EOpenMobileDeviceIOSPathStatus::Satisfiable
			: EOpenMobileDeviceIOSPathStatus::Satisfied
	);
}
