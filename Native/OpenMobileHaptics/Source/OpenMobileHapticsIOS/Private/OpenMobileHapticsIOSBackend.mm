#include "OpenMobileHapticsIOSBackend.h"

#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsAppleAHAPPlaybackPolicy.h"
#include "OpenMobileHapticsAppleBridgeService.h"
#include "OpenMobileHapticsAppleContinuousPolicy.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsFallbackPolicy.h"
#include "OpenMobileHapticsIntensityPolicy.h"
#include "OpenMobileHapticsIOSBridge.h"
#include "OpenMobileHapticsPlatformOverridePolicy.h"
#include "OpenMobileHapticsSettings.h"
#include "Misc/ScopeLock.h"

namespace OpenMobileHapticsIOSBackendPrivate
{
	enum class EApplePatternTranslationOutcome : uint8
	{
		Ready,
		Suppressed,
		FallbackRequired,
		Invalid
	};

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

	FName TranslationAttempt(FName Translation, FName Reason)
	{
		return *FString::Printf(
			TEXT("%s:%s"),
			*Translation.ToString(),
			*Reason.ToString()
		);
	}

	FOpenMobileHapticsApplePlaybackEventCallback MakePlaybackCallback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		FName PatternName,
		FName Channel,
		FName ResolvedPath,
		FString FailureMessage,
		FOpenMobileHapticsBackendEventCallback Callback
	)
	{
		return [
			Token,
			PatternName,
			Channel,
			ResolvedPath,
			FailureMessage = MoveTemp(FailureMessage),
			Callback = MoveTemp(Callback)
		](EOpenMobileHapticsApplePlaybackEvent Event) mutable
		{
			if (!Callback)
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
			BackendCallback.Event.PatternOrEffect = PatternName;
			BackendCallback.Event.Channel = Channel;
			BackendCallback.Event.ResolvedPath = ResolvedPath;
			if (Event == EOpenMobileHapticsApplePlaybackEvent::Failed)
			{
				BackendCallback.Event.Error = FOpenMobileHapticError::FromCommon(
					EOpenMobileErrorCode::NativeFailure,
					FailureMessage,
					EOpenMobileHapticFailureStage::Playback
				);
				BackendCallback.Event.Error.Handle = Token.PlaybackHandle;
				BackendCallback.Event.Error.FailedItem = PatternName;
				BackendCallback.Event.Error.Channel = Channel;
				BackendCallback.Event.Error.bRejectedBeforeSubmission = false;
				BackendCallback.Event.Error.bInterruptedAfterAcceptance = true;
			}
			Callback(BackendCallback);
		};
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
			ForgetAllAHAPIntensityScales();
			BridgeService->InvalidateHardwareProbe();
			FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
		}
	);
}

FOpenMobileHapticsIOSBackend::~FOpenMobileHapticsIOSBackend() = default;

void FOpenMobileHapticsIOSBackend::RememberAHAPIntensityScale(
	uint64 RequestId,
	float Scale
)
{
	FScopeLock Lock(&AHAPIntensityMutex);
	AHAPStaticIntensityScales.Add(RequestId, Scale);
}

void FOpenMobileHapticsIOSBackend::ForgetAHAPIntensityScale(uint64 RequestId)
{
	FScopeLock Lock(&AHAPIntensityMutex);
	AHAPStaticIntensityScales.Remove(RequestId);
}

void FOpenMobileHapticsIOSBackend::ForgetAllAHAPIntensityScales()
{
	FScopeLock Lock(&AHAPIntensityMutex);
	AHAPStaticIntensityScales.Reset();
}

