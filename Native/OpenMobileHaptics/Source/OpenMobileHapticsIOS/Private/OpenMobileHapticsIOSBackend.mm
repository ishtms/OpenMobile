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
	Capabilities.AmplitudeControl = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.WaveformTiming = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Looping = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Primitives = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Envelopes = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.FrequencyControl = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.TransientEvents = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.ContinuousEvents = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.DynamicParameters = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.AudioEvents = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.AHAP = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Scheduling = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Pause = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Resume = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Seek = EOpenMobileHapticSupportState::Unsupported;
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
				Capabilities.AmplitudeControl =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.SemanticEffects =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.PredefinedEffects =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.WaveformTiming =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.Looping =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.Primitives =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.Envelopes =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.FrequencyControl =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.TransientEvents =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.ContinuousEvents =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.DynamicParameters =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.AudioEvents = Hardware.supportsAudio
					? EOpenMobileHapticSupportState::Supported
					: EOpenMobileHapticSupportState::Unsupported;
				Capabilities.AHAP = EOpenMobileHapticSupportState::Supported;
				Capabilities.Scheduling =
					EOpenMobileHapticSupportState::Supported;
				Capabilities.Pause = EOpenMobileHapticSupportState::Supported;
				Capabilities.Resume = EOpenMobileHapticSupportState::Supported;
				Capabilities.Seek = EOpenMobileHapticSupportState::Supported;
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
				Capabilities.AmplitudeControl =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.SemanticEffects =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.PredefinedEffects =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.WaveformTiming =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.Looping =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.Primitives =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.Envelopes =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.FrequencyControl =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.TransientEvents =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.ContinuousEvents =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.DynamicParameters =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.AudioEvents =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.AHAP = EOpenMobileHapticSupportState::Unsupported;
				Capabilities.Scheduling =
					EOpenMobileHapticSupportState::Unsupported;
				Capabilities.Pause = EOpenMobileHapticSupportState::Unsupported;
				Capabilities.Resume = EOpenMobileHapticSupportState::Unsupported;
				Capabilities.Seek = EOpenMobileHapticSupportState::Unsupported;
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
