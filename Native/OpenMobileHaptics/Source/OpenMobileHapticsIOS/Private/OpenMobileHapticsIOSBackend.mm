#include "OpenMobileHapticsIOSBackend.h"

#include "OpenMobileHapticsAppleBridgeService.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsIntensityPolicy.h"
#include "OpenMobileHapticsIOSBridge.h"

namespace OpenMobileHapticsIOSBackendPrivate
{
	FOpenMobileHapticsBackendSubmission MakeBridgeFailure(
		EOpenMobileHapticsAppleSubmissionResult Result
	)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		const bool bUnsupported = Result
			== EOpenMobileHapticsAppleSubmissionResult::Unsupported;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			bUnsupported
				? EOpenMobileErrorCode::NotSupported
				: EOpenMobileErrorCode::NativeFailure,
			bUnsupported
				? TEXT("The Apple feedback path is unavailable.")
				: TEXT("Apple could not submit Haptics feedback.")
		);
		return Submission;
	}
}

FOpenMobileHapticsIOSBackend::FOpenMobileHapticsIOSBackend()
	: BridgeService(MakeUnique<FOpenMobileHapticsAppleBridgeService>(
		CreateOpenMobileHapticsIOSBridge()
	))
{
	BridgeService->SetEventCallback(
		[this](EOpenMobileHapticsAppleBridgeEvent Event)
		{
			static_cast<void>(Event);
			BridgeService->InvalidateHardwareProbe();
			FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
		}
	);
}

FOpenMobileHapticsIOSBackend::~FOpenMobileHapticsIOSBackend() = default;

FOpenMobileHapticCapabilities
FOpenMobileHapticsIOSBackend::ProbeHardwareCapabilities() const
{
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.BackendName = GetBackendName();
	const FOpenMobileHapticsAppleHardwareProbe Probe =
		BridgeService->GetHardwareProbe();
	if (Probe.RichHaptics
		== EOpenMobileHapticsAppleHardwareState::TemporarilyUnavailable)
	{
		Capabilities.Availability =
			EOpenMobileHapticAvailability::TemporarilyUnavailable;
		Capabilities.Detail =
			TEXT("Apple haptic capabilities are temporarily unavailable.");
		return Capabilities;
	}

	const bool bSupported = Probe.RichHaptics
		== EOpenMobileHapticsAppleHardwareState::Supported;
	const EOpenMobileHapticSupportState Support = bSupported
		? EOpenMobileHapticSupportState::Supported
		: EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Availability = bSupported
		? EOpenMobileHapticAvailability::RichHaptics
		: EOpenMobileHapticAvailability::NoActuator;
	Capabilities.BasicVibration = Support;
	Capabilities.SemanticFeedback = Support;
	Capabilities.RichHaptics = Support;
	Capabilities.AmplitudeControl = Support;
	Capabilities.SemanticEffects = Support;
	Capabilities.PredefinedEffects = Support;
	Capabilities.WaveformTiming = Support;
	Capabilities.Looping = Support;
	Capabilities.Primitives = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Envelopes = Support;
	Capabilities.FrequencyControl = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.TransientEvents = Support;
	Capabilities.ContinuousEvents = Support;
	Capabilities.DynamicParameters = Support;
	Capabilities.AudioEvents = bSupported && Probe.bSupportsAudio
		? EOpenMobileHapticSupportState::Supported
		: EOpenMobileHapticSupportState::Unsupported;
	Capabilities.AHAP = Support;
	Capabilities.Scheduling = Support;
	Capabilities.Pause = Support;
	Capabilities.Resume = Support;
	Capabilities.Seek = Support;
	Capabilities.Detail = bSupported
		? TEXT("Apple reports Core Haptics hardware support.")
		: TEXT("Apple reports no Core Haptics actuator.");
	return Capabilities;
}

FOpenMobileHapticCapabilities FOpenMobileHapticsIOSBackend::GetCapabilities() const
{
	return ProbeHardwareCapabilities();
}

void FOpenMobileHapticsIOSBackend::HandleLifecycleChange()
{
	BridgeService->InvalidateHardwareProbe();
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
	EOpenMobileHapticsAppleSubmissionResult BridgeResult =
		EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	if (Resolution.Path == EOpenMobileHapticsSemanticPath::BasicVibration)
	{
		BridgeResult = BridgeService->PlaySystemVibration();
	}
	else
	{
		const FOpenMobileHapticsSemanticDescriptor Descriptor =
			FOpenMobileHapticsSemanticPolicy::Describe(Request.Effect);
		BridgeResult = BridgeService->PlaySemantic(
			Descriptor.Behavior,
			Request.Intensity
		);
	}
	if (BridgeResult != EOpenMobileHapticsAppleSubmissionResult::Accepted)
	{
		return OpenMobileHapticsIOSBackendPrivate::MakeBridgeFailure(
			BridgeResult
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
		const EOpenMobileHapticsAppleSubmissionResult BridgeResult =
			BridgeService->PlaySystemVibration();
		if (BridgeResult
			!= EOpenMobileHapticsAppleSubmissionResult::Accepted)
		{
			return OpenMobileHapticsIOSBackendPrivate::MakeBridgeFailure(
				BridgeResult
			);
		}
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
		const EOpenMobileHapticsAppleSubmissionResult BridgeResult =
			BridgeService->PlaySemantic(
			EOpenMobileHapticsSemanticBehavior::ImpactMedium,
			Request.Intensity
		);
		if (BridgeResult
			!= EOpenMobileHapticsAppleSubmissionResult::Accepted)
		{
			return OpenMobileHapticsIOSBackendPrivate::MakeBridgeFailure(
				BridgeResult
			);
		}
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
	BridgeService->Shutdown();
}
