#include "OpenMobileHapticsIOSBackend.h"

#include "Misc/ScopeLock.h"
#include "OpenMobileHapticsIntensityPolicy.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreHaptics/CoreHaptics.h>
#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

@interface OpenMobileHapticsSemanticGeneratorCache : NSObject
{
	UISelectionFeedbackGenerator* SelectionGenerator;
	UIImpactFeedbackGenerator* ImpactGenerators[5];
	UINotificationFeedbackGenerator* NotificationGenerator;
	NSUInteger ActivityGeneration;
}

- (void)playBehavior:(EOpenMobileHapticsSemanticBehavior)Behavior
	intensity:(CGFloat)Intensity;
- (void)releaseGenerators;

@end

@implementation OpenMobileHapticsSemanticGeneratorCache

- (void)playBehavior:(EOpenMobileHapticsSemanticBehavior)Behavior
	intensity:(CGFloat)Intensity
{
	switch (Behavior)
	{
	case EOpenMobileHapticsSemanticBehavior::Selection:
		if (!SelectionGenerator)
		{
			SelectionGenerator = [[UISelectionFeedbackGenerator alloc] init];
		}
		[SelectionGenerator prepare];
		[SelectionGenerator selectionChanged];
		[SelectionGenerator prepare];
		break;
	case EOpenMobileHapticsSemanticBehavior::ImpactLight:
	case EOpenMobileHapticsSemanticBehavior::ImpactMedium:
	case EOpenMobileHapticsSemanticBehavior::ImpactHeavy:
	case EOpenMobileHapticsSemanticBehavior::ImpactSoft:
	case EOpenMobileHapticsSemanticBehavior::ImpactRigid:
	{
		const int32 Index = static_cast<int32>(Behavior)
			- static_cast<int32>(EOpenMobileHapticsSemanticBehavior::ImpactLight);
		if (!ImpactGenerators[Index])
		{
			const UIImpactFeedbackStyle Styles[] = {
				UIImpactFeedbackStyleLight,
				UIImpactFeedbackStyleMedium,
				UIImpactFeedbackStyleHeavy,
				UIImpactFeedbackStyleSoft,
				UIImpactFeedbackStyleRigid
			};
			ImpactGenerators[Index] =
				[[UIImpactFeedbackGenerator alloc] initWithStyle:Styles[Index]];
		}
		[ImpactGenerators[Index] prepare];
		[ImpactGenerators[Index] impactOccurredWithIntensity:Intensity];
		[ImpactGenerators[Index] prepare];
		break;
	}
	case EOpenMobileHapticsSemanticBehavior::NotificationSuccess:
	case EOpenMobileHapticsSemanticBehavior::NotificationWarning:
	case EOpenMobileHapticsSemanticBehavior::NotificationError:
	{
		if (!NotificationGenerator)
		{
			NotificationGenerator =
				[[UINotificationFeedbackGenerator alloc] init];
		}
		UINotificationFeedbackType Type = UINotificationFeedbackTypeSuccess;
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
		[NotificationGenerator prepare];
		[NotificationGenerator notificationOccurred:Type];
		[NotificationGenerator prepare];
		break;
	}
	}

	const NSUInteger ExpectedGeneration = ++ActivityGeneration;
	dispatch_after(
		dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
		dispatch_get_main_queue(),
		^{
			if (ActivityGeneration == ExpectedGeneration)
			{
				[self releaseGenerators];
			}
		}
	);
}

- (void)releaseGenerators
{
	[SelectionGenerator release];
	SelectionGenerator = nil;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ImpactGenerators); ++Index)
	{
		[ImpactGenerators[Index] release];
		ImpactGenerators[Index] = nil;
	}
	[NotificationGenerator release];
	NotificationGenerator = nil;
}

- (void)dealloc
{
	[self releaseGenerators];
	[super dealloc];
}

@end

