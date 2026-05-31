#include "OpenMobileHapticsIOSBackend.h"

#include "Misc/ScopeLock.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreHaptics/CoreHaptics.h>
#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

namespace OpenMobileHapticsIOSBackendPrivate
{
	void PlaySystemSemantic(
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity
	)
	{
		dispatch_async(dispatch_get_main_queue(), ^{
			switch (Behavior)
			{
			case EOpenMobileHapticsSemanticBehavior::Selection:
			{
				UISelectionFeedbackGenerator* Generator =
					[[UISelectionFeedbackGenerator alloc] init];
				[Generator prepare];
				[Generator selectionChanged];
				[Generator release];
				break;
			}
			case EOpenMobileHapticsSemanticBehavior::ImpactLight:
			case EOpenMobileHapticsSemanticBehavior::ImpactMedium:
			case EOpenMobileHapticsSemanticBehavior::ImpactHeavy:
			case EOpenMobileHapticsSemanticBehavior::ImpactSoft:
			case EOpenMobileHapticsSemanticBehavior::ImpactRigid:
			{
				UIImpactFeedbackStyle Style = UIImpactFeedbackStyleMedium;
				switch (Behavior)
				{
				case EOpenMobileHapticsSemanticBehavior::ImpactLight:
					Style = UIImpactFeedbackStyleLight;
					break;
				case EOpenMobileHapticsSemanticBehavior::ImpactHeavy:
					Style = UIImpactFeedbackStyleHeavy;
					break;
				case EOpenMobileHapticsSemanticBehavior::ImpactSoft:
					Style = UIImpactFeedbackStyleSoft;
					break;
				case EOpenMobileHapticsSemanticBehavior::ImpactRigid:
					Style = UIImpactFeedbackStyleRigid;
					break;
				default:
					break;
				}
				UIImpactFeedbackGenerator* Generator =
					[[UIImpactFeedbackGenerator alloc] initWithStyle:Style];
				[Generator prepare];
				[Generator impactOccurredWithIntensity:Intensity];
				[Generator release];
				break;
			}
			case EOpenMobileHapticsSemanticBehavior::NotificationSuccess:
			case EOpenMobileHapticsSemanticBehavior::NotificationWarning:
			case EOpenMobileHapticsSemanticBehavior::NotificationError:
			{
				UINotificationFeedbackType Type =
					UINotificationFeedbackTypeSuccess;
				if (Behavior
					== EOpenMobileHapticsSemanticBehavior::NotificationWarning)
				{
					Type = UINotificationFeedbackTypeWarning;
				}
				else if (Behavior
					== EOpenMobileHapticsSemanticBehavior::NotificationError)
				{
					Type = UINotificationFeedbackTypeError;
				}
				UINotificationFeedbackGenerator* Generator =
					[[UINotificationFeedbackGenerator alloc] init];
				[Generator prepare];
				[Generator notificationOccurred:Type];
				[Generator release];
				break;
			}
			}
		});
	}
}

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

FOpenMobileHapticsBackendSubmission
FOpenMobileHapticsIOSBackend::SubmitSemantic(
	const FOpenMobileHapticSemanticRequest& Request,
	const FOpenMobileHapticsSemanticResolution& Resolution,
	const FOpenMobileHapticsBackendRequestToken& Token,
	FOpenMobileHapticsBackendEventCallback Callback
)
{
	static_cast<void>(Token);
	static_cast<void>(Callback);
	FOpenMobileHapticsBackendSubmission Submission;
	if (Resolution.Path == EOpenMobileHapticsSemanticPath::Unsupported)
	{
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Apple device has no available semantic Haptics path.")
		);
		return Submission;
	}
	if (Resolution.Path == EOpenMobileHapticsSemanticPath::BasicVibration)
	{
		dispatch_async(dispatch_get_main_queue(), ^{
			AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
		});
	}
	else
	{
		const FOpenMobileHapticsSemanticDescriptor Descriptor =
			FOpenMobileHapticsSemanticPolicy::Describe(Request.Effect);
		OpenMobileHapticsIOSBackendPrivate::PlaySystemSemantic(
			Descriptor.Behavior,
			Request.Intensity
		);
	}
	Submission.Result.Outcome = Resolution.bFallback
		? EOpenMobileHapticPlaybackOutcome::Fallback
		: EOpenMobileHapticPlaybackOutcome::Accepted;
	Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
	Submission.Result.ResolvedPath =
		FOpenMobileHapticsSemanticPolicy::PathName(Resolution.Path);
	return Submission;
}