FOpenMobileHapticDynamicParameterUpdate
FOpenMobileHapticsIOSBackend::ComposeAHAPDynamicUpdate(
	uint64 RequestId,
	const FOpenMobileHapticDynamicParameterUpdate& Update
) const
{
	FScopeLock Lock(&AHAPIntensityMutex);
	const float* Scale = AHAPStaticIntensityScales.Find(RequestId);
	return Scale
		? FOpenMobileHapticsAppleAHAPPlaybackPolicy::ComposeDynamicUpdate(
			Update,
			*Scale
		)
		: Update;
}

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
	const bool bAHAPEnabled = bCoreHapticsEnabled
		&& Settings->IOS.bPackageAHAPResources;
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
	Capabilities.Looping = bCoreHapticsEnabled ? Supported : Unsupported;
	Capabilities.Primitives = Unsupported;
	Capabilities.Envelopes = Unsupported;
	Capabilities.FrequencyControl = Unsupported;
	Capabilities.TransientEvents = bCoreHapticsEnabled
		? EOpenMobileHapticSupportState::Supported
		: EOpenMobileHapticSupportState::Unsupported;
	Capabilities.ContinuousEvents = bCoreHapticsEnabled
		? Supported
		: Unsupported;
	Capabilities.DynamicParameters = bCoreHapticsEnabled
		? Supported
		: Unsupported;
	Capabilities.AudioEvents = bAHAPEnabled && Probe.bSupportsAudio
		? Supported
		: Unsupported;
	Capabilities.AHAP = bAHAPEnabled ? Supported : Unsupported;
	Capabilities.Scheduling = Unsupported;
	Capabilities.Pause = Unsupported;
	Capabilities.Resume = Unsupported;
	Capabilities.Seek = Unsupported;
	Capabilities.Detail = bCoreHapticsEnabled
		? TEXT("Apple transient and continuous Core Haptics playback is available.")
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
	Support.bDynamicParameters = Support.bStop;
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
	const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
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
	FOpenMobileHapticNamedPatternRequest FallbackRequest = Request;
	if (Parameters.bHasInitialDynamicParameters
		&& Parameters.InitialDynamicParameters.bUpdateIntensity)
	{
		FallbackRequest.Intensity = FOpenMobileHapticsIntensityPolicy::Scale(
			Request.Intensity,
			Parameters.InitialDynamicParameters.Intensity,
			1.0f,
			1.0f,
			1.0f,
			1.0f
		);
	}

	const FOpenMobileHapticCapabilities Capabilities = GetCapabilities();
	const FOpenMobileHapticsAppleHardwareProbe Hardware =
		BridgeService->GetHardwareProbe();
	FOpenMobileHapticLoopOptions EffectiveLoop = Pattern->Loop;
	if (Request.Options.Loop.bLoop)
	{
		EffectiveLoop = Request.Options.Loop;
	}
	FOpenMobileHapticsPlatformOverrideResolution Override =
		FOpenMobileHapticsPlatformOverridePolicy::Resolve(
			*Pattern,
			EOpenMobileHapticOverridePlatform::IOS,
			Hardware.OSMajorVersion,
			Capabilities,
			Request.Options.FallbackPolicy
		);
	if (Override.Path == EOpenMobileHapticsPlatformOverridePath::ExactOverride)
	{
		const UOpenMobileHapticIOSPatternAsset* IOSAsset =
			Cast<UOpenMobileHapticIOSPatternAsset>(
				Override.OverrideAsset.ResolveObject()
			);
		if (!IOSAsset)
		{
			Override.Path =
				EOpenMobileHapticsPlatformOverridePath::PortablePattern;
			Override.OverrideAsset.Reset();
			Override.Reason = TEXT("InvalidAHAPOverride");
		}
		else
		{
			FOpenMobileHapticsAppleAHAPResolution AHAP =
				FOpenMobileHapticsAppleAHAPPlaybackPolicy::Resolve(
					*IOSAsset,
					EffectiveLoop,
					Capabilities,
					FOpenMobileHapticsAppleAHAPPlaybackPolicy::MakeLimits(
						*GetDefault<UOpenMobileHapticsSettings>()
					)
				);
			if (AHAP.Outcome
				== EOpenMobileHapticsAppleAHAPOutcome::Invalid)
			{
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result =
					FOpenMobileHapticPlaybackResult::MakeRejected(
						EOpenMobileErrorCode::InvalidArgument,
						TEXT("The Apple AHAP pattern or loop is invalid.")
					);
				Submission.Result.FallbackAttempts.Add(TranslationAttempt(
					TEXT("AppleAHAP"),
					AHAP.Reason
				));
				return Submission;
			}
			if (AHAP.Outcome
				== EOpenMobileHapticsAppleAHAPOutcome::FallbackRequired)
			{
				Override.Path =
					EOpenMobileHapticsPlatformOverridePath::PortablePattern;
				Override.OverrideAsset.Reset();
				Override.Reason = AHAP.Reason;
			}
			else
			{
				AHAP.Pattern.bHasInitialDynamicParameters = true;
				if (Parameters.bHasInitialDynamicParameters)
				{
					AHAP.Pattern.InitialDynamicParameters =
						Parameters.InitialDynamicParameters;
				}
				AHAP.Pattern.InitialDynamicParameters.bUpdateIntensity = true;
				const float PolicyIntensity =
					Parameters.bHasInitialDynamicParameters
						&& Parameters.InitialDynamicParameters.bUpdateIntensity
							? Parameters.InitialDynamicParameters.Intensity
							: 1.0f;
				AHAP.Pattern.InitialDynamicParameters.Intensity =
					FOpenMobileHapticsIntensityPolicy::Scale(
						Request.Intensity,
						PolicyIntensity,
						1.0f,
						1.0f,
						1.0f,
						1.0f
					);
				TArray<FName> Attempts;
				Attempts.Add(TranslationAttempt(
					TEXT("AppleAHAP"),
					AHAP.Reason
				));
				const EOpenMobileHapticsAppleEngineResult EngineResult =
					BridgeService->EnsureEngine();
				if (EngineResult
					!= EOpenMobileHapticsAppleEngineResult::Ready)
				{
					if (EngineResult
						== EOpenMobileHapticsAppleEngineResult::
							UnsupportedHardware
						|| EngineResult
							== EOpenMobileHapticsAppleEngineResult::
								TemporarilyUnavailable)
					{
						Attempts.Add(TranslationAttempt(
							TEXT("AppleAHAP"),
							TEXT("EngineUnavailable")
						));
						const FOpenMobileHapticsFallbackResolution Fallback =
							ResolveWithoutRichPlayback(
								*Pattern,
								Capabilities,
								Request.Options.FallbackPolicy
							);
						Attempts.Append(
							FOpenMobileHapticsFallbackPolicy::
								MakeDiagnosticTrace(Fallback)
						);
						return SubmitFallbackResolution(
							*BridgeService,
							FallbackRequest,
							*Pattern,
							Fallback,
							MoveTemp(Attempts)
						);
					}
					FOpenMobileHapticsBackendSubmission Submission;
					Submission.Result =
						FOpenMobileHapticPlaybackResult::MakeRejected(
							EOpenMobileErrorCode::NativeFailure,
							TEXT("Apple could not create the Core Haptics engine.")
						);
					AppendAttempts(Submission, Attempts);
					return Submission;
				}

				const FName ResolvedPath =
					AHAP.Pattern.bRequiresAdvancedPlayer
						? FName(TEXT("AppleAHAPAdvancedPattern"))
						: FName(TEXT("AppleAHAPStandardPattern"));
				RememberAHAPIntensityScale(
					Token.RequestId,
					Request.Intensity
				);
				FOpenMobileHapticsBackendEventCallback AHAPCallback =
					[this,
					 RequestId = Token.RequestId,
					 Callback = MoveTemp(Callback)](
						const FOpenMobileHapticsBackendCallback& Event
					) mutable
					{
						ForgetAHAPIntensityScale(RequestId);
						if (Callback)
						{
							Callback(Event);
						}
					};
				const EOpenMobileHapticsAppleSubmissionResult BridgeResult =
					BridgeService->PlayAHAPPattern(
						Token.RequestId,
						AHAP.Pattern,
						MakePlaybackCallback(
							Token,
							Request.PatternName,
							Request.Options.Channel,
							ResolvedPath,
							TEXT("Apple AHAP playback failed."),
							MoveTemp(AHAPCallback)
						)
					);
				if (BridgeResult
					!= EOpenMobileHapticsAppleSubmissionResult::Accepted)
				{
					ForgetAHAPIntensityScale(Token.RequestId);
					FOpenMobileHapticsBackendSubmission Submission =
						MakeBridgeFailure(BridgeResult);
					AppendAttempts(Submission, Attempts);
					return Submission;
				}
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result.Outcome =
					EOpenMobileHapticPlaybackOutcome::Accepted;
				Submission.Result.State =
					EOpenMobileHapticPlaybackState::Accepted;
				Submission.Result.ResolvedPath = ResolvedPath;
				AppendAttempts(Submission, Attempts);
				Submission.bCreatesControllablePlayback = true;
				Submission.bExpectsCallbacks = true;
				return Submission;
			}
		}
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
			FallbackRequest,
			*Pattern,
			Ladder,
			MoveTemp(Attempts)
		);
	}

	const FOpenMobileHapticCookedPatternData& CookedPattern =
		Pattern->GetCookedPattern();
	bool bUseContinuousTranslation = !CookedPattern.ParameterCurves.IsEmpty();
	bUseContinuousTranslation |= EffectiveLoop.bLoop;
	for (const FOpenMobileHapticCookedPatternEvent& Event
		: CookedPattern.Events)
	{
		bUseContinuousTranslation |= Event.Type
			== EOpenMobileHapticPatternEventType::Continuous;
	}

	FOpenMobileHapticsAppleTransientResolution Transient;
	FOpenMobileHapticsAppleContinuousResolution Continuous;
	EApplePatternTranslationOutcome TranslationOutcome =
		EApplePatternTranslationOutcome::Invalid;
	const FName TranslationName = bUseContinuousTranslation
		? FName(TEXT("AppleContinuous"))
		: FName(TEXT("AppleTransient"));
	FName TranslationReason;
	if (bUseContinuousTranslation)
	{
		Continuous = FOpenMobileHapticsAppleContinuousPolicy::Resolve(
			CookedPattern,
			EffectiveLoop,
			Capabilities,
			Request.Intensity,
			FOpenMobileHapticsAppleContinuousPolicy::MakeLimits(
				*GetDefault<UOpenMobileHapticsSettings>(),
				Capabilities
			)
		);
		TranslationReason = Continuous.Reason;
		switch (Continuous.Outcome)
		{
		case EOpenMobileHapticsAppleContinuousOutcome::Ready:
			TranslationOutcome = EApplePatternTranslationOutcome::Ready;
			break;
		case EOpenMobileHapticsAppleContinuousOutcome::Suppressed:
			TranslationOutcome = EApplePatternTranslationOutcome::Suppressed;
			break;
		case EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired:
			TranslationOutcome =
				EApplePatternTranslationOutcome::FallbackRequired;
			break;
		case EOpenMobileHapticsAppleContinuousOutcome::Invalid:
			TranslationOutcome = EApplePatternTranslationOutcome::Invalid;
			break;
		}
	}
	else
	{
		Transient = FOpenMobileHapticsAppleTransientPolicy::Resolve(
			CookedPattern,
			Capabilities,
			Request.Intensity
		);
		TranslationReason = Transient.Reason;
		switch (Transient.Outcome)
		{
		case EOpenMobileHapticsAppleTransientOutcome::Ready:
			TranslationOutcome = EApplePatternTranslationOutcome::Ready;
			break;
		case EOpenMobileHapticsAppleTransientOutcome::Suppressed:
			TranslationOutcome = EApplePatternTranslationOutcome::Suppressed;
			break;
		case EOpenMobileHapticsAppleTransientOutcome::FallbackRequired:
			TranslationOutcome =
				EApplePatternTranslationOutcome::FallbackRequired;
			break;
		case EOpenMobileHapticsAppleTransientOutcome::Invalid:
			TranslationOutcome = EApplePatternTranslationOutcome::Invalid;
			break;
		}
	}

	if (TranslationOutcome == EApplePatternTranslationOutcome::Suppressed)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result.Outcome = EOpenMobileHapticPlaybackOutcome::Suppressed;
		Submission.Result.State = EOpenMobileHapticPlaybackState::Completed;
		Submission.Result.ResolvedPath = TranslationReason;
		Attempts.Add(TranslationAttempt(
			TranslationName,
			TranslationReason
		));
		AppendAttempts(Submission, Attempts);
		return Submission;
	}
	if (TranslationOutcome == EApplePatternTranslationOutcome::Invalid)
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::InvalidArgument,
			bUseContinuousTranslation
				? TEXT("The portable Apple continuous pattern is invalid.")
				: TEXT("The portable Apple transient pattern is invalid.")
		);
		Attempts.Add(TranslationAttempt(
			TranslationName,
			TranslationReason
		));
		AppendAttempts(Submission, Attempts);
		return Submission;
	}
	if (TranslationOutcome
		== EApplePatternTranslationOutcome::FallbackRequired)
	{
		Attempts.Add(TranslationAttempt(
			TranslationName,
			TranslationReason
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
			FallbackRequest,
			*Pattern,
			Fallback,
			MoveTemp(Attempts)
		);
	}
	if (Parameters.bHasInitialDynamicParameters)
	{
		if (bUseContinuousTranslation)
		{
			Continuous.Pattern.bHasInitialDynamicParameters = true;
			Continuous.Pattern.InitialDynamicParameters =
				Parameters.InitialDynamicParameters;
		}
		else
		{
			Transient.Pattern.bHasInitialDynamicParameters = true;
			Transient.Pattern.InitialDynamicParameters =
				Parameters.InitialDynamicParameters;
		}
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
			Attempts.Add(TranslationAttempt(
				TranslationName,
				TEXT("EngineUnavailable")
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
				FallbackRequest,
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

	const FName ResolvedPath = bUseContinuousTranslation
		? FName(TEXT("AppleContinuousPattern"))
		: FName(TEXT("AppleTransientPattern"));
	FOpenMobileHapticsApplePlaybackEventCallback NativeCallback =
		MakePlaybackCallback(
			Token,
			Request.PatternName,
			Request.Options.Channel,
			ResolvedPath,
			bUseContinuousTranslation
				? TEXT("Apple continuous playback failed.")
				: TEXT("Apple transient playback failed."),
			MoveTemp(Callback)
		);
	EOpenMobileHapticsAppleSubmissionResult BridgeResult =
		EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	if (bUseContinuousTranslation)
	{
		BridgeResult = BridgeService->PlayContinuousPattern(
			Token.RequestId,
			Continuous.Pattern,
			MoveTemp(NativeCallback)
		);
	}
	else
	{
		BridgeResult = BridgeService->PlayTransientPattern(
			Token.RequestId,
			Transient.Pattern,
			MoveTemp(NativeCallback)
		);
	}
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
	Submission.Result.ResolvedPath = ResolvedPath;
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
		ForgetAHAPIntensityScale(Token.RequestId);
		FOpenMobileHapticControlResult Accepted;
		Accepted.Outcome = EOpenMobileHapticControlOutcome::Accepted;
		return Accepted;
	}
	return FOpenMobileHapticControlResult::MakeRejected(
		Result == EOpenMobileHapticsAppleSubmissionResult::Unsupported
			? EOpenMobileErrorCode::NotSupported
			: EOpenMobileErrorCode::NativeFailure,
		TEXT("Apple could not stop the pattern.")
	);
}

FOpenMobileHapticControlResult
FOpenMobileHapticsIOSBackend::UpdatePlaybackParameters(
	const FOpenMobileHapticsBackendRequestToken& Token,
	const FOpenMobileHapticDynamicParameterUpdate& Update
)
{
	const EOpenMobileHapticsAppleSubmissionResult Result =
		BridgeService->UpdatePattern(
			Token.RequestId,
			ComposeAHAPDynamicUpdate(Token.RequestId, Update)
		);
	FOpenMobileHapticControlResult Control;
	if (Result == EOpenMobileHapticsAppleSubmissionResult::Accepted)
	{
		Control.Outcome = EOpenMobileHapticControlOutcome::Accepted;
		return Control;
	}
	if (Result == EOpenMobileHapticsAppleSubmissionResult::StaleRequest)
	{
		ForgetAHAPIntensityScale(Token.RequestId);
		Control.Outcome = EOpenMobileHapticControlOutcome::StaleHandle;
		Control.Error = FOpenMobileHapticError::FromCommon(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Apple pattern is no longer active."),
			EOpenMobileHapticFailureStage::Playback
		);
		return Control;
	}
	if (Result == EOpenMobileHapticsAppleSubmissionResult::Unsupported)
	{
		Control.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		Control.Error = FOpenMobileHapticError::FromCommon(
			EOpenMobileErrorCode::NotSupported,
			TEXT("Apple dynamic Haptics parameters are unavailable."),
			EOpenMobileHapticFailureStage::Capability
		);
		return Control;
	}
	return FOpenMobileHapticControlResult::MakeRejected(
		EOpenMobileErrorCode::NativeFailure,
		TEXT("Apple could not update the pattern parameters.")
	);
}

void FOpenMobileHapticsIOSBackend::BeginShutdown()
{
	ForgetAllAHAPIntensityScales();
	BridgeService->Shutdown();
}