namespace OpenMobileHapticsIOSBackendPrivate
{
	void PlaySystemSemantic(
		OpenMobileHapticsSemanticGeneratorCache* Cache,
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity
	)
	{
		[Cache retain];
		dispatch_async(dispatch_get_main_queue(), ^{
			[Cache playBehavior:Behavior intensity:Intensity];
			[Cache release];
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
		if (!SemanticGeneratorCache)
		{
			SemanticGeneratorCache =
				[[OpenMobileHapticsSemanticGeneratorCache alloc] init];
		}
		const FOpenMobileHapticsSemanticDescriptor Descriptor =
			FOpenMobileHapticsSemanticPolicy::Describe(Request.Effect);
		OpenMobileHapticsIOSBackendPrivate::PlaySystemSemantic(
			static_cast<OpenMobileHapticsSemanticGeneratorCache*>(
				SemanticGeneratorCache
			),
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

FOpenMobileHapticsBackendSubmission
FOpenMobileHapticsIOSBackend::SubmitOneShot(
	const FOpenMobileHapticOneShotRequest& Request,
	const FOpenMobileHapticsOneShotResolution& Resolution,
	const FOpenMobileHapticsBackendRequestToken& Token,
	FOpenMobileHapticsBackendEventCallback Callback
)
{
	static_cast<void>(Token);
	static_cast<void>(Callback);
	FOpenMobileHapticsBackendSubmission Submission;
	if (Resolution.Path == EOpenMobileHapticsOneShotPath::Unsupported)
	{
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Apple device has no available one-shot vibration path.")
		);
		return Submission;
	}
	if (Resolution.Path == EOpenMobileHapticsOneShotPath::BasicVibration)
	{
		const FOpenMobileHapticsIntensityResolution IntensityResolution =
			FOpenMobileHapticsIntensityPolicy::ResolveBasicVibration(
				Request.Intensity,
				EOpenMobileHapticSupportState::Unsupported,
				Request.Options.FallbackPolicy
			);
		if (IntensityResolution.Outcome
			== EOpenMobileHapticsIntensityOutcome::Suppressed)
		{
			Submission.Result.Outcome =
				EOpenMobileHapticPlaybackOutcome::Suppressed;
			Submission.Result.State = EOpenMobileHapticPlaybackState::Completed;
			Submission.Result.ResolvedPath = TEXT("UnavailableIntensity");
			return Submission;
		}
		if (IntensityResolution.Outcome
			== EOpenMobileHapticsIntensityOutcome::Rejected)
		{
			Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
				EOpenMobileErrorCode::NotSupported,
				TEXT("Apple system vibration cannot reproduce the requested intensity.")
			);
			return Submission;
		}
		dispatch_async(dispatch_get_main_queue(), ^{
			AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
		});
		Submission.Result.Intensity.bNativeIntensityKnown =
			IntensityResolution.bNativeIntensityKnown;
		Submission.Result.Intensity.Native =
			IntensityResolution.NativeIntensity;
		Submission.Result.Intensity.bNativeClamped =
			IntensityResolution.bNativeClamped;
		if (IntensityResolution.Outcome
			== EOpenMobileHapticsIntensityOutcome::DefaultAmplitudeFallback)
		{
			Submission.Result.Outcome =
				EOpenMobileHapticPlaybackOutcome::Fallback;
			Submission.Result.ResolvedPath =
				TEXT("BasicVibrationDefaultAmplitude");
		}
	}
	else
	{
		if (!SemanticGeneratorCache)
		{
			SemanticGeneratorCache =
				[[OpenMobileHapticsSemanticGeneratorCache alloc] init];
		}
		OpenMobileHapticsIOSBackendPrivate::PlaySystemSemantic(
			static_cast<OpenMobileHapticsSemanticGeneratorCache*>(
				SemanticGeneratorCache
			),
			EOpenMobileHapticsSemanticBehavior::ImpactMedium,
			Request.Intensity
		);
		Submission.Result.Intensity.bNativeIntensityKnown = true;
		Submission.Result.Intensity.Native = Request.Intensity;
	}
	if (Submission.Result.Outcome
		!= EOpenMobileHapticPlaybackOutcome::Fallback)
	{
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Accepted;
	}
	Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
	if (Submission.Result.ResolvedPath.IsNone())
	{
		Submission.Result.ResolvedPath =
			FOpenMobileHapticsOneShotPolicy::PathName(Resolution.Path);
	}
	return Submission;
}

void FOpenMobileHapticsIOSBackend::BeginShutdown()
{
	OpenMobileHapticsSemanticGeneratorCache* Cache =
		static_cast<OpenMobileHapticsSemanticGeneratorCache*>(
			SemanticGeneratorCache
		);
	SemanticGeneratorCache = nullptr;
	if (Cache)
	{
		dispatch_async(dispatch_get_main_queue(), ^{
			[Cache releaseGenerators];
			[Cache release];
		});
	}
}
