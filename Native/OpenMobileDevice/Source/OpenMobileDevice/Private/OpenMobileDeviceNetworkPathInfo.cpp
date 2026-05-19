#include "OpenMobileDeviceNetworkPathInfo.h"

namespace OpenMobileDeviceNetworkPathInfoPrivate
{
	EOpenMobileNetworkTransport NormalizeAndroidTransport(int32 NativeType)
	{
		switch (NativeType)
		{
		case 0:
			return EOpenMobileNetworkTransport::Cellular;
		case 1:
			return EOpenMobileNetworkTransport::Wifi;
		case 2:
			return EOpenMobileNetworkTransport::Bluetooth;
		case 3:
			return EOpenMobileNetworkTransport::Ethernet;
		case 4:
			return EOpenMobileNetworkTransport::VPN;
		case -1:
			return EOpenMobileNetworkTransport::Unknown;
		default:
			return EOpenMobileNetworkTransport::Other;
		}
	}

	TArray<EOpenMobileNetworkTransport> NormalizeAndroidTransports(
		const TArray<int32>& NativeTypes
	)
	{
		TSet<EOpenMobileNetworkTransport> Present;
		for (int32 NativeType : NativeTypes)
		{
			Present.Add(NormalizeAndroidTransport(NativeType));
		}
		const EOpenMobileNetworkTransport StableOrder[] = {
			EOpenMobileNetworkTransport::VPN,
			EOpenMobileNetworkTransport::Wifi,
			EOpenMobileNetworkTransport::Cellular,
			EOpenMobileNetworkTransport::Ethernet,
			EOpenMobileNetworkTransport::Bluetooth,
			EOpenMobileNetworkTransport::Other,
			EOpenMobileNetworkTransport::Unknown
		};
		TArray<EOpenMobileNetworkTransport> Result;
		for (EOpenMobileNetworkTransport Transport : StableOrder)
		{
			if (Present.Contains(Transport))
			{
				Result.Add(Transport);
			}
		}
		return Result;
	}
}

FOpenMobileNetworkPathSnapshot FOpenMobileDeviceNetworkPathInfo::BuildAndroid(
	const FOpenMobileDeviceAndroidNetworkPathTraits& Traits
)
{
	FOpenMobileNetworkPathSnapshot Snapshot;
	const TArray<EOpenMobileNetworkTransport> Transports =
		OpenMobileDeviceNetworkPathInfoPrivate::NormalizeAndroidTransports(
			Traits.NativeTransportTypes
		);
	ApplyTransports(
		Snapshot,
		Traits.bTransportsAvailable,
		Transports,
		Transports.IsEmpty()
			? TOptional<EOpenMobileNetworkTransport>()
			: TOptional<EOpenMobileNetworkTransport>(Transports[0])
	);
	ApplyPolicyHints(
		Snapshot,
		Traits.bMeteredStateAvailable
			? TOptional<bool>(Traits.bIsMetered)
			: TOptional<bool>(),
		TOptional<bool>(),
		Traits.bConstrainedStateAvailable
			? TOptional<bool>(Traits.bIsConstrained)
			: TOptional<bool>()
	);
	if (!Traits.bQuerySucceeded)
	{
		return Snapshot;
	}
	if (Traits.bCaptivePortalSupported)
	{
		Snapshot.bIsCaptivePortal =
			FOpenMobileDeviceOptionalBool::MakeAvailable(
				Traits.bHasActiveNetwork
				&& Traits.bCapabilitiesAvailable
				&& Traits.bCaptivePortal
			);
	}
	if (!Traits.bHasActiveNetwork)
	{
		Snapshot.PathState = EOpenMobileNetworkPathState::Unavailable;
		Snapshot.ValidationSource =
			EOpenMobileNetworkValidationSource::PlatformPath;
		return Snapshot;
	}
	if (!Traits.bCapabilitiesAvailable)
	{
		Snapshot.PathState = EOpenMobileNetworkPathState::Available;
		Snapshot.ValidationSource =
			EOpenMobileNetworkValidationSource::PlatformPath;
		return Snapshot;
	}
	if (Traits.bCaptivePortal)
	{
		Snapshot.PathState = EOpenMobileNetworkPathState::CaptivePortal;
		Snapshot.ValidationSource =
			EOpenMobileNetworkValidationSource::OsValidatedPath;
		return Snapshot;
	}
	if (Traits.bInternetDeclared
		&& Traits.bInternetValidated
		&& !Traits.bRestricted)
	{
		Snapshot.PathState = EOpenMobileNetworkPathState::InternetCapable;
		Snapshot.ValidationSource =
			EOpenMobileNetworkValidationSource::OsValidatedPath;
		return Snapshot;
	}
	Snapshot.PathState = Traits.bLocalNetwork || !Traits.bInternetDeclared
		? EOpenMobileNetworkPathState::LocalOnly
		: EOpenMobileNetworkPathState::Available;
	Snapshot.ValidationSource =
		EOpenMobileNetworkValidationSource::DeclaredCapability;
	return Snapshot;
}

