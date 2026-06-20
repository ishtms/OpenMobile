#include "OpenMobileHapticsIOSBackend.h"

#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsAppleBridgeService.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsFallbackPolicy.h"
#include "OpenMobileHapticsIntensityPolicy.h"
#include "OpenMobileHapticsIOSBridge.h"
#include "OpenMobileHapticsPlatformOverridePolicy.h"
#include "OpenMobileHapticsSettings.h"

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

	void AppendAttempts(
		FOpenMobileHapticsBackendSubmission& Submission,
		const TArray<FName>& Attempts
	)
	{
		Submission.Result.FallbackAttempts.Append(Attempts);
	}

	FOpenMobileHapticsBackendSubmission SubmitFallbackResolution(
		FOpenMobileHapticsAppleBridgeService& Service,
		const FOpenMobileHapticNamedPatternRequest& Request,
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticsFallbackResolution& Resolution,
		TArray<FName> Attempts
	)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		if (Resolution.Path == EOpenMobileHapticsFallbackPath::Semantic)
		{
			const FOpenMobileHapticsSemanticDescriptor Descriptor =
				FOpenMobileHapticsSemanticPolicy::Describe(
					Pattern.SemanticFallback
				);
			const EOpenMobileHapticsAppleSubmissionResult BridgeResult =
				Service.PlaySemantic(Descriptor.Behavior, Request.Intensity);
			if (BridgeResult
				!= EOpenMobileHapticsAppleSubmissionResult::Accepted)
			{
				Submission = MakeBridgeFailure(BridgeResult);
			}
			else
			{
				Submission.Result.Outcome =
					EOpenMobileHapticPlaybackOutcome::Fallback;
				Submission.Result.State =
					EOpenMobileHapticPlaybackState::Accepted;
				Submission.Result.ResolvedPath = TEXT("AppleSemanticFallback");
			}
			AppendAttempts(Submission, Attempts);
			return Submission;
		}
		if (Resolution.Path == EOpenMobileHapticsFallbackPath::BasicVibration)
		{
			const EOpenMobileHapticsAppleSubmissionResult BridgeResult =
				Service.PlaySystemVibration();
			if (BridgeResult
				!= EOpenMobileHapticsAppleSubmissionResult::Accepted)
			{
				Submission = MakeBridgeFailure(BridgeResult);
			}
			else
			{
				Submission.Result.Outcome =
					EOpenMobileHapticPlaybackOutcome::Fallback;
				Submission.Result.State =
					EOpenMobileHapticPlaybackState::Accepted;
				Submission.Result.ResolvedPath =
					TEXT("AppleSystemVibrationFallback");
			}
			AppendAttempts(Submission, Attempts);
			return Submission;
		}
		if (Resolution.Path == EOpenMobileHapticsFallbackPath::NoEffect)
		{
			Submission.Result.Outcome =
				EOpenMobileHapticPlaybackOutcome::Suppressed;
			Submission.Result.State = EOpenMobileHapticPlaybackState::Completed;
			Submission.Result.ResolvedPath = TEXT("NoEffect");
			AppendAttempts(Submission, Attempts);
			return Submission;
		}

		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Apple pattern and its declared fallbacks are unavailable.")
		);
		AppendAttempts(Submission, Attempts);
		return Submission;
	}

	FOpenMobileHapticsFallbackResolution ResolveWithoutRichPlayback(
		const UOpenMobileHapticPatternAsset& Pattern,
		FOpenMobileHapticCapabilities Capabilities,
		EOpenMobileHapticFallbackPolicy RequestPolicy
	)
	{
		Capabilities.RichHaptics = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.WaveformTiming =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.TransientEvents =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.ContinuousEvents =
			EOpenMobileHapticSupportState::Unsupported;
		FOpenMobileHapticsPlatformOverrideResolution Override;
		Override.Path =
			EOpenMobileHapticsPlatformOverridePath::PortablePattern;
		Override.Reason = TEXT("NativeSupportChanged");
		return FOpenMobileHapticsFallbackPolicy::Resolve(
			Pattern,
			Override,
			Capabilities,
			RequestPolicy
		);
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
	if (!bSupported)
	{
		const EOpenMobileHapticSupportState Unsupported =
			EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Availability = EOpenMobileHapticAvailability::NoActuator;
		Capabilities.BasicVibration = Unsupported;
		Capabilities.SemanticFeedback = Unsupported;
		Capabilities.RichHaptics = Unsupported;
		Capabilities.AmplitudeControl = Unsupported;
		Capabilities.SemanticEffects = Unsupported;
		Capabilities.PredefinedEffects = Unsupported;
		Capabilities.WaveformTiming = Unsupported;
		Capabilities.Looping = Unsupported;
		Capabilities.Primitives = Unsupported;
		Capabilities.Envelopes = Unsupported;
		Capabilities.FrequencyControl = Unsupported;
		Capabilities.TransientEvents = Unsupported;
		Capabilities.ContinuousEvents = Unsupported;
		Capabilities.DynamicParameters = Unsupported;
		Capabilities.AudioEvents = Unsupported;
		Capabilities.AHAP = Unsupported;
		Capabilities.Scheduling = Unsupported;
		Capabilities.Pause = Unsupported;
		Capabilities.Resume = Unsupported;
		Capabilities.Seek = Unsupported;
		Capabilities.Detail = TEXT("Apple reports no Core Haptics actuator.");
		return Capabilities;
	}

	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const bool bSemanticEnabled = Settings->IOS.bEnableSemanticFeedback;
	const bool bCoreHapticsEnabled = Settings->bEnableCustomPlayback
		&& Settings->IOS.bEnableCoreHaptics;
	const EOpenMobileHapticSupportState Supported =
		EOpenMobileHapticSupportState::Supported;
	const EOpenMobileHapticSupportState Unsupported =
		EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Availability = bCoreHapticsEnabled
		? EOpenMobileHapticAvailability::RichHaptics
		: bSemanticEnabled
			? EOpenMobileHapticAvailability::SemanticFeedback
			: EOpenMobileHapticAvailability::DisabledByPolicy;
	Capabilities.BasicVibration = bSemanticEnabled || bCoreHapticsEnabled
		? Supported
		: Unsupported;
	Capabilities.SemanticFeedback = bSemanticEnabled ? Supported : Unsupported;
	Capabilities.RichHaptics = bCoreHapticsEnabled ? Supported : Unsupported;
	Capabilities.AmplitudeControl = bCoreHapticsEnabled
		? Supported
		: Unsupported;
	Capabilities.SemanticEffects = bSemanticEnabled ? Supported : Unsupported;
	Capabilities.PredefinedEffects = Unsupported;
	Capabilities.WaveformTiming = bCoreHapticsEnabled ? Supported : Unsupported;
	Capabilities.Looping = Unsupported;
	Capabilities.Primitives = Unsupported;
	Capabilities.Envelopes = Unsupported;
	Capabilities.FrequencyControl = Unsupported;
	Capabilities.TransientEvents = bCoreHapticsEnabled
		? EOpenMobileHapticSupportState::Supported
		: EOpenMobileHapticSupportState::Unsupported;
	Capabilities.ContinuousEvents = Unsupported;
	Capabilities.DynamicParameters = Unsupported;
	Capabilities.AudioEvents = Unsupported;
	Capabilities.AHAP = Unsupported;
	Capabilities.Scheduling = Unsupported;
	Capabilities.Pause = Unsupported;
	Capabilities.Resume = Unsupported;
	Capabilities.Seek = Unsupported;
	Capabilities.Detail = bCoreHapticsEnabled
		? TEXT("Apple transient Core Haptics playback is available.")
		: bSemanticEnabled
			? TEXT("Apple UIKit semantic feedback is available.")
			: TEXT("Apple Haptics playback is disabled by project policy.");
	return Capabilities;
}

