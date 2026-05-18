#include "OpenMobileDeviceNetworkPathInfo.h"

FOpenMobileNetworkPathSnapshot FOpenMobileDeviceNetworkPathInfo::BuildAndroid(
	const FOpenMobileDeviceAndroidNetworkPathTraits& Traits
)
{
	FOpenMobileNetworkPathSnapshot Snapshot;
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