void FOpenMobileDeviceNetworkPathInfo::ApplyTransports(
	FOpenMobileNetworkPathSnapshot& Snapshot,
	bool bTransportsAvailable,
	const TArray<EOpenMobileNetworkTransport>& Transports,
	const TOptional<EOpenMobileNetworkTransport>& DefaultTransport
)
{
	Snapshot.bTransportsAvailable = bTransportsAvailable;
	Snapshot.Transports.Reset();
	Snapshot.bDefaultTransportAvailable = false;
	Snapshot.DefaultTransport = EOpenMobileNetworkTransport::Unknown;
	if (!bTransportsAvailable)
	{
		return;
	}
	for (EOpenMobileNetworkTransport Transport : Transports)
	{
		Snapshot.Transports.AddUnique(Transport);
	}
	if (DefaultTransport.IsSet())
	{
		Snapshot.bDefaultTransportAvailable = true;
		Snapshot.DefaultTransport = DefaultTransport.GetValue();
	}
}

void FOpenMobileDeviceNetworkPathInfo::ApplyPolicyHints(
	FOpenMobileNetworkPathSnapshot& Snapshot,
	const TOptional<bool>& bIsMetered,
	const TOptional<bool>& bIsExpensive,
	const TOptional<bool>& bIsConstrained
)
{
	Snapshot.bIsMetered = bIsMetered.IsSet()
		? FOpenMobileDeviceOptionalBool::MakeAvailable(bIsMetered.GetValue())
		: FOpenMobileDeviceOptionalBool();
	Snapshot.bIsExpensive = bIsExpensive.IsSet()
		? FOpenMobileDeviceOptionalBool::MakeAvailable(bIsExpensive.GetValue())
		: FOpenMobileDeviceOptionalBool();
	Snapshot.bIsConstrained = bIsConstrained.IsSet()
		? FOpenMobileDeviceOptionalBool::MakeAvailable(
			bIsConstrained.GetValue()
		)
		: FOpenMobileDeviceOptionalBool();
}

FOpenMobileNetworkPathSnapshot FOpenMobileDeviceNetworkPathInfo::BuildIOS(
	EOpenMobileDeviceIOSPathStatus Status
)
{
	FOpenMobileNetworkPathSnapshot Snapshot;
	switch (Status)
	{
	case EOpenMobileDeviceIOSPathStatus::Unsatisfied:
		Snapshot.PathState = EOpenMobileNetworkPathState::Unavailable;
		Snapshot.ValidationSource =
			EOpenMobileNetworkValidationSource::PlatformPath;
		break;
	case EOpenMobileDeviceIOSPathStatus::Satisfiable:
	case EOpenMobileDeviceIOSPathStatus::Satisfied:
		Snapshot.PathState = EOpenMobileNetworkPathState::Available;
		Snapshot.ValidationSource =
			EOpenMobileNetworkValidationSource::PlatformPath;
		break;
	case EOpenMobileDeviceIOSPathStatus::Invalid:
	default:
		break;
	}
	return Snapshot;
}