FOpenMobileHapticCapabilities FOpenMobileHapticsIOSBackend::GetCapabilities() const
{
	return ProbeHardwareCapabilities();
}

FOpenMobileHapticsBackendControlSupport
FOpenMobileHapticsIOSBackend::GetControlSupport() const
{
	FOpenMobileHapticsBackendControlSupport Support;
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	Support.bStop = Settings->bEnableCustomPlayback
		&& Settings->IOS.bEnableCoreHaptics;
	return Support;
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

FOpenMobileHapticsBackendSubmission
FOpenMobileHapticsIOSBackend::SubmitNamedPattern(
	const FOpenMobileHapticNamedPatternRequest& Request,
	const FOpenMobileHapticsBackendRequestToken& Token,
	FOpenMobileHapticsBackendEventCallback Callback
)
{
	using namespace OpenMobileHapticsIOSBackendPrivate;
	const UOpenMobileHapticPatternAsset* Pattern =
		Cast<UOpenMobileHapticPatternAsset>(
			Request.PatternAsset.ResolveObject()
		);
	if (!Pattern)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The named pattern has no loaded portable Apple asset.")
		);
		return Submission;
	}

	const FOpenMobileHapticCapabilities Capabilities = GetCapabilities();
	const FOpenMobileHapticsAppleHardwareProbe Hardware =
		BridgeService->GetHardwareProbe();
	FOpenMobileHapticsPlatformOverrideResolution Override =
		FOpenMobileHapticsPlatformOverridePolicy::Resolve(
			*Pattern,
			EOpenMobileHapticOverridePlatform::IOS,
			Hardware.OSMajorVersion,
			Capabilities,
			Request.Options.FallbackPolicy
		);
	if (Override.Path
		== EOpenMobileHapticsPlatformOverridePath::ExactOverride)
	{
		Override.Path = EOpenMobileHapticsPlatformOverridePath::PortablePattern;
		Override.OverrideAsset.Reset();
		Override.Reason = TEXT("AHAPPlaybackPending");
	}
	const FOpenMobileHapticsFallbackResolution Ladder =
		FOpenMobileHapticsFallbackPolicy::Resolve(
			*Pattern,
			Override,
			Capabilities,
			Request.Options.FallbackPolicy
		);
	TArray<FName> Attempts =
		FOpenMobileHapticsFallbackPolicy::MakeDiagnosticTrace(Ladder);
	if (Ladder.Path != EOpenMobileHapticsFallbackPath::PortableRich)
	{
		return SubmitFallbackResolution(
			*BridgeService,
			Request,
			*Pattern,
			Ladder,
			MoveTemp(Attempts)
		);
	}

	const FOpenMobileHapticsAppleTransientResolution Transient =
		FOpenMobileHapticsAppleTransientPolicy::Resolve(
			Pattern->GetCookedPattern(),
			Capabilities,
			Request.Intensity
		);
	if (Transient.Outcome
		== EOpenMobileHapticsAppleTransientOutcome::Suppressed)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Suppressed;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Completed;
		Submission.Result.ResolvedPath = Transient.Reason;
		Attempts.Add(*FString::Printf(
			TEXT("AppleTransient:%s"),
			*Transient.Reason.ToString()
		));
		AppendAttempts(Submission, Attempts);
		return Submission;
	}
	if (Transient.Outcome
		== EOpenMobileHapticsAppleTransientOutcome::Invalid)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The portable Apple transient pattern is invalid.")
		);
		Attempts.Add(*FString::Printf(
			TEXT("AppleTransient:%s"),
			*Transient.Reason.ToString()
		));
		AppendAttempts(Submission, Attempts);
		return Submission;
	}
	if (Transient.Outcome
		== EOpenMobileHapticsAppleTransientOutcome::FallbackRequired)
	{
		Attempts.Add(*FString::Printf(
			TEXT("AppleTransient:%s"),
			*Transient.Reason.ToString()
		));
		const FOpenMobileHapticsFallbackResolution Fallback =
			ResolveWithoutRichPlayback(
				*Pattern,
				Capabilities,
				Request.Options.FallbackPolicy
			);
		Attempts.Append(
			FOpenMobileHapticsFallbackPolicy::MakeDiagnosticTrace(Fallback)
		);
		return SubmitFallbackResolution(
			*BridgeService,
			Request,
			*Pattern,
			Fallback,
			MoveTemp(Attempts)
		);
	}

	const EOpenMobileHapticsAppleEngineResult EngineResult =
		BridgeService->EnsureEngine();
	if (EngineResult != EOpenMobileHapticsAppleEngineResult::Ready)
	{
		if (EngineResult
			== EOpenMobileHapticsAppleEngineResult::UnsupportedHardware
			|| EngineResult
				== EOpenMobileHapticsAppleEngineResult::TemporarilyUnavailable)
		{
			Attempts.Add(TEXT("AppleTransient:EngineUnavailable"));
			const FOpenMobileHapticsFallbackResolution Fallback =
				ResolveWithoutRichPlayback(
					*Pattern,
					Capabilities,
					Request.Options.FallbackPolicy
				);
			Attempts.Append(
				FOpenMobileHapticsFallbackPolicy::MakeDiagnosticTrace(Fallback)
			);
			return SubmitFallbackResolution(
				*BridgeService,
				Request,
				*Pattern,
				Fallback,
				MoveTemp(Attempts)
			);
		}
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("Apple could not create the Core Haptics engine.")
		);
		AppendAttempts(Submission, Attempts);
		return Submission;
	}

	FOpenMobileHapticsBackendEventCallback CompletionCallback =
		MoveTemp(Callback);
	const EOpenMobileHapticsAppleSubmissionResult BridgeResult =
		BridgeService->PlayTransientPattern(
			Token.RequestId,
			Transient.Pattern,
			[Token, Request, CompletionCallback = MoveTemp(CompletionCallback)](
				EOpenMobileHapticsApplePlaybackEvent Event
			) mutable
			{
				if (!CompletionCallback)
				{
					return;
				}
				FOpenMobileHapticsBackendCallback BackendCallback;
				BackendCallback.Token = Token;
				BackendCallback.Sequence = 1;
				BackendCallback.Event.Handle = Token.PlaybackHandle;
				BackendCallback.Event.State = Event
					== EOpenMobileHapticsApplePlaybackEvent::Completed
						? EOpenMobileHapticPlaybackState::Completed
						: EOpenMobileHapticPlaybackState::Failed;
				BackendCallback.Event.Evidence =
					EOpenMobileHapticEventEvidence::NativeConfirmed;
				BackendCallback.Event.TimestampSeconds = FPlatformTime::Seconds();
				BackendCallback.Event.PatternOrEffect = Request.PatternName;
				BackendCallback.Event.Channel = Request.Options.Channel;
				BackendCallback.Event.ResolvedPath =
					TEXT("AppleTransientPattern");
				if (Event == EOpenMobileHapticsApplePlaybackEvent::Failed)
				{
					BackendCallback.Event.Error =
						FOpenMobileHapticError::FromCommon(
							EOpenMobileErrorCode::NativeFailure,
							TEXT("Apple transient playback failed."),
							EOpenMobileHapticFailureStage::Playback
						);
					BackendCallback.Event.Error.Handle = Token.PlaybackHandle;
					BackendCallback.Event.Error.FailedItem = Request.PatternName;
					BackendCallback.Event.Error.Channel = Request.Options.Channel;
					BackendCallback.Event.Error.bRejectedBeforeSubmission = false;
					BackendCallback.Event.Error.bInterruptedAfterAcceptance = true;
				}
				CompletionCallback(BackendCallback);
			}
		);
	if (BridgeResult != EOpenMobileHapticsAppleSubmissionResult::Accepted)
	{
		FOpenMobileHapticsBackendSubmission Submission =
			MakeBridgeFailure(BridgeResult);
		AppendAttempts(Submission, Attempts);
		return Submission;
	}

	FOpenMobileHapticsBackendSubmission Submission;
	Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Fallback;
	Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
	Submission.Result.ResolvedPath = TEXT("AppleTransientPattern");
	AppendAttempts(Submission, Attempts);
	Submission.bCreatesControllablePlayback = true;
	Submission.bExpectsCallbacks = true;
	return Submission;
}

FOpenMobileHapticControlResult FOpenMobileHapticsIOSBackend::StopPlayback(
	const FOpenMobileHapticsBackendRequestToken& Token
)
{
	const EOpenMobileHapticsAppleSubmissionResult Result =
		BridgeService->StopPattern(Token.RequestId);
	if (Result == EOpenMobileHapticsAppleSubmissionResult::Accepted)
	{
		FOpenMobileHapticControlResult Accepted;
		Accepted.Outcome = EOpenMobileHapticControlOutcome::Accepted;
		return Accepted;
	}
	return FOpenMobileHapticControlResult::MakeRejected(
		Result == EOpenMobileHapticsAppleSubmissionResult::Unsupported
			? EOpenMobileErrorCode::NotSupported
			: EOpenMobileErrorCode::NativeFailure,
		TEXT("Apple could not stop the transient pattern.")
	);
}

void FOpenMobileHapticsIOSBackend::BeginShutdown()
{
	BridgeService->Shutdown();
}
