#include "OpenMobileHapticsIOSBackend.h"

#include "Misc/ScopeLock.h"

#import <CoreHaptics/CoreHaptics.h>
#import <TargetConditionals.h>

FOpenMobileHapticCapabilities
FOpenMobileHapticsIOSBackend::ProbeHardwareCapabilities() const
{
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.BackendName = GetBackendName();
#if TARGET_OS_SIMULATOR
	Capabilities.Availability = EOpenMobileHapticAvailability::NoActuator;
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.SemanticFeedback = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.RichHaptics = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Detail = TEXT("The iOS Simulator has no phone haptic actuator.");
#else
	@autoreleasepool
	{
		if (@available(iOS 13.0, *))
		{
			id<CHHapticDeviceCapability> Hardware =
				[CHHapticEngine capabilitiesForHardware];
			if (!Hardware)
			{
				Capabilities.Availability =
					EOpenMobileHapticAvailability::TemporarilyUnavailable;
				Capabilities.Detail =
					TEXT("Apple haptic capabilities are temporarily unavailable.");
				return Capabilities;
			}
			if (Hardware.supportsHaptics)
			{
				Capabilities.Availability =
					EOpenMobileHapticAvailability::RichHaptics;
				Capabilities.BasicVibration =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.SemanticFeedback =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.RichHaptics =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.Detail =
					TEXT("Apple reports Core Haptics hardware support.");
			}
			else
			{
				Capabilities.Availability =
					EOpenMobileHapticAvailability::NoActuator;
				Capabilities.BasicVibration =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.SemanticFeedback =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.RichHaptics =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.Detail =
					TEXT("Apple reports no Core Haptics actuator.");
			}
		}
		else
		{
			Capabilities.Availability =
				EOpenMobileHapticAvailability::UnsupportedPlatform;
			Capabilities.Detail =
				TEXT("Core Haptics requires iOS or iPadOS 13 or newer.");
		}
	}
#endif
	return Capabilities;
}

FOpenMobileHapticCapabilities FOpenMobileHapticsIOSBackend::GetCapabilities() const
{
	FScopeLock Lock(&CacheMutex);
	if (StableCapabilities.IsSet())
	{
		return StableCapabilities.GetValue();
	}
	FOpenMobileHapticCapabilities Capabilities = ProbeHardwareCapabilities();
	if (Capabilities.Availability
		!= EOpenMobileHapticAvailability::TemporarilyUnavailable)
	{
		StableCapabilities = Capabilities;
	}
	return Capabilities;
}
