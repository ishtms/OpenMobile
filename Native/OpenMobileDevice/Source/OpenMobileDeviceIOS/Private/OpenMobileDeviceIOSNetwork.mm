#include "OpenMobileDeviceIOSNetwork.h"

#include "HAL/PlatformMisc.h"
#include "OpenMobileDeviceNetworkPathInfo.h"

#import <SystemConfiguration/SystemConfiguration.h>

#include <netinet/in.h>

namespace OpenMobileDeviceIOSNetworkPrivate
{
	void ApplyTransport(FOpenMobileNetworkPathSnapshot& Snapshot)
	{
		EOpenMobileNetworkTransport Transport =
			EOpenMobileNetworkTransport::Unknown;
		bool bHasTransport = true;
		switch (FPlatformMisc::GetNetworkConnectionType())
		{
		case ENetworkConnectionType::Cell:
			Transport = EOpenMobileNetworkTransport::Cellular;
			break;
		case ENetworkConnectionType::WiFi:
			Transport = EOpenMobileNetworkTransport::Wifi;
			break;
		case ENetworkConnectionType::Bluetooth:
			Transport = EOpenMobileNetworkTransport::Bluetooth;
			break;
		case ENetworkConnectionType::Ethernet:
			Transport = EOpenMobileNetworkTransport::Ethernet;
			break;
		case ENetworkConnectionType::WiMAX:
			Transport = EOpenMobileNetworkTransport::Other;
			break;
		case ENetworkConnectionType::None:
		case ENetworkConnectionType::AirplaneMode:
			bHasTransport = false;
			break;
		case ENetworkConnectionType::Unknown:
		default:
			break;
		}
		FOpenMobileDeviceNetworkPathInfo::ApplyTransports(
			Snapshot,
			true,
			bHasTransport
				? TArray<EOpenMobileNetworkTransport>({Transport})
				: TArray<EOpenMobileNetworkTransport>(),
			bHasTransport
				? TOptional<EOpenMobileNetworkTransport>(Transport)
				: TOptional<EOpenMobileNetworkTransport>()
		);
		if (Snapshot.PathState != EOpenMobileNetworkPathState::Unknown)
		{
			const FPlatformMisc::FNetworkConnectionCharacteristics Policy =
				FPlatformMisc::GetNetworkConnectionCharacteristics();
			FOpenMobileDeviceNetworkPathInfo::ApplyPolicyHints(
				Snapshot,
				TOptional<bool>(),
				TOptional<bool>(Policy.bIsExpensive),
				TOptional<bool>(Policy.bIsConstrained)
			);
		}
	}

	FOpenMobileNetworkPathSnapshot BuildSnapshot(
		EOpenMobileDeviceIOSPathStatus Status
	)
	{
		FOpenMobileNetworkPathSnapshot Snapshot =
			FOpenMobileDeviceNetworkPathInfo::BuildIOS(Status);
		ApplyTransport(Snapshot);
		return Snapshot;
	}
}

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
		return OpenMobileDeviceIOSNetworkPrivate::BuildSnapshot(
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
		return OpenMobileDeviceIOSNetworkPrivate::BuildSnapshot(
			EOpenMobileDeviceIOSPathStatus::Invalid
		);
	}
	if ((Flags & kSCNetworkReachabilityFlagsReachable) == 0)
	{
		return OpenMobileDeviceIOSNetworkPrivate::BuildSnapshot(
			EOpenMobileDeviceIOSPathStatus::Unsatisfied
		);
	}
	const bool bConnectionRequired =
		(Flags & kSCNetworkReachabilityFlagsConnectionRequired) != 0;
	return OpenMobileDeviceIOSNetworkPrivate::BuildSnapshot(
		bConnectionRequired
			? EOpenMobileDeviceIOSPathStatus::Satisfiable
			: EOpenMobileDeviceIOSPathStatus::Satisfied
	);
}
