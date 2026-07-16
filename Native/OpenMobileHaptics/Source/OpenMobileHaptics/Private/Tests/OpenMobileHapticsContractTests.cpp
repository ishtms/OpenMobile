#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/ThreadSafeCounter.h"
#include "IOpenMobileHapticsBackend.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/CoreRedirects.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsChannelPolicy.h"
#include "OpenMobileHapticsDurationPolicy.h"
#include "OpenMobileHapticsDynamicParameterPolicy.h"
#include "OpenMobileHapticsAHAPPolicy.h"
#include "OpenMobileHapticsAppleAHAPPlaybackPolicy.h"
#include "OpenMobileHapticsAppleAudioResourcePolicy.h"
#include "OpenMobileHapticsEnvelopePolicy.h"
#include "OpenMobileHapticsErrorMapper.h"
#include "OpenMobileHapticsFallbackPolicy.h"
#include "OpenMobileHapticsIntensityPolicy.h"
#include "OpenMobileHapticLibrary.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsLibraryResolver.h"
#include "OpenMobileHapticsLifecyclePolicy.h"
#include "OpenMobileHapticsOneShotPolicy.h"
#include "OpenMobileHapticsOverlapPolicy.h"
#include "OpenMobileHapticsPlatformOverridePolicy.h"
#include "OpenMobileHapticsPrimitiveCompositionPolicy.h"
#include "OpenMobileHapticsPatternCompiler.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsAndroidConfigurationPolicy.h"
#include "OpenMobileHapticsAndroidFallbackPolicy.h"
#include "OpenMobileHapticsAndroidWaveformPolicy.h"
#include "OpenMobileHapticsRateLimiter.h"
#include "OpenMobileHapticsRecoveryPolicy.h"
#include "OpenMobileHapticsRepeatPolicy.h"
#include "OpenMobileHapticsSemanticPolicy.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsSubsystem.h"
#include "OpenMobileHapticsTimingPolicy.h"
#include "OpenMobileHapticsTimelineManager.h"
#include "OpenMobileHapticsTypes.h"
#include "UObject/UnrealType.h"

namespace OpenMobileHapticsTests
{
	class FMockBackend final : public IOpenMobileHapticsBackend
	{
	public:
		explicit FMockBackend(FName InName, int32 InPriority = 0)
			: Name(InName)
			, Priority(InPriority)
		{
			Capabilities.BackendName = Name;
		}

		virtual FName GetBackendName() const override { return Name; }
		virtual int32 GetPriority() const override { return Priority; }
		virtual bool IsAvailable() const override { return bAvailable; }
		virtual FOpenMobileHapticCapabilities GetCapabilities() const override
		{
			return Capabilities;
		}
		virtual bool IsCustomPlaybackConfigured() const override
		{
			return bCustomPlaybackConfigured;
		}
		virtual void HandleLifecycleChange() override
		{
			++LifecycleChangeCount;
			if (bApplyCapabilitiesAfterLifecycle)
			{
				Capabilities = CapabilitiesAfterLifecycle;
			}
		}
		virtual void HandleApplicationLifecycle(
			const FOpenMobileHapticsLifecycleTransition& Transition
		) override
		{
			++LifecycleTransitionCount;
			LastLifecycleTransition = Transition;
			if (OnHandleApplicationLifecycle)
			{
				OnHandleApplicationLifecycle();
			}
			if (Transition.bInterruptsPlayback)
			{
				ReleasePreparedResources();
			}
			if (Transition.bRefreshesNativeServices
				&& bApplyCapabilitiesAfterLifecycle)
			{
				Capabilities = CapabilitiesAfterLifecycle;
			}
		}
		virtual void HandleInterruption(
			EOpenMobileHapticsInterruptionReason Reason
		) override
		{
			++InterruptionCount;
			LastInterruptionReason = Reason;
			if (OnHandleInterruption)
			{
				OnHandleInterruption();
			}
			ReleasePreparedResources();
		}
		virtual EOpenMobileHapticsRecoveryResult
		RecoverFromInterruption() override
		{
			++RecoveryAttemptCount;
			return RecoveryResult;
		}
		virtual EOpenMobileHapticPreparationState
		GetPreparationState() const override
		{
			return PreparationState;
		}
		virtual FOpenMobileHapticsBackendPreparationResult PrepareResources(
			const FOpenMobileHapticsBackendPreparationRequest& Request
		) override
		{
			++PrepareResourcesCount;
			LastPreparationRequest = Request;
			PreparationState = PreparationResult.State;
			return PreparationResult;
		}
		virtual void ReleasePreparedResources() override
		{
			++ReleasePreparedResourcesCount;
			PreparationState = EOpenMobileHapticPreparationState::Unprepared;
		}
		virtual FOpenMobileHapticsBackendControlSupport
		GetControlSupport() const override
		{
			return ControlSupport;
		}

		virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
			const FOpenMobileHapticSemanticRequest& Request,
			const FOpenMobileHapticsSemanticResolution& Resolution,
			const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			LastSemanticRequest = Request;
			LastSemanticResolution = Resolution;
			LastSemanticPlaybackParameters = Parameters;
			++SemanticSubmissionCount;
			LastToken = Token;
			if (bNativePolicySuppressesSemantic)
			{
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result.Outcome =
					EOpenMobileHapticPlaybackOutcome::Suppressed;
				Submission.Result.State =
					EOpenMobileHapticPlaybackState::Completed;
				Submission.Result.ResolvedPath = TEXT("SystemSemantic");
				return Submission;
			}
			const bool bScheduled = Parameters.Timing.StartDelaySeconds > 0.0;
			return MakeSubmission(bScheduled, bScheduled, MoveTemp(Callback));
		}

		virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
			const FOpenMobileHapticOneShotRequest& Request,
			const FOpenMobileHapticsOneShotResolution& Resolution,
			const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			LastOneShotRequest = Request;
			LastOneShotResolution = Resolution;
			LastOneShotPlaybackParameters = Parameters;
			++OneShotSubmissionCount;
			LastToken = Token;
			if (bBusyOneShot)
			{
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result =
					FOpenMobileHapticPlaybackResult::MakeRejected(
						EOpenMobileErrorCode::Busy,
						TEXT("Injected busy channel.")
					);
				return Submission;
			}
			return MakeSubmission(
				bOneShotControllable,
				bOneShotControllable,
				MoveTemp(Callback)
			);
		}

		virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
			const FOpenMobileHapticNamedPatternRequest& Request,
			const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			LastNamedRequest = Request;
			LastNamedPlaybackParameters = Parameters;
			++NamedSubmissionCount;
			LastToken = Token;
			if (bFailNamedSubmissions)
			{
				FOpenMobileHapticsBackendSubmission Submission;
				Submission.Result =
					FOpenMobileHapticPlaybackResult::MakeRejected(
						EOpenMobileErrorCode::NotSupported,
						TEXT("Injected named-pattern failure.")
					);
				return Submission;
			}
			return MakeSubmission(true, true, MoveTemp(Callback));
		}

		virtual FOpenMobileHapticControlResult StopPlayback(
			const FOpenMobileHapticsBackendRequestToken& Token
		) override
		{
			++StopPlaybackCount;
			LastStoppedToken = Token;
			StoppedTokens.Add(Token);
			FOpenMobileHapticControlResult Result;
			Result.Outcome = ControlSupport.bStop
				? EOpenMobileHapticControlOutcome::Accepted
				: EOpenMobileHapticControlOutcome::Unsupported;
			return Result;
		}

		virtual FOpenMobileHapticControlResult PausePlayback(
			const FOpenMobileHapticsBackendRequestToken& Token,
			const FOpenMobileHapticsBackendControlCommand& Command
		) override
		{
			LastControlToken = Token;
			LastControlCommand = Command;
			++PauseCount;
			return MakePlaybackControlResult(
				SubmissionControlSupport.PauseImplementation
			);
		}

		virtual FOpenMobileHapticControlResult ResumePlayback(
			const FOpenMobileHapticsBackendRequestToken& Token,
			const FOpenMobileHapticsBackendControlCommand& Command
		) override
		{
			LastControlToken = Token;
			LastControlCommand = Command;
			++ResumeCount;
			return MakePlaybackControlResult(
				SubmissionControlSupport.ResumeImplementation
			);
		}

		virtual FOpenMobileHapticControlResult SeekPlayback(
			const FOpenMobileHapticsBackendRequestToken& Token,
			const FOpenMobileHapticsBackendControlCommand& Command
		) override
		{
			LastControlToken = Token;
			LastControlCommand = Command;
			++SeekCount;
			return MakePlaybackControlResult(
				SubmissionControlSupport.SeekImplementation
			);
		}

		virtual FOpenMobileHapticControlResult UpdatePlaybackParameters(
			const FOpenMobileHapticsBackendRequestToken& Token,
			const FOpenMobileHapticDynamicParameterUpdate& Update
		) override
		{
			LastDynamicToken = Token;
			LastDynamicUpdate = Update;
			++DynamicUpdateCount;
			FOpenMobileHapticControlResult Result;
			if (!ControlSupport.bDynamicParameters)
			{
				Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
			}
			else if (bFailDynamicUpdates)
			{
				Result = FOpenMobileHapticControlResult::MakeRejected(
					EOpenMobileErrorCode::NativeFailure,
					TEXT("Injected dynamic parameter failure.")
				);
			}
			else
			{
				Result.Outcome = EOpenMobileHapticControlOutcome::Accepted;
			}
			return Result;
		}

		virtual FOpenMobileHapticControlResult StopChannel(FName Channel) override
		{
			LastStoppedChannel = Channel;
			++StopChannelCount;
			FOpenMobileHapticControlResult Result;
			Result.Outcome = ControlSupport.bStopChannel
				? EOpenMobileHapticControlOutcome::Accepted
				: EOpenMobileHapticControlOutcome::Unsupported;
			return Result;
		}

		virtual FOpenMobileHapticControlResult StopAll() override
		{
			++StopAllCount;
			FOpenMobileHapticControlResult Result;
			Result.Outcome = ControlSupport.bStopAll
				? EOpenMobileHapticControlOutcome::Accepted
				: EOpenMobileHapticControlOutcome::Unsupported;
			return Result;
		}

		virtual void BeginShutdown() override
		{
			++ShutdownCount;
		}

		void Emit(
			int32 PendingIndex,
			EOpenMobileHapticPlaybackState State,
			uint64 Sequence,
			EOpenMobileHapticEventEvidence Evidence =
				EOpenMobileHapticEventEvidence::Estimated
		)
		{
			FOpenMobileHapticPlaybackEvent Event;
			Event.State = State;
			Event.Evidence = Evidence;
			Event.TimestampSeconds = CurrentTimeSeconds;
			EmitEvent(PendingIndex, Sequence, MoveTemp(Event));
		}

		void EmitEvent(
			int32 PendingIndex,
			uint64 Sequence,
			FOpenMobileHapticPlaybackEvent Event
		)
		{
			FOpenMobileHapticsBackendCallback Callback;
			Callback.Token = PendingCallbacks[PendingIndex].Token;
			Callback.Sequence = Sequence;
			Callback.Event = MoveTemp(Event);
			if (!Callback.Event.Handle.IsValid())
			{
				Callback.Event.Handle = Callback.Token.PlaybackHandle;
			}
			PendingCallbacks[PendingIndex].Callback(Callback);
		}

		int32 GetPendingCallbackCount() const
		{
			return PendingCallbacks.Num();
		}

		FOpenMobileHapticCapabilities Capabilities;
		FOpenMobileHapticCapabilities CapabilitiesAfterLifecycle;
		EOpenMobileHapticPreparationState PreparationState =
			EOpenMobileHapticPreparationState::Unprepared;
		FOpenMobileHapticsBackendPreparationResult PreparationResult = {
			EOpenMobileHapticPreparationState::Prepared,
			{}
		};
		FOpenMobileHapticsBackendPreparationRequest LastPreparationRequest;
		FOpenMobileHapticsBackendControlSupport ControlSupport;
		FOpenMobileHapticsBackendPlaybackControlSupport
			SubmissionControlSupport;
		bool bAvailable = true;
		bool bCustomPlaybackConfigured = true;
		bool bFailSubmissions = false;
		bool bFailSubmissionsWithoutError = false;
		bool bNativePolicySuppressesSemantic = false;
		bool bFailNamedSubmissions = false;
		bool bBusyOneShot = false;
		bool bOneShotControllable = true;
		bool bFailDynamicUpdates = false;
		bool bFailPlaybackControls = false;
		bool bApplyCapabilitiesAfterLifecycle = false;
		EOpenMobileHapticsRecoveryResult RecoveryResult =
			EOpenMobileHapticsRecoveryResult::Recovered;
		EOpenMobileHapticsInterruptionReason LastInterruptionReason =
			EOpenMobileHapticsInterruptionReason::EngineStopped;
		TFunction<void()> OnHandleInterruption;
		TFunction<void()> OnHandleApplicationLifecycle;
		double CurrentTimeSeconds = 0.0;
		int32 SemanticSubmissionCount = 0;
		int32 OneShotSubmissionCount = 0;
		int32 NamedSubmissionCount = 0;
		int32 ShutdownCount = 0;
		int32 LifecycleChangeCount = 0;
		int32 LifecycleTransitionCount = 0;
		int32 InterruptionCount = 0;
		int32 RecoveryAttemptCount = 0;
		int32 StopChannelCount = 0;
		int32 StopAllCount = 0;
		int32 StopPlaybackCount = 0;
		int32 DynamicUpdateCount = 0;
		int32 PauseCount = 0;
		int32 ResumeCount = 0;
		int32 SeekCount = 0;
		int32 PrepareResourcesCount = 0;
		int32 ReleasePreparedResourcesCount = 0;
		FOpenMobileHapticsBackendRequestToken LastToken;
		FOpenMobileHapticsBackendRequestToken LastStoppedToken;
		TArray<FOpenMobileHapticsBackendRequestToken> StoppedTokens;
		FOpenMobileHapticsLifecycleTransition LastLifecycleTransition;
		FOpenMobileHapticsBackendRequestToken LastDynamicToken;
		FOpenMobileHapticsBackendRequestToken LastControlToken;
		FOpenMobileHapticsBackendControlCommand LastControlCommand;
		FName LastStoppedChannel;
		FOpenMobileHapticSemanticRequest LastSemanticRequest;
		FOpenMobileHapticOneShotRequest LastOneShotRequest;
		FOpenMobileHapticNamedPatternRequest LastNamedRequest;
		FOpenMobileHapticsBackendPlaybackParameters
			LastSemanticPlaybackParameters;
		FOpenMobileHapticsBackendPlaybackParameters
			LastOneShotPlaybackParameters;
		FOpenMobileHapticsBackendPlaybackParameters
			LastNamedPlaybackParameters;
		FOpenMobileHapticDynamicParameterUpdate LastDynamicUpdate;
		FOpenMobileHapticsSemanticResolution LastSemanticResolution;
		FOpenMobileHapticsOneShotResolution LastOneShotResolution;
		FName SubmissionResolvedPath;
		TArray<FName> SubmissionFallbackAttempts;

	private:
		struct FPendingCallback
		{
			FOpenMobileHapticsBackendRequestToken Token;
			FOpenMobileHapticsBackendEventCallback Callback;
		};

		FOpenMobileHapticsBackendSubmission MakeSubmission(
			bool bControllable,
			bool bExpectsCallbacks,
			FOpenMobileHapticsBackendEventCallback Callback
		)
		{
			FOpenMobileHapticsBackendSubmission Submission;
			if (bFailSubmissionsWithoutError)
			{
				return Submission;
			}
			if (bFailSubmissions)
			{
				Submission.Result =
					FOpenMobileHapticPlaybackResult::MakeRejected(
						EOpenMobileErrorCode::NativeFailure,
						TEXT("Injected backend failure.")
					);
				return Submission;
			}

			Submission.Result.Outcome =
				EOpenMobileHapticPlaybackOutcome::Accepted;
			Submission.Result.State = EOpenMobileHapticPlaybackState::Accepted;
			Submission.Result.ResolvedPath = SubmissionResolvedPath;
			Submission.Result.FallbackAttempts = SubmissionFallbackAttempts;
			Submission.bCreatesControllablePlayback = bControllable;
			Submission.bExpectsCallbacks = bExpectsCallbacks;
			Submission.PlaybackControlSupport = SubmissionControlSupport;
			if (bExpectsCallbacks)
			{
				PendingCallbacks.Add({LastToken, MoveTemp(Callback)});
			}
			return Submission;
		}

		FOpenMobileHapticControlResult MakePlaybackControlResult(
			EOpenMobileHapticControlImplementation Implementation
		) const
		{
			if (bFailPlaybackControls)
			{
				return FOpenMobileHapticControlResult::MakeRejected(
					EOpenMobileErrorCode::NativeFailure,
					TEXT("Injected playback control failure.")
				);
			}
			FOpenMobileHapticControlResult Result;
			Result.Outcome = Implementation
				== EOpenMobileHapticControlImplementation::Unsupported
					? EOpenMobileHapticControlOutcome::Unsupported
					: EOpenMobileHapticControlOutcome::Accepted;
			Result.Implementation = Implementation;
			return Result;
		}

		FName Name;
		int32 Priority = 0;
		TArray<FPendingCallback> PendingCallbacks;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsIntensityPolicyTest,
	"OpenMobile.Haptics.Intensity.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsIntensityPolicyTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	const float Scaled = FOpenMobileHapticsIntensityPolicy::Scale(
		0.5f,
		0.5f,
		0.8f,
		0.5f,
		0.25f,
		0.5f
	);
	TestTrue(TEXT("Intensity scale order is deterministic"),
		FMath::IsNearlyEqual(Scaled, 0.0125f));
	TestEqual(TEXT("Scale result is finally clamped"),
		FOpenMobileHapticsIntensityPolicy::Scale(
			1.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f),
		1.0f);
	TestEqual(TEXT("Underflow resolves to silence"),
		FOpenMobileHapticsIntensityPolicy::Scale(
			UE_SMALL_NUMBER,
			UE_SMALL_NUMBER,
			UE_SMALL_NUMBER,
			UE_SMALL_NUMBER,
			UE_SMALL_NUMBER,
			UE_SMALL_NUMBER
		),
		0.0f);
	TestEqual(TEXT("Missing amplitude control reports fallback"),
		FOpenMobileHapticsIntensityPolicy::ResolveBasicVibration(
			0.5f,
			EOpenMobileHapticSupportState::Unsupported,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Outcome,
		EOpenMobileHapticsIntensityOutcome::DefaultAmplitudeFallback);
	TestEqual(TEXT("Exact intensity rejects missing amplitude control"),
		FOpenMobileHapticsIntensityPolicy::ResolveBasicVibration(
			0.5f,
			EOpenMobileHapticSupportState::Unsupported,
			EOpenMobileHapticFallbackPolicy::ExactOnly
		).Outcome,
		EOpenMobileHapticsIntensityOutcome::Rejected);
	TestEqual(TEXT("Zero is always silent"),
		FOpenMobileHapticsIntensityPolicy::ResolveBasicVibration(
			0.0f,
			EOpenMobileHapticSupportState::Supported,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Outcome,
		EOpenMobileHapticsIntensityOutcome::Suppressed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleAudioResourcePolicyTest,
	"OpenMobile.Haptics.Apple.AHAP.AudioResources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleAudioResourcePolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	auto MakeCAF = [](const TCHAR* Path)
	{
		FOpenMobileHapticIOSAudioResource Resource;
		Resource.RelativePath = Path;
		Resource.Data = {'c', 'a', 'f', 'f', 0, 1, 0, 0};
		return Resource;
	};

	const FOpenMobileHapticsAppleAudioResourceValidation Valid =
		FOpenMobileHapticsAppleAudioResourcePolicy::Validate(
			{TEXT("Audio/click.caf")},
			{MakeCAF(TEXT("Audio/click.caf"))}
		);
	TestTrue(TEXT("A referenced CAF resource is accepted"), Valid.bSuccess);

	TestEqual(TEXT("Missing resources have a typed error"),
		FOpenMobileHapticsAppleAudioResourcePolicy::Validate(
			{TEXT("Audio/click.caf")},
			{}
		).Error,
		EOpenMobileHapticsAppleAudioResourceError::MissingResource);
	TestEqual(TEXT("Traversal is rejected as an unsafe path"),
		FOpenMobileHapticsAppleAudioResourcePolicy::Validate(
			{TEXT("../click.caf")},
			{}
		).Error,
		EOpenMobileHapticsAppleAudioResourceError::UnsafePath);
	TestEqual(TEXT("Unsupported containers have a distinct error"),
		FOpenMobileHapticsAppleAudioResourcePolicy::Validate(
			{TEXT("Audio/click.mp3")},
			{}
		).Error,
		EOpenMobileHapticsAppleAudioResourceError::UnsupportedFormat);
	TestEqual(TEXT("Duplicate resources are rejected"),
		FOpenMobileHapticsAppleAudioResourcePolicy::Validate(
			{TEXT("Audio/click.caf")},
			{
				MakeCAF(TEXT("Audio/click.caf")),
				MakeCAF(TEXT("Audio/click.caf"))
			}
		).Error,
		EOpenMobileHapticsAppleAudioResourceError::DuplicateResource);

	FOpenMobileHapticsAppleAudioResourceLimits Limits;
	Limits.MaximumResourceBytes = 8;
	Limits.MaximumTotalBytes = 12;
	TestEqual(TEXT("Aggregate resource bytes are bounded"),
		FOpenMobileHapticsAppleAudioResourcePolicy::Validate(
			{TEXT("Audio/a.caf"), TEXT("Audio/b.caf")},
			{
				MakeCAF(TEXT("Audio/a.caf")),
				MakeCAF(TEXT("Audio/b.caf"))
			},
			Limits
		).Error,
		EOpenMobileHapticsAppleAudioResourceError::TotalSizeExceeded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsDurationPolicyTest,
	"OpenMobile.Haptics.Duration.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsDurationPolicyTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	TestTrue(TEXT("Minimum duration is accepted"),
		FOpenMobileHapticsDurationPolicy::IsWithinBounds(0.001, 0.001, 1.0));
	TestTrue(TEXT("Maximum duration is accepted"),
		FOpenMobileHapticsDurationPolicy::IsWithinBounds(1.0, 0.001, 1.0));
	TestFalse(TEXT("Nonfinite duration is rejected"),
		FOpenMobileHapticsDurationPolicy::IsWithinBounds(
			std::numeric_limits<double>::infinity(), 0.001, 1.0));

	const FOpenMobileHapticsNativeDurationResolution Rejected =
		FOpenMobileHapticsDurationPolicy::ResolveNativeLimit(
			0.3, 0.1, false);
	TestEqual(TEXT("Unsplittable native overflow is rejected"),
		Rejected.Outcome,
		EOpenMobileHapticsNativeDurationOutcome::Rejected);
	const FOpenMobileHapticsNativeDurationResolution Split =
		FOpenMobileHapticsDurationPolicy::ResolveNativeLimit(
			0.3, 0.1, true);
	TestEqual(TEXT("Splittable native overflow is segmented"),
		Split.Outcome, EOpenMobileHapticsNativeDurationOutcome::Split);
	TestEqual(TEXT("Split count is deterministic"), Split.SegmentCount, 3);

	FOpenMobileHapticPattern Pattern;
	Pattern.Events.Add({
		EOpenMobileHapticPatternEventType::Continuous,
		0.0,
		0.25,
		1.0f,
		0.5f,
		0.5f
	});
	Pattern.Events.Add({
		EOpenMobileHapticPatternEventType::Transient,
		0.5,
		0.0,
		1.0f,
		0.5f,
		0.5f
	});
	double PatternDuration = 0.0;
	TestTrue(TEXT("Accumulated pattern duration is calculated"),
		FOpenMobileHapticsDurationPolicy::TryCalculatePatternDuration(
			Pattern, 1.0, PatternDuration));
	TestEqual(TEXT("Pattern duration includes sparse start times"),
		PatternDuration, 0.5);
	TestFalse(TEXT("Per-event duration limit is enforced"),
		FOpenMobileHapticsDurationPolicy::TryCalculatePatternDuration(
			Pattern, 1.0, 0.1, PatternDuration));
	Pattern.Events[1].StartTimeSeconds =
		std::numeric_limits<double>::max();
	Pattern.Events[1].DurationSeconds =
		std::numeric_limits<double>::max();
	TestFalse(TEXT("Pattern duration overflow is rejected"),
		FOpenMobileHapticsDurationPolicy::TryCalculatePatternDuration(
			Pattern, 1.0, PatternDuration));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPatternCompilerTest,
	"OpenMobile.Haptics.Pattern.Compiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPatternCompilerTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsPatternCompileLimits Limits;
	Limits.MaximumEventCount = 3;
	Limits.MaximumCurveCount = 2;
	Limits.MaximumCurvePointCount = 4;
	Limits.MaximumDurationSeconds = 1.0;
	Limits.MaximumEventDurationSeconds = 0.5;
	Limits.MinimumGranularitySeconds = 0.01;
	FOpenMobileHapticCapabilities NativeLimits;
	NativeLimits.MaximumEventCount = {true, 2};
	NativeLimits.MaximumControlPointCount = {true, 2};
	NativeLimits.MaximumDurationSeconds = {true, 0.5};
	NativeLimits.MinimumTimingGranularitySeconds = {true, 0.02};
	const FOpenMobileHapticsPatternCompileLimits ResolvedLimits =
		FOpenMobileHapticsPatternCompiler::MakeLimits(
			*GetDefault<UOpenMobileHapticsSettings>(),
			NativeLimits
		);
	TestEqual(TEXT("Native event limit narrows project settings"),
		ResolvedLimits.MaximumEventCount, 2);
	TestEqual(TEXT("Native control-point limit narrows curve data"),
		ResolvedLimits.MaximumCurvePointCount, 2);
	TestEqual(TEXT("Native duration narrows project settings"),
		ResolvedLimits.MaximumDurationSeconds, 0.5);
	TestEqual(TEXT("Native granularity raises the portable minimum"),
		ResolvedLimits.MinimumGranularitySeconds, 0.02);

	const FOpenMobileHapticsPatternCompileResult Empty =
		FOpenMobileHapticsPatternCompiler::Compile({}, Limits);
	TestEqual(TEXT("Empty patterns are rejected"), Empty.Error,
		EOpenMobileHapticsPatternCompileError::Empty);

	FOpenMobileHapticPattern Pattern;
	FOpenMobileHapticPatternEvent Transient;
	Transient.Type = EOpenMobileHapticPatternEventType::Transient;
	Transient.StartTimeSeconds = 0.0;
	Transient.DurationSeconds = 0.0;
	Pattern.Events.Add(Transient);
	FOpenMobileHapticPatternEvent Silence;
	Silence.Type = EOpenMobileHapticPatternEventType::Silence;
	Silence.StartTimeSeconds = 0.014;
	Silence.DurationSeconds = 0.016;
	Pattern.Events.Add(Silence);
	FOpenMobileHapticPatternEvent ZeroIntensity;
	ZeroIntensity.Type = EOpenMobileHapticPatternEventType::Continuous;
	ZeroIntensity.StartTimeSeconds = 0.05;
	ZeroIntensity.DurationSeconds = 0.02;
	ZeroIntensity.Intensity = 0.0f;
	Pattern.Events.Add(ZeroIntensity);
	FOpenMobileHapticParameterCurve IntensityCurve;
	IntensityCurve.Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;
	IntensityCurve.StartTimeSeconds = 0.05;
	IntensityCurve.ControlPoints = {{0.0, 1.0f}, {0.019, 0.25f}};
	Pattern.ParameterCurves.Add(IntensityCurve);
	FOpenMobileHapticParameterCurve SharpnessCurve;
	SharpnessCurve.Parameter =
		EOpenMobileHapticCurveParameter::SharpnessControl;
	SharpnessCurve.StartTimeSeconds = 0.05;
	SharpnessCurve.ControlPoints = {{0.0, 0.5f}, {0.019, 1.0f}};
	Pattern.ParameterCurves.Add(SharpnessCurve);
	const FOpenMobileHapticsPatternCompileResult Valid =
		FOpenMobileHapticsPatternCompiler::Compile(Pattern, Limits);
	TestTrue(TEXT("Valid sparse pattern compiles"), Valid.IsSuccess());
	TestEqual(TEXT("Compiled event count is immutable and stable"),
		Valid.Pattern->GetEvents().Num(), 3);
	TestEqual(TEXT("Start times use platform-neutral granularity"),
		Valid.Pattern->GetEvents()[1].StartTimeSeconds, 0.01);
	TestEqual(TEXT("Durations use platform-neutral granularity"),
		Valid.Pattern->GetEvents()[1].DurationSeconds, 0.02);
	TestEqual(TEXT("Intentional silence remains an event"),
		Valid.Pattern->GetEvents()[1].Type,
		EOpenMobileHapticPatternEventType::Silence);
	TestEqual(TEXT("Silence compiles to zero intensity"),
		Valid.Pattern->GetEvents()[1].Intensity, 0.0f);
	TestEqual(TEXT("Zero-intensity continuous events are preserved"),
		Valid.Pattern->GetEvents()[2].Intensity, 0.0f);
	TestEqual(TEXT("Resolved sparse duration is stable"),
		Valid.Pattern->GetDurationSeconds(), 0.07);
	TestEqual(TEXT("Parameter curves compile once"),
		Valid.Pattern->GetParameterCurves().Num(), 2);
	TestEqual(TEXT("Curve start uses portable granularity"),
		Valid.Pattern->GetParameterCurves()[0].StartTimeSeconds, 0.05);
	TestEqual(TEXT("Curve point time uses portable granularity"),
		Valid.Pattern->GetParameterCurves()[0].ControlPoints[1]
			.RelativeTimeSeconds, 0.02);
	TestEqual(TEXT("Normalized curve value remains stable"),
		Valid.Pattern->GetParameterCurves()[0].ControlPoints[1].Value, 0.25f);

	FOpenMobileHapticPattern Unsorted = Pattern;
	Unsorted.Events[2].StartTimeSeconds = 0.005;
	TestEqual(TEXT("Unsorted events are rejected"),
		FOpenMobileHapticsPatternCompiler::Compile(Unsorted, Limits).Error,
		EOpenMobileHapticsPatternCompileError::Unsorted);
	FOpenMobileHapticPattern Overlap = Pattern;
	Overlap.Events[2].StartTimeSeconds = 0.02;
	TestEqual(TEXT("Overlapping events are rejected"),
		FOpenMobileHapticsPatternCompiler::Compile(Overlap, Limits).Error,
		EOpenMobileHapticsPatternCompileError::Overlap);
	FOpenMobileHapticPattern Nonfinite = Pattern;
	Nonfinite.Events[1].Intensity =
		std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Nonfinite pattern values are rejected"),
		FOpenMobileHapticsPatternCompiler::Compile(Nonfinite, Limits).Error,
		EOpenMobileHapticsPatternCompileError::Nonfinite);
	FOpenMobileHapticPattern TooDense = Pattern;
	TooDense.Events.Add(ZeroIntensity);
	TestEqual(TEXT("Patterns above the event limit are rejected"),
		FOpenMobileHapticsPatternCompiler::Compile(TooDense, Limits).Error,
		EOpenMobileHapticsPatternCompileError::EventLimit);
	FOpenMobileHapticPattern TooLong = Pattern;
	TooLong.Events[2].StartTimeSeconds = 0.99;
	TooLong.Events[2].DurationSeconds = 0.02;
	TestEqual(TEXT("Patterns above the duration limit are rejected"),
		FOpenMobileHapticsPatternCompiler::Compile(TooLong, Limits).Error,
		EOpenMobileHapticsPatternCompileError::DurationLimit);
	FOpenMobileHapticPattern EventTooLong = Pattern;
	EventTooLong.Events[1].DurationSeconds = 0.51;
	TestEqual(TEXT("Events above the duration limit are rejected"),
		FOpenMobileHapticsPatternCompiler::Compile(EventTooLong, Limits).Error,
		EOpenMobileHapticsPatternCompileError::EventDurationLimit);
	FOpenMobileHapticPattern UnsortedCurve = Pattern;
	UnsortedCurve.ParameterCurves[0].ControlPoints[1].RelativeTimeSeconds = 0.0;
	TestEqual(TEXT("Duplicate curve point times are rejected"),
		FOpenMobileHapticsPatternCompiler::Compile(UnsortedCurve, Limits).Error,
		EOpenMobileHapticsPatternCompileError::CurveUnsorted);
	FOpenMobileHapticPattern InvalidCurve = Pattern;
	InvalidCurve.ParameterCurves[1].ControlPoints.Reset();
	const FOpenMobileHapticsPatternCompileResult InvalidCurveResult =
		FOpenMobileHapticsPatternCompiler::Compile(InvalidCurve, Limits);
	TestEqual(TEXT("Invalid curve identifies its curve index"),
		InvalidCurveResult.CurveIndex, 1);
	TestEqual(TEXT("Whole-curve failures do not invent a point index"),
		InvalidCurveResult.ControlPointIndex, INDEX_NONE);
	FOpenMobileHapticPattern LongCurve = Pattern;
	LongCurve.ParameterCurves[0].ControlPoints[1].RelativeTimeSeconds = 0.03;
	TestEqual(TEXT("Curves cannot exceed the compiled timeline"),
		FOpenMobileHapticsPatternCompiler::Compile(LongCurve, Limits).Error,
		EOpenMobileHapticsPatternCompileError::CurveDurationLimit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsRepeatPolicyTest,
	"OpenMobile.Haptics.Pattern.RepeatPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsRepeatPolicyTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticLoopOptions NoLoop;
	const FOpenMobileHapticsRepeatPlanResult Single =
		FOpenMobileHapticsRepeatPolicy::Resolve(NoLoop, 0.8, 32, 30.0);
	TestTrue(TEXT("Non-looping pattern has one iteration"),
		Single.IsSuccess());
	TestEqual(TEXT("Non-looping iteration count is one"),
		Single.Plan.TotalIterationCount, 1);

	FOpenMobileHapticLoopOptions Finite;
	Finite.bLoop = true;
	Finite.RepeatCount = 1;
	Finite.RepeatStartTimeSeconds = 0.2;
	Finite.MaximumDurationSeconds = 5.0;
	const FOpenMobileHapticsRepeatPlanResult OneRepeat =
		FOpenMobileHapticsRepeatPolicy::Resolve(Finite, 0.8, 32, 30.0);
	TestTrue(TEXT("One repeat is accepted"), OneRepeat.IsSuccess());
	TestEqual(TEXT("One repeat means two total iterations"),
		OneRepeat.Plan.TotalIterationCount, 2);
	TestEqual(TEXT("Repeat section duration is stable"),
		OneRepeat.Plan.RepeatDurationSeconds, 0.6);
	TestEqual(TEXT("Finite total duration includes the repeat section"),
		OneRepeat.Plan.TotalDurationSeconds, 1.4);

	FOpenMobileHapticLoopOptions Indefinite = Finite;
	Indefinite.RepeatCount = 0;
	const FOpenMobileHapticsRepeatPlanResult UntilStopped =
		FOpenMobileHapticsRepeatPolicy::Resolve(
			Indefinite, 0.8, 32, 30.0);
	TestTrue(TEXT("Zero repeat count loops until stopped"),
		UntilStopped.Plan.bRepeatUntilStopped);
	TestEqual(TEXT("Indefinite playback keeps an explicit safety bound"),
		UntilStopped.Plan.MaximumDurationSeconds, 5.0);

	Finite.RepeatStartTimeSeconds = 0.8;
	TestEqual(TEXT("Repeat start at pattern end is rejected"),
		FOpenMobileHapticsRepeatPolicy::Resolve(
			Finite, 0.8, 32, 30.0).Error,
		EOpenMobileHapticsRepeatError::InvalidRepeatStart);
	Finite.RepeatStartTimeSeconds = 0.2;
	Finite.RepeatCount = 33;
	TestEqual(TEXT("Repeat count above project limit is rejected"),
		FOpenMobileHapticsRepeatPolicy::Resolve(
			Finite, 0.8, 32, 30.0).Error,
		EOpenMobileHapticsRepeatError::RepeatLimit);
	Indefinite.MaximumDurationSeconds = 0.0;
	TestEqual(TEXT("Unbounded indefinite repeat is rejected"),
		FOpenMobileHapticsRepeatPolicy::Resolve(
			Indefinite, 0.8, 32, 30.0).Error,
		EOpenMobileHapticsRepeatError::InvalidSafetyDuration);

	FOpenMobileHapticPlaybackHandle Handle;
	Handle.Id = FGuid(42, 0, 0, 1);
	FOpenMobileHapticsRepeatCursor Cursor(UntilStopped.Plan, Handle, 10.0);
	TestFalse(TEXT("No repeat is due during the first iteration"),
		Cursor.Advance(10.5).bShouldSubmit);
	const FOpenMobileHapticsRepeatAdvance Hitched = Cursor.Advance(12.0);
	TestTrue(TEXT("Clock hitch schedules one repeat"), Hitched.bShouldSubmit);
	TestEqual(TEXT("Clock hitch skips missed repeat callbacks"),
		Hitched.IterationIndex, 3);
	TestFalse(TEXT("Same time never accumulates another callback"),
		Cursor.Advance(12.0).bShouldSubmit);
	FOpenMobileHapticPlaybackHandle StaleHandle;
	StaleHandle.Id = FGuid(7, 0, 0, 1);
	TestFalse(TEXT("Stale handle cannot stop repeat ownership"),
		Cursor.Stop(StaleHandle));
	TestTrue(TEXT("Owning handle stops all future repeats"),
		Cursor.Stop(Handle));
	TestFalse(TEXT("Stopped cursor never schedules again"),
		Cursor.Advance(12.6).bShouldSubmit);
	FOpenMobileHapticsRepeatCursor TeardownCursor(
		UntilStopped.Plan,
		Handle,
		20.0
	);
	TeardownCursor.Cancel();
	TestFalse(TEXT("Teardown cancellation removes future repeats"),
		TeardownCursor.Advance(20.8).bShouldSubmit);
	return true;
}

#if WITH_EDITORONLY_DATA
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticPatternAssetTest,
	"OpenMobile.Haptics.Pattern.Asset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticPatternAssetTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticPatternAsset* Asset =
		NewObject<UOpenMobileHapticPatternAsset>();
	Asset->DefaultCategory = TEXT("Gameplay");
	Asset->Priority = EOpenMobileHapticChannelPriority::High;
	Asset->OverlapPolicy = EOpenMobileHapticOverlapPolicy::Queue;
	Asset->FallbackPolicy = EOpenMobileHapticFallbackPolicy::NoBasicVibration;
	FOpenMobileHapticPatternEvent Transient;
	Transient.Type = EOpenMobileHapticPatternEventType::Transient;
	Asset->SourcePattern.Events.Add(Transient);
	FOpenMobileHapticPatternEvent Silence;
	Silence.Type = EOpenMobileHapticPatternEventType::Silence;
	Silence.StartTimeSeconds = 0.01;
	Silence.DurationSeconds = 0.02;
	Asset->SourcePattern.Events.Add(Silence);
	FOpenMobileHapticPatternEvent Continuous;
	Continuous.Type = EOpenMobileHapticPatternEventType::Continuous;
	Continuous.StartTimeSeconds = 0.03;
	Continuous.DurationSeconds = 0.04;
	Continuous.Intensity = 0.75f;
	Asset->SourcePattern.Events.Add(Continuous);
	FOpenMobileHapticParameterCurve IntensityCurve;
	IntensityCurve.Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;
	IntensityCurve.StartTimeSeconds = 0.03;
	IntensityCurve.ControlPoints = {{0.0, 1.0f}, {0.04, 0.25f}};
	Asset->SourcePattern.ParameterCurves.Add(IntensityCurve);
	Asset->AndroidOverride =
		TSoftObjectPtr<UOpenMobileHapticAndroidPatternAsset>(
			FSoftObjectPath(TEXT("/Game/Haptics/Android.Pattern"))
		);
	Asset->IOSOverride = TSoftObjectPtr<UOpenMobileHapticIOSPatternAsset>(
		FSoftObjectPath(TEXT("/Game/Haptics/IOS.Pattern")));

	TArray<FString> Errors;
	TestTrue(TEXT("Valid asset builds compact derived data"),
		Asset->RebuildDerivedData(Errors));
	TestTrue(TEXT("Derived data matches its source"),
		Asset->IsDerivedDataCurrent());
	TestEqual(TEXT("Cooked event count matches source"),
		Asset->GetCookedPattern().Events.Num(), 3);
	TestEqual(TEXT("Cooked silence remains zero amplitude"),
		Asset->GetCookedPattern().Events[1].Intensity, static_cast<uint16>(0));
	TestEqual(TEXT("Cooked duration uses bounded microseconds"),
		Asset->GetCookedPattern().DurationMicroseconds,
		static_cast<uint32>(70000));
	TestEqual(TEXT("Cooked curve count matches source"),
		Asset->GetCookedPattern().ParameterCurves.Num(), 1);
	TestEqual(TEXT("Cooked curve start is quantized"),
		Asset->GetCookedPattern().ParameterCurves[0].StartTimeMicroseconds,
		static_cast<uint32>(30000));
	TestEqual(TEXT("Cooked curve endpoint is quantized"),
		Asset->GetCookedPattern().ParameterCurves[0].ControlPoints[1]
			.RelativeTimeMicroseconds,
		static_cast<uint32>(40000));
	TestFalse(TEXT("Rebuild reports no errors"), !Errors.IsEmpty());

	UOpenMobileHapticPatternAsset* Duplicate =
		DuplicateObject<UOpenMobileHapticPatternAsset>(
			Asset,
			GetTransientPackage()
		);
	TestTrue(TEXT("Duplicated asset retains current derived data"),
		Duplicate && Duplicate->IsDerivedDataCurrent());
	TestEqual(TEXT("Duplication retains soft Android override"),
		Duplicate->AndroidOverride.ToSoftObjectPath(),
		Asset->AndroidOverride.ToSoftObjectPath());
	TestEqual(TEXT("Duplication retains soft iOS override"),
		Duplicate->IOSOverride.ToSoftObjectPath(),
		Asset->IOSOverride.ToSoftObjectPath());

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FOpenMobileHapticCookedPatternData Saved = Asset->GetCookedPattern();
	Saved.Serialize(Writer);
	FMemoryReader Reader(Bytes);
	FOpenMobileHapticCookedPatternData Loaded;
	Loaded.Serialize(Reader);
	TestFalse(TEXT("Cooked serialization remains readable"), Reader.IsError());
	TestEqual(TEXT("Cooked serialization preserves event count"),
		Loaded.Events.Num(), Saved.Events.Num());
	TestEqual(TEXT("Cooked serialization preserves frequency intent"),
		Loaded.Events[2].FrequencyIntent,
		Saved.Events[2].FrequencyIntent);
	TestEqual(TEXT("Cooked serialization preserves curves"),
		Loaded.ParameterCurves.Num(), Saved.ParameterCurves.Num());
	TestEqual(TEXT("Cooked serialization preserves curve values"),
		Loaded.ParameterCurves[0].ControlPoints[1].Value,
		Saved.ParameterCurves[0].ControlPoints[1].Value);

	FOpenMobileHapticCookedPatternData VersionTwo = Saved;
	VersionTwo.DataFormatVersion = 2;
	TArray<uint8> VersionTwoBytes;
	FMemoryWriter VersionTwoWriter(VersionTwoBytes);
	VersionTwo.Serialize(VersionTwoWriter);
	FMemoryReader VersionTwoReader(VersionTwoBytes);
	FOpenMobileHapticCookedPatternData LoadedVersionTwo;
	LoadedVersionTwo.Serialize(VersionTwoReader);
	TestFalse(TEXT("Version two cooked data remains readable"),
		VersionTwoReader.IsError());
	TestTrue(TEXT("Version two data has no fabricated curves"),
		LoadedVersionTwo.ParameterCurves.IsEmpty());

	FOpenMobileHapticCookedPatternData Legacy = Saved;
	Legacy.DataFormatVersion = 1;
	TArray<uint8> LegacyBytes;
	FMemoryWriter LegacyWriter(LegacyBytes);
	Legacy.Serialize(LegacyWriter);
	FMemoryReader LegacyReader(LegacyBytes);
	FOpenMobileHapticCookedPatternData LoadedLegacy;
	LoadedLegacy.Serialize(LegacyReader);
	TestFalse(TEXT("Legacy cooked data remains readable"),
		LegacyReader.IsError());
	TestEqual(TEXT("Legacy data receives neutral frequency intent"),
		LoadedLegacy.Events[2].FrequencyIntent,
		static_cast<uint16>(MAX_uint16 / 2));
	TestTrue(TEXT("Legacy data has no fabricated curves"),
		LoadedLegacy.ParameterCurves.IsEmpty());

	TArray<uint8> ExcessiveCurveBytes;
	FMemoryWriter ExcessiveCurveWriter(ExcessiveCurveBytes);
	uint8 VersionThree =
		FOpenMobileHapticCookedPatternData::CurrentFormatVersion;
	uint32 ZeroUInt32 = 0;
	int32 ZeroInt32 = 0;
	int32 ExcessiveCurveCount = 129;
	ExcessiveCurveWriter << VersionThree;
	ExcessiveCurveWriter << ZeroUInt32;
	ExcessiveCurveWriter << ZeroUInt32;
	ExcessiveCurveWriter << ZeroUInt32;
	ExcessiveCurveWriter << ZeroInt32;
	ExcessiveCurveWriter << ExcessiveCurveCount;
	FMemoryReader ExcessiveCurveReader(ExcessiveCurveBytes);
	FOpenMobileHapticCookedPatternData ExcessiveCurves;
	ExcessiveCurves.Serialize(ExcessiveCurveReader);
	TestTrue(TEXT("Cooked curve allocation is bounded"),
		ExcessiveCurveReader.IsError());

	TArray<uint8> ExcessivePointBytes;
	FMemoryWriter ExcessivePointWriter(ExcessivePointBytes);
	int32 OneCurve = 1;
	uint8 IntensityParameter = static_cast<uint8>(
		EOpenMobileHapticCurveParameter::IntensityControl
	);
	int32 ExcessivePointCount = 4097;
	ExcessivePointWriter << VersionThree;
	ExcessivePointWriter << ZeroUInt32;
	ExcessivePointWriter << ZeroUInt32;
	ExcessivePointWriter << ZeroUInt32;
	ExcessivePointWriter << ZeroInt32;
	ExcessivePointWriter << OneCurve;
	ExcessivePointWriter << IntensityParameter;
	ExcessivePointWriter << ZeroUInt32;
	ExcessivePointWriter << ExcessivePointCount;
	FMemoryReader ExcessivePointReader(ExcessivePointBytes);
	FOpenMobileHapticCookedPatternData ExcessivePoints;
	ExcessivePoints.Serialize(ExcessivePointReader);
	TestTrue(TEXT("Cooked curve point allocation is bounded"),
		ExcessivePointReader.IsError());

	TArray<uint8> CookBytes;
	FMemoryWriter CookMemoryWriter(CookBytes);
	FObjectAndNameAsStringProxyArchive CookWriter(
		CookMemoryWriter,
		false
	);
	CookWriter.SetFilterEditorOnly(true);
	Asset->Serialize(CookWriter);
	UOpenMobileHapticPatternAsset* CookedCopy =
		NewObject<UOpenMobileHapticPatternAsset>();
	FMemoryReader CookMemoryReader(CookBytes);
	FObjectAndNameAsStringProxyArchive CookReader(
		CookMemoryReader,
		false
	);
	CookReader.SetFilterEditorOnly(true);
	CookedCopy->Serialize(CookReader);
	TestFalse(TEXT("Cook-filtered serialization remains readable"),
		CookMemoryReader.IsError());
	TestTrue(TEXT("Cook filtering removes the editor timeline"),
		CookedCopy->SourcePattern.Events.IsEmpty());
	TestEqual(TEXT("Cook filtering preserves derived events"),
		CookedCopy->GetCookedPattern().Events.Num(), 3);
	TestEqual(TEXT("Cook filtering preserves derived curves"),
		CookedCopy->GetCookedPattern().ParameterCurves.Num(), 1);

	const FName RenameSource = MakeUniqueObjectName(
		GetTransientPackage(),
		UOpenMobileHapticPatternAsset::StaticClass(),
		TEXT("PatternBeforeRename")
	);
	UOpenMobileHapticPatternAsset* RenamedAsset =
		NewObject<UOpenMobileHapticPatternAsset>(
			GetTransientPackage(),
			RenameSource
		);
	const FPrimaryAssetId BeforeRename = RenamedAsset->GetPrimaryAssetId();
	const FName RenameTarget = MakeUniqueObjectName(
		GetTransientPackage(),
		UOpenMobileHapticPatternAsset::StaticClass(),
		TEXT("PatternAfterRename")
	);
	TestTrue(TEXT("Pattern asset can be renamed"),
		RenamedAsset->Rename(*RenameTarget.ToString()));
	const FPrimaryAssetId AfterRename = RenamedAsset->GetPrimaryAssetId();
	TestEqual(TEXT("Rename preserves the primary asset type"),
		AfterRename.PrimaryAssetType, BeforeRename.PrimaryAssetType);
	TestEqual(TEXT("Rename updates the primary asset name"),
		AfterRename.PrimaryAssetName, RenameTarget);

	const FString RedirectSource = TEXT("OpenMobileHapticPatternAssetTest");
	const FString OldPath = TEXT("/Game/Haptics/OldPattern.OldPattern");
	const FString NewPath = TEXT("/Game/Haptics/NewPattern.NewPattern");
	TArray<FCoreRedirect> Redirects;
	Redirects.Emplace(ECoreRedirectFlags::Type_Object, OldPath, NewPath);
	TestTrue(TEXT("Pattern redirect registers"),
		FCoreRedirects::AddRedirectList(Redirects, RedirectSource));
	FSoftObjectPath RedirectedPath(OldPath);
	TestTrue(TEXT("Soft pattern path follows a redirect"),
		RedirectedPath.FixupCoreRedirects());
	TestEqual(TEXT("Redirect resolves to the renamed pattern"),
		RedirectedPath, FSoftObjectPath(NewPath));
	TestTrue(TEXT("Pattern redirect unregisters"),
		FCoreRedirects::RemoveRedirectList(Redirects, RedirectSource));

	Asset->SourcePattern.Events[2].Intensity = 0.5f;
	TestFalse(TEXT("Source edits invalidate derived data"),
		Asset->IsDerivedDataCurrent());
	TestTrue(TEXT("Edited source rebuilds derived data"),
		Asset->RebuildDerivedData(Errors));
	Asset->SourcePattern.Events[1].DurationSeconds =
		std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Invalid asset source cannot rebuild"),
		Asset->RebuildDerivedData(Errors));
	TestFalse(TEXT("Invalid asset identifies its source event"),
		Errors.IsEmpty());
	TestTrue(TEXT("Invalid asset identifies its source field"),
		Errors[0].Contains(TEXT("DurationSeconds")));
	return true;
}
#endif

#if WITH_EDITORONLY_DATA
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticNamedLibraryResolverTest,
	"OpenMobile.Haptics.Pattern.NamedLibraryResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticNamedLibraryResolverTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticPatternAsset* ConfirmPattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	UOpenMobileHapticPatternAsset* RecoilPattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	UOpenMobileHapticPatternAsset* OverridePattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	for (UOpenMobileHapticPatternAsset* Pattern :
		{ConfirmPattern, RecoilPattern, OverridePattern})
	{
		Pattern->SourcePattern.Events.AddDefaulted();
		TArray<FString> BuildErrors;
		TestTrue(TEXT("Named pattern asset builds for preparation"),
			Pattern->RebuildDerivedData(BuildErrors));
	}
	UOpenMobileHapticLibrary* PrimaryLibrary =
		NewObject<UOpenMobileHapticLibrary>();
	PrimaryLibrary->Patterns = {
		{TEXT("UI_Confirm"), ConfirmPattern},
		{TEXT("Weapon_Recoil"), RecoilPattern}
	};
	UOpenMobileHapticLibrary* SecondaryLibrary =
		NewObject<UOpenMobileHapticLibrary>();
	SecondaryLibrary->Patterns = {
		{TEXT("UI_Confirm"), OverridePattern},
		{TEXT("Vehicle_Bump"), OverridePattern}
	};

	FOpenMobileHapticsLibraryResolver Resolver;
	const uint64 Generation = Resolver.BeginPreparation();
	TestEqual(TEXT("Preparation exposes a loading state"),
		Resolver.GetStatus(TEXT("UI_Confirm")),
		EOpenMobileHapticNamedPatternStatus::Loading);
	TArray<FString> Errors;
	TestTrue(TEXT("Ordered loaded libraries prepare"),
		Resolver.CompletePreparation(
			Generation,
			{PrimaryLibrary, SecondaryLibrary},
			Errors
		));
	TestTrue(TEXT("Valid preparation has no errors"), Errors.IsEmpty());
	FSoftObjectPath ResolvedPath;
	TestTrue(TEXT("Prepared lookup finds the first library entry"),
		Resolver.Find(TEXT("UI_Confirm"), ResolvedPath));
	TestEqual(TEXT("Earlier configured library wins deterministically"),
		ResolvedPath, FSoftObjectPath(ConfirmPattern));
	TestTrue(TEXT("Later library contributes unique names"),
		Resolver.Find(TEXT("Vehicle_Bump"), ResolvedPath));
	TestEqual(TEXT("Prepared names report loaded status"),
		Resolver.GetStatus(TEXT("Vehicle_Bump")),
		EOpenMobileHapticNamedPatternStatus::Loaded);
	TestEqual(TEXT("Unknown prepared names report missing status"),
		Resolver.GetStatus(TEXT("Missing")),
		EOpenMobileHapticNamedPatternStatus::Missing);

	Resolver.Release();
	TestEqual(TEXT("Release returns lookups to unprepared"),
		Resolver.GetStatus(TEXT("UI_Confirm")),
		EOpenMobileHapticNamedPatternStatus::Unprepared);
	TestFalse(TEXT("Released generation rejects a stale completion"),
		Resolver.CompletePreparation(
			Generation,
			{PrimaryLibrary},
			Errors
		));

	UOpenMobileHapticLibrary* InvalidLibrary =
		NewObject<UOpenMobileHapticLibrary>();
	InvalidLibrary->Patterns = {
		{TEXT("Duplicate"), ConfirmPattern},
		{TEXT("Duplicate"), RecoilPattern},
		{NAME_None, RecoilPattern},
		{TEXT("MissingAsset"), nullptr}
	};
	TMap<FName, FSoftObjectPath> InvalidLookup;
	Errors.Reset();
	TestFalse(TEXT("Invalid names and assets fail library validation"),
		InvalidLibrary->BuildPatternLookup(InvalidLookup, Errors));
	TestTrue(TEXT("Invalid library reports every rejected entry"),
		Errors.Num() >= 3);
	TestTrue(TEXT("Failed validation leaves no partial lookup"),
		InvalidLookup.IsEmpty());

	UOpenMobileHapticLibrary* UnloadedLibrary =
		NewObject<UOpenMobileHapticLibrary>();
	UnloadedLibrary->Patterns = {{
		TEXT("Unloaded"),
		TSoftObjectPtr<UOpenMobileHapticPatternAsset>(
			FSoftObjectPath(TEXT("/Game/Haptics/Unloaded.Unloaded"))
		)
	}};
	const uint64 UnloadedGeneration = Resolver.BeginPreparation();
	TestFalse(TEXT("Unloaded pattern assets cannot become prepared"),
		Resolver.CompletePreparation(
			UnloadedGeneration,
			{UnloadedLibrary},
			Errors
		));
	TestEqual(TEXT("Unloaded pattern leaves resolver invalid"),
		Resolver.GetStatus(TEXT("Unloaded")),
		EOpenMobileHapticNamedPatternStatus::Invalid);

	const FString LibraryRedirectSource =
		TEXT("OpenMobileHapticNamedLibraryResolverTest");
	const FString OldPatternPath =
		TEXT("/Game/Haptics/OldNamed.OldNamed");
	const FString NewPatternPath =
		TEXT("/Game/Haptics/NewNamed.NewNamed");
	TArray<FCoreRedirect> LibraryRedirects;
	LibraryRedirects.Emplace(
		ECoreRedirectFlags::Type_Object,
		OldPatternPath,
		NewPatternPath
	);
	TestTrue(TEXT("Named pattern redirect registers"),
		FCoreRedirects::AddRedirectList(
			LibraryRedirects,
			LibraryRedirectSource
		));
	FSoftObjectPath LibraryPatternPath(OldPatternPath);
	TestTrue(TEXT("Named library soft path follows asset rename redirect"),
		LibraryPatternPath.FixupCoreRedirects());
	TestEqual(TEXT("Named library redirect resolves the new asset path"),
		LibraryPatternPath, FSoftObjectPath(NewPatternPath));
	TestTrue(TEXT("Named pattern redirect unregisters"),
		FCoreRedirects::RemoveRedirectList(
			LibraryRedirects,
			LibraryRedirectSource
		));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticNamedLibrarySubsystemTest,
	"OpenMobile.Haptics.Pattern.NamedLibrarySubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticNamedLibrarySubsystemTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("Android"));
	Backend.Capabilities.RichHaptics =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.WaveformTiming =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Scheduling =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	UOpenMobileHapticPatternAsset* Pattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	Pattern->SourcePattern.Events.AddDefaulted();
	TArray<FString> PatternErrors;
	TestTrue(TEXT("Subsystem pattern asset builds"),
		Pattern->RebuildDerivedData(PatternErrors));
	UOpenMobileHapticLibrary* Library = NewObject<UOpenMobileHapticLibrary>();
	Library->Patterns = {
		{TEXT("Weapon_Recoil"), Pattern},
		{TEXT("Weapon_Recoil_Alias"), Pattern}
	};
	FOpenMobileHapticNamedLibrarySettings LibrarySettings;
	LibrarySettings.Name = TEXT("Gameplay");
	LibrarySettings.Asset = FSoftObjectPath(Library);
	Settings->NamedLibraries = {LibrarySettings};

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	TestEqual(TEXT("Preparation starts unprepared"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Unprepared);
	TestEqual(TEXT("Configured library starts unprepared"),
		Subsystem->GetNamedPatternStatus(TEXT("Weapon_Recoil")),
		EOpenMobileHapticNamedPatternStatus::Unprepared);
	const FOpenMobileHapticPlaybackResult Unprepared =
		Subsystem->PlayNamedPattern(TEXT("Weapon_Recoil"));
	TestEqual(TEXT("Unprepared configured request is rejected"),
		Unprepared.Error.Code, EOpenMobileHapticErrorCode::NotConfigured);
	TestEqual(TEXT("Unprepared request never reaches the backend"),
		Backend.NamedSubmissionCount, 0);
	TestEqual(TEXT("Unprepared play increments the dropped counter"),
		Subsystem->GetDiagnostics().Performance.DroppedRequestCount,
		static_cast<int64>(1));

	TArray<FString> Errors;
	TestTrue(TEXT("Loaded libraries can complete preparation"),
		Subsystem->PrepareLoadedNamedLibraries({Library}, Errors));
	TestEqual(TEXT("Loaded preparation prewarms the selected backend"),
		Backend.PrepareResourcesCount, 1);
	TestEqual(TEXT("Duplicate resource identities compile only once"),
		Backend.LastPreparationRequest.Patterns.Num(), 1);
	TestTrue(TEXT("Prepared patterns have stable native identities"),
		Backend.LastPreparationRequest.Patterns[0]
		&& Backend.LastPreparationRequest.Patterns[0]->ResourceId != 0);
	TestEqual(TEXT("Successful preparation exposes prepared state"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Prepared);
	const FOpenMobileHapticsPerformanceDiagnostics PreparedPerformance =
		Subsystem->GetDiagnostics().Performance;
	TestEqual(TEXT("Completed preparation is counted"),
		PreparedPerformance.PreparationCount, static_cast<int64>(1));
	TestTrue(TEXT("Preparation latency is nonnegative"),
		PreparedPerformance.LastPreparationLatencyMilliseconds >= 0.0);
	TestTrue(TEXT("Preparation records a timeline cache miss"),
		PreparedPerformance.TimelineCacheMissCount >= 1);
	TestTrue(TEXT("Duplicate prepared identity records a cache hit"),
		PreparedPerformance.TimelineCacheHitCount >= 1);
	Errors.Reset();
	TestTrue(TEXT("Already prepared libraries complete immediately"),
		Subsystem->PrepareLoadedNamedLibraries({Library}, Errors));
	TestEqual(TEXT("Already prepared resources are not compiled twice"),
		Backend.PrepareResourcesCount, 1);
	TestEqual(TEXT("Prepared pattern reports loaded"),
		Subsystem->GetNamedPatternStatus(TEXT("Weapon_Recoil")),
		EOpenMobileHapticNamedPatternStatus::Loaded);
	Backend.SubmissionResolvedPath = TEXT("PortableRich");
	Backend.SubmissionFallbackAttempts = {
		TEXT("ExactOverride:Unsupported"),
		TEXT("PortableRich:Selected")
	};
	const FOpenMobileHapticPlaybackResult Prepared =
		Subsystem->PlayNamedPattern(TEXT("Weapon_Recoil"));
	TestTrue(TEXT("Prepared named request reaches the backend"),
		Prepared.IsAccepted());
	TestEqual(TEXT("Native pattern submission is counted"),
		Subsystem->GetDiagnostics().Performance.NativeSubmissionCount,
		static_cast<int64>(1));
	TestTrue(TEXT("Native submission latency is nonnegative"),
		Subsystem->GetDiagnostics().Performance
			.LastNativeSubmissionLatencyMilliseconds >= 0.0);
	TestEqual(TEXT("Prepared request carries the soft asset path"),
		Backend.LastNamedRequest.PatternAsset,
		FSoftObjectPath(Pattern));
	TestEqual(TEXT("Diagnostics expose the last loaded lookup"),
		Subsystem->GetDiagnostics().LastNamedPatternStatus,
		EOpenMobileHapticNamedPatternStatus::Loaded);
	TestEqual(TEXT("Diagnostics retain the selected fallback path"),
		Subsystem->GetDiagnostics().LastResolvedPath,
		FName(TEXT("PortableRich")));
	TestEqual(TEXT("Diagnostics retain bounded fallback attempts"),
		Subsystem->GetDiagnostics().LastFallbackAttempts.Num(), 2);
	FOpenMobileHapticPlaybackOptions ScheduledOptions;
	ScheduledOptions.Channel = TEXT("ScheduledPreparedAsset");
	ScheduledOptions.Schedule.Mode = EOpenMobileHapticScheduleMode::Relative;
	ScheduledOptions.Schedule.TimeSeconds = 0.15;
	const FOpenMobileHapticPlaybackResult ScheduledPrepared =
		Subsystem->PlayNamedPatternAdvanced(
			TEXT("Weapon_Recoil"),
			1.0f,
			ScheduledOptions
		);
	TestTrue(TEXT("Prepared resources support delayed playback"),
		ScheduledPrepared.IsAccepted());
	const TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> PreparedGuard =
		Backend.LastNamedPlaybackParameters.ScheduledStartGuard;

	Subsystem->ReleaseNamedLibraries();
	TestFalse(TEXT("Prepared-asset release invalidates delayed playback"),
		PreparedGuard->CanStart(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()));
	TestEqual(TEXT("Explicit release reaches native prepared resources"),
		Backend.ReleasePreparedResourcesCount, 1);
	TestEqual(TEXT("Explicit release clears preparation state"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Unprepared);
	TestEqual(TEXT("Release unloads the prepared registry"),
		Subsystem->GetNamedPatternStatus(TEXT("Weapon_Recoil")),
		EOpenMobileHapticNamedPatternStatus::Unprepared);
	const FOpenMobileHapticLibraryPreloadHandle LoadHandle =
		Subsystem->PreloadNamedLibraries();
	const FOpenMobileHapticLibraryPreloadHandle CoalescedHandle =
		Subsystem->PreloadNamedLibraries();
	TestTrue(TEXT("Async preload returns a stable handle"),
		LoadHandle.IsValid());
	TestEqual(TEXT("Concurrent preparation shares one handle"),
		CoalescedHandle, LoadHandle);
	TestEqual(TEXT("Concurrent preparation does not reach native twice"),
		Backend.PrepareResourcesCount, 1);
	TestEqual(TEXT("Async preload exposes preparing state"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Preparing);
	TestEqual(TEXT("Async preload enters loading state"),
		Subsystem->GetNamedPatternStatus(TEXT("Weapon_Recoil")),
		EOpenMobileHapticNamedPatternStatus::Loading);
	const int32 SubmissionsBeforePreparingPlay = Backend.NamedSubmissionCount;
	const FOpenMobileHapticPlaybackResult PreparingPlay =
		Subsystem->PlayNamedPattern(TEXT("Weapon_Recoil"));
	TestEqual(TEXT("Play during preparation is rejected deterministically"),
		PreparingPlay.Error.Code, EOpenMobileHapticErrorCode::NotConfigured);
	TestEqual(TEXT("Play during preparation never submits partial resources"),
		Backend.NamedSubmissionCount, SubmissionsBeforePreparingPlay);
	TestEqual(TEXT("Active async preload can be cancelled"),
		Subsystem->CancelNamedLibraryPreload(LoadHandle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Cancelled preload cannot publish loaded state"),
		Subsystem->GetNamedPatternStatus(TEXT("Weapon_Recoil")),
		EOpenMobileHapticNamedPatternStatus::Unprepared);
	TestEqual(TEXT("Cancellation clears aggregate preparation state"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Unprepared);

	Backend.PreparationResult.State =
		EOpenMobileHapticPreparationState::Failed;
	Backend.PreparationResult.Errors = {TEXT("Injected native prepare failure.")};
	Errors.Reset();
	TestFalse(TEXT("Native preparation failure rejects loaded libraries"),
		Subsystem->PrepareLoadedNamedLibraries({Library}, Errors));
	TestEqual(TEXT("Native failure exposes failed preparation state"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Failed);
	TestEqual(TEXT("Native preparation failure is reported"),
		Errors, Backend.PreparationResult.Errors);

	Settings->NamedLibraries = SavedLibraries;
	Subsystem->Deinitialize();
	TestTrue(TEXT("Teardown releases prepared backend ownership"),
		Backend.ReleasePreparedResourcesCount >= 3);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleAHAPPlaybackPolicyTest,
	"OpenMobile.Haptics.Apple.AHAP.PlaybackPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleAHAPPlaybackPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.AHAP = EOpenMobileHapticSupportState::Supported;
	Capabilities.AudioEvents = EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsAppleAHAPLimits Limits;
	Limits.MaximumFiniteRepeatCount = 4;
	Limits.MaximumDurationSeconds = 5.0;
	Limits.MinimumCompletionDurationSeconds = 0.1;
	TArray<FString> Errors;

	UOpenMobileHapticIOSPatternAsset* Fixed =
		NewObject<UOpenMobileHapticIOSPatternAsset>();
	TestTrue(TEXT("Fixed AHAP fixture builds"), Fixed->SetAHAPSource(
		TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{"
			"\"EventType\":\"HapticTransient\",\"Time\":0}}]}"),
		Errors));
	FOpenMobileHapticsAppleAHAPResolution Resolution =
		FOpenMobileHapticsAppleAHAPPlaybackPolicy::Resolve(
			*Fixed,
			{},
			Capabilities,
			Limits
		);
	TestEqual(TEXT("Fixed AHAP is ready"), Resolution.Outcome,
		EOpenMobileHapticsAppleAHAPOutcome::Ready);
	TestFalse(TEXT("Fixed AHAP selects a standard player"),
		Resolution.Pattern.bRequiresAdvancedPlayer);
	TestEqual(TEXT("Transient AHAP has a bounded completion window"),
		Resolution.Pattern.SafetyDurationSeconds, 0.1);
	FOpenMobileHapticDynamicParameterUpdate RuntimeUpdate;
	RuntimeUpdate.Intensity = 0.8f;
	RuntimeUpdate.bUpdateSharpness = true;
	RuntimeUpdate.Sharpness = 0.7f;
	const FOpenMobileHapticDynamicParameterUpdate ComposedUpdate =
		FOpenMobileHapticsAppleAHAPPlaybackPolicy::ComposeDynamicUpdate(
			RuntimeUpdate,
			0.5f
		);
	TestEqual(TEXT("AHAP runtime intensity retains the request scale"),
		ComposedUpdate.Intensity, 0.4f);
	TestEqual(TEXT("AHAP runtime sharpness remains unchanged"),
		ComposedUpdate.Sharpness, 0.7f);

	UOpenMobileHapticIOSPatternAsset* Controlled =
		NewObject<UOpenMobileHapticIOSPatternAsset>();
	TestTrue(TEXT("Controlled AHAP fixture builds"),
		Controlled->SetAHAPSource(
			TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{"
				"\"EventType\":\"HapticContinuous\",\"Time\":0,"
				"\"Duration\":0.5}},{\"ParameterCurve\":{"
				"\"ParameterID\":\"HapticIntensityControl\",\"Time\":0,"
				"\"ParameterCurveControlPoints\":[{\"Time\":0,"
				"\"ParameterValue\":0.2},{\"Time\":0.5,"
				"\"ParameterValue\":0.8}]}}]}"),
			Errors));
	Resolution = FOpenMobileHapticsAppleAHAPPlaybackPolicy::Resolve(
		*Controlled, {}, Capabilities, Limits);
	TestEqual(TEXT("Controlled AHAP is ready"), Resolution.Outcome,
		EOpenMobileHapticsAppleAHAPOutcome::Ready);
	TestTrue(TEXT("Authored controls select an advanced player"),
		Resolution.Pattern.bRequiresAdvancedPlayer);

	FOpenMobileHapticLoopOptions Loop;
	Loop.bLoop = true;
	Loop.RepeatCount = 2;
	Loop.MaximumDurationSeconds = 2.0;
	Resolution = FOpenMobileHapticsAppleAHAPPlaybackPolicy::Resolve(
		*Controlled, Loop, Capabilities, Limits);
	TestEqual(TEXT("Full-pattern finite AHAP loop is ready"),
		Resolution.Outcome, EOpenMobileHapticsAppleAHAPOutcome::Ready);
	TestTrue(TEXT("Looping selects an advanced player"),
		Resolution.Pattern.bRequiresAdvancedPlayer);
	TestTrue(TEXT("Native player owns the full-pattern loop"),
		Resolution.Pattern.bLoop);
	TestEqual(TEXT("Finite loop safety covers every iteration"),
		Resolution.Pattern.SafetyDurationSeconds, 1.5);
	Loop.RepeatStartTimeSeconds = 0.1;
	Resolution = FOpenMobileHapticsAppleAHAPPlaybackPolicy::Resolve(
		*Controlled, Loop, Capabilities, Limits);
	TestEqual(TEXT("Suffix AHAP loops preserve the portable fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsAppleAHAPOutcome::FallbackRequired);

	UOpenMobileHapticIOSPatternAsset* Audio =
		NewObject<UOpenMobileHapticIOSPatternAsset>();
	TestTrue(TEXT("Synthesized audio AHAP fixture builds"),
		Audio->SetAHAPSource(
			TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{"
				"\"EventType\":\"AudioContinuous\",\"Time\":0,"
				"\"Duration\":0.2}}]}"), Errors));
	Capabilities.AudioEvents = EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsAppleAHAPPlaybackPolicy::Resolve(
		*Audio, {}, Capabilities, Limits);
	TestEqual(TEXT("Unavailable AHAP audio preserves the fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsAppleAHAPOutcome::FallbackRequired);
	Resolution = FOpenMobileHapticsAppleAHAPPlaybackPolicy::Resolve(
		*NewObject<UOpenMobileHapticIOSPatternAsset>(),
		{}, Capabilities, Limits);
	TestEqual(TEXT("Invalid AHAP never reaches native playback"),
		Resolution.Outcome, EOpenMobileHapticsAppleAHAPOutcome::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAHAPNormalizationTest,
	"OpenMobile.Haptics.Apple.AHAP.Normalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAHAPNormalizationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FString Source = TEXT(
		"{\n"
		"  \"Pattern\": [\n"
		"    {\"Event\":{\"Time\":0,\"EventParameters\":["
		"{\"ParameterValue\":0.75,\"ParameterID\":\"HapticIntensity\"}],"
		"\"EventType\":\"HapticTransient\"}},\n"
		"    {\"Parameter\":{\"ParameterValue\":0.6,\"Time\":0.1,"
		"\"ParameterID\":\"HapticIntensityControl\"}},\n"
		"    {\"ParameterCurve\":{\"ParameterCurveControlPoints\":["
		"{\"ParameterValue\":0.25,\"Time\":0},"
		"{\"Time\":0.2,\"ParameterValue\":0.8}],\"Time\":0.2,"
		"\"ParameterID\":\"HapticSharpnessControl\"}},\n"
		"    {\"Event\":{\"Duration\":0.4,"
		"\"EventType\":\"AudioContinuous\",\"Time\":0.3,"
		"\"EventParameters\":[{\"ParameterID\":\"AudioVolume\","
		"\"ParameterValue\":0.5}]}}\n"
		"  ],\n"
		"  \"Version\": 1\n"
		"}"
	);
	const FOpenMobileHapticsAHAPNormalizationResult Result =
		FOpenMobileHapticsAHAPPolicy::Normalize(Source);
	TestTrue(TEXT("Reference AHAP normalizes"), Result.bSuccess);
	TestEqual(TEXT("All pattern entries are retained"),
		Result.Resource.PatternEntryCount, 4);
	TestEqual(TEXT("Haptic event count is retained"),
		Result.Resource.HapticEventCount, 1);
	TestEqual(TEXT("Audio event count is retained"),
		Result.Resource.AudioEventCount, 1);
	TestEqual(TEXT("Dynamic parameter count is retained"),
		Result.Resource.ParameterCount, 1);
	TestEqual(TEXT("Parameter curve count is retained"),
		Result.Resource.ParameterCurveCount, 1);
	TestTrue(TEXT("Controls require the advanced player"),
		Result.Resource.bRequiresAdvancedPlayer);
	TestTrue(TEXT("Audio presence is retained"),
		Result.Resource.bContainsAudioEvents);
	TestTrue(TEXT("Haptic presence is retained"),
		Result.Resource.bContainsHapticEvents);
	TestEqual(TEXT("Pattern duration includes event endings"),
		Result.Resource.DurationSeconds, 0.7);
	TestEqual(TEXT("Normalization is deterministic"),
		FOpenMobileHapticsAHAPPolicy::Normalize(
			Result.Resource.NormalizedJson
		).Resource.NormalizedJson,
		Result.Resource.NormalizedJson);

	auto ExpectError = [this](
		const TCHAR* Label,
		const TCHAR* Json,
		EOpenMobileHapticsAHAPError Expected
	)
	{
		const FOpenMobileHapticsAHAPNormalizationResult Invalid =
			FOpenMobileHapticsAHAPPolicy::Normalize(Json);
		TestFalse(Label, Invalid.bSuccess);
		TestEqual(Label, Invalid.Error, Expected);
	};
	ExpectError(
		TEXT("Malformed JSON is rejected"),
		TEXT("{invalid"),
		EOpenMobileHapticsAHAPError::MalformedJson
	);
	ExpectError(
		TEXT("Missing version is rejected"),
		TEXT("{\"Pattern\":[{\"Event\":{\"EventType\":"
			"\"HapticTransient\",\"Time\":0}}]}"),
		EOpenMobileHapticsAHAPError::MissingVersion
	);
	ExpectError(
		TEXT("Missing pattern is rejected"),
		TEXT("{\"Version\":1}"),
		EOpenMobileHapticsAHAPError::MissingPattern
	);
	ExpectError(
		TEXT("Missing event keys are rejected"),
		TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{\"Time\":0}}]}"),
		EOpenMobileHapticsAHAPError::MissingKey
	);
	ExpectError(
		TEXT("Future versions are rejected"),
		TEXT("{\"Version\":2,\"Pattern\":[{\"Event\":{\"EventType\":"
			"\"HapticTransient\",\"Time\":0}}]}"),
		EOpenMobileHapticsAHAPError::UnsupportedVersion
	);
	ExpectError(
		TEXT("Future keys are rejected"),
		TEXT("{\"Version\":1,\"Future\":true,\"Pattern\":[{\"Event\":{"
			"\"EventType\":\"HapticTransient\",\"Time\":0}}]}"),
		EOpenMobileHapticsAHAPError::UnsupportedKey
	);
	ExpectError(
		TEXT("Nonfinite values are rejected"),
		TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{\"EventType\":"
			"\"HapticTransient\",\"Time\":1e400}}]}"),
		EOpenMobileHapticsAHAPError::Nonfinite
	);
	ExpectError(
		TEXT("Out-of-range parameters are rejected"),
		TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{\"EventType\":"
			"\"HapticTransient\",\"Time\":0,\"EventParameters\":[{"
			"\"ParameterID\":\"HapticIntensity\","
			"\"ParameterValue\":1.1}]}}]}"),
		EOpenMobileHapticsAHAPError::InvalidValue
	);
	ExpectError(
		TEXT("External audio paths are rejected"),
		TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{\"EventType\":"
			"\"AudioCustom\",\"Time\":0,\"EventWaveformPath\":"
			"\"../outside.caf\"}}]}"),
		EOpenMobileHapticsAHAPError::ExternalResourcePath
	);
	FOpenMobileHapticsAHAPLimits Limits;
	Limits.MaximumSourceBytes = 32;
	TestEqual(TEXT("Source byte limits are enforced"),
		FOpenMobileHapticsAHAPPolicy::Normalize(Source, Limits).Error,
		EOpenMobileHapticsAHAPError::SourceTooLarge);
	Limits = {};
	Limits.MaximumPatternEntries = 1;
	TestEqual(TEXT("Pattern entry limits are enforced"),
		FOpenMobileHapticsAHAPPolicy::Normalize(Source, Limits).Error,
		EOpenMobileHapticsAHAPError::LimitExceeded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticPlatformOverrideAssetTest,
	"OpenMobile.Haptics.Pattern.PlatformOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticPlatformOverrideAssetTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticAndroidPatternAsset* Android =
		NewObject<UOpenMobileHapticAndroidPatternAsset>();
	Android->Format = EOpenMobileHapticAndroidPatternFormat::Primitives;
	Android->Primitives = {{
		EOpenMobileHapticAndroidPrimitive::Click,
		0.8f,
		0
	}};
	TArray<FString> Errors;
	TestTrue(TEXT("Valid Android primitive asset validates"),
		Android->Validate(Errors));
	TestTrue(TEXT("Android override cooks only for Android"),
		Android->ShouldCookForPlatform(TEXT("Android")));
	TestFalse(TEXT("Android override is filtered from iOS cooks"),
		Android->ShouldCookForPlatform(TEXT("IOS")));

	UOpenMobileHapticAndroidPatternAsset* InvalidWaveform =
		NewObject<UOpenMobileHapticAndroidPatternAsset>();
	InvalidWaveform->Format =
		EOpenMobileHapticAndroidPatternFormat::Waveform;
	InvalidWaveform->WaveformTimingsMilliseconds = {0, 20};
	InvalidWaveform->WaveformAmplitudes = {255};
	TestFalse(TEXT("Mismatched Android waveform arrays are invalid"),
		InvalidWaveform->Validate(Errors));

	UOpenMobileHapticIOSPatternAsset* IOS =
		NewObject<UOpenMobileHapticIOSPatternAsset>();
	const FString AHAPSource = TEXT(
		"{\"Version\":1.0,\"Pattern\":[{\"Event\":"
		"{\"EventType\":\"HapticTransient\",\"Time\":0}}]}"
	);
	TestTrue(TEXT("Valid AHAP source is accepted"),
		IOS->SetAHAPSource(AHAPSource, Errors));
	TestTrue(TEXT("Valid AHAP asset validates"), IOS->Validate(Errors));
	TestEqual(TEXT("Imported AHAP is stored in normalized form"),
		IOS->GetNormalizedAHAPJson(),
		TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{"
			"\"EventType\":\"HapticTransient\",\"Time\":0}}]}"));
	TestEqual(TEXT("Imported AHAP duration is retained"),
		IOS->GetAHAPDurationSeconds(), 0.0);
	TestFalse(TEXT("Fixed haptic AHAP uses a standard player"),
		IOS->RequiresAdvancedPlayer());
	TestFalse(TEXT("Haptic-only AHAP has no audio"),
		IOS->ContainsAudioEvents());
	TestTrue(TEXT("iOS override cooks only for iOS"),
		IOS->ShouldCookForPlatform(TEXT("IOS")));
	TestFalse(TEXT("iOS override is filtered from Android cooks"),
		IOS->ShouldCookForPlatform(TEXT("Android")));
	UOpenMobileHapticsSettings* MutableSettings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const bool bPackageAHAPResources =
		MutableSettings->IOS.bPackageAHAPResources;
	MutableSettings->IOS.bPackageAHAPResources = false;
	TestFalse(TEXT("Disabled AHAP packaging filters the iOS resource"),
		IOS->ShouldCookForPlatform(TEXT("IOS")));
	MutableSettings->IOS.bPackageAHAPResources = bPackageAHAPResources;
	TestFalse(TEXT("Malformed replacement source is rejected"),
		IOS->SetAHAPSource(TEXT("{invalid"), Errors));
	TestEqual(TEXT("Failed replacement preserves the cooked resource"),
		IOS->GetNormalizedAHAPJson(),
		TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{"
			"\"EventType\":\"HapticTransient\",\"Time\":0}}]}"));

	UOpenMobileHapticPatternAsset* Portable =
		NewObject<UOpenMobileHapticPatternAsset>();
	Portable->SourcePattern.Events.AddDefaulted();
	TestTrue(TEXT("Portable fallback builds"),
		Portable->RebuildDerivedData(Errors));
	Portable->AndroidOverride = Android;
	Portable->IOSOverride = IOS;
	TestEqual(TEXT("Android target resolves only the Android override"),
		Portable->GetOverrideForPlatform(
			EOpenMobileHapticOverridePlatform::Android),
		FSoftObjectPath(Android));
	TestEqual(TEXT("iOS target resolves only the iOS override"),
		Portable->GetOverrideForPlatform(
			EOpenMobileHapticOverridePlatform::IOS),
		FSoftObjectPath(IOS));

	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.Primitives = EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsPlatformOverrideResolution Resolution =
		FOpenMobileHapticsPlatformOverridePolicy::Resolve(
			*Portable,
			EOpenMobileHapticOverridePlatform::Android,
			30,
			Capabilities,
			EOpenMobileHapticFallbackPolicy::Automatic
		);
	TestEqual(TEXT("Supported primitive override is exact"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::ExactOverride);
	TestEqual(TEXT("Exact resolution carries the override path"),
		Resolution.OverrideAsset, FSoftObjectPath(Android));
	Capabilities.PrimitiveSupport = {{
		TEXT("Click"),
		EOpenMobileHapticSupportState::Unsupported
	}};
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		30,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unsupported exact primitive preserves portable fallback"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::PortablePattern);
	Capabilities.PrimitiveSupport.Reset();

	Capabilities.Primitives = EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		30,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unsupported override uses the portable pattern"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::PortablePattern);
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		30,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::ExactOnly
	);
	TestEqual(TEXT("Exact-only rejects an unsupported override"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::Rejected);
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		30,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::NoEffectAllowed
	);
	TestEqual(TEXT("No-effect policy still tries the portable pattern"),
		Resolution.Path,
		EOpenMobileHapticsPlatformOverridePath::PortablePattern);
	Portable->AndroidOverride = nullptr;
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		30,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Missing override uses the portable pattern"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::PortablePattern);
	Portable->AndroidOverride = InvalidWaveform;
	Capabilities.WaveformTiming = EOpenMobileHapticSupportState::Supported;
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		30,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Invalid override uses the portable pattern"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::PortablePattern);
	Portable->AndroidOverride = Android;
	UOpenMobileHapticAndroidPatternAsset* Envelope =
		NewObject<UOpenMobileHapticAndroidPatternAsset>();
	Envelope->Format =
		EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope;
	FOpenMobileHapticAndroidEnvelopePoint EnvelopeStart;
	FOpenMobileHapticAndroidEnvelopePoint EnvelopeEnd;
	EnvelopeStart.TimeSeconds = 0.02f;
	EnvelopeStart.FrequencyHz = 120.0f;
	EnvelopeEnd.TimeSeconds = 0.1f;
	EnvelopeEnd.Amplitude = 0.0f;
	EnvelopeEnd.FrequencyHz = 120.0f;
	Envelope->EnvelopePoints = {EnvelopeStart, EnvelopeEnd};
	TestTrue(TEXT("Valid Android envelope asset validates"),
		Envelope->Validate(Errors));
	Envelope->EnvelopePoints[0].FrequencyHz = 0.0f;
	TestFalse(TEXT("Waveform envelope frequency must be explicit"),
		Envelope->Validate(Errors));
	Envelope->EnvelopePoints[0].FrequencyHz = 120.0f;
	Envelope->Format = EOpenMobileHapticAndroidPatternFormat::BasicEnvelope;
	Envelope->EnvelopePoints[1].Amplitude = 0.1f;
	TestFalse(TEXT("Basic envelopes must end at zero intensity"),
		Envelope->Validate(Errors));
	Envelope->EnvelopePoints[1].Amplitude = 0.0f;
	Envelope->EnvelopePoints[0].TimeSeconds = 0.0f;
	TestFalse(TEXT("Envelope transitions must have positive duration"),
		Envelope->Validate(Errors));
	Envelope->EnvelopePoints[0].TimeSeconds = 0.02f;
	TestTrue(TEXT("Hardware-neutral basic envelope validates"),
		Envelope->Validate(Errors));
	Envelope->Format =
		EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope;
	Portable->AndroidOverride = Envelope;
	Capabilities.Envelopes = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		36,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unsupported envelope preserves portable fallback"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::PortablePattern);
	Capabilities.Envelopes = EOpenMobileHapticSupportState::Supported;
	Capabilities.FrequencyControl = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.MaximumControlPointCount = {true, 16};
	Capabilities.MaximumDurationSeconds = {true, 1.0};
	Capabilities.MinimumTimingGranularitySeconds = {true, 0.02};
	Capabilities.MaximumControlPointDurationSeconds = {true, 0.5};
	Capabilities.FrequencyRange = {true, 60.0f, 200.0f};
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		36,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Direct frequency requires explicit hardware support"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::PortablePattern);
	Capabilities.FrequencyControl = EOpenMobileHapticSupportState::Supported;
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		36,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Fully supported direct frequency override is exact"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::ExactOverride);
	Portable->AndroidOverride = Android;
	Resolution = FOpenMobileHapticsPlatformOverridePolicy::Resolve(
		*Portable,
		EOpenMobileHapticOverridePlatform::Android,
		25,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Older Android versions use portable fallback"),
		Resolution.Path, EOpenMobileHapticsPlatformOverridePath::PortablePattern);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPrimitiveCompositionPolicyTest,
	"OpenMobile.Haptics.Pattern.PrimitiveCompositionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPrimitiveCompositionPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticAndroidPatternAsset* Asset =
		NewObject<UOpenMobileHapticAndroidPatternAsset>();
	Asset->Format = EOpenMobileHapticAndroidPatternFormat::Primitives;
	Asset->Primitives = {
		{EOpenMobileHapticAndroidPrimitive::Click, 0.75f, 0},
		{EOpenMobileHapticAndroidPrimitive::Tick, 0.5f, 25},
		{EOpenMobileHapticAndroidPrimitive::Thud, 0.25f, 40},
		{EOpenMobileHapticAndroidPrimitive::Spin, 1.0f, 10},
		{EOpenMobileHapticAndroidPrimitive::LowTick, 0.4f, 0}
	};
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.Primitives = EOpenMobileHapticSupportState::Supported;
	for (const FName Name : {
		FName(TEXT("Click")),
		FName(TEXT("Tick")),
		FName(TEXT("Thud")),
		FName(TEXT("Spin")),
		FName(TEXT("LowTick"))
	})
	{
		Capabilities.PrimitiveSupport.Emplace(
			Name,
			EOpenMobileHapticSupportState::Supported
		);
	}

	FOpenMobileHapticsPrimitiveCompositionResolution Resolution =
		FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
			*Asset,
			Capabilities,
			30,
			0.8f,
			EOpenMobileHapticFallbackPolicy::Automatic
		);
	TestEqual(TEXT("Fully supported primitives are ready"),
		Resolution.Outcome,
		EOpenMobileHapticsPrimitiveCompositionOutcome::Ready);
	TestEqual(TEXT("Every primitive is preserved"),
		Resolution.Primitives.Num(), 5);
	TestEqual(TEXT("Request intensity scales each primitive once"),
		Resolution.Scales[0], 0.6f);
	TestEqual(TEXT("Delays are preserved"), Resolution.DelaysMilliseconds[2], 40);

	Capabilities.PrimitiveSupport[2].Support =
		EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
		*Asset,
		Capabilities,
		30,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Partial support falls back before composition"),
		Resolution.Outcome,
		EOpenMobileHapticsPrimitiveCompositionOutcome::FallbackRequired);
	Resolution = FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
		*Asset,
		Capabilities,
		30,
		1.0f,
		EOpenMobileHapticFallbackPolicy::ExactOnly
	);
	TestEqual(TEXT("Exact-only partial support rejects"),
		Resolution.Outcome,
		EOpenMobileHapticsPrimitiveCompositionOutcome::Rejected);

	Capabilities.PrimitiveSupport[2].Support =
		EOpenMobileHapticSupportState::Unknown;
	Resolution = FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
		*Asset,
		Capabilities,
		30,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unknown support never reaches native composition"),
		Resolution.Outcome,
		EOpenMobileHapticsPrimitiveCompositionOutcome::FallbackRequired);
	Resolution = FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
		*Asset,
		Capabilities,
		29,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Older Android APIs require fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsPrimitiveCompositionOutcome::FallbackRequired);

	Capabilities.PrimitiveSupport[2].Support =
		EOpenMobileHapticSupportState::Supported;
	Asset->Primitives[0].Scale = -0.1f;
	Resolution = FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
		*Asset,
		Capabilities,
		30,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Invalid scale rejects before JNI"), Resolution.Outcome,
		EOpenMobileHapticsPrimitiveCompositionOutcome::Rejected);
	Asset->Primitives[0].Scale = 0.75f;
	Asset->Primitives[0].DelayMilliseconds = 10001;
	Resolution = FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
		*Asset,
		Capabilities,
		30,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Excessive delays reject before JNI"), Resolution.Outcome,
		EOpenMobileHapticsPrimitiveCompositionOutcome::Rejected);
	Asset->Primitives = {
		{EOpenMobileHapticAndroidPrimitive::Click, 1.0f, 8000},
		{EOpenMobileHapticAndroidPrimitive::Click, 1.0f, 8000},
		{EOpenMobileHapticAndroidPrimitive::Click, 1.0f, 8000},
		{EOpenMobileHapticAndroidPrimitive::Click, 1.0f, 8000}
	};
	Resolution = FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
		*Asset,
		Capabilities,
		30,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Total delay is bounded before JNI"), Resolution.Outcome,
		EOpenMobileHapticsPrimitiveCompositionOutcome::Rejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsEnvelopePolicyTest,
	"OpenMobile.Haptics.Pattern.AndroidEnvelopePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsEnvelopePolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticAndroidPatternAsset* Asset =
		NewObject<UOpenMobileHapticAndroidPatternAsset>();
	Asset->Format = EOpenMobileHapticAndroidPatternFormat::BasicEnvelope;
	FOpenMobileHapticAndroidEnvelopePoint Rise;
	Rise.TimeSeconds = 0.02f;
	Rise.Amplitude = 0.8f;
	Rise.Sharpness = 0.25f;
	FOpenMobileHapticAndroidEnvelopePoint Stop;
	Stop.TimeSeconds = 0.05f;
	Stop.Amplitude = 0.0f;
	Stop.Sharpness = 0.75f;
	Asset->EnvelopePoints = {Rise, Stop};

	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.Envelopes = EOpenMobileHapticSupportState::Supported;
	Capabilities.FrequencyControl = EOpenMobileHapticSupportState::Supported;
	Capabilities.MaximumControlPointCount = {true, 16};
	Capabilities.MaximumDurationSeconds = {true, 1.0};
	Capabilities.MinimumTimingGranularitySeconds = {true, 0.02};
	Capabilities.MaximumControlPointDurationSeconds = {true, 0.5};
	Capabilities.FrequencyRange = {true, 60.0f, 200.0f};

	FOpenMobileHapticsEnvelopeResolution Resolution =
		FOpenMobileHapticsEnvelopePolicy::Resolve(
			*Asset,
			Capabilities,
			36,
			0.5f,
			EOpenMobileHapticFallbackPolicy::Automatic
		);
	TestEqual(TEXT("Basic envelope is ready"), Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::Ready);
	TestEqual(TEXT("Basic envelope keeps its format"), Resolution.Format,
		EOpenMobileHapticAndroidPatternFormat::BasicEnvelope);
	TestEqual(TEXT("Basic intensity is scaled once"),
		Resolution.Amplitudes[0], 0.4f);
	TestEqual(TEXT("Basic ending is exactly zero"),
		Resolution.Amplitudes[1], 0.0f);
	TestEqual(TEXT("Basic sharpness is hardware-neutral"),
		Resolution.ControlValues[0], 0.25f);
	TestEqual(TEXT("First transition starts at zero"),
		Resolution.DurationsMilliseconds[0], 20LL);
	TestEqual(TEXT("Cumulative times become segment durations"),
		Resolution.DurationsMilliseconds[1], 30LL);

	Asset->Format = EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope;
	Asset->EnvelopePoints[0].FrequencyHz = 80.0f;
	Asset->EnvelopePoints[1].FrequencyHz = 120.0f;
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Direct-frequency envelope is ready"), Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::Ready);
	TestEqual(TEXT("Waveform frequency is preserved"),
		Resolution.ControlValues[1], 120.0f);
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		std::numeric_limits<float>::quiet_NaN(),
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Nonfinite request intensity rejects"), Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::Rejected);

	Capabilities.FrequencyControl = EOpenMobileHapticSupportState::Unknown;
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unknown frequency control requires fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);
	Capabilities.FrequencyControl = EOpenMobileHapticSupportState::Supported;

	Capabilities.FrequencyRange = {};
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unknown frequency range requires fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);
	Capabilities.FrequencyRange = {true, 60.0f, 200.0f};

	Capabilities.MaximumControlPointCount = {true, 1};
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Hardware point-count limit requires fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);
	Capabilities.MaximumControlPointCount = {true, 16};

	Asset->EnvelopePoints[1].FrequencyHz = 220.0f;
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Out-of-range frequency falls back"), Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::ExactOnly
	);
	TestEqual(TEXT("Exact frequency rejects unsupported hardware"),
		Resolution.Outcome, EOpenMobileHapticsEnvelopeOutcome::Rejected);
	Asset->EnvelopePoints[1].FrequencyHz = 120.0f;

	Capabilities.Envelopes = EOpenMobileHapticSupportState::Unknown;
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unknown support never reaches the builder"),
		Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);
	Capabilities.Envelopes = EOpenMobileHapticSupportState::Supported;

	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		35,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Older APIs require fallback"), Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);

	Capabilities.MinimumTimingGranularitySeconds = {};
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unknown timing limits require fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);
	Capabilities.MinimumTimingGranularitySeconds = {true, 0.02};

	Asset->EnvelopePoints[0].TimeSeconds = 0.01f;
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Too-short segment requires fallback"), Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);
	Asset->EnvelopePoints[0].TimeSeconds = 0.02f;

	Capabilities.MaximumControlPointDurationSeconds = {true, 0.025};
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Too-long segment requires fallback"), Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);
	Capabilities.MaximumControlPointDurationSeconds = {true, 0.5};

	Capabilities.MaximumDurationSeconds = {true, 0.04};
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Total duration limit requires fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::FallbackRequired);
	Capabilities.MaximumDurationSeconds = {true, 1.0};

	Asset->Format = EOpenMobileHapticAndroidPatternFormat::BasicEnvelope;
	Asset->EnvelopePoints[1].Amplitude = 0.1f;
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Invalid endpoint rejects before native code"),
		Resolution.Outcome, EOpenMobileHapticsEnvelopeOutcome::Rejected);
	Asset->EnvelopePoints[1].Amplitude = 0.0f;
	Asset->Format = static_cast<EOpenMobileHapticAndroidPatternFormat>(255);
	Resolution = FOpenMobileHapticsEnvelopePolicy::Resolve(
		*Asset,
		Capabilities,
		36,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Future format values reject safely"), Resolution.Outcome,
		EOpenMobileHapticsEnvelopeOutcome::Rejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAndroidConfigurationPolicyTest,
	"OpenMobile.Haptics.Android.ConfigurationPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAndroidConfigurationPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.Availability = EOpenMobileHapticAvailability::RichHaptics;
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	Capabilities.SemanticFeedback = EOpenMobileHapticSupportState::Supported;
	Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
	Capabilities.AmplitudeControl = EOpenMobileHapticSupportState::Supported;
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Supported;
	Capabilities.WaveformTiming = EOpenMobileHapticSupportState::Supported;
	Capabilities.Looping = EOpenMobileHapticSupportState::Supported;
	Capabilities.Primitives = EOpenMobileHapticSupportState::Supported;
	Capabilities.Envelopes = EOpenMobileHapticSupportState::Supported;
	Capabilities.FrequencyControl = EOpenMobileHapticSupportState::Supported;
	Capabilities.TransientEvents = EOpenMobileHapticSupportState::Supported;
	Capabilities.ContinuousEvents = EOpenMobileHapticSupportState::Supported;
	Capabilities.BackgroundAlerts = EOpenMobileHapticSupportState::Supported;
	Capabilities.PresetSupport = {{
		TEXT("Click"), EOpenMobileHapticSupportState::Supported
	}};
	Capabilities.PrimitiveSupport = {{
		TEXT("Tick"), EOpenMobileHapticSupportState::Supported
	}};
	Capabilities.MaximumControlPointCount = {true, 16};
	Capabilities.MaximumDurationSeconds = {true, 1.0};
	Capabilities.MinimumTimingGranularitySeconds = {true, 0.01};
	Capabilities.MaximumControlPointDurationSeconds = {true, 0.1};
	Capabilities.FrequencyRange = {true, 40.0f, 200.0f};

	FOpenMobileHapticsAndroidConfigurationPolicy::ApplyCapabilityMask(
		false,
		Capabilities
	);
	TestEqual(TEXT("Semantic view feedback remains available"),
		Capabilities.SemanticEffects,
		EOpenMobileHapticSupportState::Supported);
	TestEqual(TEXT("Semantic-only builds report semantic availability"),
		Capabilities.Availability,
		EOpenMobileHapticAvailability::SemanticFeedback);
	for (const EOpenMobileHapticSupportState Support : {
		Capabilities.BasicVibration,
		Capabilities.RichHaptics,
		Capabilities.AmplitudeControl,
		Capabilities.PredefinedEffects,
		Capabilities.WaveformTiming,
		Capabilities.Looping,
		Capabilities.Primitives,
		Capabilities.Envelopes,
		Capabilities.FrequencyControl,
		Capabilities.TransientEvents,
		Capabilities.ContinuousEvents,
		Capabilities.BackgroundAlerts
	})
	{
		TestEqual(TEXT("Custom vibration capability is masked"),
			Support, EOpenMobileHapticSupportState::Unsupported);
	}
	TestEqual(TEXT("Named preset support is masked"),
		Capabilities.PresetSupport[0].Support,
		EOpenMobileHapticSupportState::Unsupported);
	TestEqual(TEXT("Named primitive support is masked"),
		Capabilities.PrimitiveSupport[0].Support,
		EOpenMobileHapticSupportState::Unsupported);
	TestFalse(TEXT("Custom native limits are hidden"),
		Capabilities.MaximumControlPointCount.bKnown
		|| Capabilities.MaximumDurationSeconds.bKnown
		|| Capabilities.MinimumTimingGranularitySeconds.bKnown
		|| Capabilities.MaximumControlPointDurationSeconds.bKnown
		|| Capabilities.FrequencyRange.bKnown);
	TestTrue(TEXT("Capability detail explains the build configuration"),
		Capabilities.Detail.Contains(TEXT("not packaged")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAndroidWaveformPolicyTest,
	"OpenMobile.Haptics.Pattern.AndroidWaveformPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAndroidWaveformPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticAndroidPatternAsset* Asset =
		NewObject<UOpenMobileHapticAndroidPatternAsset>();
	Asset->Format = EOpenMobileHapticAndroidPatternFormat::Waveform;
	Asset->WaveformTimingsMilliseconds = {0, 10, 20, 30};
	Asset->WaveformAmplitudes = {0, 255, 128, 64};
	Asset->WaveformRepeatIndex = 1;
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.WaveformTiming = EOpenMobileHapticSupportState::Supported;
	Capabilities.AmplitudeControl = EOpenMobileHapticSupportState::Supported;

	FOpenMobileHapticsAndroidWaveformResolution Resolution =
		FOpenMobileHapticsAndroidWaveformPolicy::ResolveOverride(
			*Asset,
			Capabilities,
			26,
			0.5f,
			EOpenMobileHapticFallbackPolicy::Automatic
		);
	TestEqual(TEXT("Supported waveform is ready"), Resolution.Outcome,
		EOpenMobileHapticsAndroidWaveformOutcome::Ready);
	TestEqual(TEXT("Every timing pair is preserved"),
		Resolution.TimingsMilliseconds,
		TArray<int64>({0, 10, 20, 30}));
	TestEqual(TEXT("Silence remains zero amplitude"),
		Resolution.Amplitudes[0], 0);
	TestEqual(TEXT("Active amplitudes are scaled once"),
		Resolution.Amplitudes[1], 128);
	TestEqual(TEXT("Repeat index is preserved"), Resolution.RepeatIndex, 1);

	Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolveOverride(
		*Asset,
		Capabilities,
		26,
		0.5f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Basic hardware keeps silence explicit"),
		Resolution.Amplitudes[0], 0);
	TestEqual(TEXT("Basic hardware uses default amplitude for active pairs"),
		Resolution.Amplitudes[1], -1);
	TestTrue(TEXT("Default amplitude use is reported"),
		Resolution.bUsesDefaultAmplitude);

	Capabilities.WaveformTiming = EOpenMobileHapticSupportState::Unknown;
	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolveOverride(
		*Asset,
		Capabilities,
		26,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unknown waveform support requires fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidWaveformOutcome::FallbackRequired);
	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolveOverride(
		*Asset,
		Capabilities,
		26,
		1.0f,
		EOpenMobileHapticFallbackPolicy::ExactOnly
	);
	TestEqual(TEXT("Exact-only rejects unknown support"), Resolution.Outcome,
		EOpenMobileHapticsAndroidWaveformOutcome::Rejected);
	Capabilities.WaveformTiming = EOpenMobileHapticSupportState::Supported;

	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolveOverride(
		*Asset,
		Capabilities,
		25,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Android 25 requires fallback"), Resolution.Outcome,
		EOpenMobileHapticsAndroidWaveformOutcome::FallbackRequired);
	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolveOverride(
		*Asset,
		Capabilities,
		26,
		std::numeric_limits<float>::quiet_NaN(),
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Nonfinite intensity rejects before JNI"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidWaveformOutcome::Rejected);

#if WITH_EDITORONLY_DATA
	UOpenMobileHapticPatternAsset* Portable =
		NewObject<UOpenMobileHapticPatternAsset>();
	FOpenMobileHapticPatternEvent Continuous;
	Continuous.Type = EOpenMobileHapticPatternEventType::Continuous;
	Continuous.StartTimeSeconds = 0.01;
	Continuous.DurationSeconds = 0.02;
	Continuous.Intensity = 0.8f;
	FOpenMobileHapticPatternEvent Silence;
	Silence.Type = EOpenMobileHapticPatternEventType::Silence;
	Silence.StartTimeSeconds = 0.03;
	Silence.DurationSeconds = 0.01;
	FOpenMobileHapticPatternEvent Transient;
	Transient.Type = EOpenMobileHapticPatternEventType::Transient;
	Transient.StartTimeSeconds = 0.05;
	Portable->SourcePattern.Events = {Continuous, Silence, Transient};
	Portable->Loop.bLoop = true;
	Portable->Loop.RepeatCount = 0;
	Portable->Loop.RepeatStartTimeSeconds = 0.03;
	Portable->Loop.MaximumDurationSeconds = 1.0;
	TArray<FString> Errors;
	TestTrue(TEXT("Portable waveform source builds"),
		Portable->RebuildDerivedData(Errors));

	Capabilities.AmplitudeControl = EOpenMobileHapticSupportState::Supported;
	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolvePortable(
		*Portable,
		Capabilities,
		0.5f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Portable pattern is ready"), Resolution.Outcome,
		EOpenMobileHapticsAndroidWaveformOutcome::Ready);
	TestEqual(TEXT("Portable gaps and silence become explicit timing pairs"),
		Resolution.TimingsMilliseconds,
		TArray<int64>({10, 20, 20, 1}));
	TestEqual(TEXT("Portable active and silent amplitudes are preserved"),
		Resolution.Amplitudes,
		TArray<int32>({0, 102, 0, 128}));
	TestEqual(TEXT("Portable repeat start becomes an exact pair index"),
		Resolution.RepeatIndex, 2);

	Portable->Loop.RepeatCount = 1;
	Errors.Reset();
	TestTrue(TEXT("Finite repeat source rebuilds"),
		Portable->RebuildDerivedData(Errors));
	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolvePortable(
		*Portable,
		Capabilities,
		0.5f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Finite repeats are unrolled once"),
		Resolution.TimingsMilliseconds,
		TArray<int64>({10, 20, 20, 1, 20, 1}));
	TestEqual(TEXT("Unrolled repeats preserve every amplitude"),
		Resolution.Amplitudes,
		TArray<int32>({0, 102, 0, 128, 0, 128}));
	TestEqual(TEXT("Finite native waveforms do not keep a repeat index"),
		Resolution.RepeatIndex, INDEX_NONE);

	Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolvePortable(
		*Portable,
		Capabilities,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Portable silence stays off on basic hardware"),
		Resolution.Amplitudes[0], 0);
	TestEqual(TEXT("Portable activity uses default amplitude"),
		Resolution.Amplitudes[1], -1);

	FOpenMobileHapticParameterCurve PortableCurve;
	PortableCurve.Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;
	PortableCurve.StartTimeSeconds = 0.01;
	PortableCurve.ControlPoints = {{0.0, 1.0f}, {0.02, 0.25f}};
	Portable->SourcePattern.ParameterCurves.Add(PortableCurve);
	Errors.Reset();
	TestTrue(TEXT("Portable curve source rebuilds"),
		Portable->RebuildDerivedData(Errors));
	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolvePortable(
		*Portable,
		Capabilities,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Android waveforms never ignore portable curves"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidWaveformOutcome::FallbackRequired);
	TestEqual(TEXT("Curve fallback is diagnostic"), Resolution.Reason,
		FName(TEXT("ParameterCurves")));

	Portable->FallbackPolicy = EOpenMobileHapticFallbackPolicy::ExactOnly;
	Resolution = FOpenMobileHapticsAndroidWaveformPolicy::ResolvePortable(
		*Portable,
		Capabilities,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Asset exact-only policy blocks portable translation"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidWaveformOutcome::Rejected);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAndroidFallbackPolicyTest,
	"OpenMobile.Haptics.Pattern.AndroidEnvelopeFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAndroidFallbackPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticPatternAsset* Pattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	Pattern->PrimitiveOrPresetFallback = TEXT("Click");
	Pattern->LowestAllowedFallback =
		EOpenMobileHapticFallbackFloor::PrimitiveOrPredefined;
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.Primitives = EOpenMobileHapticSupportState::Supported;
	Capabilities.PrimitiveSupport = {{
		TEXT("Click"),
		EOpenMobileHapticSupportState::Supported
	}};

	FOpenMobileHapticsAndroidFallbackResolution Resolution =
		FOpenMobileHapticsAndroidFallbackPolicy::ResolvePrimitive(
			*Pattern,
			Capabilities,
			EOpenMobileHapticFallbackPolicy::Automatic
		);
	TestEqual(TEXT("Declared primitive is the first native fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::Primitive);
	TestEqual(TEXT("Stable primitive intent is resolved"),
		Resolution.Primitive, EOpenMobileHapticAndroidPrimitive::Click);

	Pattern->LowestAllowedFallback =
		EOpenMobileHapticFallbackFloor::BasicVibration;
	Pattern->PrimitiveOrPresetFallback = TEXT("HeavyClick");
	Capabilities.Primitives = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Supported;
	Capabilities.PresetSupport = {{
		TEXT("HeavyClick"),
		EOpenMobileHapticSupportState::Supported
	}};
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::Resolve(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Supported declared preset follows primitives"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::Predefined);
	TestEqual(TEXT("Stable preset intent is resolved"),
		Resolution.PredefinedEffect,
		EOpenMobileHapticAndroidPredefinedEffect::HeavyClick);
	TestEqual(TEXT("Selection semantics map to the tick preset"),
		FOpenMobileHapticsAndroidFallbackPolicy::PredefinedForSemantic(
			EOpenMobileHapticsSemanticBehavior::Selection
		),
		EOpenMobileHapticAndroidPredefinedEffect::Tick);
	TestEqual(TEXT("Success semantics map to the double-click preset"),
		FOpenMobileHapticsAndroidFallbackPolicy::PredefinedForSemantic(
			EOpenMobileHapticsSemanticBehavior::NotificationSuccess
		),
		EOpenMobileHapticAndroidPredefinedEffect::DoubleClick);
	TestTrue(TEXT("Detailed preset support accepts the exact preset"),
		FOpenMobileHapticsAndroidFallbackPolicy::SupportsPredefined(
			EOpenMobileHapticAndroidPredefinedEffect::HeavyClick,
			Capabilities
		));
	TestFalse(TEXT("Aggregate preset support cannot substitute another preset"),
		FOpenMobileHapticsAndroidFallbackPolicy::SupportsPredefined(
			EOpenMobileHapticAndroidPredefinedEffect::Click,
			Capabilities
		));

	Capabilities.PresetSupport[0].Support =
		EOpenMobileHapticSupportState::Unsupported;
	Pattern->bAllowSemanticFallback = true;
	Pattern->SemanticFallback = EOpenMobileHapticSemanticEffect::ImpactHeavy;
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::Resolve(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unavailable preset follows the asset semantic policy"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::Semantic);
	TestEqual(TEXT("Semantic fallback meaning is preserved"),
		Resolution.SemanticEffect,
		EOpenMobileHapticSemanticEffect::ImpactHeavy);

	Pattern->bAllowSemanticFallback = false;
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::Resolve(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unavailable preset can fall back to basic vibration"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::BasicVibration);
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::Resolve(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::NoEffectAllowed
	);
	TestEqual(TEXT("Permitted no-effect follows every unavailable native path"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::NoEffect);

	Pattern->PrimitiveOrPresetFallback = TEXT("Click");
	Capabilities.Primitives = EOpenMobileHapticSupportState::Supported;
	Capabilities.PrimitiveSupport[0].Support =
		EOpenMobileHapticSupportState::Supported;
	Capabilities.PresetSupport = {{
		TEXT("Click"),
		EOpenMobileHapticSupportState::Supported
	}};
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::Resolve(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Supported primitive precedes an equivalent preset"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::Primitive);
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::Resolve(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic,
		false,
		true
	);
	TestEqual(TEXT("A stale primitive capability can retry the preset"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::Predefined);

	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::ResolvePrimitive(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::NoEffectAllowed
	);
	TestEqual(TEXT("Primitive precedes permitted no-effect"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::Primitive);

	Capabilities.PrimitiveSupport[0].Support =
		EOpenMobileHapticSupportState::Unknown;
	Capabilities.PresetSupport[0].Support =
		EOpenMobileHapticSupportState::Unknown;
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::ResolvePrimitive(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::NoEffectAllowed
	);
	TestEqual(TEXT("Unknown primitive and preset support select no-effect"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::NoEffect);

	Capabilities.PrimitiveSupport[0].Support =
		EOpenMobileHapticSupportState::Supported;
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::ResolvePrimitive(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::ExactOnly
	);
	TestEqual(TEXT("Exact-only rejects before fallback"), Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::Rejected);

	Pattern->FallbackPolicy = EOpenMobileHapticFallbackPolicy::ExactOnly;
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::ResolvePrimitive(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Asset exact-only policy cannot be weakened"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::Rejected);
	Pattern->FallbackPolicy = EOpenMobileHapticFallbackPolicy::Automatic;
	Pattern->LowestAllowedFallback = EOpenMobileHapticFallbackFloor::PortableRich;
	Resolution = FOpenMobileHapticsAndroidFallbackPolicy::ResolvePrimitive(
		*Pattern,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Asset fallback floor blocks primitive degradation"),
		Resolution.Outcome,
		EOpenMobileHapticsAndroidFallbackOutcome::Rejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsFallbackLadderTest,
	"OpenMobile.Haptics.Pattern.FallbackLadder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsFallbackLadderTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticPatternAsset* Pattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	Pattern->SourcePattern.Events.AddDefaulted();
	TArray<FString> Errors;
	TestTrue(TEXT("Fallback pattern builds"),
		Pattern->RebuildDerivedData(Errors));
	Pattern->LowestAllowedFallback =
		EOpenMobileHapticFallbackFloor::BasicVibration;
	Pattern->PrimitiveOrPresetFallback = TEXT("Click");
	Pattern->bAllowSemanticFallback = true;
	Pattern->SemanticFallback = EOpenMobileHapticSemanticEffect::Click;

	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
	Capabilities.TransientEvents = EOpenMobileHapticSupportState::Supported;
	Capabilities.Primitives = EOpenMobileHapticSupportState::Supported;
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsPlatformOverrideResolution Override;
	Override.Path = EOpenMobileHapticsPlatformOverridePath::ExactOverride;

	FOpenMobileHapticsFallbackResolution Resolution =
		FOpenMobileHapticsFallbackPolicy::Resolve(
			*Pattern,
			Override,
			Capabilities,
			EOpenMobileHapticFallbackPolicy::Automatic
		);
	TestEqual(TEXT("Exact rich override is first"), Resolution.Path,
		EOpenMobileHapticsFallbackPath::ExactOverride);
	TestEqual(TEXT("Exact selection records one trace entry"),
		Resolution.Attempts.Num(), 1);

	Override.Path = EOpenMobileHapticsPlatformOverridePath::PortablePattern;
	Override.Reason = TEXT("Capability");
	Resolution = FOpenMobileHapticsFallbackPolicy::Resolve(
		*Pattern,
		Override,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Portable rich translation is second"), Resolution.Path,
		EOpenMobileHapticsFallbackPath::PortableRich);
	Capabilities.RichHaptics = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.TransientEvents = EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsFallbackPolicy::Resolve(
		*Pattern,
		Override,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Declared primitive or preset is third"), Resolution.Path,
		EOpenMobileHapticsFallbackPath::PrimitiveOrPredefined);
	Capabilities.Primitives = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsFallbackPolicy::Resolve(
		*Pattern,
		Override,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Declared semantic meaning is fourth"), Resolution.Path,
		EOpenMobileHapticsFallbackPath::Semantic);
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsFallbackPolicy::Resolve(
		*Pattern,
		Override,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Basic vibration is fifth"), Resolution.Path,
		EOpenMobileHapticsFallbackPath::BasicVibration);
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsFallbackPolicy::Resolve(
		*Pattern,
		Override,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::NoEffectAllowed
	);
	TestEqual(TEXT("Permitted no effect is the terminal fallback"),
		Resolution.Path, EOpenMobileHapticsFallbackPath::NoEffect);
	TestTrue(TEXT("No-effect fallback is not a failure"),
		Resolution.bSuccessfulOutcome);
	TestEqual(TEXT("Every skipped and selected rung is traced"),
		Resolution.Attempts.Num(), 6);
	const TArray<FName> DiagnosticTrace =
		FOpenMobileHapticsFallbackPolicy::MakeDiagnosticTrace(Resolution);
	TestEqual(TEXT("Every ladder attempt has a diagnostic entry"),
		DiagnosticTrace.Num(), Resolution.Attempts.Num());

	Resolution = FOpenMobileHapticsFallbackPolicy::Resolve(
		*Pattern,
		Override,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unavailable automatic ladder rejects"), Resolution.Path,
		EOpenMobileHapticsFallbackPath::Rejected);
	Pattern->LowestAllowedFallback = EOpenMobileHapticFallbackFloor::Semantic;
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	Resolution = FOpenMobileHapticsFallbackPolicy::Resolve(
		*Pattern,
		Override,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Asset floor blocks misleading basic vibration"),
		Resolution.Path, EOpenMobileHapticsFallbackPath::Rejected);
	Pattern->LowestAllowedFallback =
		EOpenMobileHapticFallbackFloor::BasicVibration;
	Capabilities.Primitives = EOpenMobileHapticSupportState::Supported;
	Capabilities.PrimitiveSupport = {{
		TEXT("Click"),
		EOpenMobileHapticSupportState::Unsupported
	}};
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsFallbackPolicy::Resolve(
		*Pattern,
		Override,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::Automatic
	);
	TestEqual(TEXT("Unsupported declared primitive falls through safely"),
		Resolution.Path, EOpenMobileHapticsFallbackPath::BasicVibration);
	Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
	Capabilities.TransientEvents = EOpenMobileHapticSupportState::Supported;
	Resolution = FOpenMobileHapticsFallbackPolicy::Resolve(
		*Pattern,
		Override,
		Capabilities,
		EOpenMobileHapticFallbackPolicy::ExactOnly
	);
	TestEqual(TEXT("Exact-only rejects portable rich translation"),
		Resolution.Path, EOpenMobileHapticsFallbackPath::Rejected);
	Pattern->LowestAllowedFallback =
		static_cast<EOpenMobileHapticFallbackFloor>(MAX_uint8);
	TestFalse(TEXT("Invalid fallback floor fails asset rebuild"),
		Pattern->RebuildDerivedData(Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsStarterPresetPackTest,
	"OpenMobile.Haptics.Pattern.StarterPresetPack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsStarterPresetPackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FSoftObjectPath LibraryPath(TEXT(
		"/OpenMobileHaptics/StarterPresets/OpenMobileStarterHaptics."
		"OpenMobileStarterHaptics"
	));
	UOpenMobileHapticLibrary* Library = Cast<UOpenMobileHapticLibrary>(
		LibraryPath.TryLoad()
	);
	TestNotNull(TEXT("The built-in starter library is packaged"), Library);
	if (!Library)
	{
		return false;
	}
	TestEqual(TEXT("The starter pack has a stable version"),
		Library->LibraryVersion, 1);
	TestTrue(TEXT("The starter pack remains small"),
		Library->Patterns.Num() >= 5 && Library->Patterns.Num() <= 8);

	const TSet<FName> ExpectedNames = {
		TEXT("OpenMobile.UI.Selection"),
		TEXT("OpenMobile.UI.Confirm"),
		TEXT("OpenMobile.Combat.Impact"),
		TEXT("OpenMobile.Vehicle.Bump"),
		TEXT("OpenMobile.Reward.Success"),
		TEXT("OpenMobile.Notification.Warning")
	};
	TSet<FName> ActualNames;
	TSet<FName> ActualCategories;
	bool bHasFrequencyWarning = false;
	bool bHasAccessibilityWarning = false;
	for (const FOpenMobileHapticLibraryEntry& Entry : Library->Patterns)
	{
		ActualNames.Add(Entry.Name);
		UOpenMobileHapticPatternAsset* Pattern = Entry.Pattern.LoadSynchronous();
		TestNotNull(TEXT("Every starter entry resolves to a pattern"), Pattern);
		if (!Pattern)
		{
			continue;
		}
		ActualCategories.Add(Pattern->DefaultCategory);
		TestEqual(TEXT("Every starter pattern uses format version one"),
			Pattern->PatternVersion, 1);
		TestTrue(TEXT("Starter derived data is current"),
			Pattern->IsDerivedDataCurrent());
		TestTrue(TEXT("Starter patterns are safe to cook"),
			!Pattern->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly));
		TestTrue(TEXT("Starter patterns are foreground only"),
			!Pattern->bSuitableForBackgroundPlayback);
		bHasFrequencyWarning |= !Pattern->bSuitableForFrequentRepetition;
		bHasAccessibilityWarning |=
			!Pattern->bSuitableForAccessibilitySensitiveUse;
		const FOpenMobileHapticCookedPatternData& Cooked =
			Pattern->GetCookedPattern();
		TestTrue(TEXT("Starter patterns have bounded duration"),
			Cooked.DurationMicroseconds <= 400000);
		for (const FOpenMobileHapticCookedPatternEvent& Event : Cooked.Events)
		{
			TestTrue(TEXT("Starter intensity remains conservative"),
				Event.Intensity <= static_cast<uint16>(0.8f * MAX_uint16));
		}
#if WITH_EDITOR
		FDataValidationContext Context;
		TestEqual(TEXT("Starter asset validation succeeds"),
			Pattern->IsDataValid(Context), EDataValidationResult::Valid);
#endif
	}
	TestEqual(TEXT("Stable starter names do not drift"),
		ActualNames.Num(), ExpectedNames.Num());
	for (const FName ExpectedName : ExpectedNames)
	{
		TestTrue(TEXT("Every stable starter name is present"),
			ActualNames.Contains(ExpectedName));
	}
	TestTrue(TEXT("The pack covers UI"), ActualCategories.Contains(TEXT("UI")));
	TestTrue(TEXT("The pack covers combat and vehicle gameplay"),
		ActualCategories.Contains(TEXT("Gameplay")));
	TestTrue(TEXT("The pack covers rewards and notifications"),
		ActualCategories.Contains(TEXT("Alerts")));
	TestTrue(TEXT("Frequent repetition cautions are present"),
		bHasFrequencyWarning);
	TestTrue(TEXT("Accessibility cautions are present"),
		bHasAccessibilityWarning);
	UOpenMobileHapticLibrary* InvalidLibrary = DuplicateObject(
		Library,
		GetTransientPackage()
	);
	InvalidLibrary->LibraryVersion = 0;
	TMap<FName, FSoftObjectPath> InvalidLookup;
	TArray<FString> ValidationErrors;
	TestFalse(TEXT("Invalid pack versions fail validation"),
		InvalidLibrary->BuildPatternLookup(InvalidLookup, ValidationErrors));
	UOpenMobileHapticPatternAsset* InvalidPattern = DuplicateObject(
		Library->Patterns[0].Pattern.LoadSynchronous(),
		GetTransientPackage()
	);
	InvalidPattern->PatternVersion = 0;
	TestFalse(TEXT("Invalid pattern versions fail validation"),
		InvalidPattern->RebuildDerivedData(ValidationErrors));

	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	TestTrue(TEXT("The starter library is configured by default"),
		Settings->NamedLibraries.ContainsByPredicate(
			[&LibraryPath](const FOpenMobileHapticNamedLibrarySettings& Entry)
			{
				return Entry.Name == TEXT("OpenMobileStarter")
					&& Entry.Asset == LibraryPath;
			}
		));
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsOneShotPolicyTest,
	"OpenMobile.Haptics.OneShot.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsOneShotPolicyTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Supported;
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	TestEqual(TEXT("Short pulse prefers semantic impact feedback"),
		FOpenMobileHapticsOneShotPolicy::Resolve(Capabilities, 0.03).Path,
		EOpenMobileHapticsOneShotPath::SystemSemantic);
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Unsupported;
	TestEqual(TEXT("Short pulse falls back to a predefined effect"),
		FOpenMobileHapticsOneShotPolicy::Resolve(Capabilities, 0.03).Path,
		EOpenMobileHapticsOneShotPath::PredefinedEffect);
	Capabilities.PresetSupport = {{
		TEXT("Click"),
		EOpenMobileHapticSupportState::Unsupported
	}};
	TestEqual(TEXT("Unsupported Click never uses another supported preset"),
		FOpenMobileHapticsOneShotPolicy::Resolve(Capabilities, 0.03).Path,
		EOpenMobileHapticsOneShotPath::BasicVibration);
	Capabilities.PresetSupport[0].Support =
		EOpenMobileHapticSupportState::Unknown;
	TestEqual(TEXT("Unknown Click support uses a deterministic basic pulse"),
		FOpenMobileHapticsOneShotPolicy::Resolve(Capabilities, 0.03).Path,
		EOpenMobileHapticsOneShotPath::BasicVibration);
	Capabilities.PresetSupport[0].Support =
		EOpenMobileHapticSupportState::Supported;
	TestEqual(TEXT("Detailed Click support selects the preset"),
		FOpenMobileHapticsOneShotPolicy::Resolve(Capabilities, 0.03).Path,
		EOpenMobileHapticsOneShotPath::PredefinedEffect);
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Unsupported;
	TestEqual(TEXT("Basic vibration is the final pulse path"),
		FOpenMobileHapticsOneShotPolicy::Resolve(Capabilities, 0.2).Path,
		EOpenMobileHapticsOneShotPath::BasicVibration);
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Unsupported;
	TestEqual(TEXT("Missing pulse support is explicit"),
		FOpenMobileHapticsOneShotPolicy::Resolve(Capabilities, 0.2).Path,
		EOpenMobileHapticsOneShotPath::Unsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsOneShotSubmissionTest,
	"OpenMobile.Haptics.OneShot.Submission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsOneShotSubmissionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("OneShotMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::BasicVibration;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	TestEqual(TEXT("One-shot minimum is conservative"),
		Settings->MinimumOneShotDurationSeconds, 0.001f);
	TestEqual(TEXT("One-shot maximum requires longer-pattern APIs"),
		Settings->MaximumOneShotDurationSeconds, 1.0f);
	TestEqual(TEXT("Pattern events have a conservative duration bound"),
		Settings->MaximumPatternEventDurationSeconds, 10.0f);

	const int32 InitialSubmissionCount = Backend.OneShotSubmissionCount;
	TestEqual(TEXT("Zero duration is silent"),
		Subsystem->Vibrate(0.0f, 1.0f, TEXT("ZeroDuration")).Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Zero intensity is silent"),
		Subsystem->Vibrate(0.05f, 0.0f, TEXT("ZeroIntensity")).Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	for (const float InvalidDuration : {
		-0.01f,
		0.0005f,
		1.01f,
		std::numeric_limits<float>::quiet_NaN()
	})
	{
		const FOpenMobileHapticPlaybackResult Invalid =
			Subsystem->Vibrate(
				InvalidDuration,
				1.0f,
				TEXT("InvalidDuration")
			);
		TestEqual(TEXT("Invalid one-shot duration is rejected"),
			Invalid.Error.Code, EOpenMobileHapticErrorCode::InvalidRequest);
	}
	TestEqual(TEXT("Silent and invalid pulses do not reach the backend"),
		Backend.OneShotSubmissionCount, InitialSubmissionCount);

	FOpenMobileHapticUserPolicy Policy;
	Policy.bEnabled = false;
	Subsystem->SetUserPolicy(Policy);
	TestEqual(TEXT("Disabled policy suppresses one-shot vibration"),
		Subsystem->Vibrate(0.05f, 1.0f, TEXT("DisabledPulse")).Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	Policy.bEnabled = true;
	Subsystem->SetUserPolicy(Policy);

	const FOpenMobileHapticPlaybackResult Minimum = Subsystem->Vibrate(
		Settings->MinimumOneShotDurationSeconds,
		0.25f,
		TEXT("MinimumPulse")
	);
	const FOpenMobileHapticPlaybackResult Maximum = Subsystem->Vibrate(
		Settings->MaximumOneShotDurationSeconds,
		1.0f,
		TEXT("MaximumPulse")
	);
	TestTrue(TEXT("Minimum one-shot duration is accepted"),
		Minimum.IsAccepted());
	TestTrue(TEXT("Maximum one-shot duration is accepted"),
		Maximum.IsAccepted());
	TestEqual(TEXT("Requested duration is reported"),
		Maximum.Duration.RequestedSeconds,
		static_cast<double>(Settings->MaximumOneShotDurationSeconds));
	TestEqual(TEXT("Resolved duration is reported"),
		Maximum.Duration.ResolvedSeconds,
		static_cast<double>(Settings->MaximumOneShotDurationSeconds));
	TestFalse(TEXT("Unknown native duration remains explicit"),
		Maximum.Duration.bNativeDurationKnown);
	TestEqual(TEXT("Requested intensity is reported"),
		Maximum.Intensity.Requested, 1.0f);
	TestEqual(TEXT("Resolved intensity is reported"),
		Maximum.Intensity.Resolved, 1.0f);
	TestTrue(TEXT("Controllable mock playback receives a handle"),
		Maximum.Handle.IsValid());
	TestEqual(TEXT("One-shot resolution reaches the backend"),
		Backend.LastOneShotResolution.Path,
		EOpenMobileHapticsOneShotPath::BasicVibration);

	Backend.bOneShotControllable = false;
	const FOpenMobileHapticPlaybackResult FireAndForget = Subsystem->Vibrate(
		0.1f,
		0.75f,
		TEXT("FireAndForgetPulse")
	);
	TestTrue(TEXT("Fire-and-forget one-shot is accepted"),
		FireAndForget.IsAccepted());
	TestFalse(TEXT("Fire-and-forget one-shot has no handle"),
		FireAndForget.Handle.IsValid());
	Backend.bOneShotControllable = true;

	Backend.bBusyOneShot = true;
	const FOpenMobileHapticPlaybackResult Busy = Subsystem->Vibrate(
		0.1f,
		1.0f,
		TEXT("BusyPulse")
	);
	TestEqual(TEXT("Busy one-shot has a typed error"),
		Busy.Error.Code, EOpenMobileHapticErrorCode::ChannelBusy);
	TestFalse(TEXT("Busy one-shot has no handle"), Busy.Handle.IsValid());
	Backend.bBusyOneShot = false;
	TestEqual(TEXT("Diagnostics retain the latest accepted duration"),
		Subsystem->GetDiagnostics().LastDuration.RequestedSeconds,
		0.1);

	Backend.Capabilities.MaximumDurationSeconds = {true, 0.05};
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	const int32 BeforeNativeLimit = Backend.OneShotSubmissionCount;
	const FOpenMobileHapticPlaybackResult NativeLimited = Subsystem->Vibrate(
		0.2f,
		1.0f,
		TEXT("NativeLimitedPulse")
	);
	TestEqual(TEXT("Unsplittable native duration is rejected"),
		NativeLimited.Error.Code,
		EOpenMobileHapticErrorCode::UnsupportedFeature);
	TestEqual(TEXT("Native duration overflow is rejected before submission"),
		Backend.OneShotSubmissionCount, BeforeNativeLimit);
	Backend.Capabilities.MaximumDurationSeconds = {};
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();

	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Unsupported;
	const FOpenMobileHapticPlaybackResult DefaultAmplitude =
		Subsystem->Vibrate(0.2f, 0.4f, TEXT("DefaultAmplitudePulse"));
	TestEqual(TEXT("Missing amplitude control is a reported fallback"),
		DefaultAmplitude.Outcome,
		EOpenMobileHapticPlaybackOutcome::Fallback);
	TestEqual(TEXT("Default-amplitude fallback is named"),
		DefaultAmplitude.ResolvedPath,
		FName(TEXT("BasicVibrationDefaultAmplitude")));
	TestTrue(TEXT("Default native amplitude is known"),
		DefaultAmplitude.Intensity.bNativeIntensityKnown);
	TestEqual(TEXT("Default native amplitude is maximum"),
		DefaultAmplitude.Intensity.Native, 1.0f);
	TestEqual(TEXT("Diagnostics retain the native amplitude fallback"),
		Subsystem->GetDiagnostics().LastIntensity.Native, 1.0f);
	FOpenMobileHapticOneShotRequest ExactIntensityRequest;
	ExactIntensityRequest.DurationSeconds = 0.2f;
	ExactIntensityRequest.Intensity = 0.4f;
	ExactIntensityRequest.Options.Channel = TEXT("ExactIntensityPulse");
	ExactIntensityRequest.Options.FallbackPolicy =
		EOpenMobileHapticFallbackPolicy::ExactOnly;
	TestEqual(TEXT("Exact partial intensity rejects fixed-amplitude hardware"),
		Subsystem->SubmitOneShot(ExactIntensityRequest).Error.Code,
		EOpenMobileHapticErrorCode::UnsupportedFeature);
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();

	const FOpenMobileHapticPlaybackResult FirstRateLimited =
		Subsystem->Vibrate(0.05f, 1.0f, TEXT("PulseRate"));
	const FOpenMobileHapticPlaybackResult SecondRateLimited =
		Subsystem->Vibrate(0.05f, 1.0f, TEXT("PulseRate"));
	TestTrue(TEXT("First pulse on a channel is accepted"),
		FirstRateLimited.IsAccepted());
	TestEqual(TEXT("Repeated pulse is suppressed before native submission"),
		SecondRateLimited.Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Channel interval suppression is structured"),
		SecondRateLimited.SuppressionReason,
		EOpenMobileHapticSuppressionReason::ChannelMinimumInterval);
	TestEqual(TEXT("Expected rate suppression has no error"),
		SecondRateLimited.Error.Code,
		EOpenMobileHapticErrorCode::None);

	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Unsupported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	const FOpenMobileHapticPlaybackResult Unsupported =
		Subsystem->Vibrate(0.2f, 1.0f, TEXT("UnsupportedPulse"));
	TestEqual(TEXT("Unsupported pulse has a typed error"),
		Unsupported.Error.Code,
		EOpenMobileHapticErrorCode::UnsupportedFeature);
	Backend.bCustomPlaybackConfigured = false;
	const FOpenMobileHapticPlaybackResult NotConfigured =
		Subsystem->Vibrate(0.2f, 1.0f, TEXT("UnpackagedPulse"));
	TestEqual(TEXT("Unpackaged custom vibration is distinguished"),
		NotConfigured.Error.Code,
		EOpenMobileHapticErrorCode::NotConfigured);
	FOpenMobileHapticOneShotRequest OptionalRequest;
	OptionalRequest.DurationSeconds = 0.2f;
	OptionalRequest.Options.Channel = TEXT("OptionalUnpackagedPulse");
	OptionalRequest.Options.FallbackPolicy =
		EOpenMobileHapticFallbackPolicy::NoEffectAllowed;
	TestEqual(TEXT("No-effect policy still suppresses unpackaged vibration"),
		Subsystem->SubmitOneShot(OptionalRequest).Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsSemanticPolicyTest,
	"OpenMobile.Haptics.Semantic.PolicyAndMappings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSemanticPolicyTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	struct FExpectedMapping
	{
		EOpenMobileHapticSemanticEffect Effect;
		EOpenMobileHapticsSemanticBehavior Behavior;
		FName Name;
		FName Category;
	};
	const FExpectedMapping Mappings[] = {
		{EOpenMobileHapticSemanticEffect::Selection,
			EOpenMobileHapticsSemanticBehavior::Selection,
			TEXT("Selection"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactLight,
			EOpenMobileHapticsSemanticBehavior::ImpactLight,
			TEXT("ImpactLight"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactMedium,
			EOpenMobileHapticsSemanticBehavior::ImpactMedium,
			TEXT("ImpactMedium"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactHeavy,
			EOpenMobileHapticsSemanticBehavior::ImpactHeavy,
			TEXT("ImpactHeavy"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactSoft,
			EOpenMobileHapticsSemanticBehavior::ImpactSoft,
			TEXT("ImpactSoft"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::ImpactRigid,
			EOpenMobileHapticsSemanticBehavior::ImpactRigid,
			TEXT("ImpactRigid"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::NotificationSuccess,
			EOpenMobileHapticsSemanticBehavior::NotificationSuccess,
			TEXT("NotificationSuccess"), TEXT("Alerts")},
		{EOpenMobileHapticSemanticEffect::NotificationWarning,
			EOpenMobileHapticsSemanticBehavior::NotificationWarning,
			TEXT("NotificationWarning"), TEXT("Alerts")},
		{EOpenMobileHapticSemanticEffect::NotificationError,
			EOpenMobileHapticsSemanticBehavior::NotificationError,
			TEXT("NotificationError"), TEXT("Alerts")},
		{EOpenMobileHapticSemanticEffect::Confirm,
			EOpenMobileHapticsSemanticBehavior::NotificationSuccess,
			TEXT("Confirm"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::Reject,
			EOpenMobileHapticsSemanticBehavior::NotificationError,
			TEXT("Reject"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::Tick,
			EOpenMobileHapticsSemanticBehavior::Selection,
			TEXT("Tick"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::Click,
			EOpenMobileHapticsSemanticBehavior::ImpactLight,
			TEXT("Click"), TEXT("UI")},
		{EOpenMobileHapticSemanticEffect::Bump,
			EOpenMobileHapticsSemanticBehavior::ImpactMedium,
			TEXT("Bump"), TEXT("Gameplay")},
		{EOpenMobileHapticSemanticEffect::Damage,
			EOpenMobileHapticsSemanticBehavior::ImpactHeavy,
			TEXT("Damage"), TEXT("Gameplay")},
		{EOpenMobileHapticSemanticEffect::Pickup,
			EOpenMobileHapticsSemanticBehavior::ImpactSoft,
			TEXT("Pickup"), TEXT("Gameplay")},
		{EOpenMobileHapticSemanticEffect::Achievement,
			EOpenMobileHapticsSemanticBehavior::NotificationSuccess,
			TEXT("Achievement"), TEXT("Alerts")}
	};
	for (const FExpectedMapping& Mapping : Mappings)
	{
		const FOpenMobileHapticsSemanticDescriptor Descriptor =
			FOpenMobileHapticsSemanticPolicy::Describe(Mapping.Effect);
		TestEqual(TEXT("Semantic behavior is stable"), Descriptor.Behavior,
			Mapping.Behavior);
		TestEqual(TEXT("Semantic name is stable"), Descriptor.Name,
			Mapping.Name);
		TestEqual(TEXT("Semantic category is stable"), Descriptor.Category,
			Mapping.Category);
	}

	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	TestEqual(
		TEXT("System semantic behavior is preferred"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Path,
		EOpenMobileHapticsSemanticPath::SystemSemantic
	);
	Capabilities.SemanticEffects = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Supported;
	const FOpenMobileHapticsSemanticResolution Predefined =
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::Automatic
		);
	TestEqual(TEXT("Predefined fallback is selected"), Predefined.Path,
		EOpenMobileHapticsSemanticPath::PredefinedEffect);
	TestTrue(TEXT("Predefined path is reported as fallback"),
		Predefined.bFallback);
	Capabilities.PredefinedEffects = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	TestEqual(
		TEXT("Basic vibration is the last active fallback"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Path,
		EOpenMobileHapticsSemanticPath::BasicVibration
	);
	TestEqual(
		TEXT("No-basic policy rejects the basic fallback"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::NoBasicVibration
		).Path,
		EOpenMobileHapticsSemanticPath::Unsupported
	);
	TestEqual(
		TEXT("Exact-only policy rejects portable fallbacks"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::ExactOnly
		).Path,
		EOpenMobileHapticsSemanticPath::Unsupported
	);
	TestTrue(
		TEXT("No-effect policy suppresses unavailable feedback"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			Capabilities,
			EOpenMobileHapticFallbackPolicy::NoEffectAllowed
		).bSuppressWhenUnavailable
	);
	FOpenMobileHapticCapabilities AndroidProfile;
	AndroidProfile.BackendName = TEXT("AndroidMock");
	AndroidProfile.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	AndroidProfile.PredefinedEffects = EOpenMobileHapticSupportState::Supported;
	TestEqual(TEXT("Android profile prefers view semantics"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			AndroidProfile,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Path,
		EOpenMobileHapticsSemanticPath::SystemSemantic);
	FOpenMobileHapticCapabilities AppleProfile;
	AppleProfile.BackendName = TEXT("AppleMock");
	AppleProfile.SemanticEffects = EOpenMobileHapticSupportState::Supported;
	AppleProfile.RichHaptics = EOpenMobileHapticSupportState::Supported;
	TestEqual(TEXT("Apple profile prefers UIKit semantics"),
		FOpenMobileHapticsSemanticPolicy::Resolve(
			AppleProfile,
			EOpenMobileHapticFallbackPolicy::Automatic
		).Path,
		EOpenMobileHapticsSemanticPath::SystemSemantic);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsSemanticSubmissionPolicyTest,
	"OpenMobile.Haptics.Semantic.SubmissionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSemanticSubmissionPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(true);

	FMockBackend Backend(TEXT("SemanticMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const UFunction* SelectionFunction =
		UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
			TEXT("PlaySelectionFeedback")
		);
	TestNotNull(TEXT("Selection feedback has a Blueprint node"),
		SelectionFunction);
	TestTrue(TEXT("Selection feedback node is Blueprint callable"),
		SelectionFunction
			&& SelectionFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	const FOpenMobileHapticPlaybackResult DedicatedSelection =
		Subsystem->PlaySelectionFeedback();
	TestTrue(TEXT("Dedicated selection entry point reaches the backend"),
		DedicatedSelection.IsAccepted());
	TestEqual(TEXT("Dedicated selection entry point uses selection semantics"),
		Backend.LastSemanticRequest.Effect,
		EOpenMobileHapticSemanticEffect::Selection);
	const int32 SelectionSubmissionCount = Backend.SemanticSubmissionCount;
	const FOpenMobileHapticPlaybackResult CoalescedSelection =
		Subsystem->PlaySelectionFeedback();
	TestEqual(TEXT("Equivalent selection feedback is coalesced"),
		CoalescedSelection.SuppressionReason,
		EOpenMobileHapticSuppressionReason::EquivalentRequest);
	TestEqual(TEXT("Coalesced selection does not reach the backend"),
		Backend.SemanticSubmissionCount, SelectionSubmissionCount);
	TestEqual(TEXT("Coalescing remains an expected non-error outcome"),
		CoalescedSelection.Error.Code,
		EOpenMobileHapticErrorCode::None);
	const FOpenMobileHapticsPerformanceDiagnostics SelectionPerformance =
		Subsystem->GetDiagnostics().Performance;
	TestEqual(TEXT("Coalesced selection increments dropped requests"),
		SelectionPerformance.DroppedRequestCount, static_cast<int64>(1));
	TestEqual(TEXT("Only submitted selection reaches native timing"),
		SelectionPerformance.NativeSubmissionCount, static_cast<int64>(1));

	FOpenMobileHapticUserPolicy Policy;
	Policy.MasterIntensity = 0.8f;
	Policy.CategoryScales.Add(TEXT("Alerts"), 0.5f);
	Policy.EffectScales.Add(TEXT("NotificationWarning"), 0.25f);
	Subsystem->SetUserPolicy(Policy);
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = TEXT("AlertsPolicyTest");
	Options.Category = TEXT("Alerts");
	const FOpenMobileHapticPlaybackResult Scaled = Subsystem->SubmitSemantic({
		EOpenMobileHapticSemanticEffect::NotificationWarning,
		0.5f,
		Options
	});
	TestEqual(TEXT("Supported semantic request is accepted"), Scaled.Outcome,
		EOpenMobileHapticPlaybackOutcome::Accepted);
	TestEqual(TEXT("System semantic path is reported"), Scaled.ResolvedPath,
		FName(TEXT("SystemSemantic")));
	TestTrue(TEXT("Player scales are applied before submission"),
		FMath::IsNearlyEqual(Backend.LastSemanticRequest.Intensity, 0.05f));

	const int32 SubmittedBeforeRejections = Backend.SemanticSubmissionCount;
	FOpenMobileHapticSemanticRequest InvalidRequest;
	InvalidRequest.Intensity = std::numeric_limits<float>::quiet_NaN();
	const FOpenMobileHapticPlaybackResult Invalid =
		Subsystem->SubmitSemantic(InvalidRequest);
	TestEqual(TEXT("Nonfinite semantic intensity is rejected"),
		Invalid.Error.Code, EOpenMobileHapticErrorCode::InvalidRequest);

	Policy.bEnabled = false;
	Subsystem->SetUserPolicy(Policy);
	const FOpenMobileHapticPlaybackResult Disabled =
		Subsystem->PlaySelectionFeedback();
	TestEqual(TEXT("Disabled player policy suppresses feedback"),
		Disabled.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);

	Policy.bEnabled = true;
	Policy.CategoryScales.Add(TEXT("UI"), 0.0f);
	Subsystem->SetUserPolicy(Policy);
	const FOpenMobileHapticPlaybackResult ZeroScale =
		Subsystem->PlaySemanticFeedback(EOpenMobileHapticSemanticEffect::Click);
	TestEqual(TEXT("Zero effective intensity suppresses feedback"),
		ZeroScale.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);

	Policy.CategoryScales.Remove(TEXT("UI"));
	Subsystem->SetUserPolicy(Policy);
	UOpenMobileHapticsSettings* MutableSettings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const EOpenMobileHapticBackgroundPolicy SavedBackgroundPolicy =
		MutableSettings->BackgroundPolicy;
	MutableSettings->BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::CriticalOnly;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.PredefinedEffects =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.BackgroundAlerts =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(false);
	const FOpenMobileHapticPlaybackResult Background =
		Subsystem->PlaySemanticFeedback(EOpenMobileHapticSemanticEffect::Confirm);
	TestEqual(TEXT("Background semantic feedback is suppressed"),
		Background.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Noncritical background work never reaches the backend"),
		Backend.SemanticSubmissionCount, SubmittedBeforeRejections);
	Options.Channel = TEXT("CriticalBackgroundTest");
	Options.Category = TEXT("Alerts");
	Options.Priority = EOpenMobileHapticChannelPriority::Critical;
	const FOpenMobileHapticPlaybackResult CriticalBackground =
		Subsystem->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::NotificationWarning,
			1.0f,
			Options
		});
	TestTrue(TEXT("Explicit critical policy permits critical background work"),
		CriticalBackground.IsAccepted());
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(true);
	MutableSettings->BackgroundPolicy = SavedBackgroundPolicy;
	Backend.bNativePolicySuppressesSemantic = true;
	Options.Channel = TEXT("MissingPresenterTest");
	Options.Category = TEXT("UI");
	Options.Priority = EOpenMobileHapticChannelPriority::Normal;
	const FOpenMobileHapticPlaybackResult MissingPresenter =
		Subsystem->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::Selection,
			1.0f,
			Options
		});
	TestEqual(TEXT("Known native policy suppression is a nonfailure"),
		MissingPresenter.Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestFalse(TEXT("Native policy suppression does not invent an error"),
		MissingPresenter.Error.IsSet());
	Backend.bNativePolicySuppressesSemantic = false;

	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.PredefinedEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	Options.Channel = TEXT("FallbackPolicyTest");
	Options.Category = TEXT("Gameplay");
	const FOpenMobileHapticPlaybackResult Fallback = Subsystem->SubmitSemantic({
		EOpenMobileHapticSemanticEffect::Damage,
		1.0f,
		Options
	});
	TestEqual(TEXT("Portable fallback is reported"), Fallback.Outcome,
		EOpenMobileHapticPlaybackOutcome::Fallback);
	TestEqual(TEXT("Portable fallback path is reported"), Fallback.ResolvedPath,
		FName(TEXT("PredefinedEffect")));
	TestEqual(TEXT("Resolved fallback reaches the backend"),
		Backend.LastSemanticResolution.Path,
		EOpenMobileHapticsSemanticPath::PredefinedEffect);
	Backend.Capabilities.PredefinedEffects =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Unsupported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	Options.Channel = TEXT("SemanticDefaultAmplitudeTest");
	const FOpenMobileHapticPlaybackResult SemanticDefaultAmplitude =
		Subsystem->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::Damage,
			0.5f,
			Options
		});
	TestEqual(TEXT("Semantic basic fallback reports fixed amplitude"),
		SemanticDefaultAmplitude.ResolvedPath,
		FName(TEXT("BasicVibrationDefaultAmplitude")));
	TestTrue(TEXT("Semantic fixed amplitude is explicit"),
		SemanticDefaultAmplitude.Intensity.bNativeIntensityKnown);

	Options.Channel = TEXT("ExactPolicyTest");
	Options.FallbackPolicy = EOpenMobileHapticFallbackPolicy::ExactOnly;
	const FOpenMobileHapticPlaybackResult Unsupported =
		Subsystem->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::Damage,
			1.0f,
			Options
		});
	TestEqual(TEXT("Unavailable exact feedback is typed as unsupported"),
		Unsupported.Error.Code,
		EOpenMobileHapticErrorCode::UnsupportedFeature);
	Backend.bCustomPlaybackConfigured = false;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Unsupported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	Options.Channel = TEXT("UnpackagedSemanticFallback");
	Options.FallbackPolicy = EOpenMobileHapticFallbackPolicy::Automatic;
	const FOpenMobileHapticPlaybackResult NotConfigured =
		Subsystem->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::Damage,
			1.0f,
			Options
		});
	TestEqual(TEXT("Unpackaged semantic fallback is distinguished"),
		NotConfigured.Error.Code,
		EOpenMobileHapticErrorCode::NotConfigured);
	Options.Channel = TEXT("OptionalUnpackagedSemanticFallback");
	Options.FallbackPolicy = EOpenMobileHapticFallbackPolicy::NoEffectAllowed;
	TestEqual(TEXT("Optional unpackaged semantic fallback is suppressed"),
		Subsystem->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::Damage,
			1.0f,
			Options
		}).Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	Backend.bCustomPlaybackConfigured = true;
	Options.FallbackPolicy = EOpenMobileHapticFallbackPolicy::Automatic;

	UOpenMobileHapticsSubsystem* DebounceSubsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	const FOpenMobileHapticPlaybackResult FirstSelection =
		DebounceSubsystem->PlaySemanticFeedback(
			EOpenMobileHapticSemanticEffect::Selection
		);
	const FOpenMobileHapticPlaybackResult SecondSelection =
		DebounceSubsystem->PlaySemanticFeedback(
			EOpenMobileHapticSemanticEffect::Selection
		);
	TestTrue(TEXT("First selection reaches the backend"),
		FirstSelection.IsAccepted());
	TestEqual(TEXT("Repeated selection is debounced"),
		SecondSelection.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);

	DebounceSubsystem->Deinitialize();
	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsImpactPresetTest,
	"OpenMobile.Haptics.Semantic.ImpactPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsImpactPresetTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("ImpactMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const UFunction* ImpactFunction =
		UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
			TEXT("PlayImpactFeedback")
		);
	TestTrue(TEXT("Impact preset has a Blueprint-callable node"),
		ImpactFunction
			&& ImpactFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));

	struct FExpectedImpact
	{
		EOpenMobileHapticImpactStyle Style;
		EOpenMobileHapticSemanticEffect Effect;
	};
	const FExpectedImpact Styles[] = {
		{EOpenMobileHapticImpactStyle::Light,
			EOpenMobileHapticSemanticEffect::ImpactLight},
		{EOpenMobileHapticImpactStyle::Medium,
			EOpenMobileHapticSemanticEffect::ImpactMedium},
		{EOpenMobileHapticImpactStyle::Heavy,
			EOpenMobileHapticSemanticEffect::ImpactHeavy},
		{EOpenMobileHapticImpactStyle::Soft,
			EOpenMobileHapticSemanticEffect::ImpactSoft},
		{EOpenMobileHapticImpactStyle::Rigid,
			EOpenMobileHapticSemanticEffect::ImpactRigid}
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Styles); ++Index)
	{
		const FName Channel(*FString::Printf(TEXT("Impact%d"), Index));
		const FOpenMobileHapticPlaybackResult Result =
			Subsystem->PlayImpactFeedback(Styles[Index].Style, 0.5f, Channel);
		TestTrue(TEXT("Impact style is accepted"), Result.IsAccepted());
		TestEqual(TEXT("Impact style maps to its stable semantic effect"),
			Backend.LastSemanticRequest.Effect, Styles[Index].Effect);
		TestEqual(TEXT("Normalized impact intensity reaches the backend"),
			Backend.LastSemanticRequest.Intensity, 0.5f);
	}

	const int32 BeforeZeroIntensity = Backend.SemanticSubmissionCount;
	const FOpenMobileHapticPlaybackResult ZeroIntensity =
		Subsystem->PlayImpactFeedback(
			EOpenMobileHapticImpactStyle::Light,
			0.0f,
			TEXT("ZeroImpact")
		);
	TestEqual(TEXT("Zero impact intensity is explicitly suppressed"),
		ZeroIntensity.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Zero impact intensity never reaches the backend"),
		Backend.SemanticSubmissionCount, BeforeZeroIntensity);

	const int32 SubmissionCount = Backend.SemanticSubmissionCount;
	for (const float InvalidIntensity : {
		-0.01f,
		1.01f,
		std::numeric_limits<float>::quiet_NaN()
	})
	{
		const FOpenMobileHapticPlaybackResult Invalid =
			Subsystem->PlayImpactFeedback(
				EOpenMobileHapticImpactStyle::Heavy,
				InvalidIntensity,
				TEXT("InvalidImpact")
			);
		TestEqual(TEXT("Invalid impact intensity is rejected"),
			Invalid.Error.Code, EOpenMobileHapticErrorCode::InvalidRequest);
	}
	const FOpenMobileHapticPlaybackResult InvalidStyle =
		Subsystem->PlayImpactFeedback(
			static_cast<EOpenMobileHapticImpactStyle>(MAX_uint8),
			1.0f,
			TEXT("InvalidImpactStyle")
		);
	TestEqual(TEXT("Unknown impact style is rejected"),
		InvalidStyle.Error.Code, EOpenMobileHapticErrorCode::InvalidRequest);
	TestEqual(TEXT("Invalid impacts never reach the backend"),
		Backend.SemanticSubmissionCount, SubmissionCount);

	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.PredefinedEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	const FOpenMobileHapticPlaybackResult SoftFallback =
		Subsystem->PlayImpactFeedback(
			EOpenMobileHapticImpactStyle::Soft,
			1.0f,
			TEXT("ImpactFallback")
		);
	TestEqual(TEXT("Unavailable impact style uses a predefined fallback"),
		SoftFallback.Outcome, EOpenMobileHapticPlaybackOutcome::Fallback);
	TestEqual(TEXT("Impact fallback order is stable"),
		Backend.LastSemanticResolution.Path,
		EOpenMobileHapticsSemanticPath::PredefinedEffect);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsNotificationPresetTest,
	"OpenMobile.Haptics.Semantic.NotificationPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsNotificationPresetTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("NotificationMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const UFunction* NotificationFunction =
		UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
			TEXT("PlayNotificationFeedback")
		);
	TestTrue(TEXT("Notification preset has a Blueprint-callable node"),
		NotificationFunction
			&& NotificationFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));

	struct FExpectedNotification
	{
		EOpenMobileHapticNotificationType Type;
		EOpenMobileHapticSemanticEffect Effect;
	};
	const FExpectedNotification Notifications[] = {
		{EOpenMobileHapticNotificationType::Success,
			EOpenMobileHapticSemanticEffect::NotificationSuccess},
		{EOpenMobileHapticNotificationType::Warning,
			EOpenMobileHapticSemanticEffect::NotificationWarning},
		{EOpenMobileHapticNotificationType::Error,
			EOpenMobileHapticSemanticEffect::NotificationError}
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Notifications); ++Index)
	{
		const FName Channel(*FString::Printf(TEXT("Notification%d"), Index));
		const FOpenMobileHapticPlaybackResult Result =
			Subsystem->PlayNotificationFeedback(
				Notifications[Index].Type,
				1.0f,
				Channel
			);
		TestTrue(TEXT("Notification preset is accepted"), Result.IsAccepted());
		TestEqual(TEXT("Notification meaning maps to its stable effect"),
			Backend.LastSemanticRequest.Effect, Notifications[Index].Effect);
		TestEqual(TEXT("Notification defaults to the Alerts category"),
			Backend.LastSemanticRequest.Options.Category,
			FName(TEXT("Alerts")));
	}

	FOpenMobileHapticUserPolicy Policy;
	Policy.CategoryScales.Add(TEXT("Alerts"), 0.0f);
	Subsystem->SetUserPolicy(Policy);
	const int32 BeforePolicySuppression = Backend.SemanticSubmissionCount;
	const FOpenMobileHapticPlaybackResult Suppressed =
		Subsystem->PlayNotificationFeedback(
			EOpenMobileHapticNotificationType::Warning
		);
	TestEqual(TEXT("Alerts category can suppress notifications"),
		Suppressed.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Alerts suppression happens before native submission"),
		Backend.SemanticSubmissionCount, BeforePolicySuppression);

	UOpenMobileHapticsSubsystem* RapidSubsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const FOpenMobileHapticPlaybackResult FirstRapid =
		RapidSubsystem->PlayNotificationFeedback(
			EOpenMobileHapticNotificationType::Success
		);
	const FOpenMobileHapticPlaybackResult SecondRapid =
		RapidSubsystem->PlayNotificationFeedback(
			EOpenMobileHapticNotificationType::Error
		);
	TestTrue(TEXT("First rapid notification is accepted"),
		FirstRapid.IsAccepted());
	TestEqual(TEXT("Notification default channel is Alerts"),
		FirstRapid.Channel, FName(TEXT("Alerts")));
	TestEqual(TEXT("Rapid notification repeat is rate limited"),
		SecondRapid.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);

	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.PredefinedEffects =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	const FOpenMobileHapticPlaybackResult BasicFallback =
		RapidSubsystem->PlayNotificationFeedback(
			EOpenMobileHapticNotificationType::Error,
			1.0f,
			TEXT("BasicNotificationFallback")
		);
	TestEqual(TEXT("Notification can use its basic-vibration fallback"),
		BasicFallback.Outcome, EOpenMobileHapticPlaybackOutcome::Fallback);
	TestEqual(TEXT("Notification basic fallback path is reported"),
		Backend.LastSemanticResolution.Path,
		EOpenMobileHapticsSemanticPath::BasicVibration);

	RapidSubsystem->Deinitialize();
	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsGamePresetTest,
	"OpenMobile.Haptics.Semantic.GamePresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsGamePresetTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("GamePresetMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();

	const UFunction* GamePresetFunction =
		UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
			TEXT("PlayGameFeedback")
		);
	TestTrue(TEXT("Game preset has a Blueprint-callable node"),
		GamePresetFunction
			&& GamePresetFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	struct FExpectedPreset
	{
		EOpenMobileHapticGamePreset Preset;
		EOpenMobileHapticSemanticEffect Effect;
		FName Category;
	};
	const FExpectedPreset Presets[] = {
		{EOpenMobileHapticGamePreset::Confirm,
			EOpenMobileHapticSemanticEffect::Confirm, TEXT("UI")},
		{EOpenMobileHapticGamePreset::Reject,
			EOpenMobileHapticSemanticEffect::Reject, TEXT("UI")},
		{EOpenMobileHapticGamePreset::Tick,
			EOpenMobileHapticSemanticEffect::Tick, TEXT("UI")},
		{EOpenMobileHapticGamePreset::Click,
			EOpenMobileHapticSemanticEffect::Click, TEXT("UI")},
		{EOpenMobileHapticGamePreset::Bump,
			EOpenMobileHapticSemanticEffect::Bump, TEXT("Gameplay")},
		{EOpenMobileHapticGamePreset::Damage,
			EOpenMobileHapticSemanticEffect::Damage, TEXT("Gameplay")},
		{EOpenMobileHapticGamePreset::Pickup,
			EOpenMobileHapticSemanticEffect::Pickup, TEXT("Gameplay")},
		{EOpenMobileHapticGamePreset::Achievement,
			EOpenMobileHapticSemanticEffect::Achievement, TEXT("Alerts")}
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Presets); ++Index)
	{
		const FName Channel(*FString::Printf(TEXT("GamePreset%d"), Index));
		const FOpenMobileHapticPlaybackResult Result =
			Subsystem->PlayGameFeedback(Presets[Index].Preset, 1.0f, Channel);
		TestTrue(TEXT("Built-in game preset is accepted"), Result.IsAccepted());
		TestEqual(TEXT("Game preset keeps its stable semantic mapping"),
			Backend.LastSemanticRequest.Effect, Presets[Index].Effect);
		TestEqual(TEXT("Game preset uses its stable category"),
			Backend.LastSemanticRequest.Options.Category,
			Presets[Index].Category);
	}

	FOpenMobileHapticUserPolicy Policy;
	Policy.CategoryScales.Add(TEXT("Gameplay"), 0.0f);
	Subsystem->SetUserPolicy(Policy);
	const int32 BeforeCategorySuppression = Backend.SemanticSubmissionCount;
	const FOpenMobileHapticPlaybackResult CategorySuppressed =
		Subsystem->PlayGameFeedback(
			EOpenMobileHapticGamePreset::Damage,
			1.0f,
			TEXT("ScaledDamage")
		);
	TestEqual(TEXT("Game preset category scaling is applied"),
		CategorySuppressed.Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Suppressed game preset never reaches the backend"),
		Backend.SemanticSubmissionCount, BeforeCategorySuppression);
	Subsystem->SetUserPolicy({});

	UOpenMobileHapticLibrary* Library =
		NewObject<UOpenMobileHapticLibrary>(
			GetTransientPackage(),
			TEXT("GamePresetTestLibrary")
		);
	Library->GamePresetOverrides = {
		{EOpenMobileHapticGamePreset::Confirm, TEXT("CustomConfirm")},
		{EOpenMobileHapticGamePreset::Reject, TEXT("CustomReject")}
	};
	FOpenMobileHapticNamedLibrarySettings LibrarySettings;
	LibrarySettings.Name = TEXT("GamePresets");
	LibrarySettings.Asset = FSoftObjectPath(Library);
	Settings->NamedLibraries = {LibrarySettings};
	const FOpenMobileHapticPlaybackResult Override =
		Subsystem->PlayGameFeedback(
			EOpenMobileHapticGamePreset::Confirm,
			1.0f,
			TEXT("GameOverride")
		);
	TestTrue(TEXT("Loaded named-library override is accepted"),
		Override.IsAccepted());
	TestEqual(TEXT("Named-library override reaches named playback"),
		Backend.LastNamedRequest.PatternName, FName(TEXT("CustomConfirm")));

	Backend.bFailNamedSubmissions = true;
	const FOpenMobileHapticPlaybackResult UnsupportedOverride =
		Subsystem->PlayGameFeedback(
			EOpenMobileHapticGamePreset::Reject,
			1.0f,
			TEXT("UnsupportedGameOverride")
		);
	TestEqual(TEXT("Unsupported override uses the built-in fallback"),
		UnsupportedOverride.Outcome,
		EOpenMobileHapticPlaybackOutcome::Fallback);
	TestEqual(TEXT("Built-in fallback preserves the preset meaning"),
		Backend.LastSemanticRequest.Effect,
		EOpenMobileHapticSemanticEffect::Reject);
	Backend.bFailNamedSubmissions = false;

	LibrarySettings.Asset =
		FSoftObjectPath(TEXT("/Game/Haptics/Missing.Missing"));
	Settings->NamedLibraries = {LibrarySettings};
	const int32 NamedBeforeMissingAsset = Backend.NamedSubmissionCount;
	const FOpenMobileHapticPlaybackResult MissingAsset =
		Subsystem->PlayGameFeedback(
			EOpenMobileHapticGamePreset::Tick,
			1.0f,
			TEXT("MissingGameOverride")
		);
	TestTrue(TEXT("Missing override asset keeps the built-in preset"),
		MissingAsset.IsAccepted());
	TestEqual(TEXT("Missing override asset is not synchronously loaded"),
		Backend.NamedSubmissionCount, NamedBeforeMissingAsset);
	TestEqual(TEXT("Missing override keeps the stable semantic mapping"),
		Backend.LastSemanticRequest.Effect,
		EOpenMobileHapticSemanticEffect::Tick);

	Settings->NamedLibraries = SavedLibraries;
	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsSemanticRateLimitTest,
	"OpenMobile.Haptics.Semantic.RateLimitBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSemanticRateLimitTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsRateLimiter SelectionLimiter;
	TestFalse(TEXT("First selection is allowed"),
		SelectionLimiter.ShouldSuppress(TEXT("UI"), true, 10.0,
			0.0, 0.04, 30));
	TestTrue(TEXT("Selection inside the debounce window is suppressed"),
		SelectionLimiter.ShouldSuppress(TEXT("UI"), true, 10.039,
			0.0, 0.04, 30));
	TestFalse(TEXT("Selection at the debounce boundary is allowed"),
		SelectionLimiter.ShouldSuppress(TEXT("UI"), true, 10.04,
			0.0, 0.04, 30));

	FOpenMobileHapticsRateLimiter ChannelLimiter;
	TestFalse(TEXT("First channel submission is allowed"),
		ChannelLimiter.ShouldSuppress(TEXT("Gameplay"), false, 20.0,
			0.02, 0.04, 30));
	TestTrue(TEXT("Channel submission inside its interval is suppressed"),
		ChannelLimiter.ShouldSuppress(TEXT("Gameplay"), false, 20.019,
			0.02, 0.04, 30));
	TestFalse(TEXT("Channel submission at its interval is allowed"),
		ChannelLimiter.ShouldSuppress(TEXT("Gameplay"), false, 20.02,
			0.02, 0.04, 30));

	FOpenMobileHapticsRateLimiter BurstLimiter;
	TestFalse(TEXT("First burst event is allowed"),
		BurstLimiter.ShouldSuppress(TEXT("A"), false, 30.0,
			0.0, 0.0, 3));
	TestFalse(TEXT("Second burst event is allowed"),
		BurstLimiter.ShouldSuppress(TEXT("B"), false, 30.1,
			0.0, 0.0, 3));
	TestTrue(TEXT("Submission cap suppresses the next event"),
		BurstLimiter.ShouldSuppress(TEXT("C"), false, 30.2,
			0.0, 0.0, 3));
	TestFalse(TEXT("Old burst events expire at one second"),
		BurstLimiter.ShouldSuppress(TEXT("D"), false, 31.0,
			0.0, 0.0, 3));

	FOpenMobileHapticsRateLimiter PerChannelLimiter;
	TestFalse(TEXT("Fast selection channel accepts its first event"),
		PerChannelLimiter.ShouldSuppress(TEXT("Fast"), true, 40.0,
			0.02, 0.02, 30));
	TestFalse(TEXT("Fast selection channel uses its shorter interval"),
		PerChannelLimiter.ShouldSuppress(TEXT("Fast"), true, 40.025,
			0.02, 0.02, 30));
	TestFalse(TEXT("Slow selection channel accepts its first event"),
		PerChannelLimiter.ShouldSuppress(TEXT("Slow"), true, 40.0,
			0.04, 0.02, 30));
	TestTrue(TEXT("Slow selection channel keeps its longer interval"),
		PerChannelLimiter.ShouldSuppress(TEXT("Slow"), true, 40.025,
			0.04, 0.02, 30));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsRateLimitPolicyTest,
	"OpenMobile.Haptics.Policy.RateLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsRateLimitPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	double NowSeconds = 100.0;
	FOpenMobileHapticsRateLimiter Limiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	FOpenMobileHapticsRateLimitPolicy Policy;
	Policy.ChannelMinimumIntervalSeconds = 0.0;
	Policy.EffectMinimumIntervalSeconds = 0.05;
	Policy.EquivalentRequestDebounceSeconds = 0.0;
	Policy.MaximumChannelSubmissionsPerSecond = 3;
	Policy.MaximumGlobalSubmissionsPerSecond = 10;
	FOpenMobileHapticsRateLimitRequest Request;
	Request.Channel = TEXT("UI");
	Request.Category = TEXT("UI");
	Request.Effect = TEXT("Selection");

	TestEqual(TEXT("First effect is allowed"),
		Limiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);
	NowSeconds = 100.01;
	Request.Effect = TEXT("Click");
	TestEqual(TEXT("Another effect does not inherit the first interval"),
		Limiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);
	NowSeconds = 100.049;
	Request.Channel = TEXT("Gameplay");
	Request.Category = TEXT("Gameplay");
	Request.Effect = TEXT("Selection");
	TestEqual(TEXT("Effect interval follows the effect across channels"),
		Limiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::EffectMinimumInterval);
	NowSeconds = 100.05;
	TestEqual(TEXT("Exact effect interval boundary is allowed"),
		Limiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);

	NowSeconds = 200.0;
	FOpenMobileHapticsRateLimiter WindowLimiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	Policy.EffectMinimumIntervalSeconds = 0.0;
	Policy.MaximumChannelSubmissionsPerSecond = 2;
	Request.Channel = TEXT("UI");
	Request.Category = TEXT("UI");
	Request.Effect = TEXT("A");
	TestEqual(TEXT("First channel-window request is allowed"),
		WindowLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);
	NowSeconds = 200.1;
	Request.Effect = TEXT("B");
	TestEqual(TEXT("Second channel-window request is allowed"),
		WindowLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);
	NowSeconds = 200.2;
	Request.Effect = TEXT("C");
	TestEqual(TEXT("Per-channel window suppresses the burst"),
		WindowLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::ChannelWindow);
	Request.Channel = TEXT("Alerts");
	Request.Category = TEXT("Alerts");
	TestEqual(TEXT("Another channel has an independent window"),
		WindowLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);
	NowSeconds = 201.0;
	Request.Channel = TEXT("UI");
	Request.Category = TEXT("UI");
	TestEqual(TEXT("Exact one-second window boundary expires"),
		WindowLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);

	NowSeconds = 300.0;
	FOpenMobileHapticsRateLimiter CoalescingLimiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	Policy.MaximumChannelSubmissionsPerSecond = 30;
	Policy.ChannelMinimumIntervalSeconds = 0.0;
	Policy.EffectMinimumIntervalSeconds = 0.0;
	Policy.EquivalentRequestDebounceSeconds = 0.04;
	Request.Channel = TEXT("UI");
	Request.Category = TEXT("UI");
	Request.Effect = TEXT("Selection");
	Request.Priority = EOpenMobileHapticChannelPriority::Normal;
	Request.EquivalenceHash = 11;
	Request.bCoalescible = true;
	TestEqual(TEXT("First equivalent UI request is allowed"),
		CoalescingLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);
	NowSeconds = 300.039;
	TestEqual(TEXT("Equivalent UI request is coalesced"),
		CoalescingLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::EquivalentRequest);
	Request.EquivalenceHash = 12;
	TestEqual(TEXT("Meaningfully different UI request is retained"),
		CoalescingLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);
	Request.EquivalenceHash = 11;
	Request.Priority = EOpenMobileHapticChannelPriority::Critical;
	TestEqual(TEXT("Critical request is not coalesced with normal feedback"),
		CoalescingLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::Allowed);

	NowSeconds = 400.0;
	FOpenMobileHapticsRateLimiter ClockLimiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	Request.bCoalescible = false;
	Request.Priority = EOpenMobileHapticChannelPriority::Normal;
	TestTrue(TEXT("Clock baseline is allowed"),
		ClockLimiter.Evaluate(Request, Policy).IsAllowed());
	NowSeconds = 399.0;
	const FOpenMobileHapticsRateLimitDecision BackwardClock =
		ClockLimiter.Evaluate(Request, Policy);
	TestTrue(TEXT("Backward monotonic clock resets stale history"),
		BackwardClock.IsAllowed());
	TestTrue(TEXT("Backward clock reset is explicit"),
		BackwardClock.bClockReset);
	NowSeconds = std::numeric_limits<double>::quiet_NaN();
	TestEqual(TEXT("Nonfinite clock fails closed"),
		ClockLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::InvalidClock);

	NowSeconds = 500.0;
	FOpenMobileHapticsRateLimiter HardChannelLimiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	Policy.MaximumChannelSubmissionsPerSecond = MAX_int32;
	Policy.MaximumGlobalSubmissionsPerSecond = MAX_int32;
	for (int32 Index = 0;
		Index < FOpenMobileHapticsRateLimiter::
			HardMaximumChannelSubmissionsPerSecond;
		++Index)
	{
		Request.Effect = FName(*FString::Printf(TEXT("Channel%d"), Index));
		TestTrue(TEXT("Hard channel budget admits bounded traffic"),
			HardChannelLimiter.Evaluate(Request, Policy).IsAllowed());
	}
	Request.Effect = TEXT("ChannelOverflow");
	TestEqual(TEXT("Extreme channel configuration is hard capped"),
		HardChannelLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::ChannelWindow);

	NowSeconds = 600.0;
	FOpenMobileHapticsRateLimiter HardGlobalLimiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	Request.Priority = EOpenMobileHapticChannelPriority::Critical;
	for (int32 Index = 0;
		Index < FOpenMobileHapticsRateLimiter::
			HardMaximumGlobalSubmissionsPerSecond;
		++Index)
	{
		Request.Channel = FName(*FString::Printf(TEXT("Global%d"), Index));
		Request.Category = Request.Channel;
		Request.Effect = Request.Channel;
		TestTrue(TEXT("Hard global budget admits bounded traffic"),
			HardGlobalLimiter.Evaluate(Request, Policy).IsAllowed());
	}
	Request.Channel = TEXT("GlobalOverflow");
	Request.Category = Request.Channel;
	Request.Effect = Request.Channel;
	TestEqual(TEXT("Critical traffic still obeys the hard global cap"),
		HardGlobalLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::GlobalWindow);

	NowSeconds = 650.0;
	FOpenMobileHapticsRateLimiter ReservedCriticalLimiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	Policy.MaximumChannelSubmissionsPerSecond = 30;
	Policy.MaximumGlobalSubmissionsPerSecond = 2;
	Request.Priority = EOpenMobileHapticChannelPriority::Normal;
	Request.Channel = TEXT("NormalA");
	Request.Category = TEXT("Gameplay");
	Request.Effect = TEXT("NormalA");
	TestTrue(TEXT("Normal traffic can use the unreserved budget"),
		ReservedCriticalLimiter.Evaluate(Request, Policy).IsAllowed());
	Request.Channel = TEXT("NormalB");
	Request.Effect = TEXT("NormalB");
	TestEqual(TEXT("Normal traffic preserves one critical slot"),
		ReservedCriticalLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::GlobalWindow);
	Request.Priority = EOpenMobileHapticChannelPriority::Critical;
	Request.Channel = TEXT("CriticalReserved");
	Request.Category = TEXT("Alerts");
	Request.Effect = TEXT("CriticalReserved");
	TestTrue(TEXT("Critical traffic can consume its reserved slot"),
		ReservedCriticalLimiter.Evaluate(Request, Policy).IsAllowed());

	FOpenMobileHapticsRateLimiter CriticalFirstLimiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	Policy.MaximumGlobalSubmissionsPerSecond = 3;
	Request.Channel = TEXT("CriticalFirst");
	Request.Category = TEXT("Alerts");
	Request.Effect = TEXT("CriticalFirst");
	TestTrue(TEXT("Critical traffic can arrive before normal traffic"),
		CriticalFirstLimiter.Evaluate(Request, Policy).IsAllowed());
	Request.Priority = EOpenMobileHapticChannelPriority::Normal;
	Request.Channel = TEXT("NormalAfterCriticalA");
	Request.Category = TEXT("Gameplay");
	Request.Effect = TEXT("NormalAfterCriticalA");
	TestTrue(TEXT("First normal slot remains available after Critical"),
		CriticalFirstLimiter.Evaluate(Request, Policy).IsAllowed());
	Request.Channel = TEXT("NormalAfterCriticalB");
	Request.Effect = TEXT("NormalAfterCriticalB");
	TestTrue(TEXT("Normal partition is independent of arrival order"),
		CriticalFirstLimiter.Evaluate(Request, Policy).IsAllowed());
	Request.Channel = TEXT("NormalAfterCriticalOverflow");
	Request.Effect = TEXT("NormalAfterCriticalOverflow");
	TestEqual(TEXT("Combined traffic still obeys the global cap"),
		CriticalFirstLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::GlobalWindow);

	NowSeconds = 700.0;
	FOpenMobileHapticsRateLimiter PriorityLimiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	Policy.MaximumChannelSubmissionsPerSecond = 1;
	Policy.MaximumGlobalSubmissionsPerSecond = 10;
	Request.Channel = TEXT("Gameplay");
	Request.Category = TEXT("Gameplay");
	Request.Effect = TEXT("Collision");
	Request.Priority = EOpenMobileHapticChannelPriority::Low;
	TestTrue(TEXT("Low-priority gameplay starts its channel window"),
		PriorityLimiter.Evaluate(Request, Policy).IsAllowed());
	Request.Priority = EOpenMobileHapticChannelPriority::Critical;
	Request.Effect = TEXT("UrgentGameplay");
	TestEqual(TEXT("Critical priority does not bypass channel safety"),
		PriorityLimiter.Evaluate(Request, Policy).Outcome,
		EOpenMobileHapticsRateLimitOutcome::ChannelWindow);
	Request.Channel = TEXT("Critical");
	Request.Category = TEXT("Alerts");
	Request.Effect = TEXT("CriticalAlert");
	TestTrue(TEXT("Critical channel keeps an independent budget"),
		PriorityLimiter.Evaluate(Request, Policy).IsAllowed());

	NowSeconds = 800.0;
	FOpenMobileHapticsRateLimiter SustainedLimiter(
		[&NowSeconds]()
		{
			return NowSeconds;
		}
	);
	Policy.MaximumChannelSubmissionsPerSecond = 2;
	Request.Channel = TEXT("Sustained");
	Request.Category = TEXT("Gameplay");
	Request.Effect = TEXT("Pulse");
	Request.Priority = EOpenMobileHapticChannelPriority::Normal;
	TestTrue(TEXT("Sustained request zero is allowed"),
		SustainedLimiter.Evaluate(Request, Policy).IsAllowed());
	NowSeconds = 800.5;
	TestTrue(TEXT("Sustained request one is allowed"),
		SustainedLimiter.Evaluate(Request, Policy).IsAllowed());
	NowSeconds = 801.0;
	TestTrue(TEXT("Sustained exact boundary remains allowed"),
		SustainedLimiter.Evaluate(Request, Policy).IsAllowed());
	NowSeconds = 801.5;
	TestTrue(TEXT("Sustained window remains bounded and live"),
		SustainedLimiter.Evaluate(Request, Policy).IsAllowed());

	FOpenMobileHapticsRateLimiter ConcurrentLimiter(
		[]()
		{
			return 900.0;
		}
	);
	Policy.MaximumChannelSubmissionsPerSecond =
		FOpenMobileHapticsRateLimiter::
			HardMaximumChannelSubmissionsPerSecond;
	Policy.MaximumGlobalSubmissionsPerSecond =
		FOpenMobileHapticsRateLimiter::
			HardMaximumGlobalSubmissionsPerSecond;
	Request.Channel = TEXT("Concurrent");
	Request.Category = TEXT("Gameplay");
	Request.Effect = TEXT("ConcurrentPulse");
	FThreadSafeCounter AllowedCount;
	ParallelFor(64,
		[&ConcurrentLimiter, &Policy, Request, &AllowedCount](int32)
		{
			if (ConcurrentLimiter.Evaluate(Request, Policy).IsAllowed())
			{
				AllowedCount.Increment();
			}
		});
	TestEqual(TEXT("Concurrent callers cannot overrun the channel cap"),
		AllowedCount.GetValue(),
		FOpenMobileHapticsRateLimiter::
			HardMaximumChannelSubmissionsPerSecond);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsRateLimitSubsystemPathsTest,
	"OpenMobile.Haptics.RateLimit.SubsystemPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsRateLimitSubsystemPathsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	const TArray<FOpenMobileHapticEffectSettings> SavedEffectOverrides =
		Settings->EffectOverrides;
	const float SavedDefaultMinimumIntervalSeconds =
		Settings->DefaultMinimumIntervalSeconds;
	Settings->NamedLibraries.Reset();
	Settings->EffectOverrides.Reset();
	Settings->DefaultMinimumIntervalSeconds = 0.0f;
	FOpenMobileHapticEffectSettings EffectLimit;
	EffectLimit.Name = TEXT("RateNamed");
	EffectLimit.MinimumIntervalSeconds = 1.0f;
	Settings->EffectOverrides.Add(EffectLimit);

	FMockBackend Backend(TEXT("RateLimitSubsystem"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	FOpenMobileHapticNamedPatternRequest FirstRequest;
	FirstRequest.PatternName = TEXT("RateNamed");
	FirstRequest.Options.Channel = TEXT("NamedRateA");
	const FOpenMobileHapticPlaybackResult First =
		Subsystem->SubmitNamedPattern(FirstRequest);
	TestTrue(TEXT("First named effect is accepted"), First.IsAccepted());

	FOpenMobileHapticNamedPatternRequest RepeatedRequest = FirstRequest;
	RepeatedRequest.Options.Channel = TEXT("NamedRateB");
	const FOpenMobileHapticPlaybackResult Repeated =
		Subsystem->SubmitNamedPattern(RepeatedRequest);
	TestEqual(TEXT("Named effect interval crosses channel boundaries"),
		Repeated.SuppressionReason,
		EOpenMobileHapticSuppressionReason::EffectMinimumInterval);
	TestEqual(TEXT("Expected named suppression has no error"),
		Repeated.Error.Code,
		EOpenMobileHapticErrorCode::None);

	RepeatedRequest.PatternName = TEXT("AnotherNamedEffect");
	const FOpenMobileHapticPlaybackResult Different =
		Subsystem->SubmitNamedPattern(RepeatedRequest);
	TestTrue(TEXT("Different named effect remains eligible"),
		Different.IsAccepted());
	TestEqual(TEXT("Only eligible named work reaches the backend"),
		Backend.NamedSubmissionCount, 2);

	FOpenMobileHapticSemanticRequest FirstSemantic;
	FirstSemantic.Effect = EOpenMobileHapticSemanticEffect::Selection;
	FirstSemantic.Options.Channel = TEXT("MeaningfulUIDifference");
	FirstSemantic.Options.Category = TEXT("UI");
	FirstSemantic.Options.InterruptionPolicy =
		EOpenMobileHapticInterruptionPolicy::Stop;
	TestTrue(TEXT("First UI semantic meaning is accepted"),
		Subsystem->SubmitSemantic(FirstSemantic).IsAccepted());
	FOpenMobileHapticSemanticRequest RestartSemantic = FirstSemantic;
	RestartSemantic.Options.InterruptionPolicy =
		EOpenMobileHapticInterruptionPolicy::Restart;
	TestTrue(TEXT("Different interruption meaning is not coalesced"),
		Subsystem->SubmitSemantic(RestartSemantic).IsAccepted());
	TestEqual(TEXT("Both meaningful UI requests reach the backend"),
		Backend.SemanticSubmissionCount, 2);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	Settings->EffectOverrides = SavedEffectOverrides;
	Settings->DefaultMinimumIntervalSeconds =
		SavedDefaultMinimumIntervalSeconds;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsRateLimitForegroundPolicyTest,
	"OpenMobile.Haptics.RateLimit.ForegroundPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsRateLimitForegroundPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const float SavedDefaultMinimumIntervalSeconds =
		Settings->DefaultMinimumIntervalSeconds;
	const bool bSavedRetainRateLimitState =
		Settings->bRetainRateLimitStateAcrossForeground;
	Settings->DefaultMinimumIntervalSeconds = 1.0f;
	Settings->bRetainRateLimitStateAcrossForeground = true;

	FMockBackend Backend(TEXT("RateLimitForeground"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::BasicVibration;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	Backend.ControlSupport.bStop = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	auto CycleForeground = []()
	{
		FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
			EOpenMobileHapticsLifecycleEvent::WillDeactivate
		);
		FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
			EOpenMobileHapticsLifecycleEvent::WillEnterBackground
		);
		FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
			EOpenMobileHapticsLifecycleEvent::HasEnteredForeground
		);
		FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
			EOpenMobileHapticsLifecycleEvent::HasReactivated
		);
	};

	FOpenMobileHapticOneShotRequest Request;
	Request.DurationSeconds = 0.05f;
	Request.Options.Channel = TEXT("RetainedRateHistory");
	TestTrue(TEXT("Retained-history fixture is accepted"),
		Subsystem->SubmitOneShot(Request).IsAccepted());
	CycleForeground();
	const FOpenMobileHapticPlaybackResult Retained =
		Subsystem->SubmitOneShot(Request);
	TestEqual(TEXT("Foreground retains recent comfort history"),
		Retained.SuppressionReason,
		EOpenMobileHapticSuppressionReason::ChannelMinimumInterval);

	Settings->bRetainRateLimitStateAcrossForeground = false;
	Request.Options.Channel = TEXT("ResetRateHistory");
	TestTrue(TEXT("Reset-history fixture is accepted"),
		Subsystem->SubmitOneShot(Request).IsAccepted());
	CycleForeground();
	TestTrue(TEXT("Configured foreground reset clears limiter history"),
		Subsystem->SubmitOneShot(Request).IsAccepted());

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->DefaultMinimumIntervalSeconds =
		SavedDefaultMinimumIntervalSeconds;
	Settings->bRetainRateLimitStateAcrossForeground =
		bSavedRetainRateLimitState;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsDynamicParameterPolicyTest,
	"OpenMobile.Haptics.Playback.DynamicParameterCoalescing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsDynamicParameterPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsDynamicParameterPolicy Policy;
	Policy.RegisterPlayback(7, 10.0);
	const double MinimumIntervalSeconds = 1.0 / 60.0;

	FOpenMobileHapticDynamicParameterUpdate Intensity;
	Intensity.Intensity = 0.2f;
	FOpenMobileHapticDynamicParameterUpdate Ready;
	TestEqual(TEXT("First tick update is coalesced after initial submission"),
		Policy.Queue(7, Intensity, 10.005, MinimumIntervalSeconds, Ready),
		EOpenMobileHapticsDynamicParameterQueueOutcome::Coalesced);

	FOpenMobileHapticDynamicParameterUpdate Sharpness;
	Sharpness.bUpdateIntensity = false;
	Sharpness.bUpdateSharpness = true;
	Sharpness.Sharpness = 0.8f;
	TestEqual(TEXT("Different parameters merge into one pending batch"),
		Policy.Queue(7, Sharpness, 10.010, MinimumIntervalSeconds, Ready),
		EOpenMobileHapticsDynamicParameterQueueOutcome::Coalesced);
	Intensity.Intensity = 0.7f;
	TestEqual(TEXT("Newest value replaces the pending value"),
		Policy.Queue(7, Intensity, 10.012, MinimumIntervalSeconds, Ready),
		EOpenMobileHapticsDynamicParameterQueueOutcome::Coalesced);

	TArray<FOpenMobileHapticsScheduledDynamicParameterUpdate> Scheduled;
	Policy.CollectReady(10.016, MinimumIntervalSeconds, Scheduled);
	TestTrue(TEXT("Native rate boundary is enforced"), Scheduled.IsEmpty());
	Policy.CollectReady(10.017, MinimumIntervalSeconds, Scheduled);
	TestEqual(TEXT("One native batch becomes ready"), Scheduled.Num(), 1);
	if (Scheduled.Num() == 1)
	{
		TestEqual(TEXT("Ready batch preserves playback ownership"),
			Scheduled[0].RequestId, static_cast<uint64>(7));
		TestEqual(TEXT("Ready batch keeps the latest intensity"),
			Scheduled[0].Update.Intensity, 0.7f);
		TestEqual(TEXT("Ready batch also keeps sharpness"),
			Scheduled[0].Update.Sharpness, 0.8f);
		Policy.MarkAttempted(7, 10.017);
	}

	Intensity.Intensity = 0.4f;
	Policy.Queue(7, Intensity, 10.020, MinimumIntervalSeconds, Ready);
	Policy.RemovePlayback(7);
	Policy.CollectReady(11.0, MinimumIntervalSeconds, Scheduled);
	TestTrue(TEXT("Stopping ownership drops queued updates"),
		Scheduled.IsEmpty());

	FOpenMobileHapticDynamicParameterUpdate Invalid;
	Invalid.Intensity = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite values are rejected"),
		FOpenMobileHapticsDynamicParameterPolicy::IsValid(Invalid));
	Invalid.Intensity = 1.0f;
	Invalid.bUpdateIntensity = false;
	TestFalse(TEXT("Empty updates are rejected"),
		FOpenMobileHapticsDynamicParameterPolicy::IsValid(Invalid));
	Invalid.bUpdateSharpness = true;
	Invalid.Sharpness = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Nonfinite sharpness is rejected"),
		FOpenMobileHapticsDynamicParameterPolicy::IsValid(Invalid));
	Invalid.Sharpness = 1.01f;
	TestFalse(TEXT("Sharpness above the normalized range is rejected"),
		FOpenMobileHapticsDynamicParameterPolicy::IsValid(Invalid));
	Invalid.bUpdateSharpness = false;
	Invalid.bUpdateIntensity = true;
	Invalid.Intensity = -0.01f;
	TestFalse(TEXT("Intensity below the normalized range is rejected"),
		FOpenMobileHapticsDynamicParameterPolicy::IsValid(Invalid));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsDynamicParameterSubsystemTest,
	"OpenMobile.Haptics.Playback.DynamicParameterLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsDynamicParameterSubsystemTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();

	FMockBackend Backend(TEXT("DynamicParameters"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.Capabilities.DynamicParameters =
		EOpenMobileHapticSupportState::Supported;
	Backend.ControlSupport.bStop = true;
	Backend.ControlSupport.bDynamicParameters = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	FOpenMobileHapticPlaybackOptions CombatOptions;
	CombatOptions.Category = TEXT("Combat");
	CombatOptions.Channel = TEXT("DynamicEngine");
	const FOpenMobileHapticPlaybackResult Playback =
		Subsystem->PlayNamedPatternAdvanced(
			TEXT("EngineLoop"),
			1.0f,
			CombatOptions
		);
	TestTrue(TEXT("Dynamic playback has a controllable handle"),
		Playback.Handle.IsValid());
	TestTrue(TEXT("Backend receives initial dynamic parameters"),
		Backend.LastNamedPlaybackParameters.bHasInitialDynamicParameters);
	TestEqual(TEXT("Initial runtime intensity is neutral"),
		Backend.LastNamedPlaybackParameters.InitialDynamicParameters.Intensity,
		1.0f);

	FOpenMobileHapticDynamicParameterUpdate Update;
	Update.Intensity = 0.2f;
	Update.bUpdateSharpness = true;
	Update.Sharpness = 0.75f;
	TestEqual(TEXT("Valid handle update is accepted for coalescing"),
		Subsystem->UpdatePlaybackParameters(Playback.Handle, Update).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	Update.Intensity = 0.7f;
	Subsystem->UpdatePlaybackParameters(Playback.Handle, Update);
	TestEqual(TEXT("Gameplay ticks do not call the backend immediately"),
		Backend.DynamicUpdateCount, 0);
	const double FutureTime = FPlatformTime::Seconds() + 1.0;
	Subsystem->FlushDynamicParameterUpdatesForTests(FutureTime);
	TestEqual(TEXT("Coalesced values use one backend call"),
		Backend.DynamicUpdateCount, 1);
	TestEqual(TEXT("Latest intensity reaches the backend"),
		Backend.LastDynamicUpdate.Intensity, 0.7f);
	TestEqual(TEXT("Sharpness shares the same native batch"),
		Backend.LastDynamicUpdate.Sharpness, 0.75f);

	FOpenMobileHapticDynamicParameterUpdate Invalid = Update;
	Invalid.Intensity = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Nonfinite updates are rejected before queuing"),
		Subsystem->UpdatePlaybackParameters(Playback.Handle, Invalid).Outcome,
		EOpenMobileHapticControlOutcome::Rejected);
	TestEqual(TEXT("Invalid updates leave playback active"),
		Subsystem->GetPlaybackState(Playback.Handle),
		EOpenMobileHapticPlaybackState::Accepted);

	Backend.ControlSupport.bDynamicParameters = false;
	const int32 BeforeUnsupported = Backend.DynamicUpdateCount;
	TestEqual(TEXT("Unsupported backend rejects the update"),
		Subsystem->UpdatePlaybackParameters(Playback.Handle, Update).Outcome,
		EOpenMobileHapticControlOutcome::Unsupported);
	TestEqual(TEXT("Unsupported update never crosses the backend seam"),
		Backend.DynamicUpdateCount, BeforeUnsupported);
	TestEqual(TEXT("Unsupported update leaves playback active"),
		Subsystem->GetPlaybackState(Playback.Handle),
		EOpenMobileHapticPlaybackState::Accepted);
	Backend.ControlSupport.bDynamicParameters = true;

	FOpenMobileHapticPlaybackHandle Unknown;
	Unknown.Id = FGuid(91, 0, 0, 1);
	TestEqual(TEXT("Unknown dynamic handle is stale"),
		Subsystem->UpdatePlaybackParameters(Unknown, Update).Outcome,
		EOpenMobileHapticControlOutcome::StaleHandle);

	Backend.bFailDynamicUpdates = true;
	Subsystem->UpdatePlaybackParameters(Playback.Handle, Update);
	Subsystem->FlushDynamicParameterUpdatesForTests(FutureTime + 1.0);
	TestEqual(TEXT("Native update failure leaves playback active"),
		Subsystem->GetPlaybackState(Playback.Handle),
		EOpenMobileHapticPlaybackState::Accepted);
	TestEqual(TEXT("Native update failure reaches diagnostics"),
		Subsystem->GetDiagnostics().LastError.Code,
		EOpenMobileHapticErrorCode::NativeEngineFailure);
	const int32 AfterNativeFailure = Backend.DynamicUpdateCount;
	Subsystem->UpdatePlaybackParameters(Playback.Handle, Update);
	Subsystem->FlushDynamicParameterUpdatesForTests(FutureTime + 1.001);
	TestEqual(TEXT("Failed native calls still advance the rate limit"),
		Backend.DynamicUpdateCount, AfterNativeFailure);
	Backend.bFailDynamicUpdates = false;

	const int32 BeforeStop = Backend.DynamicUpdateCount;
	Subsystem->UpdatePlaybackParameters(Playback.Handle, Update);
	Subsystem->StopPlayback(Playback.Handle);
	Subsystem->FlushDynamicParameterUpdatesForTests(FutureTime + 2.0);
	TestEqual(TEXT("Stop removes a racing queued update"),
		Backend.DynamicUpdateCount, BeforeStop);

	const FOpenMobileHapticPlaybackResult ResetPlayback =
		Subsystem->PlayNamedPattern(
			TEXT("ResetLoop"),
			1.0f,
			TEXT("DynamicReset")
		);
	Subsystem->UpdatePlaybackParameters(ResetPlayback.Handle, Update);
	const int32 BeforeReset = Backend.DynamicUpdateCount;
	Backend.Emit(1, EOpenMobileHapticPlaybackState::Failed, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	Subsystem->FlushDynamicParameterUpdatesForTests(FutureTime + 3.0);
	TestEqual(TEXT("Engine reset drops its queued update"),
		Backend.DynamicUpdateCount, BeforeReset);
	TestEqual(TEXT("Reset playback handle becomes stale"),
		Subsystem->UpdatePlaybackParameters(ResetPlayback.Handle, Update).Outcome,
		EOpenMobileHapticControlOutcome::StaleHandle);

	FOpenMobileHapticPlaybackOptions PolicyOptions = CombatOptions;
	PolicyOptions.Channel = TEXT("DynamicPolicy");
	const FOpenMobileHapticPlaybackResult PolicyPlayback =
		Subsystem->PlayNamedPatternAdvanced(
			TEXT("PolicyLoop"),
			1.0f,
			PolicyOptions
		);
	FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	Policy.MasterIntensity = 0.5f;
	Policy.CategoryScales.Add(TEXT("Combat"), 0.5f);
	TestEqual(TEXT("Policy change remains accepted"),
		Subsystem->SetUserPolicy(Policy).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	Subsystem->FlushDynamicParameterUpdatesForTests(FutureTime + 4.0);
	TestEqual(TEXT("Active player receives master and category scales"),
		Backend.LastDynamicUpdate.Intensity, 0.25f);
	TestEqual(TEXT("Policy update preserves the owning request"),
		Backend.LastDynamicToken.PlaybackHandle, PolicyPlayback.Handle);

	Update.bUpdateSharpness = false;
	Update.Intensity = 0.8f;
	Subsystem->UpdatePlaybackParameters(PolicyPlayback.Handle, Update);
	Subsystem->FlushDynamicParameterUpdatesForTests(FutureTime + 5.0);
	TestEqual(TEXT("Runtime and policy intensity compose once"),
		Backend.LastDynamicUpdate.Intensity, 0.2f);
	Subsystem->SetHapticsEnabled(false);
	Subsystem->FlushDynamicParameterUpdatesForTests(FutureTime + 6.0);
	TestEqual(TEXT("Disabling policy mutes active compatible playback"),
		Backend.LastDynamicUpdate.Intensity, 0.0f);
	Subsystem->SetHapticsEnabled(true);
	Subsystem->FlushDynamicParameterUpdatesForTests(FutureTime + 7.0);
	TestEqual(TEXT("Re-enabling policy restores composed intensity"),
		Backend.LastDynamicUpdate.Intensity, 0.2f);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsGlobalEnableTest,
	"OpenMobile.Haptics.Policy.GlobalEnable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsGlobalEnableTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const bool SavedEnabledByDefault = Settings->bEnabledByDefault;
	const bool SavedCriticalDefault =
		Settings->bAllowCriticalFeedbackWhenDisabledByDefault;
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	const float SavedDefaultMinimumIntervalSeconds =
		Settings->DefaultMinimumIntervalSeconds;
	Settings->bEnabledByDefault = false;
	Settings->bAllowCriticalFeedbackWhenDisabledByDefault = false;
	Settings->NamedLibraries.Reset();
	Settings->DefaultMinimumIntervalSeconds = 0.0f;

	FMockBackend Backend(TEXT("GlobalEnable"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.Capabilities.RichHaptics =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Scheduling =
		EOpenMobileHapticSupportState::Supported;
	Backend.ControlSupport.bStop = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* FirstGameInstance = NewObject<UGameInstance>();
	FSubsystemCollection<UGameInstanceSubsystem> FirstCollection;
	UOpenMobileHapticsSubsystem* FirstSubsystem =
		NewObject<UOpenMobileHapticsSubsystem>(FirstGameInstance);
	FirstSubsystem->Initialize(FirstCollection);
	TestFalse(TEXT("Project default initializes the runtime switch"),
		FirstSubsystem->IsHapticsEnabled());
	TestFalse(TEXT("Native facade reports the runtime switch"),
		static_cast<IOpenMobileHaptics*>(FirstSubsystem)
			->IsHapticsEnabledNative());
	TestEqual(TEXT("Disabled policy is visible in capabilities"),
		FirstSubsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::DisabledByPolicy);

	FOpenMobileHapticUserPolicy PreservedPolicy =
		FirstSubsystem->GetUserPolicy();
	PreservedPolicy.MasterIntensity = 0.65f;
	PreservedPolicy.CategoryScales.Add(TEXT("Gameplay"), 0.75f);
	FirstSubsystem->SetUserPolicy(PreservedPolicy);
	TestEqual(TEXT("Dedicated enable operation is accepted"),
		FirstSubsystem->SetHapticsEnabled(true).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestTrue(TEXT("Dedicated getter reflects the enabled state"),
		FirstSubsystem->IsHapticsEnabled());
	TestEqual(TEXT("Dedicated switch preserves master intensity"),
		FirstSubsystem->GetUserPolicy().MasterIntensity, 0.65f);
	TestEqual(TEXT("Dedicated switch preserves category preferences"),
		FirstSubsystem->GetUserPolicy().CategoryScales.FindRef(TEXT("Gameplay")),
		0.75f);
	FirstSubsystem->Deinitialize();

	UGameInstance* SecondGameInstance = NewObject<UGameInstance>();
	FSubsystemCollection<UGameInstanceSubsystem> SecondCollection;
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(SecondGameInstance);
	Subsystem->Initialize(SecondCollection);
	TestFalse(TEXT("Recreated subsystem resolves the project default again"),
		Subsystem->IsHapticsEnabled());
	TestFalse(TEXT("Runtime preference is not written to a global save"),
		Subsystem->GetUserPolicy().bEnabled);
	TestFalse(TEXT("Critical feedback is opt-in by default"),
		Subsystem->GetUserPolicy()
			.bAllowCriticalFeedbackWhenDisabled);
	Subsystem->SetHapticsEnabled(true);

	FOpenMobileHapticOneShotRequest ActiveRequest;
	ActiveRequest.DurationSeconds = 1.0f;
	ActiveRequest.Options.Channel = TEXT("GlobalEnablePlayback");
	ActiveRequest.Options.Category = TEXT("Gameplay");
	ActiveRequest.Options.OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;
	const FOpenMobileHapticPlaybackResult Active =
		Subsystem->SubmitOneShot(ActiveRequest);
	FOpenMobileHapticOneShotRequest QueuedRequest = ActiveRequest;
	QueuedRequest.Options.OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::Queue;
	const FOpenMobileHapticPlaybackResult Queued =
		Subsystem->SubmitOneShot(QueuedRequest);
	TestTrue(TEXT("Nonessential playback starts before disable"),
		Active.IsAccepted());
	TestEqual(TEXT("Conflicting nonessential work is queued"),
		Queued.State, EOpenMobileHapticPlaybackState::Scheduled);
	const int32 StopsBeforeDisable = Backend.StopPlaybackCount;
	TestEqual(TEXT("Disabling active feedback is accepted"),
		Subsystem->SetHapticsEnabled(false).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Active nonessential feedback is stopped"),
		Subsystem->GetPlaybackState(Active.Handle),
		EOpenMobileHapticPlaybackState::Stopped);
	TestEqual(TEXT("Queued nonessential feedback is cancelled"),
		Subsystem->GetPlaybackState(Queued.Handle),
		EOpenMobileHapticPlaybackState::Cancelled);
	TestEqual(TEXT("Disable stops only the active native playback"),
		Backend.StopPlaybackCount, StopsBeforeDisable + 1);
	TestEqual(TEXT("Disable releases all nonessential queue capacity"),
		Subsystem->GetDiagnostics().QueuedPlaybackCount, 0);

	Subsystem->SetHapticsEnabled(true);
	FOpenMobileHapticNamedPatternRequest ScheduledRequest;
	ScheduledRequest.PatternName = TEXT("ScheduledPolicyPattern");
	ScheduledRequest.Options.Channel = TEXT("ScheduledPolicyPlayback");
	ScheduledRequest.Options.Category = TEXT("Gameplay");
	ScheduledRequest.Options.Schedule.Mode =
		EOpenMobileHapticScheduleMode::Relative;
	ScheduledRequest.Options.Schedule.TimeSeconds = 0.25;
	const FOpenMobileHapticPlaybackResult Scheduled =
		Subsystem->SubmitNamedPattern(ScheduledRequest);
	TestEqual(TEXT("Future nonessential feedback is scheduled"),
		Scheduled.State, EOpenMobileHapticPlaybackState::Scheduled);
	const int32 StopsBeforeScheduledDisable = Backend.StopPlaybackCount;
	Subsystem->SetHapticsEnabled(false);
	TestEqual(TEXT("Disable cancels scheduled nonessential feedback"),
		Subsystem->GetPlaybackState(Scheduled.Handle),
		EOpenMobileHapticPlaybackState::Cancelled);
	TestEqual(TEXT("Scheduled native work is stopped before its start"),
		Backend.StopPlaybackCount, StopsBeforeScheduledDisable + 1);

	FOpenMobileHapticOneShotRequest CriticalAlert;
	CriticalAlert.DurationSeconds = 1.0f;
	CriticalAlert.Options.Channel = TEXT("CriticalAlert");
	CriticalAlert.Options.Category = TEXT("Alerts");
	CriticalAlert.Options.Priority =
		EOpenMobileHapticChannelPriority::Critical;
	TestEqual(TEXT("Critical alert remains blocked without opt-in"),
		Subsystem->SubmitOneShot(CriticalAlert).Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);

	FOpenMobileHapticUserPolicy CriticalPolicy = Subsystem->GetUserPolicy();
	CriticalPolicy.bAllowCriticalFeedbackWhenDisabled = true;
	Subsystem->SetUserPolicy(CriticalPolicy);
	const FOpenMobileHapticPlaybackResult Critical =
		Subsystem->SubmitOneShot(CriticalAlert);
	TestTrue(TEXT("Opted-in critical alert can play while globally disabled"),
		Critical.IsAccepted());
	FOpenMobileHapticOneShotRequest CriticalAccessibility = CriticalAlert;
	CriticalAccessibility.Options.Channel = TEXT("CriticalAccessibility");
	CriticalAccessibility.Options.Category = TEXT("Accessibility");
	const FOpenMobileHapticPlaybackResult Accessibility =
		Subsystem->SubmitOneShot(CriticalAccessibility);
	TestTrue(TEXT("Opted-in critical accessibility feedback can play"),
		Accessibility.IsAccepted());
	FOpenMobileHapticOneShotRequest ScheduledCritical = CriticalAlert;
	ScheduledCritical.Options.Channel = TEXT("ScheduledCriticalAlert");
	ScheduledCritical.Options.Schedule.Mode =
		EOpenMobileHapticScheduleMode::Relative;
	ScheduledCritical.Options.Schedule.TimeSeconds = 0.2;
	const FOpenMobileHapticPlaybackResult CriticalDelayed =
		Subsystem->SubmitOneShot(ScheduledCritical);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	const TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> CriticalDelayedGuard =
		Backend.LastOneShotPlaybackParameters.ScheduledStartGuard;
	TestEqual(TEXT("Opted-in critical delayed feedback is scheduled"),
		CriticalDelayed.State, EOpenMobileHapticPlaybackState::Scheduled);
	Subsystem->SetHapticsEnabled(true);
	Subsystem->SetHapticsEnabled(false);
	TestTrue(TEXT("Switch changes preserve permitted critical delayed work"),
		CriticalDelayedGuard
		&& CriticalDelayedGuard->CanStart(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()));
	TestEqual(TEXT("Permitted critical delayed work stays scheduled"),
		Subsystem->GetPlaybackState(CriticalDelayed.Handle),
		EOpenMobileHapticPlaybackState::Scheduled);
	FOpenMobileHapticOneShotRequest WrongCategory = CriticalAlert;
	WrongCategory.Options.Channel = TEXT("CriticalGameplay");
	WrongCategory.Options.Category = TEXT("Gameplay");
	TestEqual(TEXT("Critical gameplay cannot bypass the global switch"),
		Subsystem->SubmitOneShot(WrongCategory).Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	FOpenMobileHapticOneShotRequest WrongPriority = CriticalAlert;
	WrongPriority.Options.Channel = TEXT("NormalAlert");
	WrongPriority.Options.Priority =
		EOpenMobileHapticChannelPriority::Normal;
	TestEqual(TEXT("Ordinary alerts cannot bypass the global switch"),
		Subsystem->SubmitOneShot(WrongPriority).Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);

	Subsystem->SetHapticsEnabled(true);
	ActiveRequest.Options.Channel = TEXT("OrdinaryAlongsideCritical");
	const FOpenMobileHapticPlaybackResult Ordinary =
		Subsystem->SubmitOneShot(ActiveRequest);
	const int32 StopsBeforeSelectiveDisable = Backend.StopPlaybackCount;
	Subsystem->SetHapticsEnabled(false);
	TestEqual(TEXT("Selective disable preserves opted-in critical feedback"),
		Subsystem->GetPlaybackState(Critical.Handle),
		EOpenMobileHapticPlaybackState::Accepted);
	TestEqual(TEXT("Selective disable preserves critical accessibility feedback"),
		Subsystem->GetPlaybackState(Accessibility.Handle),
		EOpenMobileHapticPlaybackState::Accepted);
	TestEqual(TEXT("Selective disable stops ordinary feedback"),
		Subsystem->GetPlaybackState(Ordinary.Handle),
		EOpenMobileHapticPlaybackState::Stopped);
	TestEqual(TEXT("Selective disable stops one native request"),
		Backend.StopPlaybackCount, StopsBeforeSelectiveDisable + 1);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->bEnabledByDefault = SavedEnabledByDefault;
	Settings->bAllowCriticalFeedbackWhenDisabledByDefault = SavedCriticalDefault;
	Settings->NamedLibraries = SavedLibraries;
	Settings->DefaultMinimumIntervalSeconds =
		SavedDefaultMinimumIntervalSeconds;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsTypeDefaultsTest,
	"OpenMobile.Haptics.API.TypeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsTypeDefaultsTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const FOpenMobileHapticPlaybackHandle Handle;
	TestFalse(TEXT("Default playback handle is invalid"), Handle.IsValid());

	const FOpenMobileHapticCapabilities Capabilities;
	TestEqual(
		TEXT("Default capabilities report an unsupported platform"),
		Capabilities.Availability,
		EOpenMobileHapticAvailability::UnsupportedPlatform
	);
	TestEqual(
		TEXT("Unknown feature support remains explicit"),
		Capabilities.BasicVibration,
		EOpenMobileHapticSupportState::Unknown
	);

	const FOpenMobileHapticPlaybackOptions Options;
	TestEqual(
		TEXT("Default channel is Gameplay"),
		Options.Channel,
		FName(TEXT("Gameplay"))
	);
	TestEqual(
		TEXT("Default overlap replaces same-channel work"),
		Options.OverlapPolicy,
		EOpenMobileHapticOverlapPolicy::Replace
	);
	TestEqual(
		TEXT("Default schedule is immediate"),
		Options.Schedule.Mode,
		EOpenMobileHapticScheduleMode::Immediate
	);
	TestFalse(TEXT("Looping requires explicit opt-in"), Options.Loop.bLoop);
	const FOpenMobileHapticParameterCurve Curve;
	const FOpenMobileHapticCurvePoint CurvePoint;
	TestEqual(TEXT("Parameter curves default to intensity control"),
		Curve.Parameter,
		EOpenMobileHapticCurveParameter::IntensityControl);
	TestEqual(TEXT("Default intensity curve points are neutral"),
		CurvePoint.Value, 1.0f);
	const FOpenMobileHapticCookedCurvePoint CookedCurvePoint;
	TestEqual(TEXT("Default cooked intensity curve points are neutral"),
		CookedCurvePoint.Value, static_cast<uint16>(MAX_uint16));

	const FOpenMobileHapticUserPolicy Policy;
	TestTrue(TEXT("Haptics are enabled by default"), Policy.bEnabled);
	TestFalse(TEXT("Critical feedback requires explicit player opt-in"),
		Policy.bAllowCriticalFeedbackWhenDisabled);
	TestEqual(TEXT("Master intensity defaults to one"), Policy.MasterIntensity, 1.0f);
	const FOpenMobileHapticPlaybackResult SuppressionResult;
	TestEqual(TEXT("Playback results default to no suppression reason"),
		SuppressionResult.SuppressionReason,
		EOpenMobileHapticSuppressionReason::None);
	const FOpenMobileHapticDynamicParameterUpdate DynamicUpdate;
	TestTrue(TEXT("Runtime updates target intensity by default"),
		DynamicUpdate.bUpdateIntensity);
	TestEqual(TEXT("Runtime intensity defaults to neutral"),
		DynamicUpdate.Intensity, 1.0f);
	TestFalse(TEXT("Runtime sharpness requires explicit opt-in"),
		DynamicUpdate.bUpdateSharpness);
	TestEqual(TEXT("Runtime sharpness defaults to neutral"),
		DynamicUpdate.Sharpness, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsSettingsContractTest,
	"OpenMobile.Haptics.Settings.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsSettingsContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticsSettings* Settings =
		NewObject<UOpenMobileHapticsSettings>();
	TestEqual(
		TEXT("Haptics settings use the OpenMobile category"),
		Settings->GetCategoryName(),
		FName(TEXT("OpenMobile"))
	);
	TestEqual(
		TEXT("Haptics settings keep their own section"),
		Settings->GetSectionName(),
		FName(TEXT("OpenMobile Haptics"))
	);
#if WITH_METADATA
	TestEqual(
		TEXT("Haptics display name matches its section"),
		Settings->GetClass()->GetMetaData(TEXT("DisplayName")),
		FString(TEXT("OpenMobile Haptics"))
	);
#endif
	TestTrue(
		TEXT("Haptics settings use default config"),
		Settings->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig)
	);
	const FFloatProperty* IntensityProperty = FindFProperty<FFloatProperty>(
		UOpenMobileHapticsSettings::StaticClass(),
		GET_MEMBER_NAME_CHECKED(
			UOpenMobileHapticsSettings,
			DefaultMasterIntensity
		)
	);
	TestTrue(
		TEXT("Master intensity is serialized to config"),
		IntensityProperty && IntensityProperty->HasAnyPropertyFlags(CPF_Config)
	);
	const FBoolProperty* EnabledProperty = FindFProperty<FBoolProperty>(
		UOpenMobileHapticsSettings::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UOpenMobileHapticsSettings, bEnabledByDefault)
	);
	const FBoolProperty* CriticalDefaultProperty = FindFProperty<FBoolProperty>(
		UOpenMobileHapticsSettings::StaticClass(),
		GET_MEMBER_NAME_CHECKED(
			UOpenMobileHapticsSettings,
			bAllowCriticalFeedbackWhenDisabledByDefault
		)
	);
	TestTrue(TEXT("Enable default is serialized to config"),
		EnabledProperty && EnabledProperty->HasAnyPropertyFlags(CPF_Config));
	TestTrue(TEXT("Critical-feedback default is serialized to config"),
		CriticalDefaultProperty
			&& CriticalDefaultProperty->HasAnyPropertyFlags(CPF_Config));
	TestTrue(TEXT("Haptics are enabled by default"), Settings->bEnabledByDefault);
	TestFalse(TEXT("Critical feedback is disabled by default"),
		Settings->bAllowCriticalFeedbackWhenDisabledByDefault);
	TestEqual(
		TEXT("Default project intensity is one"),
		Settings->DefaultMasterIntensity,
		1.0f
	);
	TestEqual(TEXT("Five default channels are present"), Settings->Channels.Num(), 5);
	const FName ExpectedChannelNames[] = {
		TEXT("UI"),
		TEXT("Gameplay"),
		TEXT("Alerts"),
		TEXT("Cinematic"),
		TEXT("Critical")
	};
	const EOpenMobileHapticChannelPriority ExpectedChannelPriorities[] = {
		EOpenMobileHapticChannelPriority::Normal,
		EOpenMobileHapticChannelPriority::Normal,
		EOpenMobileHapticChannelPriority::High,
		EOpenMobileHapticChannelPriority::Normal,
		EOpenMobileHapticChannelPriority::Critical
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedChannelNames); ++Index)
	{
		TestEqual(TEXT("Default channel name is stable"),
			Settings->Channels[Index].Name, ExpectedChannelNames[Index]);
		TestEqual(TEXT("Default channel priority is stable"),
			Settings->Channels[Index].Priority,
			ExpectedChannelPriorities[Index]);
		TestTrue(TEXT("Default channel has active capacity"),
			Settings->Channels[Index].MaximumActiveHandles > 0);
		TestTrue(TEXT("Default channel has queued capacity"),
			Settings->Channels[Index].MaximumQueueDepth > 0);
		TestTrue(TEXT("Default channel has a bounded submission rate"),
			Settings->Channels[Index].MaximumSubmissionsPerSecond > 0
				&& Settings->Channels[Index].MaximumSubmissionsPerSecond
					<= FOpenMobileHapticsRateLimiter::
						HardMaximumChannelSubmissionsPerSecond);
		TestTrue(TEXT("Default mix fallback cannot recurse"),
			Settings->Channels[Index].UnsupportedMixFallbackPolicy
				!= EOpenMobileHapticOverlapPolicy::MixWhenSupported);
	}
	TestEqual(
		TEXT("Default channel is Gameplay"),
		Settings->DefaultChannel,
		FName(TEXT("Gameplay"))
	);
	TestTrue(TEXT("Android custom vibration is packaged by default"),
		Settings->bEnableAndroidCustomVibration);
	const FBoolProperty* AndroidCustomProperty = FindFProperty<FBoolProperty>(
		UOpenMobileHapticsSettings::StaticClass(),
		GET_MEMBER_NAME_CHECKED(
			UOpenMobileHapticsSettings,
			bEnableAndroidCustomVibration
		)
	);
	TestTrue(TEXT("Android custom vibration is a config property"),
		AndroidCustomProperty
			&& AndroidCustomProperty->HasAnyPropertyFlags(CPF_Config));
	TestEqual(TEXT("Portable patterns have a bounded event count"),
		Settings->MaximumPatternEventCount, 128);
	TestEqual(TEXT("Portable patterns have a bounded curve count"),
		Settings->MaximumPatternCurveCount, 16);
	TestEqual(TEXT("Portable curves have a bounded point count"),
		Settings->MaximumPatternCurvePointCount, 256);
	TestEqual(TEXT("Runtime parameter calls have a bounded rate"),
		Settings->MaximumDynamicParameterUpdatesPerSecond, 60);
	TestEqual(TEXT("Equivalent UI requests use a short debounce"),
		Settings->UIRequestDebounceSeconds, 0.02f);
	TestTrue(TEXT("Foreground transitions retain comfort history by default"),
		Settings->bRetainRateLimitStateAcrossForeground);
	TestEqual(TEXT("Prepared patterns have a default memory budget"),
		Settings->MaximumPreparedPatternMemoryKilobytes, 4096);
	TestEqual(TEXT("Prepared patterns have a finite idle lifetime"),
		Settings->PreparedPatternIdleLifetimeSeconds, 30.0f);
	TestEqual(TEXT("Overlap queues have a finite default maximum age"),
		Settings->MaximumQueuedRequestAgeSeconds, 1.0f);
	const FIntProperty* CurveCountProperty = FindFProperty<FIntProperty>(
		UOpenMobileHapticsSettings::StaticClass(),
		GET_MEMBER_NAME_CHECKED(
			UOpenMobileHapticsSettings,
			MaximumPatternCurveCount
		)
	);
	const FIntProperty* CurvePointCountProperty = FindFProperty<FIntProperty>(
		UOpenMobileHapticsSettings::StaticClass(),
		GET_MEMBER_NAME_CHECKED(
			UOpenMobileHapticsSettings,
			MaximumPatternCurvePointCount
		)
	);
	TestTrue(TEXT("Curve count is a config property"),
		CurveCountProperty
			&& CurveCountProperty->HasAnyPropertyFlags(CPF_Config));
	TestTrue(TEXT("Curve point count is a config property"),
		CurvePointCountProperty
			&& CurvePointCountProperty->HasAnyPropertyFlags(CPF_Config));
	TestEqual(TEXT("Portable timing has a stable minimum granularity"),
		Settings->MinimumPatternGranularitySeconds, 0.001f);

	TArray<FString> Errors;
	TestTrue(TEXT("Default settings validate"), Settings->Validate(Errors));
	TestTrue(TEXT("Default validation has no errors"), Errors.IsEmpty());

	Settings->DefaultMasterIntensity =
		std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite intensity is invalid"), Settings->Validate(Errors));
	Settings->DefaultMasterIntensity = 1.0f;

	FOpenMobileHapticChannelSettings DuplicateChannel = Settings->Channels[0];
	DuplicateChannel.Name = TEXT("ui");
	Settings->Channels.Add(DuplicateChannel);
	TestFalse(TEXT("Case-conflicting channels are invalid"), Settings->Validate(Errors));
	Settings->Channels.Pop();
	Settings->Channels[0].Priority =
		static_cast<EOpenMobileHapticChannelPriority>(MAX_uint8);
	TestFalse(TEXT("Unknown channel priorities are invalid"),
		Settings->Validate(Errors));
	Settings->Channels[0].Priority = EOpenMobileHapticChannelPriority::Normal;
	Settings->Channels[0].MaximumActiveHandles =
		Settings->MaximumActiveHandles + 1;
	TestFalse(TEXT("Channel active capacity cannot exceed the global bound"),
		Settings->Validate(Errors));
	Settings->Channels[0].MaximumActiveHandles = 4;
	Settings->Channels[0].UnsupportedMixFallbackPolicy =
		EOpenMobileHapticOverlapPolicy::MixWhenSupported;
	TestFalse(TEXT("Mix fallback cannot select itself"),
		Settings->Validate(Errors));
	Settings->Channels[0].UnsupportedMixFallbackPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;
	Settings->Channels[0].MaximumSubmissionsPerSecond = 0;
	TestFalse(TEXT("Zero channel submission rate is invalid"),
		Settings->Validate(Errors));
	Settings->Channels[0].MaximumSubmissionsPerSecond =
		FOpenMobileHapticsRateLimiter::
			HardMaximumChannelSubmissionsPerSecond + 1;
	TestFalse(TEXT("Channel rate cannot exceed the hard comfort limit"),
		Settings->Validate(Errors));
	Settings->Channels[0].MaximumSubmissionsPerSecond = 20;
	Settings->MaximumQueuedRequestAgeSeconds =
		std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Queue age must be finite"), Settings->Validate(Errors));
	Settings->MaximumQueuedRequestAgeSeconds = 0.0f;
	TestFalse(TEXT("Queue age must be positive"), Settings->Validate(Errors));
	Settings->MaximumQueuedRequestAgeSeconds = 1.0f;

	FOpenMobileHapticEffectSettings Effect;
	Effect.Name = TEXT("Confirm");
	Settings->EffectOverrides = {Effect, Effect};
	TestFalse(TEXT("Duplicate effects are invalid"), Settings->Validate(Errors));
	Settings->EffectOverrides.Reset();

	FOpenMobileHapticNamedLibrarySettings MissingLibrary;
	MissingLibrary.Name = TEXT("Missing");
	MissingLibrary.Asset = FSoftObjectPath(TEXT("/Game/Haptics/Missing.Missing"));
	Settings->NamedLibraries.Add(MissingLibrary);
	TestFalse(TEXT("Missing library assets are invalid"), Settings->Validate(Errors));
	Settings->NamedLibraries.Reset();

	Settings->MaximumFiniteRepeatCount = 0;
	TestFalse(TEXT("Zero loop limit is invalid"), Settings->Validate(Errors));
	Settings->MaximumFiniteRepeatCount = 32;
	Settings->MaximumPatternCurveCount = 129;
	TestFalse(TEXT("Excessive curve count is invalid"),
		Settings->Validate(Errors));
	Settings->MaximumPatternCurveCount = 16;
	Settings->MaximumPatternCurvePointCount = 0;
	TestFalse(TEXT("Zero curve point limit is invalid"),
		Settings->Validate(Errors));
	Settings->MaximumPatternCurvePointCount = 256;
	Settings->MaximumDynamicParameterUpdatesPerSecond = 0;
	TestFalse(TEXT("Zero runtime parameter rate is invalid"),
		Settings->Validate(Errors));
	Settings->MaximumDynamicParameterUpdatesPerSecond = 60;
	Settings->MaximumSubmissionsPerSecond =
		FOpenMobileHapticsRateLimiter::
			HardMaximumGlobalSubmissionsPerSecond + 1;
	TestFalse(TEXT("Global rate cannot exceed the hard comfort limit"),
		Settings->Validate(Errors));
	Settings->MaximumSubmissionsPerSecond = 30;
	Settings->UIRequestDebounceSeconds =
		std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("UI request debounce must be finite"),
		Settings->Validate(Errors));
	Settings->UIRequestDebounceSeconds = 0.02f;
	Settings->MaximumPreparedPatternMemoryKilobytes = 0;
	TestFalse(TEXT("Zero prepared pattern memory is invalid"),
		Settings->Validate(Errors));
	Settings->MaximumPreparedPatternMemoryKilobytes = 4096;
	Settings->PreparedPatternIdleLifetimeSeconds = 0.0f;
	TestFalse(TEXT("Zero prepared pattern idle lifetime is invalid"),
		Settings->Validate(Errors));
	Settings->PreparedPatternIdleLifetimeSeconds = 30.0f;

	Settings->BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::AllowAll;
	TestFalse(TEXT("Unrestricted background haptics are invalid"), Settings->Validate(Errors));
	Settings->BackgroundPolicy = EOpenMobileHapticBackgroundPolicy::StopAll;

	Settings->bEnableCustomPlayback = false;
	TestFalse(TEXT("Disabled custom playback cannot package Android vibration"), Settings->Validate(Errors));
	Settings->bEnableAndroidCustomVibration = false;
	Settings->IOS.bEnableCoreHaptics = false;
	TestFalse(TEXT("Disabled Core Haptics cannot package AHAP"), Settings->Validate(Errors));
	Settings->IOS.bPackageAHAPResources = false;
	TestTrue(TEXT("Semantic-only platform settings validate"), Settings->Validate(Errors));

	const FString ConfigPath = FPaths::CreateTempFilename(
		*FPaths::ProjectIntermediateDir(),
		TEXT("OpenMobileHapticsSettings"),
		TEXT(".ini")
	);
	Settings->bEnabledByDefault = false;
	Settings->bAllowCriticalFeedbackWhenDisabledByDefault = true;
	Settings->DefaultMasterIntensity = 0.75f;
	Settings->DefaultChannel = TEXT("UI");
	Settings->MaximumQueuedHandles = 12;
	Settings->MaximumQueuedRequestAgeSeconds = 2.5f;
	Settings->Channels[0].MaximumActiveHandles = 3;
	Settings->Channels[0].UnsupportedMixFallbackPolicy =
		EOpenMobileHapticOverlapPolicy::Queue;
	Settings->Channels[0].MaximumSubmissionsPerSecond = 12;
	Settings->MaximumPatternCurveCount = 12;
	Settings->MaximumPatternCurvePointCount = 192;
	Settings->MaximumDynamicParameterUpdatesPerSecond = 90;
	Settings->MaximumPreparedPatternMemoryKilobytes = 1024;
	Settings->PreparedPatternIdleLifetimeSeconds = 12.5f;
	Settings->SelectionDebounceSeconds = 0.06f;
	Settings->UIRequestDebounceSeconds = 0.03f;
	Settings->bRetainRateLimitStateAcrossForeground = false;
	Settings->Channels[0].IntensityScale = 0.6f;
	Effect.IntensityScale = 0.8f;
	Settings->EffectOverrides.Add(Effect);
	Settings->NamedLibraries.Add(MissingLibrary);
	Settings->SaveConfig(CPF_Config, *ConfigPath, GConfig, false);
	UOpenMobileHapticsSettings* Loaded =
		NewObject<UOpenMobileHapticsSettings>();
	Loaded->LoadConfig(UOpenMobileHapticsSettings::StaticClass(), *ConfigPath);
	IFileManager::Get().Delete(*ConfigPath, false, true, true);
	TestFalse(
		TEXT("Enable default survives editor restart serialization"),
		Loaded->bEnabledByDefault
	);
	TestTrue(TEXT("Critical default survives editor restart serialization"),
		Loaded->bAllowCriticalFeedbackWhenDisabledByDefault);
	TestEqual(
		TEXT("Master intensity survives editor restart serialization"),
		Loaded->DefaultMasterIntensity,
		0.75f
	);
	TestEqual(
		TEXT("Default channel survives editor restart serialization"),
		Loaded->DefaultChannel,
		FName(TEXT("UI"))
	);
	TestEqual(
		TEXT("Queue limit survives editor restart serialization"),
		Loaded->MaximumQueuedHandles,
		12
	);
	TestEqual(TEXT("Queue age survives editor restart serialization"),
		Loaded->MaximumQueuedRequestAgeSeconds, 2.5f);
	TestEqual(TEXT("Channel active limit survives serialization"),
		Loaded->Channels[0].MaximumActiveHandles, 3);
	TestEqual(TEXT("Channel mix fallback survives serialization"),
		Loaded->Channels[0].UnsupportedMixFallbackPolicy,
		EOpenMobileHapticOverlapPolicy::Queue);
	TestEqual(TEXT("Channel rate survives serialization"),
		Loaded->Channels[0].MaximumSubmissionsPerSecond, 12);
	TestEqual(
		TEXT("Curve limit survives editor restart serialization"),
		Loaded->MaximumPatternCurveCount,
		12
	);
	TestEqual(
		TEXT("Curve point limit survives editor restart serialization"),
		Loaded->MaximumPatternCurvePointCount,
		192
	);
	TestEqual(
		TEXT("Runtime parameter rate survives editor restart serialization"),
		Loaded->MaximumDynamicParameterUpdatesPerSecond,
		90
	);
	TestEqual(
		TEXT("Prepared memory survives editor restart serialization"),
		Loaded->MaximumPreparedPatternMemoryKilobytes,
		1024
	);
	TestEqual(
		TEXT("Prepared idle lifetime survives editor restart serialization"),
		Loaded->PreparedPatternIdleLifetimeSeconds,
		12.5f
	);
	TestEqual(
		TEXT("Rate limit survives editor restart serialization"),
		Loaded->SelectionDebounceSeconds,
		0.06f
	);
	TestEqual(TEXT("UI debounce survives editor restart serialization"),
		Loaded->UIRequestDebounceSeconds, 0.03f);
	TestFalse(TEXT("Foreground limiter policy survives serialization"),
		Loaded->bRetainRateLimitStateAcrossForeground);
	TestFalse(
		TEXT("Android packaging override survives serialization"),
		Loaded->bEnableAndroidCustomVibration
	);
	TestFalse(
		TEXT("Apple engine override survives serialization"),
		Loaded->IOS.bEnableCoreHaptics
	);
	TestEqual(
		TEXT("Channel defaults are not duplicated during reload"),
		Loaded->Channels.Num(),
		5
	);
	TestEqual(
		TEXT("Nested channel settings survive serialization"),
		Loaded->Channels[0].IntensityScale,
		0.6f
	);
	TestEqual(
		TEXT("Effect overrides survive serialization"),
		Loaded->EffectOverrides.Num(),
		1
	);
	TestEqual(
		TEXT("Named library paths survive serialization"),
		Loaded->NamedLibraries[0].Asset,
		MissingLibrary.Asset
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsUnsupportedEditorTest,
	"OpenMobile.Haptics.API.UnsupportedEditor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsUnsupportedEditorTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	const FOpenMobileHapticCapabilities Capabilities =
		Subsystem->GetHapticCapabilities();
	TestEqual(
		TEXT("Editor capability is unsupported"),
		Capabilities.Availability,
		EOpenMobileHapticAvailability::UnsupportedPlatform
	);

	const TArray<FOpenMobileHapticPlaybackResult> Results = {
		Subsystem->PlaySemanticFeedback(
			EOpenMobileHapticSemanticEffect::Selection
		),
		Subsystem->Vibrate(),
		Subsystem->PlayNamedPattern(TEXT("UI_Confirm"))
	};
	for (const FOpenMobileHapticPlaybackResult& Result : Results)
	{
		TestEqual(
			TEXT("Unsupported play is rejected"),
			Result.Outcome,
			EOpenMobileHapticPlaybackOutcome::Rejected
		);
		TestEqual(
			TEXT("Unsupported play has a typed error"),
			Result.Error.Code,
			EOpenMobileHapticErrorCode::UnsupportedFeature
		);
		TestEqual(
			TEXT("Unsupported play retains the common error"),
			Result.Error.CommonCode,
			EOpenMobileErrorCode::NotSupported
		);
		TestFalse(
			TEXT("Rejected work has no playback handle"),
			Result.Handle.IsValid()
		);
	}

	const FOpenMobileHapticControlResult StopResult =
		Subsystem->StopPlayback({});
	TestEqual(
		TEXT("Unsupported stop has a typed error"),
		StopResult.Error.Code,
		EOpenMobileHapticErrorCode::UnsupportedFeature
	);
	TestEqual(
		TEXT("Unknown handles remain invalid"),
		Subsystem->GetPlaybackState({}),
		EOpenMobileHapticPlaybackState::Invalid
	);

	FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	Policy.MasterIntensity = std::numeric_limits<float>::quiet_NaN();
	const FOpenMobileHapticControlResult InvalidPolicy =
		Subsystem->SetUserPolicy(Policy);
	TestEqual(
		TEXT("Nonfinite policy is rejected"),
		InvalidPolicy.Error.Code,
		EOpenMobileHapticErrorCode::InvalidRequest
	);

	Policy.MasterIntensity = 0.5f;
	const FOpenMobileHapticControlResult UpdatedPolicy =
		Subsystem->SetUserPolicy(Policy);
	TestEqual(
		TEXT("Valid local policy is accepted"),
		UpdatedPolicy.Outcome,
		EOpenMobileHapticControlOutcome::Accepted
	);
	TestEqual(
		TEXT("Accepted policy is retained"),
		Subsystem->GetUserPolicy().MasterIntensity,
		0.5f
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAsyncContractTest,
	"OpenMobile.Haptics.Async.ExactlyOnceAndTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAsyncContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	const UClass* ActionClass =
		UOpenMobileHapticPlaybackAsyncAction::StaticClass();
	for (const FName BranchName : {
		FName(TEXT("Completed")),
		FName(TEXT("Cancelled")),
		FName(TEXT("Failed"))
	})
	{
		const FMulticastDelegateProperty* Branch =
			FindFProperty<FMulticastDelegateProperty>(ActionClass, BranchName);
		TestTrue(
			*FString::Printf(
				TEXT("%s terminal branch is Blueprint assignable"),
				*BranchName.ToString()
			),
			Branch && Branch->HasAnyPropertyFlags(CPF_BlueprintAssignable)
		);
	}
	TestNotNull(
		TEXT("Async action exposes its playback handle"),
		ActionClass->FindPropertyByName(TEXT("PlaybackHandle"))
	);
	TestNotNull(
		TEXT("Async action exposes a Blueprint factory"),
		ActionClass->FindFunctionByName(TEXT("PlayNamedHapticAsync"))
	);

	int32 TerminalCount = 0;
	EOpenMobileHapticAsyncTerminalState LastState =
		EOpenMobileHapticAsyncTerminalState::Pending;
	UOpenMobileHapticPlaybackAsyncAction* Action =
		NewObject<UOpenMobileHapticPlaybackAsyncAction>();
	Action->OnNativeTerminal().AddLambda(
		[&TerminalCount, &LastState](
			EOpenMobileHapticAsyncTerminalState State,
			const FOpenMobileHapticPlaybackResult&)
		{
			++TerminalCount;
			LastState = State;
		}
	);
	FOpenMobileHapticPlaybackResult CompletedResult;
	CompletedResult.Outcome = EOpenMobileHapticPlaybackOutcome::Accepted;
	CompletedResult.State = EOpenMobileHapticPlaybackState::Completed;
	Action->FinishCompleted(CompletedResult);
	Action->FinishFailed(FOpenMobileHapticPlaybackResult::MakeRejected(
		EOpenMobileErrorCode::NativeFailure,
		TEXT("late failure")
	));
	Action->Cancel();
	TestEqual(TEXT("Only the first terminal path broadcasts"), TerminalCount, 1);
	TestEqual(
		TEXT("First terminal state wins"),
		LastState,
		EOpenMobileHapticAsyncTerminalState::Completed
	);

	int32 TeardownCancellationCount = 0;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* AsyncSubsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	UOpenMobileHapticPlaybackAsyncAction* TeardownAction =
		NewObject<UOpenMobileHapticPlaybackAsyncAction>();
	TeardownAction->OnNativeTerminal().AddLambda(
		[&TeardownCancellationCount](
			EOpenMobileHapticAsyncTerminalState State,
			const FOpenMobileHapticPlaybackResult&)
		{
			if (State == EOpenMobileHapticAsyncTerminalState::Cancelled)
			{
				++TeardownCancellationCount;
			}
		}
	);
	AsyncSubsystem->RegisterAsyncAction(TeardownAction);
	TeardownAction->Subsystem = AsyncSubsystem;
	AsyncSubsystem->Deinitialize();
	AsyncSubsystem->Deinitialize();
	TestEqual(
		TEXT("Game Instance teardown cancels exactly once"),
		TeardownCancellationCount,
		1
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBackendRegistryTest,
	"OpenMobile.Haptics.Backend.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBackendRegistryTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();

	FMockBackend Beta(TEXT("Beta"), 10);
	FMockBackend Alpha(TEXT("Alpha"), 10);
	FMockBackend Higher(TEXT("Higher"), 20);
	TestTrue(
		TEXT("First backend registers"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Beta)
	);
	TestTrue(
		TEXT("Equal-priority backend registers"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Alpha)
	);
	TestTrue(
		TEXT("Backend name breaks priority ties deterministically"),
		FOpenMobileHapticsBackendRegistry::FindBackend() == &Alpha
	);

	const FOpenMobileHapticsBackendRequestToken AlphaToken =
		FOpenMobileHapticsBackendRegistry::CreateRequestToken(Alpha, true);
	TestTrue(
		TEXT("Captured request token starts current"),
		FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(AlphaToken)
	);
	FMockBackend Lower(TEXT("Lower"), -10);
	TestTrue(
		TEXT("An unselected backend can register"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Lower)
	);
	TestTrue(
		TEXT("An unselected backend does not stale active request tokens"),
		FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(AlphaToken)
	);
	TestTrue(
		TEXT("Higher-priority backend registers"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Higher)
	);
	TestTrue(
		TEXT("Backend resolves lazily to the higher priority"),
		FOpenMobileHapticsBackendRegistry::FindBackend() == &Higher
	);
	TestFalse(
		TEXT("Registration invalidates stale callback tokens"),
		FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(AlphaToken)
	);

	FMockBackend Duplicate(TEXT("Higher"), 100);
	TestFalse(
		TEXT("Duplicate backend identity is rejected"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Duplicate)
	);
	TestTrue(
		TEXT("Backend unregisters"),
		FOpenMobileHapticsBackendRegistry::UnregisterBackend(Higher)
	);
	TestEqual(
		TEXT("Unregister begins shutdown before destruction"),
		Higher.ShutdownCount,
		1
	);
	TestTrue(
		TEXT("Tie-break remains stable after removal"),
		FOpenMobileHapticsBackendRegistry::FindBackend() == &Alpha
	);

	FOpenMobileHapticsBackendRegistry::BeginShutdown();
	TestEqual(TEXT("Alpha shuts down once"), Alpha.ShutdownCount, 1);
	TestEqual(TEXT("Beta shuts down once"), Beta.ShutdownCount, 1);
	TestEqual(TEXT("Lower shuts down once"), Lower.ShutdownCount, 1);
	TestNull(
		TEXT("No backend resolves during shutdown"),
		FOpenMobileHapticsBackendRegistry::FindBackend()
	);
	TestFalse(
		TEXT("Registration is rejected during shutdown"),
		FOpenMobileHapticsBackendRegistry::RegisterBackend(Duplicate)
	);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Alpha);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Beta);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Lower);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAvailabilityTest,
	"OpenMobile.Haptics.Capabilities.AvailabilityAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAvailabilityTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	TestEqual(
		TEXT("No backend remains an unsupported platform"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::UnsupportedPlatform
	);

	FMockBackend Backend(TEXT("Availability"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::NoActuator;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	TestEqual(
		TEXT("No actuator remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::NoActuator
	);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::BasicVibration;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	TestEqual(
		TEXT("Basic-only hardware remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::BasicVibration
	);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::SemanticFeedback;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	TestEqual(
		TEXT("Semantic hardware remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::SemanticFeedback
	);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	TestEqual(
		TEXT("Rich hardware remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::RichHaptics
	);

	FOpenMobileHapticUserPolicy Policy = Subsystem->GetUserPolicy();
	Policy.bEnabled = false;
	Subsystem->SetUserPolicy(Policy);
	TestEqual(
		TEXT("Player policy overrides hardware availability"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::DisabledByPolicy
	);
	Policy.bEnabled = true;
	Subsystem->SetUserPolicy(Policy);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::TemporarilyUnavailable;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	Backend.CapabilitiesAfterLifecycle.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.bApplyCapabilitiesAfterLifecycle = true;
	TestEqual(
		TEXT("Temporary engine state remains distinct"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::TemporarilyUnavailable
	);
	FOpenMobileHapticsBackendRegistry::NotifyLifecycleChange();
	TestEqual(
		TEXT("Lifecycle refresh reaches the backend"),
		Backend.LifecycleChangeCount,
		1
	);
	TestEqual(
		TEXT("Engine state refreshes after lifecycle change"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::RichHaptics
	);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsDetailedCapabilityTest,
	"OpenMobile.Haptics.Capabilities.DetailedProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsDetailedCapabilityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FOpenMobileHapticCapabilities Unknown;
	TestEqual(
		TEXT("Amplitude support defaults to unknown"),
		Unknown.AmplitudeControl,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Envelope support defaults to unknown"),
		Unknown.Envelopes,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Frequency support defaults to unknown"),
		Unknown.FrequencyControl,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Audio event support defaults to unknown"),
		Unknown.AudioEvents,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Seek support defaults to unknown"),
		Unknown.Seek,
		EOpenMobileHapticSupportState::Unknown
	);
	TestFalse(
		TEXT("Maximum event count defaults to unknown"),
		Unknown.MaximumEventCount.bKnown
	);
	TestFalse(
		TEXT("Minimum timing granularity defaults to unknown"),
		Unknown.MinimumTimingGranularitySeconds.bKnown
	);
	TestFalse(
		TEXT("Maximum control-point duration defaults to unknown"),
		Unknown.MaximumControlPointDurationSeconds.bKnown
	);
	TestFalse(
		TEXT("Frequency range defaults to unknown"),
		Unknown.FrequencyRange.bKnown
	);

	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("Detailed"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Primitives =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Envelopes =
		EOpenMobileHapticSupportState::Unsupported;
	Backend.Capabilities.PrimitiveSupport = {
		{TEXT("Click"), EOpenMobileHapticSupportState::Supported},
		{TEXT("Spin"), EOpenMobileHapticSupportState::Unsupported}
	};
	Backend.Capabilities.PresetSupport = {
		{TEXT("Tick"), EOpenMobileHapticSupportState::Supported},
		{TEXT("HeavyClick"), EOpenMobileHapticSupportState::Unknown}
	};
	Backend.Capabilities.MaximumEventCount = {true, 128};
	Backend.Capabilities.MaximumControlPointCount = {true, 16};
	Backend.Capabilities.MaximumDurationSeconds = {true, 30.0};
	Backend.Capabilities.MaximumQueueDepth = {true, 8};
	Backend.Capabilities.MinimumTimingGranularitySeconds = {true, 0.001};
	Backend.Capabilities.MaximumControlPointDurationSeconds = {true, 1.0};
	Backend.Capabilities.FrequencyRange = {true, 60.0f, 300.0f};
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const FOpenMobileHapticCapabilities First =
		Subsystem->GetCapabilitiesNative();
	TestEqual(
		TEXT("Per-primitive support is retained"),
		First.PrimitiveSupport.Num(),
		2
	);
	TestEqual(
		TEXT("Per-preset unknown state is retained"),
		First.PresetSupport[1].Support,
		EOpenMobileHapticSupportState::Unknown
	);
	TestEqual(
		TEXT("Known event limit is retained"),
		First.MaximumEventCount.Value,
		128
	);
	TestEqual(
		TEXT("Known duration limit is retained"),
		First.MaximumDurationSeconds.Seconds,
		30.0
	);
	TestEqual(
		TEXT("Known segment limit is retained"),
		First.MaximumControlPointDurationSeconds.Seconds,
		1.0
	);
	TestEqual(
		TEXT("Known frequency minimum is retained"),
		First.FrequencyRange.MinimumHertz,
		60.0f
	);
	TestEqual(
		TEXT("Known frequency maximum is retained"),
		First.FrequencyRange.MaximumHertz,
		300.0f
	);

	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::NoActuator;
	TestEqual(
		TEXT("Published snapshot is immutable between refreshes"),
		Subsystem->GetCapabilitiesNative().Availability,
		EOpenMobileHapticAvailability::RichHaptics
	);
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	TestEqual(
		TEXT("Explicit refresh publishes the new profile"),
		Subsystem->GetCapabilitiesNative().Availability,
		EOpenMobileHapticAvailability::NoActuator
	);

	TFuture<FOpenMobileHapticCapabilities> Future = Async(
		EAsyncExecution::ThreadPool,
		[Subsystem]()
		{
			return Subsystem->GetCapabilitiesNative();
		}
	);
	const FOpenMobileHapticCapabilities BackgroundSnapshot = Future.Get();
	TestEqual(
		TEXT("Background readers receive the immutable snapshot"),
		BackgroundSnapshot.BackendName,
		FName(TEXT("Detailed"))
	);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBackendSubmissionTest,
	"OpenMobile.Haptics.Backend.SubmissionAndCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBackendSubmissionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();

	FMockBackend Low(TEXT("Low"), 1);
	Low.Capabilities.Availability =
		EOpenMobileHapticAvailability::BasicVibration;
	Low.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Low.PreparationState =
		EOpenMobileHapticPreparationState::Preparing;
	FMockBackend High(TEXT("High"), 10);
	High.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	High.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	High.PreparationState =
		EOpenMobileHapticPreparationState::Prepared;
	High.ControlSupport.bStop = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Low);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	TestEqual(
		TEXT("Capability query uses the current backend"),
		Subsystem->GetHapticCapabilities().Availability,
		EOpenMobileHapticAvailability::BasicVibration
	);
	const FOpenMobileHapticPlaybackResult LowSemantic =
		Subsystem->PlaySemanticFeedback(
			EOpenMobileHapticSemanticEffect::Selection
		);
	TestTrue(TEXT("Semantic request is accepted"), LowSemantic.IsAccepted());
	TestFalse(
		TEXT("Fire-and-forget semantic request has no handle"),
		LowSemantic.Handle.IsValid()
	);
	TestTrue(TEXT("Request ID crosses the backend seam"), Low.LastToken.RequestId > 0);
	TestFalse(
		TEXT("Semantic request has no playback ID"),
		Low.LastToken.PlaybackHandle.IsValid()
	);

	FOpenMobileHapticsBackendRegistry::RegisterBackend(High);
	Subsystem->PlaySemanticFeedback(
		EOpenMobileHapticSemanticEffect::Click,
		1.0f,
		TEXT("Replacement")
	);
	TestEqual(
		TEXT("Next operation resolves the replacement backend"),
		High.SemanticSubmissionCount,
		1
	);
	TestEqual(
		TEXT("Backend preparation state is reported"),
		FOpenMobileHapticsBackendRegistry::FindBackend()->GetPreparationState(),
		EOpenMobileHapticPreparationState::Prepared
	);

	int32 EventCount = 0;
	int32 TerminalEventCount = 0;
	EOpenMobileHapticPlaybackState LastState =
		EOpenMobileHapticPlaybackState::Invalid;
	double LastTimestamp = 0.0;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&EventCount, &TerminalEventCount, &LastState, &LastTimestamp](
			const FOpenMobileHapticPlaybackEvent& Event
		)
		{
			++EventCount;
			TerminalEventCount +=
				Event.State == EOpenMobileHapticPlaybackState::Stopped
				|| Event.State == EOpenMobileHapticPlaybackState::Cancelled
				|| Event.State == EOpenMobileHapticPlaybackState::Completed
				|| Event.State == EOpenMobileHapticPlaybackState::Interrupted
				|| Event.State == EOpenMobileHapticPlaybackState::Failed;
			LastState = Event.State;
			LastTimestamp = Event.TimestampSeconds;
		}
	);
	const FOpenMobileHapticPlaybackResult Named =
		Subsystem->PlayNamedPattern(
			TEXT("UI_Confirm"),
			1.0f,
			TEXT("SubmissionCallbacks")
		);
	TestTrue(TEXT("Named request is accepted"), Named.IsAccepted());
	TestTrue(TEXT("Controllable request has a handle"), Named.Handle.IsValid());
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Accepted handle broadcasts acceptance"), EventCount, 1);
	TestEqual(
		TEXT("Accepted handle state is retained"),
		Subsystem->GetPlaybackState(Named.Handle),
		EOpenMobileHapticPlaybackState::Accepted
	);

	const FOpenMobileHapticControlResult Stop =
		Subsystem->StopPlayback(Named.Handle);
	TestEqual(
		TEXT("Supported stop routes to the owning backend"),
		Stop.Outcome,
		EOpenMobileHapticControlOutcome::Accepted
	);
	TestEqual(
		TEXT("Stop preserves request identity"),
		High.LastStoppedToken.RequestId,
		High.LastToken.RequestId
	);
	TestEqual(
		TEXT("Stop preserves playback identity"),
		High.LastStoppedToken.PlaybackHandle,
		Named.Handle
	);
	TestEqual(TEXT("Stop immediately records terminal state"),
		Subsystem->GetPlaybackState(Named.Handle),
		EOpenMobileHapticPlaybackState::Stopped);
	TestEqual(TEXT("Stop broadcasts one terminal event"), TerminalEventCount, 1);
	TestEqual(TEXT("Acceptance remains before stop"), EventCount, 2);
	TestEqual(TEXT("Stop event is distinct from cancellation"), LastState,
		EOpenMobileHapticPlaybackState::Stopped);
	TestEqual(TEXT("Repeated stop is idempotent"),
		Subsystem->StopPlayback(Named.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Repeated stop does not duplicate events"), EventCount, 2);

	High.CurrentTimeSeconds = 12.5;
	High.Emit(0, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Callback queued before stop is invalidated"), EventCount, 2);
	TestEqual(
		TEXT("Stopped state cannot regress"),
		LastState,
		EOpenMobileHapticPlaybackState::Stopped
	);
	High.Emit(0, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Duplicate callback is ignored"), EventCount, 2);

	High.Emit(0, EOpenMobileHapticPlaybackState::Completed, 2);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Post-stop terminal callback is ignored"), EventCount, 2);
	TestEqual(
		TEXT("Stopped state remains queryable"),
		Subsystem->GetPlaybackState(Named.Handle),
		EOpenMobileHapticPlaybackState::Stopped
	);
	High.Emit(0, EOpenMobileHapticPlaybackState::Completed, 3);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Post-terminal callback is ignored"), EventCount, 2);

	const FOpenMobileHapticPlaybackResult Stale =
		Subsystem->PlayNamedPattern(
			TEXT("Stale"),
			1.0f,
			TEXT("StaleBackend")
		);
	FMockBackend Newest(TEXT("Newest"), 20);
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Newest);
	TestEqual(TEXT("Control against a replaced backend becomes stale"),
		Subsystem->StopPlayback(Stale.Handle).Outcome,
		EOpenMobileHapticControlOutcome::StaleHandle);
	TestEqual(TEXT("Replaced backend handle becomes interrupted"),
		Subsystem->GetPlaybackState(Stale.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	TestEqual(TEXT("Backend replacement emits one terminal event"),
		TerminalEventCount, 2);
	High.Emit(1, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Stale backend callback is ignored"), EventCount, 4);
	TestTrue(TEXT("Stale request originally had a handle"), Stale.Handle.IsValid());

	Newest.bFailSubmissions = true;
	const FOpenMobileHapticPlaybackResult Failed =
		Subsystem->PlayNamedPattern(
			TEXT("Failure"),
			1.0f,
			TEXT("FailedSubmission")
		);
	TestEqual(
		TEXT("Injected native failure remains typed"),
		Failed.Error.Code,
		EOpenMobileHapticErrorCode::NativeEngineFailure
	);
	TestEqual(
		TEXT("Injected native failure retains the common code"),
		Failed.Error.CommonCode,
		EOpenMobileErrorCode::NativeFailure
	);
	TestFalse(TEXT("Failed submission has no handle"), Failed.Handle.IsValid());

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Newest);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(High);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Low);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPlaybackLifecycleOrderingTest,
	"OpenMobile.Haptics.Playback.Lifecycle.Ordering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPlaybackLifecycleOrderingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();

	FMockBackend Backend(TEXT("Lifecycle"));
	Backend.SubmissionResolvedPath = TEXT("PortableTimeline");
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	TArray<FOpenMobileHapticPlaybackEvent> Events;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&Events](const FOpenMobileHapticPlaybackEvent& Event)
		{
			Events.Add(Event);
		}
	);
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = TEXT("Gameplay");
	const FOpenMobileHapticPlaybackResult Playback =
		Subsystem->PlayNamedPatternAdvanced(
			TEXT("LifecyclePattern"),
			1.0f,
			Options
		);
	TestTrue(TEXT("Lifecycle playback is accepted"), Playback.IsAccepted());
	TestEqual(TEXT("Submission returns before any delegate runs"),
		Events.Num(), 0);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Acceptance is the first event"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		TestEqual(TEXT("Acceptance reports its state"), Events[0].State,
			EOpenMobileHapticPlaybackState::Accepted);
		TestEqual(TEXT("Acceptance reports the request handle"),
			Events[0].Handle, Playback.Handle);
		TestEqual(TEXT("Acceptance reports the effect"),
			Events[0].PatternOrEffect, FName(TEXT("LifecyclePattern")));
		TestEqual(TEXT("Acceptance reports the channel"),
			Events[0].Channel, FName(TEXT("Gameplay")));
		TestEqual(TEXT("Acceptance reports the resolved path"),
			Events[0].ResolvedPath, FName(TEXT("PortableTimeline")));
		TestEqual(TEXT("Acceptance is scheduler-confirmed"),
			Events[0].Evidence,
			EOpenMobileHapticEventEvidence::SchedulerConfirmed);
		TestTrue(TEXT("Acceptance has a monotonic timestamp"),
			Events[0].TimestampSeconds > 0.0);
	}

	Backend.CurrentTimeSeconds = FPlatformTime::Seconds();
	Backend.Emit(0, EOpenMobileHapticPlaybackState::Completed, 2);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Missing start is estimated before completion"),
		Events.Num(), 3);
	if (Events.Num() == 3)
	{
		TestEqual(TEXT("Estimated start preserves ordering"), Events[1].State,
			EOpenMobileHapticPlaybackState::Started);
		TestEqual(TEXT("Missing start is marked estimated"), Events[1].Evidence,
			EOpenMobileHapticEventEvidence::Estimated);
		TestEqual(TEXT("Completion remains terminal"), Events[2].State,
			EOpenMobileHapticPlaybackState::Completed);
		for (const FOpenMobileHapticPlaybackEvent& Event : Events)
		{
			TestEqual(TEXT("Every event preserves the request handle"),
				Event.Handle, Playback.Handle);
			TestEqual(TEXT("Every event preserves the effect"),
				Event.PatternOrEffect,
				FName(TEXT("LifecyclePattern")));
			TestEqual(TEXT("Every event preserves the channel"),
				Event.Channel, FName(TEXT("Gameplay")));
			TestEqual(TEXT("Every event preserves the resolved path"),
				Event.ResolvedPath, FName(TEXT("PortableTimeline")));
		}
	}

	Backend.Emit(0, EOpenMobileHapticPlaybackState::Completed, 3);
	Backend.Emit(0, EOpenMobileHapticPlaybackState::Started, 4);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Post-terminal callbacks cannot add events"),
		Events.Num(), 3);
	int32 TerminalCount = 0;
	for (const FOpenMobileHapticPlaybackEvent& Event : Events)
	{
		TerminalCount += Event.State == EOpenMobileHapticPlaybackState::Stopped
			|| Event.State == EOpenMobileHapticPlaybackState::Cancelled
			|| Event.State == EOpenMobileHapticPlaybackState::Completed
			|| Event.State == EOpenMobileHapticPlaybackState::Interrupted
			|| Event.State == EOpenMobileHapticPlaybackState::Failed;
	}
	TestEqual(TEXT("Accepted handle receives exactly one terminal event"),
		TerminalCount, 1);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPlaybackLifecycleMissingCallbackTest,
	"OpenMobile.Haptics.Playback.Lifecycle.MissingTerminalCallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPlaybackLifecycleMissingCallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();

	FMockBackend Backend(TEXT("MissingTerminal"));
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	TArray<FOpenMobileHapticPlaybackEvent> Events;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&Events](const FOpenMobileHapticPlaybackEvent& Event)
		{
			Events.Add(Event);
		}
	);
	const FOpenMobileHapticPlaybackResult Playback =
		Subsystem->PlayNamedPattern(TEXT("MissingTerminal"));
	Subsystem->PublishTerminalTimeout(Backend.LastToken.RequestId);
	TestEqual(TEXT("Missing terminal callback receives a bounded fallback"),
		Events.Num(), 3);
	if (Events.Num() == 3)
	{
		TestEqual(TEXT("Timeout keeps acceptance first"), Events[0].State,
			EOpenMobileHapticPlaybackState::Accepted);
		TestEqual(TEXT("Timeout estimates a missing start"), Events[1].State,
			EOpenMobileHapticPlaybackState::Started);
		TestEqual(TEXT("Timeout terminates as failed"), Events[2].State,
			EOpenMobileHapticPlaybackState::Failed);
		TestEqual(TEXT("Timeout failure is explicitly estimated"),
			Events[2].Evidence, EOpenMobileHapticEventEvidence::Estimated);
		TestEqual(TEXT("Timeout failure identifies the handle"),
			Events[2].Handle, Playback.Handle);
		TestEqual(TEXT("Timeout failure is typed"), Events[2].Error.Code,
			EOpenMobileHapticErrorCode::NativeEngineFailure);
	}
	TestEqual(TEXT("Timed-out handle retains its terminal state"),
		Subsystem->GetPlaybackState(Playback.Handle),
		EOpenMobileHapticPlaybackState::Failed);

	Backend.Emit(0, EOpenMobileHapticPlaybackState::Completed, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Late native completion cannot duplicate the timeout"),
		Events.Num(), 3);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPlaybackLifecycleCallbackRaceTest,
	"OpenMobile.Haptics.Playback.Lifecycle.CallbackRaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPlaybackLifecycleCallbackRaceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();

	FMockBackend Backend(TEXT("LifecycleRaces"));
	Backend.Capabilities.Scheduling = EOpenMobileHapticSupportState::Supported;
	Backend.SubmissionResolvedPath = TEXT("ScheduledPortableTimeline");
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	TArray<FOpenMobileHapticPlaybackEvent> Events;
	bool bAllEventsOnGameThread = true;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&Events, &bAllEventsOnGameThread](
			const FOpenMobileHapticPlaybackEvent& Event
		)
		{
			bAllEventsOnGameThread &= IsInGameThread();
			Events.Add(Event);
		}
	);
	FOpenMobileHapticPlaybackOptions Options;
	Options.Channel = TEXT("Cinematic");
	Options.Schedule.Mode = EOpenMobileHapticScheduleMode::Relative;
	Options.Schedule.TimeSeconds = 0.25;
	const FOpenMobileHapticPlaybackResult Playback =
		Subsystem->PlayNamedPatternAdvanced(
			TEXT("ScheduledLifecycle"),
			1.0f,
			Options
		);
	TestEqual(TEXT("Delayed playback reports scheduled state"), Playback.State,
		EOpenMobileHapticPlaybackState::Scheduled);
	TestEqual(TEXT("Scheduled submission returns before delegates run"),
		Events.Num(), 0);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Accepted and scheduled events publish immediately"),
		Events.Num(), 2);
	if (Events.Num() == 2)
	{
		TestEqual(TEXT("Scheduled ordering starts with acceptance"),
			Events[0].State, EOpenMobileHapticPlaybackState::Accepted);
		TestEqual(TEXT("Scheduled ordering publishes the queue state second"),
			Events[1].State, EOpenMobileHapticPlaybackState::Scheduled);
	}
	Backend.Emit(0, EOpenMobileHapticPlaybackState::Started, 0);
	FOpenMobileHapticPlaybackEvent WrongHandleStart;
	WrongHandleStart.State = EOpenMobileHapticPlaybackState::Started;
	WrongHandleStart.Handle.Id = FGuid(42, 0, 0, 1);
	Backend.EmitEvent(0, 1, MoveTemp(WrongHandleStart));
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Invalid sequence and mismatched handle are ignored"),
		Events.Num(), 2);

	Backend.CurrentTimeSeconds = FPlatformTime::Seconds() + 1.0;
	TFuture<void> Worker = Async(
		EAsyncExecution::ThreadPool,
		[&Backend]()
		{
			Backend.Emit(
				0,
				EOpenMobileHapticPlaybackState::Started,
				1,
				EOpenMobileHapticEventEvidence::NativeConfirmed
			);
		}
	);
	Worker.Wait();
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Off-thread start is appended after scheduled"),
		Events.Num(), 3);
	TestTrue(TEXT("Off-thread callback returns to the game thread"),
		bAllEventsOnGameThread);
	if (Events.Num() == 3)
	{
		TestEqual(TEXT("Started evidence is preserved"), Events[2].Evidence,
			EOpenMobileHapticEventEvidence::NativeConfirmed);
	}

	Backend.Emit(
		0,
		EOpenMobileHapticPlaybackState::Started,
		1,
		EOpenMobileHapticEventEvidence::NativeConfirmed
	);
	Backend.Emit(
		0,
		EOpenMobileHapticPlaybackState::Scheduled,
		2,
		EOpenMobileHapticEventEvidence::SchedulerConfirmed
	);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Duplicate and delayed states are ignored"), Events.Num(), 3);

	Backend.Emit(
		0,
		EOpenMobileHapticPlaybackState::Paused,
		3,
		EOpenMobileHapticEventEvidence::NativeConfirmed
	);
	Backend.Emit(
		0,
		EOpenMobileHapticPlaybackState::Resumed,
		4,
		EOpenMobileHapticEventEvidence::SchedulerConfirmed
	);
	Backend.Emit(
		0,
		EOpenMobileHapticPlaybackState::Completed,
		5,
		EOpenMobileHapticEventEvidence::NativeConfirmed
	);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Nonterminal transitions remain ordered"), Events.Num(), 6);
	if (Events.Num() == 6)
	{
		const TArray<EOpenMobileHapticPlaybackState> ExpectedStates = {
			EOpenMobileHapticPlaybackState::Accepted,
			EOpenMobileHapticPlaybackState::Scheduled,
			EOpenMobileHapticPlaybackState::Started,
			EOpenMobileHapticPlaybackState::Paused,
			EOpenMobileHapticPlaybackState::Resumed,
			EOpenMobileHapticPlaybackState::Completed
		};
		for (int32 Index = 0; Index < ExpectedStates.Num(); ++Index)
		{
			TestEqual(TEXT("Lifecycle state matches deterministic order"),
				Events[Index].State, ExpectedStates[Index]);
			TestEqual(TEXT("Lifecycle event preserves its handle"),
				Events[Index].Handle, Playback.Handle);
			TestEqual(TEXT("Lifecycle event preserves its effect"),
				Events[Index].PatternOrEffect,
				FName(TEXT("ScheduledLifecycle")));
			TestEqual(TEXT("Lifecycle event preserves its channel"),
				Events[Index].Channel, FName(TEXT("Cinematic")));
			TestEqual(TEXT("Lifecycle event preserves its resolved path"),
				Events[Index].ResolvedPath,
				FName(TEXT("ScheduledPortableTimeline")));
			if (Index > 0)
			{
				TestTrue(TEXT("Lifecycle timestamps never regress"),
					Events[Index].TimestampSeconds
						>= Events[Index - 1].TimestampSeconds);
			}
		}
	}

	const int32 EventCountAfterTerminal = Events.Num();
	Backend.Emit(0, EOpenMobileHapticPlaybackState::Failed, 6);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Post-terminal failure is ignored"), Events.Num(),
		EventCountAfterTerminal);

	const FOpenMobileHapticPlaybackResult TeardownPlayback =
		Subsystem->PlayNamedPattern(TEXT("TeardownLifecycle"));
	TestTrue(TEXT("Teardown playback receives a handle"),
		TeardownPlayback.Handle.IsValid());
	const int32 EventCountBeforeTeardown = Events.Num();
	Subsystem->Deinitialize();
	Backend.Emit(1, EOpenMobileHapticPlaybackState::Completed, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Post-teardown callback is ignored"), Events.Num(),
		EventCountBeforeTeardown);

	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPlaybackLifecycleHistoryTest,
	"OpenMobile.Haptics.Playback.Lifecycle.HistoryAndReentrancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPlaybackLifecycleHistoryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	const int32 SavedMaximumDiagnosticEvents =
		Settings->MaximumDiagnosticEvents;
	Settings->NamedLibraries.Reset();
	Settings->MaximumDiagnosticEvents = 3;

	FMockBackend Backend(TEXT("LifecycleHistory"));
	Backend.ControlSupport.bStop = true;
	Backend.SubmissionResolvedPath = TEXT("/private/native/fallback");
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	bool bReentered = false;
	FOpenMobileHapticControlResult ReentrantStop;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[Subsystem, &bReentered, &ReentrantStop](
			const FOpenMobileHapticPlaybackEvent& Event
		)
		{
			if (!bReentered
				&& Event.State == EOpenMobileHapticPlaybackState::Accepted
				&& Event.PatternOrEffect == TEXT("Reentrant"))
			{
				bReentered = true;
				static_cast<void>(Subsystem->GetDiagnostics());
				ReentrantStop = Subsystem->StopPlayback(Event.Handle);
			}
		}
	);
	const FOpenMobileHapticPlaybackResult ReentrantPlayback =
		Subsystem->PlayNamedPattern(TEXT("Reentrant"));
	TestFalse(TEXT("Accepted delegate is deferred until after submission"),
		bReentered);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestTrue(TEXT("Accepted delegate can reenter the subsystem"), bReentered);
	TestEqual(TEXT("Reentrant stop succeeds without a held lock"),
		ReentrantStop.Outcome, EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Reentrant stop owns the terminal state"),
		Subsystem->GetPlaybackState(ReentrantPlayback.Handle),
		EOpenMobileHapticPlaybackState::Stopped);

	const FOpenMobileHapticPlaybackResult PrivatePlayback =
		Subsystem->PlayNamedPattern(
			TEXT("/Game/Private/SecretPattern"),
			1.0f,
			TEXT("PrivateHistory")
		);
	FOpenMobileHapticPlaybackEvent Failure;
	Failure.State = EOpenMobileHapticPlaybackState::Failed;
	Failure.Evidence = EOpenMobileHapticEventEvidence::NativeConfirmed;
	Failure.TimestampSeconds = FPlatformTime::Seconds();
	Failure.Error = FOpenMobileHapticError::FromCommon(
		EOpenMobileErrorCode::NativeFailure,
		TEXT("Native failure at /Users/person/private/file.ahap"),
		EOpenMobileHapticFailureStage::Playback
	);
	Failure.Error.NativeDomain = TEXT("/private/native/domain");
	Backend.EmitEvent(1, 1, MoveTemp(Failure));
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	const FOpenMobileHapticsDiagnostics Diagnostics =
		Subsystem->GetDiagnostics();
	TestEqual(TEXT("Recent event history obeys the configured bound"),
		Diagnostics.RecentPlaybackEvents.Num(), 3);
	if (Diagnostics.RecentPlaybackEvents.Num() == 3)
	{
		TestEqual(TEXT("History retains the newest terminal events"),
			Diagnostics.RecentPlaybackEvents[0].State,
			EOpenMobileHapticPlaybackState::Stopped);
		TestEqual(TEXT("Path-like effect names are redacted in history"),
			Diagnostics.RecentPlaybackEvents[1].PatternOrEffect,
			FName(TEXT("redacted")));
		TestEqual(TEXT("Path-like resolved paths are redacted in history"),
			Diagnostics.RecentPlaybackEvents[2].ResolvedPath,
			FName(TEXT("redacted")));
		TestEqual(TEXT("Path-like error messages are redacted in history"),
			Diagnostics.RecentPlaybackEvents[2].Error.Message,
			FString(TEXT("redacted")));
		TestEqual(TEXT("Path-like native details are redacted in history"),
			Diagnostics.RecentPlaybackEvents[2].Error.NativeDomain,
			FString(TEXT("redacted")));
		TestEqual(TEXT("History still identifies the terminal handle"),
			Diagnostics.RecentPlaybackEvents[2].Handle,
			PrivatePlayback.Handle);
	}

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->MaximumDiagnosticEvents = SavedMaximumDiagnosticEvents;
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPlaybackControlTest,
	"OpenMobile.Haptics.Playback.StopAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPlaybackControlTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	const float SavedDefaultMinimumIntervalSeconds =
		Settings->DefaultMinimumIntervalSeconds;
	Settings->NamedLibraries.Reset();
	Settings->DefaultMinimumIntervalSeconds = 0.0f;
	FMockBackend Backend(TEXT("Control"));
	Backend.ControlSupport.bStop = true;
	Backend.ControlSupport.bStopChannel = true;
	Backend.ControlSupport.bStopAll = true;
	Backend.Capabilities.Mixing =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	int32 StoppedEvents = 0;
	int32 CancelledEvents = 0;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&StoppedEvents, &CancelledEvents](
			const FOpenMobileHapticPlaybackEvent& Event)
		{
			StoppedEvents += Event.State
				== EOpenMobileHapticPlaybackState::Stopped;
			CancelledEvents += Event.State
				== EOpenMobileHapticPlaybackState::Cancelled;
		}
	);
	FOpenMobileHapticPlaybackOptions AOptions;
	AOptions.Channel = TEXT("A");
	AOptions.OverlapPolicy = EOpenMobileHapticOverlapPolicy::MixWhenSupported;
	FOpenMobileHapticPlaybackOptions BOptions;
	BOptions.Channel = TEXT("B");
	const FOpenMobileHapticPlaybackResult A1 =
		Subsystem->PlayNamedPatternAdvanced(TEXT("A1"), 1.0f, AOptions);
	const FOpenMobileHapticPlaybackResult A2 =
		Subsystem->PlayNamedPatternAdvanced(TEXT("A2"), 1.0f, AOptions);
	const FOpenMobileHapticPlaybackResult B =
		Subsystem->PlayNamedPatternAdvanced(TEXT("B"), 1.0f, BOptions);

	TestEqual(TEXT("Cancel routes through the owning backend"),
		Subsystem->CancelPlayback(A1.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Cancel records a distinct terminal state"),
		Subsystem->GetPlaybackState(A1.Handle),
		EOpenMobileHapticPlaybackState::Cancelled);
	TestEqual(TEXT("Cancel emits once"), CancelledEvents, 1);
	TestEqual(TEXT("Repeated cancel is idempotent"),
		Subsystem->CancelPlayback(A1.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Repeated cancel does not emit again"), CancelledEvents, 1);
	Backend.Emit(0, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Cancelled callbacks are invalidated"), CancelledEvents, 1);

	TestEqual(TEXT("Channel stop is accepted"),
		Subsystem->StopChannel(TEXT("A")).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Channel stop affects matching playback"),
		Subsystem->GetPlaybackState(A2.Handle),
		EOpenMobileHapticPlaybackState::Stopped);
	TestEqual(TEXT("Channel stop leaves other playback active"),
		Subsystem->GetPlaybackState(B.Handle),
		EOpenMobileHapticPlaybackState::Accepted);
	TestEqual(TEXT("Channel stop emits once per active handle"),
		StoppedEvents, 1);
	TestEqual(TEXT("Repeated channel stop is idempotent"),
		Subsystem->StopChannel(TEXT("A")).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Repeated channel stop does not emit again"),
		StoppedEvents, 1);

	TestEqual(TEXT("Stop all is accepted"),
		Subsystem->StopAll().Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Stop all terminates remaining playback"),
		Subsystem->GetPlaybackState(B.Handle),
		EOpenMobileHapticPlaybackState::Stopped);
	TestEqual(TEXT("Stop all emits once per active handle"),
		StoppedEvents, 2);

	const FOpenMobileHapticPlaybackResult Newer =
		Subsystem->PlayNamedPatternAdvanced(TEXT("Newer"), 1.0f, BOptions);
	FOpenMobileHapticPlaybackHandle Unknown;
	Unknown.Id = FGuid(99, 0, 0, 1);
	TestEqual(TEXT("Unknown handle remains stale"),
		Subsystem->StopPlayback(Unknown).Outcome,
		EOpenMobileHapticControlOutcome::StaleHandle);
	TestEqual(TEXT("Stale stop cannot affect newer playback"),
		Subsystem->GetPlaybackState(Newer.Handle),
		EOpenMobileHapticPlaybackState::Accepted);

	const int32 StopsBeforeDeinitialize = Backend.StopPlaybackCount;
	Subsystem->Deinitialize();
	TestEqual(TEXT("Game Instance teardown avoids process-wide stop-all"),
		Backend.StopAllCount, 1);
	TestEqual(TEXT("Game Instance teardown stops its remaining handle"),
		Backend.StopPlaybackCount, StopsBeforeDeinitialize + 1);
	TestEqual(TEXT("Game Instance teardown stops the owned token"),
		Backend.LastStoppedToken.PlaybackHandle, Newer.Handle);
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->DefaultMinimumIntervalSeconds =
		SavedDefaultMinimumIntervalSeconds;
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPlaybackCursorControlTest,
	"OpenMobile.Haptics.Playback.Controls.SubsystemContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPlaybackCursorControlTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();

	FMockBackend Backend(TEXT("CursorControls"));
	Backend.ControlSupport.bStop = true;
	Backend.ControlSupport.bPause = true;
	Backend.ControlSupport.bResume = true;
	Backend.ControlSupport.bSeek = true;
	Backend.SubmissionControlSupport.PauseImplementation =
		EOpenMobileHapticControlImplementation::Native;
	Backend.SubmissionControlSupport.ResumeImplementation =
		EOpenMobileHapticControlImplementation::Native;
	Backend.SubmissionControlSupport.SeekImplementation =
		EOpenMobileHapticControlImplementation::Emulated;
	Backend.SubmissionControlSupport.SeekGranularitySeconds = 0.001;
	Backend.SubmissionControlSupport.bHasRepeatPlan = true;
	Backend.SubmissionControlSupport.RepeatPlan.PatternDurationSeconds = 2.0;
	Backend.SubmissionControlSupport.RepeatPlan.TotalDurationSeconds = 2.0;
	Backend.SubmissionControlSupport.RepeatPlan.MaximumDurationSeconds = 30.0;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	TArray<EOpenMobileHapticPlaybackState> ControlEvents;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&ControlEvents](const FOpenMobileHapticPlaybackEvent& Event)
		{
			if (Event.State == EOpenMobileHapticPlaybackState::Paused
				|| Event.State == EOpenMobileHapticPlaybackState::Resumed)
			{
				ControlEvents.Add(Event.State);
			}
		}
	);
	const FOpenMobileHapticPlaybackResult Playback =
		Subsystem->PlayNamedPattern(TEXT("Controlled"));
	TestTrue(TEXT("Control-capable playback returns a handle"),
		Playback.Handle.IsValid());
	TestEqual(TEXT("Active playback cannot resume"),
		Subsystem->ResumePlayback(Playback.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Rejected);
	TestEqual(TEXT("Invalid resume does not cross the backend seam"),
		Backend.ResumeCount, 0);

	const FOpenMobileHapticControlResult Paused =
		Subsystem->PausePlayback(Playback.Handle);
	TestEqual(TEXT("Supported playback pauses"), Paused.Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Pause reports the native implementation"),
		Paused.Implementation,
		EOpenMobileHapticControlImplementation::Native);
	TestEqual(TEXT("Pause reports the resulting state"), Paused.State,
		EOpenMobileHapticPlaybackState::Paused);
	TestEqual(TEXT("Pause starts the serialized control revision"),
		Paused.ControlRevision, static_cast<int64>(1));
	TestEqual(TEXT("Pause reaches the owning backend once"),
		Backend.PauseCount, 1);
	TestEqual(TEXT("Pause updates handle state"),
		Subsystem->GetPlaybackState(Playback.Handle),
		EOpenMobileHapticPlaybackState::Paused);
	TestEqual(TEXT("Pause emits one state event"), ControlEvents.Num(), 1);

	TestEqual(TEXT("Repeated pause is rejected before native control"),
		Subsystem->PausePlayback(Playback.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Rejected);
	TestEqual(TEXT("Repeated pause does not cross the backend seam"),
		Backend.PauseCount, 1);

	const FOpenMobileHapticControlResult Resumed =
		Subsystem->ResumePlayback(Playback.Handle);
	TestEqual(TEXT("Paused playback resumes"), Resumed.Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Resume reports the resulting state"), Resumed.State,
		EOpenMobileHapticPlaybackState::Resumed);
	TestEqual(TEXT("Resume advances the control revision"),
		Resumed.ControlRevision, static_cast<int64>(2));
	TestEqual(TEXT("Resume emits one ordered state event"),
		ControlEvents,
		TArray<EOpenMobileHapticPlaybackState>({
			EOpenMobileHapticPlaybackState::Paused,
			EOpenMobileHapticPlaybackState::Resumed
		}));

	TestEqual(TEXT("Negative seek is rejected before native control"),
		Subsystem->SeekPlayback(Playback.Handle, -0.001).Outcome,
		EOpenMobileHapticControlOutcome::Rejected);
	TestEqual(TEXT("Pattern-end seek is rejected before native control"),
		Subsystem->SeekPlayback(Playback.Handle, 2.0).Outcome,
		EOpenMobileHapticControlOutcome::Rejected);
	TestEqual(TEXT("Nonfinite seek is rejected before native control"),
		Subsystem->SeekPlayback(
			Playback.Handle,
			std::numeric_limits<double>::quiet_NaN()
		).Outcome,
		EOpenMobileHapticControlOutcome::Rejected);
	TestEqual(TEXT("Invalid seeks do not cross the backend seam"),
		Backend.SeekCount, 0);

	const FOpenMobileHapticControlResult Sought =
		Subsystem->SeekPlayback(Playback.Handle, 1.2344);
	TestEqual(TEXT("Valid seek is accepted"), Sought.Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Seek reports portable emulation"),
		Sought.Implementation,
		EOpenMobileHapticControlImplementation::Emulated);
	TestTrue(TEXT("Seek reports platform quantization"),
		Sought.bQuantized);
	TestEqual(TEXT("Seek reports the requested position"),
		Sought.RequestedPositionSeconds, 1.2344);
	TestEqual(TEXT("Seek reports the resolved position"),
		Sought.ResolvedPositionSeconds, 1.234);
	TestEqual(TEXT("Seek reports native granularity"),
		Sought.PositionGranularitySeconds, 0.001);
	TestEqual(TEXT("Seek reaches the backend with the same revision"),
		static_cast<int64>(Backend.LastControlCommand.Revision),
		Sought.ControlRevision);

	Subsystem->PausePlayback(Playback.Handle);
	Backend.bFailPlaybackControls = true;
	const FOpenMobileHapticControlResult FailedResume =
		Subsystem->ResumePlayback(Playback.Handle);
	TestEqual(TEXT("Native resume failure is returned"), FailedResume.Outcome,
		EOpenMobileHapticControlOutcome::Rejected);
	TestEqual(TEXT("Native failure rolls the logical state back"),
		Subsystem->GetPlaybackState(Playback.Handle),
		EOpenMobileHapticPlaybackState::Paused);
	Backend.bFailPlaybackControls = false;
	TestEqual(TEXT("Resume can retry after native failure"),
		Subsystem->ResumePlayback(Playback.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);

	Backend.Emit(0, EOpenMobileHapticPlaybackState::Interrupted, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Terminal callback makes later controls stale"),
		Subsystem->PausePlayback(Playback.Handle).Outcome,
		EOpenMobileHapticControlOutcome::StaleHandle);

	const int32 PauseCallsBeforeUnsupported = Backend.PauseCount;
	Backend.SubmissionControlSupport.PauseImplementation =
		EOpenMobileHapticControlImplementation::Unsupported;
	const FOpenMobileHapticPlaybackResult UnsupportedPlayback =
		Subsystem->PlayNamedPattern(
			TEXT("UnsupportedControl"),
			1.0f,
			TEXT("UnsupportedControl")
		);
	const FOpenMobileHapticControlResult Unsupported =
		Subsystem->PausePlayback(UnsupportedPlayback.Handle);
	TestEqual(TEXT("Request-specific fallback support is enforced"),
		Unsupported.Outcome,
		EOpenMobileHapticControlOutcome::Unsupported);
	TestEqual(TEXT("Unsupported control never reaches the backend"),
		Backend.PauseCount, PauseCallsBeforeUnsupported);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsErrorMappingTest,
	"OpenMobile.Haptics.Errors.MappingAndRedaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsErrorMappingTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	struct FExpectedMapping
	{
		EOpenMobileHapticsFailureReason Reason;
		EOpenMobileHapticErrorCode HapticCode;
		EOpenMobileErrorCode CommonCode;
	};
	const TArray<FExpectedMapping> Mappings = {
		{EOpenMobileHapticsFailureReason::UnsupportedHardware,
			EOpenMobileHapticErrorCode::UnsupportedHardware,
			EOpenMobileErrorCode::NotSupported},
		{EOpenMobileHapticsFailureReason::UnsupportedFeature,
			EOpenMobileHapticErrorCode::UnsupportedFeature,
			EOpenMobileErrorCode::NotSupported},
		{EOpenMobileHapticsFailureReason::DisabledPolicy,
			EOpenMobileHapticErrorCode::DisabledByPolicy,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileHapticsFailureReason::InvalidPattern,
			EOpenMobileHapticErrorCode::InvalidPattern,
			EOpenMobileErrorCode::InvalidArgument},
		{EOpenMobileHapticsFailureReason::RateLimited,
			EOpenMobileHapticErrorCode::RateLimited,
			EOpenMobileErrorCode::Busy},
		{EOpenMobileHapticsFailureReason::BusyChannel,
			EOpenMobileHapticErrorCode::ChannelBusy,
			EOpenMobileErrorCode::Busy},
		{EOpenMobileHapticsFailureReason::LifecycleRestricted,
			EOpenMobileHapticErrorCode::LifecycleRestricted,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileHapticsFailureReason::NativeEngineFailure,
			EOpenMobileHapticErrorCode::NativeEngineFailure,
			EOpenMobileErrorCode::NativeFailure},
		{EOpenMobileHapticsFailureReason::Interrupted,
			EOpenMobileHapticErrorCode::Interrupted,
			EOpenMobileErrorCode::NativeFailure}
	};
	for (const FExpectedMapping& Mapping : Mappings)
	{
		FOpenMobileHapticsErrorContext Context;
		Context.Reason = Mapping.Reason;
		Context.Stage = EOpenMobileHapticFailureStage::Playback;
		const FOpenMobileHapticError Error =
			FOpenMobileHapticsErrorMapper::Map(Context);
		TestEqual(TEXT("Known reason maps to a Haptics code"), Error.Code, Mapping.HapticCode);
		TestEqual(TEXT("Known reason maps to a common code"), Error.CommonCode, Mapping.CommonCode);
		TestFalse(TEXT("Mapped error has a useful message"), Error.Message.IsEmpty());
		TestFalse(TEXT("Mapped error has a correction"), Error.Correction.IsEmpty());
		TestTrue(TEXT("Pre-submission mapping is marked rejected"), Error.bRejectedBeforeSubmission);
	}

	FOpenMobileHapticsErrorContext FutureNative;
	FutureNative.Reason = EOpenMobileHapticsFailureReason::NativeEngineFailure;
	FutureNative.Stage = EOpenMobileHapticFailureStage::NativeSubmission;
	FutureNative.NativeDomain = TEXT("CoreHaptics.Engine");
	FutureNative.NativeCode = TEXT("FutureEngine_4097");
	FutureNative.bAfterAcceptance = true;
	const FOpenMobileHapticError FutureError =
		FOpenMobileHapticsErrorMapper::Map(FutureNative);
	TestEqual(
		TEXT("Future native domain is preserved"),
		FutureError.NativeDomain,
		FString(TEXT("CoreHaptics.Engine"))
	);
	TestEqual(
		TEXT("Future native code is preserved"),
		FutureError.NativeCode,
		FString(TEXT("FutureEngine_4097"))
	);
	TestFalse(
		TEXT("Post-acceptance failure is not a rejection"),
		FutureError.bRejectedBeforeSubmission
	);
	TestFalse(
		TEXT("Missing native messages use the mapped message"),
		FutureError.Message.IsEmpty()
	);

	FOpenMobileHapticsErrorContext Malformed;
	Malformed.Reason = EOpenMobileHapticsFailureReason::InvalidPattern;
	Malformed.Stage = EOpenMobileHapticFailureStage::Compilation;
	Malformed.FailedItem = TEXT("BrokenPattern");
	Malformed.Channel = TEXT("Gameplay");
	Malformed.FallbackAttempts = {TEXT("PortableRich"), TEXT("BasicVibration")};
	const FOpenMobileHapticError MalformedError =
		FOpenMobileHapticsErrorMapper::Map(Malformed);
	TestEqual(
		TEXT("Malformed asset keeps its failed stage"),
		MalformedError.Stage,
		EOpenMobileHapticFailureStage::Compilation
	);
	TestEqual(
		TEXT("Malformed asset keeps its safe identity"),
		MalformedError.FailedItem,
		FName(TEXT("BrokenPattern"))
	);
	TestEqual(
		TEXT("Fallback attempts remain available"),
		MalformedError.FallbackAttempts.Num(),
		2
	);

	FOpenMobileHapticsErrorContext Sensitive;
	Sensitive.Reason = EOpenMobileHapticsFailureReason::NativeEngineFailure;
	Sensitive.Stage = EOpenMobileHapticFailureStage::Playback;
	Sensitive.NativeDomain = TEXT("/Users/player/private/CoreHaptics");
	Sensitive.NativeCode = TEXT("token\nsecret");
	const FOpenMobileHapticError Redacted =
		FOpenMobileHapticsErrorMapper::Map(Sensitive);
	TestEqual(
		TEXT("Unsafe native domain is redacted"),
		Redacted.NativeDomain,
		FString(TEXT("redacted"))
	);
	TestEqual(
		TEXT("Unsafe native code is redacted"),
		Redacted.NativeCode,
		FString(TEXT("redacted"))
	);
	TestFalse(
		TEXT("Generated message does not contain a file path"),
		Redacted.Message.Contains(TEXT("/Users/"))
	);

	FOpenMobileHapticsErrorContext Interrupted;
	Interrupted.Reason = EOpenMobileHapticsFailureReason::Interrupted;
	Interrupted.Stage = EOpenMobileHapticFailureStage::Interruption;
	Interrupted.bAfterAcceptance = true;
	const FOpenMobileHapticError InterruptedError =
		FOpenMobileHapticsErrorMapper::Map(Interrupted);
	TestTrue(
		TEXT("Accepted interruption is distinguished"),
		InterruptedError.bInterruptedAfterAcceptance
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsMissingBackendErrorTest,
	"OpenMobile.Haptics.Errors.MissingBackendDetails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsMissingBackendErrorTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();
	FMockBackend Backend(TEXT("MissingDetails"));
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	Backend.bFailSubmissionsWithoutError = true;
	const FOpenMobileHapticPlaybackResult Rejected =
		Subsystem->PlayNamedPattern(
			TEXT("MissingSubmissionError"),
			1.0f,
			TEXT("MissingSubmissionError")
		);
	TestEqual(
		TEXT("Missing submission errors become native failures"),
		Rejected.Error.Code,
		EOpenMobileHapticErrorCode::NativeEngineFailure
	);
	TestEqual(
		TEXT("Missing submission errors identify their stage"),
		Rejected.Error.Stage,
		EOpenMobileHapticFailureStage::NativeSubmission
	);
	TestFalse(
		TEXT("Missing submission messages receive a useful message"),
		Rejected.Error.Message.IsEmpty()
	);
	TestTrue(
		TEXT("Missing submission errors remain pre-acceptance"),
		Rejected.Error.bRejectedBeforeSubmission
	);

	Backend.bFailSubmissionsWithoutError = false;
	const FOpenMobileHapticPlaybackResult Accepted =
		Subsystem->PlayNamedPattern(
			TEXT("MissingCallbackError"),
			1.0f,
			TEXT("MissingCallbackError")
		);
	FOpenMobileHapticError TerminalError;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&TerminalError](const FOpenMobileHapticPlaybackEvent& Event)
		{
			TerminalError = Event.Error;
		}
	);
	Backend.Emit(0, EOpenMobileHapticPlaybackState::Failed, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestTrue(
		TEXT("Accepted request has a valid handle"),
		Accepted.Handle.IsValid()
	);
	TestEqual(
		TEXT("Missing terminal errors become native failures"),
		TerminalError.Code,
		EOpenMobileHapticErrorCode::NativeEngineFailure
	);
	TestEqual(
		TEXT("Missing terminal errors identify playback"),
		TerminalError.Stage,
		EOpenMobileHapticFailureStage::Playback
	);
	TestFalse(
		TEXT("Missing terminal messages receive a useful message"),
		TerminalError.Message.IsEmpty()
	);
	TestFalse(
		TEXT("Terminal failures remain post-acceptance"),
		TerminalError.bRejectedBeforeSubmission
	);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsRecoveryPolicyTest,
	"OpenMobile.Haptics.Recovery.BoundedPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsRecoveryPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsRecoveryPolicy Policy;
	Policy.BeginInterruption(10.0, 2);

	TestEqual(
		TEXT("Recovery waits before its first native attempt"),
		Policy.TryBeginAttempt(10.099, true, true).Outcome,
		EOpenMobileHapticsRecoveryAttemptOutcome::Deferred
	);
	TestEqual(
		TEXT("Inactive applications do not consume an attempt"),
		Policy.TryBeginAttempt(11.0, false, true).Outcome,
		EOpenMobileHapticsRecoveryAttemptOutcome::Inactive
	);
	TestEqual(
		TEXT("Disabled player policy does not consume an attempt"),
		Policy.TryBeginAttempt(11.0, true, false).Outcome,
		EOpenMobileHapticsRecoveryAttemptOutcome::PolicyBlocked
	);
	const FOpenMobileHapticsRecoveryAttempt First =
		Policy.TryBeginAttempt(10.1, true, true);
	TestEqual(TEXT("The first eligible attempt starts"), First.Outcome,
		EOpenMobileHapticsRecoveryAttemptOutcome::Started);
	TestEqual(TEXT("The first attempt is numbered once"),
		First.AttemptNumber, 1);
	Policy.CompleteAttempt(false, 10.1);

	Policy.BeginInterruption(10.2, 8);
	TestEqual(TEXT("Duplicate interruptions do not reset retry ownership"),
		Policy.GetAttemptCount(), 1);
	TestEqual(TEXT("Failed recovery uses a bounded backoff"),
		Policy.TryBeginAttempt(10.349, true, true).Outcome,
		EOpenMobileHapticsRecoveryAttemptOutcome::Deferred);
	const FOpenMobileHapticsRecoveryAttempt Second =
		Policy.TryBeginAttempt(10.35, true, true);
	TestEqual(TEXT("The second bounded attempt starts"), Second.Outcome,
		EOpenMobileHapticsRecoveryAttemptOutcome::Started);
	TestEqual(TEXT("The second attempt preserves ordering"),
		Second.AttemptNumber, 2);
	Policy.CompleteAttempt(false, 10.35);
	TestEqual(TEXT("The configured boundary is terminal"),
		Policy.TryBeginAttempt(100.0, true, true).Outcome,
		EOpenMobileHapticsRecoveryAttemptOutcome::Exhausted);

	Policy.Reset();
	Policy.BeginInterruption(20.0, 1);
	TestEqual(TEXT("A fresh recovery can start"),
		Policy.TryBeginAttempt(20.1, true, true).Outcome,
		EOpenMobileHapticsRecoveryAttemptOutcome::Started);
	Policy.CompleteAttempt(true, 20.1);
	TestEqual(TEXT("Successful recovery clears pending state"),
		Policy.TryBeginAttempt(20.2, true, true).Outcome,
		EOpenMobileHapticsRecoveryAttemptOutcome::NotRecovering);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsRecoveryRetryTest,
	"OpenMobile.Haptics.Recovery.RegistryRetryBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsRecoveryRetryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const int32 SavedMaximumAttempts = Settings->MaximumRecoveryAttempts;
	Settings->MaximumRecoveryAttempts = 2;

	FMockBackend Backend(TEXT("RecoveryRetry"));
	Backend.RecoveryResult =
		EOpenMobileHapticsRecoveryResult::RetryableFailure;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::NotifyInterruption(
		Backend.GetBackendName(),
		EOpenMobileHapticsInterruptionReason::NativeServiceLost
	);
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(false);
	TestFalse(TEXT("Recovery remains unavailable while queued"),
		FOpenMobileHapticsBackendRegistry::RequestRecovery(true));
	FOpenMobileHapticsBackendRegistry::RunRecoveryAttemptForTests(
		FPlatformTime::Seconds() + 5.0
	);
	TestEqual(TEXT("Background state does not consume a native retry"),
		Backend.RecoveryAttemptCount, 0);
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(true);
	FOpenMobileHapticsBackendRegistry::RunRecoveryAttemptForTests(
		FPlatformTime::Seconds() + 5.0
	);
	TestEqual(TEXT("The first retry reaches the backend once"),
		Backend.RecoveryAttemptCount, 1);
	FOpenMobileHapticsBackendRegistry::RunRecoveryAttemptForTests(
		FPlatformTime::Seconds() + 10.0
	);
	TestEqual(TEXT("The configured final retry reaches the backend"),
		Backend.RecoveryAttemptCount, 2);
	FOpenMobileHapticsBackendRegistry::RunRecoveryAttemptForTests(
		FPlatformTime::Seconds() + 20.0
	);
	TestEqual(TEXT("Exhaustion prevents an unbounded retry loop"),
		Backend.RecoveryAttemptCount, 2);
	TestTrue(TEXT("Exhausted recovery remains unavailable"),
		FOpenMobileHapticsBackendRegistry::IsRecovering());

	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->MaximumRecoveryAttempts = SavedMaximumAttempts;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsInterruptionRecoveryTest,
	"OpenMobile.Haptics.Recovery.OrderingAndExplicitRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsInterruptionRecoveryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	const bool bSavedResume =
		Settings->bResumeEligiblePlaybackAfterForeground;
	const float SavedDefaultMinimumIntervalSeconds =
		Settings->DefaultMinimumIntervalSeconds;
	Settings->NamedLibraries.Reset();
	Settings->bResumeEligiblePlaybackAfterForeground = true;
	Settings->DefaultMinimumIntervalSeconds = 0.0f;

	FMockBackend Backend(TEXT("RecoveryOrdering"));
	Backend.ControlSupport.bStop = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	TArray<FOpenMobileHapticPlaybackEvent> Events;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&Events](const FOpenMobileHapticPlaybackEvent& Event)
		{
			Events.Add(Event);
		}
	);

	FOpenMobileHapticNamedPatternRequest Request;
	Request.PatternName = TEXT("RestartableLoop");
	Request.Options.Channel = TEXT("RecoveryRestart");
	Request.Options.Loop.bLoop = true;
	Request.Options.InterruptionPolicy =
		EOpenMobileHapticInterruptionPolicy::Restart;
	const FOpenMobileHapticPlaybackResult Original =
		Subsystem->SubmitNamedPattern(Request);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	Backend.Emit(0, EOpenMobileHapticPlaybackState::Started, 1,
		EOpenMobileHapticEventEvidence::NativeConfirmed);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	bool bInterruptedBeforeBackendCleanup = false;
	Backend.OnHandleInterruption =
		[Subsystem, Handle = Original.Handle, &bInterruptedBeforeBackendCleanup]()
		{
			bInterruptedBeforeBackendCleanup =
				Subsystem->GetPlaybackState(Handle)
				== EOpenMobileHapticPlaybackState::Interrupted;
		};
	FOpenMobileHapticsBackendRegistry::NotifyInterruption(
		Backend.GetBackendName(),
		EOpenMobileHapticsInterruptionReason::EngineReset
	);
	TestTrue(TEXT("Accepted handles interrupt before backend cleanup"),
		bInterruptedBeforeBackendCleanup);
	TestEqual(TEXT("The reset reaches backend cleanup once"),
		Backend.InterruptionCount, 1);
	TestEqual(TEXT("The reset preserves its reason"),
		Backend.LastInterruptionReason,
		EOpenMobileHapticsInterruptionReason::EngineReset);
	TestEqual(TEXT("The old handle has one terminal interruption"),
		Subsystem->GetPlaybackState(Original.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);

	Backend.Emit(0, EOpenMobileHapticPlaybackState::Completed, 2,
		EOpenMobileHapticEventEvidence::NativeConfirmed);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("A stale player cannot replace interruption"),
		Subsystem->GetPlaybackState(Original.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);

	FOpenMobileHapticsBackendRegistry::RunRecoveryAttemptForTests(
		FPlatformTime::Seconds() + 5.0
	);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Recovery rebuilds the backend once"),
		Backend.RecoveryAttemptCount, 1);
	TestEqual(TEXT("Only the explicit restart policy replays the pattern"),
		Backend.NamedSubmissionCount, 2);
	bool bFoundReplacement = false;
	for (const FOpenMobileHapticPlaybackEvent& Event : Events)
	{
		bFoundReplacement = bFoundReplacement
			|| (Event.State == EOpenMobileHapticPlaybackState::Accepted
				&& Event.RecoverySourceHandle == Original.Handle
				&& Event.Handle != Original.Handle);
	}
	TestTrue(TEXT("Restarted playback identifies the interrupted handle"),
		bFoundReplacement);

	const int32 ReplacementCallback = Backend.GetPendingCallbackCount() - 1;
	Backend.Emit(
		ReplacementCallback,
		EOpenMobileHapticPlaybackState::Started,
		1,
		EOpenMobileHapticEventEvidence::NativeConfirmed
	);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	FOpenMobileHapticsBackendRegistry::NotifyInterruption(
		Backend.GetBackendName(),
		EOpenMobileHapticsInterruptionReason::EngineReset
	);
	const int32 SubmissionsBeforeDisabledRecovery =
		Backend.NamedSubmissionCount;
	Subsystem->SetHapticsEnabled(false);
	FOpenMobileHapticsBackendRegistry::RunRecoveryAttemptForTests(
		FPlatformTime::Seconds() + 10.0
	);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Backend recovery still completes while policy is disabled"),
		Backend.RecoveryAttemptCount, 2);
	TestEqual(TEXT("Disable removes queued ordinary restart work"),
		Backend.NamedSubmissionCount, SubmissionsBeforeDisabledRecovery);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	Settings->bResumeEligiblePlaybackAfterForeground = bSavedResume;
	Settings->DefaultMinimumIntervalSeconds =
		SavedDefaultMinimumIntervalSeconds;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsInterruptionStateTest,
	"OpenMobile.Haptics.Recovery.StateCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsInterruptionStateTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	const float SavedDefaultMinimumIntervalSeconds =
		Settings->DefaultMinimumIntervalSeconds;
	Settings->NamedLibraries.Reset();
	Settings->DefaultMinimumIntervalSeconds = 0.0f;

	FMockBackend Backend(TEXT("RecoveryStates"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::BasicVibration;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Scheduling =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Mixing =
		EOpenMobileHapticSupportState::Supported;
	Backend.ControlSupport.bStop = true;
	Backend.ControlSupport.bPause = true;
	Backend.SubmissionControlSupport.PauseImplementation =
		EOpenMobileHapticControlImplementation::Native;
	Backend.SubmissionControlSupport.bHasRepeatPlan = true;
	Backend.SubmissionControlSupport.RepeatPlan.PatternDurationSeconds = 2.0;
	Backend.SubmissionControlSupport.RepeatPlan.TotalDurationSeconds = 2.0;
	Backend.SubmissionControlSupport.RepeatPlan.MaximumDurationSeconds = 30.0;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	FOpenMobileHapticPlaybackOptions ScheduledOptions;
	ScheduledOptions.Channel = TEXT("Scheduled");
	ScheduledOptions.Schedule.Mode =
		EOpenMobileHapticScheduleMode::Relative;
	ScheduledOptions.Schedule.TimeSeconds = 1.0;
	const FOpenMobileHapticPlaybackResult Scheduled =
		Subsystem->VibrateAdvanced(0.05f, 1.0f, ScheduledOptions);
	FOpenMobileHapticPlaybackOptions ConcurrentOptions;
	ConcurrentOptions.Channel = TEXT("RecoveryStates");
	ConcurrentOptions.OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::MixWhenSupported;
	const FOpenMobileHapticPlaybackResult Active =
		Subsystem->PlayNamedPatternAdvanced(
			TEXT("Active"), 1.0f, ConcurrentOptions);
	const FOpenMobileHapticPlaybackResult Paused =
		Subsystem->PlayNamedPatternAdvanced(
			TEXT("Paused"), 1.0f, ConcurrentOptions);
	FOpenMobileHapticPlaybackOptions RepeatingOptions = ConcurrentOptions;
	RepeatingOptions.Loop.bLoop = true;
	const FOpenMobileHapticPlaybackResult Repeating =
		Subsystem->PlayNamedPatternAdvanced(
			TEXT("Repeating"), 1.0f, RepeatingOptions);
	const FOpenMobileHapticPlaybackResult Stopping =
		Subsystem->PlayNamedPatternAdvanced(
			TEXT("Stopping"), 1.0f, ConcurrentOptions);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	Backend.Emit(1, EOpenMobileHapticPlaybackState::Started, 1);
	Backend.Emit(2, EOpenMobileHapticPlaybackState::Started, 1);
	Backend.Emit(3, EOpenMobileHapticPlaybackState::Started, 1);
	Backend.Emit(4, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("The paused fixture enters paused state"),
		Subsystem->PausePlayback(Paused.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("The stopping fixture reaches a terminal state"),
		Subsystem->StopPlayback(Stopping.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);

	const FOpenMobileHapticLibraryPreloadHandle Preparing =
		Subsystem->PreloadNamedLibraries();
	TestTrue(TEXT("The preparation fixture owns a request"),
		Preparing.IsValid());
	TestEqual(TEXT("The preparation fixture is still pending"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Preparing);
	FOpenMobileHapticsBackendRegistry::NotifyInterruption(
		Backend.GetBackendName(),
		EOpenMobileHapticsInterruptionReason::NativeServiceLost
	);

	TestEqual(TEXT("Scheduled playback is interrupted"),
		Subsystem->GetPlaybackState(Scheduled.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	TestEqual(TEXT("Active playback is interrupted"),
		Subsystem->GetPlaybackState(Active.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	TestEqual(TEXT("Paused playback is interrupted"),
		Subsystem->GetPlaybackState(Paused.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	TestEqual(TEXT("Repeating playback is interrupted"),
		Subsystem->GetPlaybackState(Repeating.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	TestEqual(TEXT("A completed stop is not rewritten"),
		Subsystem->GetPlaybackState(Stopping.Handle),
		EOpenMobileHapticPlaybackState::Stopped);
	TestEqual(TEXT("Interrupted preparation is cancelled"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Unprepared);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("A late preparation callback stays cancelled"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Unprepared);
	TestEqual(TEXT("Default interruption policy never replays"),
		Backend.NamedSubmissionCount, 4);

	FOpenMobileHapticsBackendRegistry::RequestRecovery(true);
	FOpenMobileHapticsBackendRegistry::RunRecoveryAttemptForTests(
		FPlatformTime::Seconds() + 5.0
	);
	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::BeginShutdown();
	const int32 InterruptionsBeforeShutdown = Backend.InterruptionCount;
	FOpenMobileHapticsBackendRegistry::NotifyInterruption(
		Backend.GetBackendName(),
		EOpenMobileHapticsInterruptionReason::EngineStopped
	);
	TestEqual(TEXT("Shutdown drops native interruption callbacks"),
		Backend.InterruptionCount, InterruptionsBeforeShutdown);

	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->DefaultMinimumIntervalSeconds =
		SavedDefaultMinimumIntervalSeconds;
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

#if WITH_EDITORONLY_DATA

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAudioScheduleSubsystemTest,
	"OpenMobile.Haptics.Timing.SubsystemSchedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAudioScheduleSubsystemTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("TimingMock"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.Capabilities.Scheduling =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.SemanticFeedback =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	Backend.ControlSupport.bStop = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();
	UOpenMobileHapticPatternAsset* Pattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	Pattern->SourcePattern.Events.AddDefaulted();
	TArray<FString> PatternErrors;
	TestTrue(TEXT("The timing test pattern builds"),
		Pattern->RebuildDerivedData(PatternErrors));

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	const FOpenMobileHapticTimingCalibrationResult Calibration =
		Subsystem->CalibrateTimingClock(
			EOpenMobileHapticTimingClock::Audio,
			100.0,
			0.003
		);
	TestTrue(TEXT("The audio schedule clock calibrates"),
		Calibration.bAccepted);

	FOpenMobileHapticNamedPatternRequest Request;
	Request.PatternName = TEXT("AudioAlignedImpact");
	Request.PatternAsset = FSoftObjectPath(Pattern);
	Request.Options.Channel = TEXT("AudioSync");
	Request.Options.Schedule.Mode =
		EOpenMobileHapticScheduleMode::AbsoluteAudioTime;
	Request.Options.Schedule.TimeSeconds = 100.5;
	const FOpenMobileHapticPlaybackResult Result =
		Subsystem->SubmitNamedPattern(Request);

	TestTrue(TEXT("The calibrated request is accepted"), Result.IsAccepted());
	TestEqual(TEXT("A future request returns scheduled state"),
		Result.State, EOpenMobileHapticPlaybackState::Scheduled);
	TestEqual(TEXT("The backend receives a resolved timing target"),
		Backend.LastNamedPlaybackParameters.Timing.Outcome,
		EOpenMobileHapticsTimingOutcome::Ready);
	TestTrue(TEXT("The backend receives a bounded future delay"),
		Backend.LastNamedPlaybackParameters.Timing.StartDelaySeconds > 0.0
			&& Backend.LastNamedPlaybackParameters.Timing.StartDelaySeconds
				<= 0.5);
	TestEqual(TEXT("External audio scheduling is explicitly best effort"),
		Result.Synchronization.Mode,
		EOpenMobileHapticSynchronizationMode::BestEffort);
	TestEqual(TEXT("The result reports the selected audio clock"),
		Result.Synchronization.Clock,
		EOpenMobileHapticTimingClock::Audio);

	FOpenMobileHapticPlaybackOptions RelativeOptions;
	RelativeOptions.Channel = TEXT("ScheduledSemantic");
	RelativeOptions.Schedule.Mode = EOpenMobileHapticScheduleMode::Relative;
	RelativeOptions.Schedule.TimeSeconds = 0.25;
	const FOpenMobileHapticPlaybackResult SemanticResult =
		Subsystem->PlaySemanticFeedbackAdvanced(
			EOpenMobileHapticSemanticEffect::Click,
			1.0f,
			RelativeOptions
		);
	TestTrue(TEXT("A relative semantic request is accepted"),
		SemanticResult.IsAccepted());
	TestEqual(TEXT("A future semantic request returns scheduled state"),
		SemanticResult.State, EOpenMobileHapticPlaybackState::Scheduled);
	TestTrue(TEXT("A scheduled semantic request has a cancellable handle"),
		SemanticResult.Handle.IsValid());
	TestEqual(TEXT("Semantic submission receives resolved timing"),
		Backend.LastSemanticPlaybackParameters.Timing.Outcome,
		EOpenMobileHapticsTimingOutcome::Ready);
	const TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> SemanticGuard =
		Backend.LastSemanticPlaybackParameters.ScheduledStartGuard;
	TestTrue(TEXT("Semantic scheduling owns a valid start guard"),
		SemanticGuard
		&& SemanticGuard->CanStart(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()));
	TestEqual(TEXT("A scheduled semantic request can be cancelled"),
		Subsystem->CancelPlayback(SemanticResult.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Cancellation becomes the terminal handle state"),
		Subsystem->GetPlaybackState(SemanticResult.Handle),
		EOpenMobileHapticPlaybackState::Cancelled);
	TestFalse(TEXT("Cancellation invalidates the native start guard"),
		SemanticGuard->CanStart(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()));

	RelativeOptions.Channel = TEXT("ScheduledOneShot");
	RelativeOptions.Schedule.TimeSeconds = 0.2;
	const FOpenMobileHapticPlaybackResult OneShotResult =
		Subsystem->VibrateAdvanced(0.05f, 1.0f, RelativeOptions);
	TestTrue(TEXT("A relative one-shot request is accepted"),
		OneShotResult.IsAccepted());
	TestEqual(TEXT("A future one-shot request returns scheduled state"),
		OneShotResult.State, EOpenMobileHapticPlaybackState::Scheduled);
	TestTrue(TEXT("A scheduled one-shot request has a cancellable handle"),
		OneShotResult.Handle.IsValid());
	TestEqual(TEXT("One-shot submission receives resolved timing"),
		Backend.LastOneShotPlaybackParameters.Timing.Outcome,
		EOpenMobileHapticsTimingOutcome::Ready);
	const TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> PolicyGuard =
		Backend.LastOneShotPlaybackParameters.ScheduledStartGuard;
	TestTrue(TEXT("The policy test owns a valid start guard"),
		PolicyGuard
		&& PolicyGuard->CanStart(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()));
	TestEqual(TEXT("A valid policy update is accepted"),
		Subsystem->UpdateUserPolicy(Subsystem->GetUserPolicy()).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestFalse(TEXT("A policy revision invalidates a delayed start"),
		PolicyGuard->CanStart(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()));

	RelativeOptions.Channel = TEXT("ScheduledLifecycle");
	RelativeOptions.Schedule.TimeSeconds = 0.15;
	const FOpenMobileHapticPlaybackResult LifecycleResult =
		Subsystem->VibrateAdvanced(0.05f, 1.0f, RelativeOptions);
	TestTrue(TEXT("The lifecycle request is scheduled"),
		LifecycleResult.IsAccepted());
	const TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> LifecycleGuard =
		Backend.LastOneShotPlaybackParameters.ScheduledStartGuard;
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(false);
	TestFalse(TEXT("Suspend invalidates the previous lifecycle generation"),
		LifecycleGuard->CanStart(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()));
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(true);

	Backend.Capabilities.Scheduling =
		EOpenMobileHapticSupportState::Unsupported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	Request.Options.Channel = TEXT("LatencyOffset");
	Request.Options.Schedule.Mode = EOpenMobileHapticScheduleMode::Immediate;
	Request.Options.Schedule.TimeSeconds = 0.0;
	Request.Options.Schedule.LatencyOffsetSeconds = 0.02;
	TestFalse(TEXT("A positive immediate offset still requires scheduling"),
		Subsystem->SubmitNamedPattern(Request).IsAccepted());

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsRecoveryPreparedAssetsTest,
	"OpenMobile.Haptics.Recovery.LazyPreparedAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsRecoveryPreparedAssetsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;

	FMockBackend Backend(TEXT("PreparedRecovery"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.Capabilities.RichHaptics =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.WaveformTiming =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UOpenMobileHapticPatternAsset* Pattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	Pattern->SourcePattern.Events.AddDefaulted();
	TArray<FString> PatternErrors;
	TestTrue(TEXT("The recovery pattern builds"),
		Pattern->RebuildDerivedData(PatternErrors));
	UOpenMobileHapticLibrary* Library = NewObject<UOpenMobileHapticLibrary>();
	Library->Patterns = {{TEXT("PreparedRecovery"), Pattern}};
	FOpenMobileHapticNamedLibrarySettings LibrarySettings;
	LibrarySettings.Name = TEXT("Recovery");
	LibrarySettings.Asset = FSoftObjectPath(Library);
	Settings->NamedLibraries = {LibrarySettings};

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	TArray<FString> Errors;
	TestTrue(TEXT("The recovery library prepares"),
		Subsystem->PrepareLoadedNamedLibraries({Library}, Errors));
	TestEqual(TEXT("Initial preparation compiles once"),
		Backend.PrepareResourcesCount, 1);
	const FOpenMobileHapticPlaybackResult Playback =
		Subsystem->PlayNamedPattern(TEXT("PreparedRecovery"));
	TestTrue(TEXT("Prepared playback is accepted"), Playback.IsAccepted());

	FOpenMobileHapticsBackendRegistry::NotifyInterruption(
		Backend.GetBackendName(),
		EOpenMobileHapticsInterruptionReason::EngineReset
	);
	TestEqual(TEXT("Reset releases native prepared ownership"),
		Backend.ReleasePreparedResourcesCount, 1);
	TestEqual(TEXT("Resolved library entries remain loaded"),
		Subsystem->GetNamedPatternStatus(TEXT("PreparedRecovery")),
		EOpenMobileHapticNamedPatternStatus::Loaded);
	TestEqual(TEXT("Native readiness becomes unprepared"),
		Subsystem->GetPreparationState(),
		EOpenMobileHapticPreparationState::Unprepared);

	FOpenMobileHapticsBackendRegistry::RequestRecovery(true);
	FOpenMobileHapticsBackendRegistry::RunRecoveryAttemptForTests(
		FPlatformTime::Seconds() + 5.0
	);
	TestEqual(TEXT("Engine recovery does not eagerly rebuild assets"),
		Backend.PrepareResourcesCount, 1);
	const FOpenMobileHapticPlaybackResult Replayed =
		Subsystem->PlayNamedPattern(
			TEXT("PreparedRecovery"),
			1.0f,
			TEXT("PreparedRecoveryReplay")
		);
	TestTrue(TEXT("The next named request restores preparation"),
		Replayed.IsAccepted());
	TestEqual(TEXT("Lazy restoration recompiles exactly once"),
		Backend.PrepareResourcesCount, 2);
	TestEqual(TEXT("Lazy restoration keeps the loaded registry"),
		Subsystem->GetNamedPatternStatus(TEXT("PreparedRecovery")),
		EOpenMobileHapticNamedPatternStatus::Loaded);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsLifecyclePolicyTest,
	"OpenMobile.Haptics.Lifecycle.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsLifecyclePolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsLifecyclePolicy Policy;
	TestEqual(TEXT("Lifecycle starts active"), Policy.GetState(),
		EOpenMobileHapticsApplicationState::Active);

	const FOpenMobileHapticsLifecycleTransition Deactivated =
		Policy.Apply(EOpenMobileHapticsLifecycleEvent::WillDeactivate);
	TestTrue(TEXT("Deactivation changes process state"),
		Deactivated.bChanged);
	TestTrue(TEXT("Deactivation interrupts accepted work"),
		Deactivated.bInterruptsPlayback);
	TestEqual(TEXT("Deactivation enters inactive state"),
		Deactivated.CurrentState,
		EOpenMobileHapticsApplicationState::Inactive);
	TestFalse(TEXT("Duplicate deactivation is idempotent"),
		Policy.Apply(
			EOpenMobileHapticsLifecycleEvent::WillDeactivate
		).bChanged);

	FOpenMobileHapticsLifecycleRequestContext Request;
	Request.BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::CriticalOnly;
	Request.BackgroundAlerts = EOpenMobileHapticSupportState::Supported;
	Request.Priority = EOpenMobileHapticChannelPriority::Critical;
	Request.Category = TEXT("Alerts");
	Request.Kind = EOpenMobileHapticsLifecycleRequestKind::Semantic;
	Request.SemanticEffect =
		EOpenMobileHapticSemanticEffect::NotificationWarning;
	TestEqual(TEXT("Inactive requests remain suppressed"),
		FOpenMobileHapticsLifecyclePolicy::Evaluate(Request, Policy.GetState()),
		EOpenMobileHapticsLifecycleRequestOutcome::Suppressed);

	const FOpenMobileHapticsLifecycleTransition Backgrounded =
		Policy.Apply(EOpenMobileHapticsLifecycleEvent::WillEnterBackground);
	TestTrue(TEXT("Background transition is recorded"),
		Backgrounded.bChanged);
	TestEqual(TEXT("Background transition reaches background state"),
		Policy.GetState(), EOpenMobileHapticsApplicationState::Background);
	TestEqual(TEXT("Explicit Android-style alert intent is allowed"),
		FOpenMobileHapticsLifecyclePolicy::Evaluate(Request, Policy.GetState()),
		EOpenMobileHapticsLifecycleRequestOutcome::BackgroundAlert);

	Request.Category = TEXT("Gameplay");
	TestEqual(TEXT("Critical gameplay cannot claim alert intent"),
		FOpenMobileHapticsLifecyclePolicy::Evaluate(Request, Policy.GetState()),
		EOpenMobileHapticsLifecycleRequestOutcome::Suppressed);
	Request.Category = TEXT("Alerts");
	Request.BackgroundAlerts = EOpenMobileHapticSupportState::Unsupported;
	TestEqual(TEXT("Unsupported platforms reject background alerts"),
		FOpenMobileHapticsLifecyclePolicy::Evaluate(Request, Policy.GetState()),
		EOpenMobileHapticsLifecycleRequestOutcome::Suppressed);
	Request.BackgroundAlerts = EOpenMobileHapticSupportState::Supported;
	Request.BackgroundPolicy = EOpenMobileHapticBackgroundPolicy::AllowAll;
	TestEqual(TEXT("Unsafe unrestricted policy fails closed"),
		FOpenMobileHapticsLifecyclePolicy::Evaluate(Request, Policy.GetState()),
		EOpenMobileHapticsLifecycleRequestOutcome::Suppressed);
	Request.BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::CriticalOnly;
	Request.Kind = EOpenMobileHapticsLifecycleRequestKind::NamedPattern;
	Request.bPatternSuitableForBackgroundPlayback = false;
	TestEqual(TEXT("Unmarked assets cannot play in the background"),
		FOpenMobileHapticsLifecyclePolicy::Evaluate(Request, Policy.GetState()),
		EOpenMobileHapticsLifecycleRequestOutcome::Suppressed);
	Request.bPatternSuitableForBackgroundPlayback = true;
	TestEqual(TEXT("Marked critical alert assets remain eligible"),
		FOpenMobileHapticsLifecyclePolicy::Evaluate(Request, Policy.GetState()),
		EOpenMobileHapticsLifecycleRequestOutcome::BackgroundAlert);

	const FOpenMobileHapticsLifecycleTransition Foregrounded =
		Policy.Apply(EOpenMobileHapticsLifecycleEvent::HasEnteredForeground);
	TestTrue(TEXT("Foreground transition stops background work"),
		Foregrounded.bInterruptsPlayback);
	TestEqual(TEXT("Foreground remains inactive until reactivation"),
		Policy.GetState(), EOpenMobileHapticsApplicationState::Inactive);
	const FOpenMobileHapticsLifecycleTransition Reactivated =
		Policy.Apply(EOpenMobileHapticsLifecycleEvent::HasReactivated);
	TestTrue(TEXT("Reactivation refreshes native services"),
		Reactivated.bRefreshesNativeServices);
	TestEqual(TEXT("Reactivation restores active state"), Policy.GetState(),
		EOpenMobileHapticsApplicationState::Active);
	TestEqual(TEXT("Active requests ignore background policy"),
		FOpenMobileHapticsLifecyclePolicy::Evaluate(Request, Policy.GetState()),
		EOpenMobileHapticsLifecycleRequestOutcome::Allowed);

	const FOpenMobileHapticsLifecycleTransition Terminated =
		Policy.Apply(EOpenMobileHapticsLifecycleEvent::WillTerminate);
	TestTrue(TEXT("Termination interrupts remaining work"),
		Terminated.bInterruptsPlayback);
	TestEqual(TEXT("Termination is terminal"), Policy.GetState(),
		EOpenMobileHapticsApplicationState::Terminating);
	TestFalse(TEXT("Late reactivation cannot escape termination"),
		Policy.Apply(
			EOpenMobileHapticsLifecycleEvent::HasReactivated
		).bChanged);
	Policy.Reset();
	const FOpenMobileHapticsLifecycleTransition DirectBackground =
		Policy.Apply(EOpenMobileHapticsLifecycleEvent::WillEnterBackground);
	TestTrue(TEXT("Direct background entry interrupts foreground work"),
		DirectBackground.bInterruptsPlayback);
	const FOpenMobileHapticsLifecycleTransition DirectReactivation =
		Policy.Apply(EOpenMobileHapticsLifecycleEvent::HasReactivated);
	TestTrue(TEXT("Direct warm resume stops background work"),
		DirectReactivation.bInterruptsPlayback);
	TestTrue(TEXT("Direct warm resume refreshes native services"),
		DirectReactivation.bRefreshesNativeServices);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsChannelPolicyTest,
	"OpenMobile.Haptics.Channels.PolicyAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsChannelPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticsSettings* Settings =
		NewObject<UOpenMobileHapticsSettings>();
	const FOpenMobileHapticsResolvedChannel UI =
		FOpenMobileHapticsChannelPolicy::Resolve(
			TEXT("UI"),
			EOpenMobileHapticChannelPriority::High,
			Settings->Channels,
			Settings->MaximumActiveHandles,
			Settings->MaximumQueueDepthPerChannel
		);
	TestTrue(TEXT("Built-in UI channel is configured"), UI.bConfigured);
	TestEqual(TEXT("Request priority can raise a channel baseline"),
		UI.EffectivePriority, EOpenMobileHapticChannelPriority::High);
	const FOpenMobileHapticsResolvedChannel Alerts =
		FOpenMobileHapticsChannelPolicy::Resolve(
			TEXT("Alerts"),
			EOpenMobileHapticChannelPriority::Low,
			Settings->Channels,
			Settings->MaximumActiveHandles,
			Settings->MaximumQueueDepthPerChannel
		);
	TestEqual(TEXT("Request priority cannot lower an Alerts baseline"),
		Alerts.EffectivePriority, EOpenMobileHapticChannelPriority::High);

	FOpenMobileHapticChannelSettings Custom;
	Custom.Name = TEXT("VehicleCabin");
	Custom.Priority = EOpenMobileHapticChannelPriority::High;
	Custom.MaximumActiveHandles = 2;
	Custom.MaximumQueueDepth = 3;
	Settings->Channels.Add(Custom);
	const FOpenMobileHapticsResolvedChannel CustomResolution =
		FOpenMobileHapticsChannelPolicy::Resolve(
			TEXT("VehicleCabin"),
			EOpenMobileHapticChannelPriority::Normal,
			Settings->Channels,
			Settings->MaximumActiveHandles,
			Settings->MaximumQueueDepthPerChannel
		);
	TestTrue(TEXT("Project channels resolve without native objects"),
		CustomResolution.bConfigured);
	TestEqual(TEXT("Project channel priority is applied"),
		CustomResolution.EffectivePriority,
		EOpenMobileHapticChannelPriority::High);
	TestEqual(TEXT("Project channel queue depth is applied"),
		CustomResolution.MaximumQueueDepth, 3);
	TestEqual(TEXT("Project channel active limit is applied"),
		CustomResolution.MaximumActiveHandles, 2);
	const FOpenMobileHapticsResolvedChannel Unconfigured =
		FOpenMobileHapticsChannelPolicy::Resolve(
			TEXT("RuntimeOnly"),
			EOpenMobileHapticChannelPriority::Normal,
			Settings->Channels,
			Settings->MaximumActiveHandles,
			Settings->MaximumQueueDepthPerChannel
		);
	TestFalse(TEXT("Unconfigured names remain explicit"),
		Unconfigured.bConfigured);
	TestEqual(TEXT("Unconfigured names use the bounded global depth"),
		Unconfigured.MaximumQueueDepth,
		Settings->MaximumQueueDepthPerChannel);
	TestEqual(TEXT("Unconfigured names use the global active bound"),
		Unconfigured.MaximumActiveHandles,
		Settings->MaximumActiveHandles);

	FOpenMobileHapticsChannelArbiter Arbiter;
	FOpenMobileHapticsChannelLimits Limits;
	Limits.MaximumActiveHandles = 2;
	Limits.MaximumQueuedHandles = 3;
	Limits.MaximumQueueDepthPerChannel = 2;
	Arbiter.Configure(Limits);
	auto MakeRequest = [](
		uint64 RequestId,
		FName Channel,
		EOpenMobileHapticChannelPriority Priority,
		bool bQueued,
		bool bRepeating,
		int32 MaximumQueueDepth = 2,
		int32 MaximumActiveHandles = 2
	)
	{
		FOpenMobileHapticsChannelAdmissionRequest Request;
		Request.RequestId = RequestId;
		Request.Channel = Channel;
		Request.Priority = Priority;
		Request.bQueued = bQueued;
		Request.bRepeating = bRepeating;
		Request.MaximumQueueDepth = MaximumQueueDepth;
		Request.MaximumActiveHandles = MaximumActiveHandles;
		return Request;
	};
	TestEqual(TEXT("First active handle is admitted"),
		Arbiter.TryReserve(MakeRequest(
			1, TEXT("Gameplay"), EOpenMobileHapticChannelPriority::Low,
			false, true
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	TestEqual(TEXT("Independent channel uses the second active slot"),
		Arbiter.TryReserve(MakeRequest(
			2, TEXT("UI"), EOpenMobileHapticChannelPriority::Normal,
			false, false
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	const FOpenMobileHapticsChannelAdmissionResult HighPriority =
		Arbiter.TryReserve(MakeRequest(
			3, TEXT("Alerts"), EOpenMobileHapticChannelPriority::High,
			false, false
		));
	TestEqual(TEXT("High priority requests can displace a lower repeat"),
		HighPriority.Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::PreemptRequired);
	TestEqual(TEXT("The lower repeated handle is selected"),
		HighPriority.PreemptRequestId, static_cast<uint64>(1));
	Arbiter.Release(1);
	TestEqual(TEXT("High priority request enters the released slot"),
		Arbiter.TryReserve(MakeRequest(
			3, TEXT("Alerts"), EOpenMobileHapticChannelPriority::High,
			false, false
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	TestEqual(TEXT("Equal or lower priority cannot invert capacity"),
		Arbiter.TryReserve(MakeRequest(
			4, TEXT("Gameplay"), EOpenMobileHapticChannelPriority::Low,
			false, false
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::ActiveCapacityReached);

	FOpenMobileHapticsChannelLimits PerChannelLimits = Limits;
	PerChannelLimits.MaximumActiveHandles = 4;
	FOpenMobileHapticsChannelArbiter PerChannelArbiter;
	PerChannelArbiter.Configure(PerChannelLimits);
	TestEqual(TEXT("First same-channel active handle is admitted"),
		PerChannelArbiter.TryReserve(MakeRequest(
			5, TEXT("Gameplay"), EOpenMobileHapticChannelPriority::Normal,
			false, false, 2, 1
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	TestEqual(TEXT("Per-channel active capacity is enforced"),
		PerChannelArbiter.TryReserve(MakeRequest(
			6, TEXT("Gameplay"), EOpenMobileHapticChannelPriority::Normal,
			false, false, 2, 1
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::
			ChannelActiveCapacityReached);
	TestEqual(TEXT("Another channel keeps independent active capacity"),
		PerChannelArbiter.TryReserve(MakeRequest(
			7, TEXT("UI"), EOpenMobileHapticChannelPriority::Normal,
			false, false, 2, 1
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);

	FOpenMobileHapticsChannelArbiter TieForward;
	FOpenMobileHapticsChannelArbiter TieReverse;
	TieForward.Configure(Limits);
	TieReverse.Configure(Limits);
	const FOpenMobileHapticsChannelAdmissionRequest Older = MakeRequest(
		10, TEXT("A"), EOpenMobileHapticChannelPriority::Low, false, true
	);
	const FOpenMobileHapticsChannelAdmissionRequest Newer = MakeRequest(
		11, TEXT("B"), EOpenMobileHapticChannelPriority::Low, false, true
	);
	TieForward.TryReserve(Older);
	TieForward.TryReserve(Newer);
	TieReverse.TryReserve(Newer);
	TieReverse.TryReserve(Older);
	const FOpenMobileHapticsChannelAdmissionRequest Incoming = MakeRequest(
		12, TEXT("Critical"), EOpenMobileHapticChannelPriority::Critical,
		false, false
	);
	TestEqual(TEXT("Tie-break selects the newest equal-priority repeat"),
		TieForward.TryReserve(Incoming).PreemptRequestId,
		static_cast<uint64>(11));
	TestEqual(TEXT("Tie-break ignores insertion and callback order"),
		TieReverse.TryReserve(Incoming).PreemptRequestId,
		static_cast<uint64>(11));
	const FOpenMobileHapticsChannelAdmissionRequest RepeatingIncoming =
		MakeRequest(
			13,
			TEXT("Critical"),
			EOpenMobileHapticChannelPriority::Critical,
			false,
			true
		);
	TestEqual(TEXT("Repeated work cannot use the starvation exception"),
		TieForward.TryReserve(RepeatingIncoming).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::ActiveCapacityReached);

	FOpenMobileHapticsChannelLimits UntrustedLimits;
	UntrustedLimits.MaximumActiveHandles = MAX_int32;
	UntrustedLimits.MaximumQueuedHandles = MAX_int32;
	UntrustedLimits.MaximumQueueDepthPerChannel = MAX_int32;
	FOpenMobileHapticsChannelArbiter HardBoundArbiter;
	HardBoundArbiter.Configure(UntrustedLimits);
	for (uint64 RequestId = 100; RequestId < 228; ++RequestId)
	{
		TestEqual(TEXT("Hard active bound admits a safe slot"),
			HardBoundArbiter.TryReserve(MakeRequest(
				RequestId,
				FName(*FString::FromInt(static_cast<int32>(RequestId))),
				EOpenMobileHapticChannelPriority::Normal,
				false,
				false,
				MAX_int32,
				MAX_int32
			)).Outcome,
			EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	}
	TestEqual(TEXT("Invalid settings cannot exceed the hard active bound"),
		HardBoundArbiter.TryReserve(MakeRequest(
			228, TEXT("Overflow"), EOpenMobileHapticChannelPriority::Normal,
			false, false, MAX_int32, MAX_int32
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::ActiveCapacityReached);
	HardBoundArbiter.Reset();
	for (uint64 RequestId = 300; RequestId < 364; ++RequestId)
	{
		TestEqual(TEXT("Hard channel queue bound admits a safe slot"),
			HardBoundArbiter.TryReserve(MakeRequest(
				RequestId,
				TEXT("Queued"),
				EOpenMobileHapticChannelPriority::Normal,
				true,
				false,
				MAX_int32,
				MAX_int32
			)).Outcome,
			EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	}
	TestEqual(TEXT("Invalid settings cannot exceed the hard queue bound"),
		HardBoundArbiter.TryReserve(MakeRequest(
			364, TEXT("Queued"), EOpenMobileHapticChannelPriority::Normal,
			true, false, MAX_int32, MAX_int32
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::
			ChannelQueueCapacityReached);

	FOpenMobileHapticsChannelArbiter QueueArbiter;
	FOpenMobileHapticsChannelLimits QueueLimits = Limits;
	QueueLimits.MaximumActiveHandles = 4;
	QueueArbiter.Configure(QueueLimits);
	TestEqual(TEXT("First scheduled handle is queued"),
		QueueArbiter.TryReserve(MakeRequest(
			20, TEXT("Cinematic"), EOpenMobileHapticChannelPriority::Normal,
			true, false
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	TestEqual(TEXT("Second scheduled handle fits its channel"),
		QueueArbiter.TryReserve(MakeRequest(
			21, TEXT("Cinematic"), EOpenMobileHapticChannelPriority::Normal,
			true, false
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	TestEqual(TEXT("Per-channel queue depth is enforced"),
		QueueArbiter.TryReserve(MakeRequest(
			22, TEXT("Cinematic"), EOpenMobileHapticChannelPriority::High,
			true, false
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::ChannelQueueCapacityReached);
	TestEqual(TEXT("Independent queued channel uses global capacity"),
		QueueArbiter.TryReserve(MakeRequest(
			23, TEXT("Alerts"), EOpenMobileHapticChannelPriority::High,
			true, false
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	TestEqual(TEXT("Global queued capacity is enforced"),
		QueueArbiter.TryReserve(MakeRequest(
			24, TEXT("UI"), EOpenMobileHapticChannelPriority::Normal,
			true, false
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::GlobalQueueCapacityReached);
	QueueArbiter.Reset();
	TestEqual(TEXT("Teardown releases all active reservations"),
		QueueArbiter.GetActiveCount(), 0);
	TestEqual(TEXT("Teardown releases all queued reservations"),
		QueueArbiter.GetQueuedCount(), 0);

	FOpenMobileHapticsChannelArbiter WaitingArbiter;
	WaitingArbiter.Configure({1, 2, 2});
	TestEqual(TEXT("Waiting test owns its active slot"),
		WaitingArbiter.TryReserve(MakeRequest(
			30, TEXT("Gameplay"), EOpenMobileHapticChannelPriority::Normal,
			false, false
		)).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	FOpenMobileHapticsChannelAdmissionRequest Waiting = MakeRequest(
		31, TEXT("Gameplay"), EOpenMobileHapticChannelPriority::High,
		true, false
	);
	Waiting.bWaitingForOverlap = true;
	TestEqual(TEXT("Overlap wait uses queue capacity without an active slot"),
		WaitingArbiter.TryReserve(Waiting).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	TestEqual(TEXT("Overlap wait does not inflate active diagnostics"),
		WaitingArbiter.GetActiveCount(), 1);
	TestEqual(TEXT("Overlap wait is visible in queued diagnostics"),
		WaitingArbiter.GetQueuedCount(), 1);
	FOpenMobileHapticsChannelAdmissionRequest Promotion = Waiting;
	Promotion.bWaitingForOverlap = false;
	Promotion.bQueued = false;
	TestEqual(TEXT("Promotion waits while active capacity remains full"),
		WaitingArbiter.TryReserve(Promotion).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::ChannelActiveCapacityReached);
	WaitingArbiter.Release(30);
	TestEqual(TEXT("Promotion converts the existing reservation atomically"),
		WaitingArbiter.TryReserve(Promotion).Outcome,
		EOpenMobileHapticsChannelAdmissionOutcome::Admitted);
	TestEqual(TEXT("Promotion consumes one active slot"),
		WaitingArbiter.GetActiveCount(), 1);
	TestEqual(TEXT("Promotion releases its overlap queue slot"),
		WaitingArbiter.GetQueuedCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsOverlapPolicyTest,
	"OpenMobile.Haptics.Overlap.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsOverlapPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FOpenMobileHapticCapabilities UnknownCapabilities;
	TestEqual(TEXT("Mixing support is unknown until a backend reports it"),
		UnknownCapabilities.Mixing,
		EOpenMobileHapticSupportState::Unknown);
	auto MakeConflict = [](
		uint64 RequestId,
		FName Channel,
		EOpenMobileHapticChannelPriority Priority,
		bool bQueued = false
	)
	{
		FOpenMobileHapticsOverlapConflict Conflict;
		Conflict.RequestId = RequestId;
		Conflict.Channel = Channel;
		Conflict.Priority = Priority;
		Conflict.bQueued = bQueued;
		return Conflict;
	};
	const TArray<FOpenMobileHapticsOverlapConflict> Conflicts = {
		MakeConflict(
			10,
			TEXT("Gameplay"),
			EOpenMobileHapticChannelPriority::Low
		),
		MakeConflict(
			11,
			TEXT("Gameplay"),
			EOpenMobileHapticChannelPriority::Normal,
			true
		),
		MakeConflict(
			12,
			TEXT("UI"),
			EOpenMobileHapticChannelPriority::Critical
		)
	};
	auto Resolve = [&Conflicts](
		EOpenMobileHapticOverlapPolicy Policy,
		EOpenMobileHapticChannelPriority Priority =
			EOpenMobileHapticChannelPriority::High,
		EOpenMobileHapticSupportState Mixing =
			EOpenMobileHapticSupportState::Unsupported,
		EOpenMobileHapticOverlapPolicy MixFallback =
			EOpenMobileHapticOverlapPolicy::Replace
	)
	{
		FOpenMobileHapticsOverlapRequest Request;
		Request.Channel = TEXT("Gameplay");
		Request.Priority = Priority;
		Request.Policy = Policy;
		Request.Mixing = Mixing;
		Request.UnsupportedMixFallback = MixFallback;
		return FOpenMobileHapticsOverlapPolicy::Resolve(Request, Conflicts);
	};

	const FOpenMobileHapticsOverlapResolution Replace = Resolve(
		EOpenMobileHapticOverlapPolicy::Replace
	);
	TestEqual(TEXT("Replace submits after ending same-channel work"),
		Replace.Outcome,
		EOpenMobileHapticsOverlapOutcome::InterruptThenSubmit);
	TestEqual(TEXT("Replace affects active and queued same-channel work"),
		Replace.TerminalRequestIds,
		TArray<uint64>({10, 11}));
	TestEqual(TEXT("Replace does not affect another channel"),
		Replace.TerminalRequestIds.Contains(12), false);
	TestEqual(TEXT("Ignore suppresses the incoming request"),
		Resolve(EOpenMobileHapticOverlapPolicy::Ignore).Outcome,
		EOpenMobileHapticsOverlapOutcome::Suppress);
	TestEqual(TEXT("Queue retains the incoming request"),
		Resolve(EOpenMobileHapticOverlapPolicy::Queue).Outcome,
		EOpenMobileHapticsOverlapOutcome::Queue);
	TestEqual(TEXT("Higher priority interrupts lower same-channel work"),
		Resolve(EOpenMobileHapticOverlapPolicy::InterruptLowerPriority).Outcome,
		EOpenMobileHapticsOverlapOutcome::InterruptThenSubmit);
	TestEqual(TEXT("Equal priority blocks interruption atomically"),
		Resolve(
			EOpenMobileHapticOverlapPolicy::InterruptLowerPriority,
			EOpenMobileHapticChannelPriority::Normal
		).Outcome,
		EOpenMobileHapticsOverlapOutcome::Suppress);
	const FOpenMobileHapticsOverlapResolution SupportedMix = Resolve(
		EOpenMobileHapticOverlapPolicy::MixWhenSupported,
		EOpenMobileHapticChannelPriority::Normal,
		EOpenMobileHapticSupportState::Supported
	);
	TestEqual(TEXT("Supported mixing permits concurrent submission"),
		SupportedMix.Outcome,
		EOpenMobileHapticsOverlapOutcome::Submit);
	TestFalse(TEXT("Supported mixing does not use its fallback"),
		SupportedMix.bUsedMixFallback);
	const FOpenMobileHapticsOverlapResolution UnknownMix = Resolve(
		EOpenMobileHapticOverlapPolicy::MixWhenSupported,
		EOpenMobileHapticChannelPriority::High,
		EOpenMobileHapticSupportState::Unknown,
		EOpenMobileHapticOverlapPolicy::Queue
	);
	TestEqual(TEXT("Unknown mixing support follows configured fallback"),
		UnknownMix.Outcome,
		EOpenMobileHapticsOverlapOutcome::Queue);
	TestTrue(TEXT("Mix fallback remains explicit"),
		UnknownMix.bUsedMixFallback);
	TestEqual(TEXT("Mix fallback reports its resolved policy"),
		UnknownMix.ResolvedPolicy,
		EOpenMobileHapticOverlapPolicy::Queue);

	FOpenMobileHapticsOverlapRequest Independent;
	Independent.Channel = TEXT("Cinematic");
	Independent.Priority = EOpenMobileHapticChannelPriority::Low;
	Independent.Policy = EOpenMobileHapticOverlapPolicy::Ignore;
	TestEqual(TEXT("Different channels do not collide"),
		FOpenMobileHapticsOverlapPolicy::Resolve(
			Independent,
			Conflicts
		).Outcome,
		EOpenMobileHapticsOverlapOutcome::Submit);

	TArray<FOpenMobileHapticsOverlapQueueEntry> Queue = {
		{20, TEXT("Gameplay"), EOpenMobileHapticChannelPriority::Normal, 1.0},
		{21, TEXT("Gameplay"), EOpenMobileHapticChannelPriority::High, 1.1},
		{22, TEXT("Gameplay"), EOpenMobileHapticChannelPriority::High, 1.1},
		{23, TEXT("UI"), EOpenMobileHapticChannelPriority::Critical, 0.5}
	};
	TestEqual(TEXT("Queue selects highest priority then oldest request ID"),
		FOpenMobileHapticsOverlapPolicy::SelectNext(
			TEXT("Gameplay"),
			Queue
		),
		static_cast<uint64>(21));
	TestFalse(TEXT("Queue age is inclusive at its exact boundary"),
		FOpenMobileHapticsOverlapPolicy::IsExpired(1.0, 2.0, 1.0));
	TestTrue(TEXT("Queue age expires stale requests"),
		FOpenMobileHapticsOverlapPolicy::IsExpired(1.0, 2.001, 1.0));
	TestFalse(TEXT("Backward clocks do not expire queued requests"),
		FOpenMobileHapticsOverlapPolicy::IsExpired(2.0, 1.0, 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsChannelSubsystemTest,
	"OpenMobile.Haptics.Channels.SubsystemAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsChannelSubsystemTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	const TArray<FOpenMobileHapticChannelSettings> SavedChannels =
		Settings->Channels;
	const int32 SavedMaximumActiveHandles = Settings->MaximumActiveHandles;
	const int32 SavedMaximumQueuedHandles = Settings->MaximumQueuedHandles;
	const int32 SavedMaximumQueueDepthPerChannel =
		Settings->MaximumQueueDepthPerChannel;
	Settings->NamedLibraries.Reset();
	Settings->MaximumActiveHandles = 2;
	Settings->MaximumQueuedHandles = 2;
	Settings->MaximumQueueDepthPerChannel = 1;
	for (FOpenMobileHapticChannelSettings& Channel : Settings->Channels)
	{
		Channel.MaximumActiveHandles = 2;
		Channel.MaximumQueueDepth = 1;
		Channel.MinimumIntervalSeconds = 0.0f;
	}

	FMockBackend Backend(TEXT("Channels"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.Capabilities.RichHaptics =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Scheduling =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Mixing =
		EOpenMobileHapticSupportState::Supported;
	Backend.ControlSupport.bStop = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	FOpenMobileHapticNamedPatternRequest InvalidPriorityRequest;
	InvalidPriorityRequest.PatternName = TEXT("InvalidPriority");
	InvalidPriorityRequest.Options.Priority =
		static_cast<EOpenMobileHapticChannelPriority>(MAX_uint8);
	const FOpenMobileHapticPlaybackResult InvalidPriority =
		Subsystem->SubmitNamedPattern(InvalidPriorityRequest);
	TestEqual(TEXT("Invalid priority values are rejected before admission"),
		InvalidPriority.Error.Code,
		EOpenMobileHapticErrorCode::InvalidRequest);
	TestEqual(TEXT("Invalid priorities never reach the backend"),
		Backend.NamedSubmissionCount, 0);
	FOpenMobileHapticNamedPatternRequest LowRepeatRequest;
	LowRepeatRequest.PatternName = TEXT("LowRepeat");
	LowRepeatRequest.Options.Channel = TEXT("Gameplay");
	LowRepeatRequest.Options.Priority = EOpenMobileHapticChannelPriority::Low;
	LowRepeatRequest.Options.Loop.bLoop = true;
	const FOpenMobileHapticPlaybackResult LowRepeat =
		Subsystem->SubmitNamedPattern(LowRepeatRequest);
	TestTrue(TEXT("Low repeated gameplay is initially accepted"),
		LowRepeat.IsAccepted());
	TestEqual(TEXT("Gameplay baseline raises the backend request priority"),
		Backend.LastNamedRequest.Options.Priority,
		EOpenMobileHapticChannelPriority::Normal);
	const FOpenMobileHapticsBackendRequestToken LowRepeatToken =
		Backend.LastToken;

	FOpenMobileHapticNamedPatternRequest IndependentRequest;
	IndependentRequest.PatternName = TEXT("IndependentUI");
	IndependentRequest.Options.Channel = TEXT("UI");
	IndependentRequest.Options.Priority = EOpenMobileHapticChannelPriority::Low;
	const FOpenMobileHapticPlaybackResult Independent =
		Subsystem->SubmitNamedPattern(IndependentRequest);
	TestTrue(TEXT("Independent UI channel uses remaining capacity"),
		Independent.IsAccepted());

	bool bInterruptedBeforeIncomingSubmission = false;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&Backend, &LowRepeat, &bInterruptedBeforeIncomingSubmission](
			const FOpenMobileHapticPlaybackEvent& Event
		)
		{
			if (Event.Handle == LowRepeat.Handle
				&& Event.State
					== EOpenMobileHapticPlaybackState::Interrupted)
			{
				bInterruptedBeforeIncomingSubmission =
					Backend.OneShotSubmissionCount == 0;
			}
		}
	);
	FOpenMobileHapticOneShotRequest CriticalRequest;
	CriticalRequest.DurationSeconds = 0.03f;
	CriticalRequest.Options.Channel = TEXT("Critical");
	CriticalRequest.Options.Category = TEXT("Alerts");
	CriticalRequest.Options.Priority = EOpenMobileHapticChannelPriority::Low;
	const FOpenMobileHapticPlaybackResult Critical =
		Subsystem->SubmitOneShot(CriticalRequest);
	TestTrue(TEXT("Short critical feedback is not starved"),
		Critical.IsAccepted());
	TestEqual(TEXT("Critical baseline reaches the backend"),
		Backend.LastOneShotRequest.Options.Priority,
		EOpenMobileHapticChannelPriority::Critical);
	TestEqual(TEXT("Only the lower repeated request is stopped"),
		Backend.StopPlaybackCount, 1);
	TestEqual(TEXT("Preemption preserves the selected request identity"),
		Backend.LastStoppedToken.RequestId, LowRepeatToken.RequestId);
	TestEqual(TEXT("Preempted feedback has an interrupted terminal state"),
		Subsystem->GetPlaybackState(LowRepeat.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	TestTrue(TEXT("Interruption is published before incoming submission"),
		bInterruptedBeforeIncomingSubmission);

	const int32 NamedBeforeCapacityRejection = Backend.NamedSubmissionCount;
	FOpenMobileHapticNamedPatternRequest CapacityRequest;
	CapacityRequest.PatternName = TEXT("CapacityRejected");
	CapacityRequest.Options.Channel = TEXT("Alerts");
	const FOpenMobileHapticPlaybackResult CapacityRejected =
		Subsystem->SubmitNamedPattern(CapacityRequest);
	TestEqual(TEXT("Full active capacity returns a typed channel error"),
		CapacityRejected.Error.Code,
		EOpenMobileHapticErrorCode::ChannelBusy);
	TestEqual(TEXT("Capacity rejection is assigned to channel policy"),
		CapacityRejected.Error.Stage,
		EOpenMobileHapticFailureStage::Channel);
	TestEqual(TEXT("Capacity rejection never reaches the backend"),
		Backend.NamedSubmissionCount, NamedBeforeCapacityRejection);

	TestEqual(TEXT("Stopping an independent handle releases capacity"),
		Subsystem->StopPlayback(Independent.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	Settings->MaximumActiveHandles = 8;
	for (FOpenMobileHapticChannelSettings& Channel : Settings->Channels)
	{
		Channel.MaximumActiveHandles = 4;
	}
	FOpenMobileHapticChannelSettings* CriticalChannel =
		Settings->Channels.FindByPredicate(
			[](const FOpenMobileHapticChannelSettings& Channel)
			{
				return Channel.Name == TEXT("Critical");
			}
		);
	TestNotNull(TEXT("Critical channel remains configured"), CriticalChannel);
	if (CriticalChannel)
	{
		CriticalChannel->MaximumActiveHandles = 1;
	}
	const int32 NamedBeforeChannelCapacity = Backend.NamedSubmissionCount;
	FOpenMobileHapticNamedPatternRequest ChannelCapacityRequest;
	ChannelCapacityRequest.PatternName = TEXT("CriticalChannelOverflow");
	ChannelCapacityRequest.Options.Channel = TEXT("Critical");
	ChannelCapacityRequest.Options.OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::MixWhenSupported;
	const FOpenMobileHapticPlaybackResult ChannelCapacityRejected =
		Subsystem->SubmitNamedPattern(ChannelCapacityRequest);
	TestEqual(TEXT("Per-channel active capacity is typed"),
		ChannelCapacityRejected.Error.Code,
		EOpenMobileHapticErrorCode::ChannelBusy);
	TestEqual(TEXT("Per-channel active rejection stays pre-submission"),
		Backend.NamedSubmissionCount, NamedBeforeChannelCapacity);
	FOpenMobileHapticNamedPatternRequest ScheduledRequest;
	ScheduledRequest.PatternName = TEXT("ScheduledCinematic");
	ScheduledRequest.Options.Channel = TEXT("Cinematic");
	ScheduledRequest.Options.Schedule.Mode =
		EOpenMobileHapticScheduleMode::Relative;
	ScheduledRequest.Options.Schedule.TimeSeconds = 5.0;
	ScheduledRequest.Options.OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::MixWhenSupported;
	const FOpenMobileHapticPlaybackResult ScheduledCinematic =
		Subsystem->SubmitNamedPattern(ScheduledRequest);
	TestTrue(TEXT("First delayed channel request is accepted"),
		ScheduledCinematic.IsAccepted());
	const int32 NamedAfterFirstScheduled = Backend.NamedSubmissionCount;
	ScheduledRequest.PatternName = TEXT("SameChannelQueueOverflow");
	const FOpenMobileHapticPlaybackResult ChannelQueueRejected =
		Subsystem->SubmitNamedPattern(ScheduledRequest);
	TestEqual(TEXT("Per-channel queued capacity is typed"),
		ChannelQueueRejected.Error.Code,
		EOpenMobileHapticErrorCode::ChannelBusy);
	TestEqual(TEXT("Per-channel queued rejection stays pre-submission"),
		Backend.NamedSubmissionCount, NamedAfterFirstScheduled);

	ScheduledRequest.PatternName = TEXT("ScheduledAlert");
	ScheduledRequest.Options.Channel = TEXT("Alerts");
	const FOpenMobileHapticPlaybackResult ScheduledAlert =
		Subsystem->SubmitNamedPattern(ScheduledRequest);
	TestTrue(TEXT("Independent channel uses remaining queued capacity"),
		ScheduledAlert.IsAccepted());
	const int32 NamedAtGlobalQueueCapacity = Backend.NamedSubmissionCount;
	ScheduledRequest.PatternName = TEXT("GlobalQueueOverflow");
	ScheduledRequest.Options.Channel = TEXT("UI");
	const FOpenMobileHapticPlaybackResult GlobalQueueRejected =
		Subsystem->SubmitNamedPattern(ScheduledRequest);
	TestEqual(TEXT("Global queued capacity is typed"),
		GlobalQueueRejected.Error.Code,
		EOpenMobileHapticErrorCode::ChannelBusy);
	TestEqual(TEXT("Global queued rejection never reaches native code"),
		Backend.NamedSubmissionCount, NamedAtGlobalQueueCapacity);
	FOpenMobileHapticSemanticRequest ScheduledSemantic;
	ScheduledSemantic.Effect = EOpenMobileHapticSemanticEffect::Click;
	ScheduledSemantic.Options.Channel = TEXT("UI");
	ScheduledSemantic.Options.Schedule = ScheduledRequest.Options.Schedule;
	const int32 SemanticsBeforeQueueRejection =
		Backend.SemanticSubmissionCount;
	const FOpenMobileHapticPlaybackResult SemanticQueueRejected =
		Subsystem->SubmitSemantic(ScheduledSemantic);
	TestEqual(TEXT("Scheduled semantic feedback shares queued capacity"),
		SemanticQueueRejected.Error.Code,
		EOpenMobileHapticErrorCode::ChannelBusy);
	TestEqual(TEXT("Semantic queue rejection stays pre-submission"),
		Backend.SemanticSubmissionCount,
		SemanticsBeforeQueueRejection);

	TestEqual(TEXT("Cancelling delayed work releases its queued slot"),
		Subsystem->CancelPlayback(ScheduledCinematic.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	const FOpenMobileHapticPlaybackResult ReusedQueueSlot =
		Subsystem->SubmitNamedPattern(ScheduledRequest);
	TestTrue(TEXT("A released queued slot can be reused"),
		ReusedQueueSlot.IsAccepted());

	Subsystem->Deinitialize();
	FOpenMobileHapticChannelSettings ProjectChannel;
	ProjectChannel.Name = TEXT("VehicleCabin");
	ProjectChannel.Priority = EOpenMobileHapticChannelPriority::High;
	ProjectChannel.MaximumActiveHandles = 1;
	ProjectChannel.MaximumQueueDepth = 1;
	ProjectChannel.MinimumIntervalSeconds = 0.0f;
	Settings->Channels.Add(ProjectChannel);
	Settings->MaximumActiveHandles = 1;
	UGameInstance* ReplacementGameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Replacement =
		NewObject<UOpenMobileHapticsSubsystem>(ReplacementGameInstance);
	FOpenMobileHapticNamedPatternRequest ProjectRequest;
	ProjectRequest.PatternName = TEXT("VehiclePulse");
	ProjectRequest.Options.Channel = TEXT("VehicleCabin");
	ProjectRequest.Options.Priority = EOpenMobileHapticChannelPriority::Low;
	const FOpenMobileHapticPlaybackResult ProjectResult =
		Replacement->SubmitNamedPattern(ProjectRequest);
	TestTrue(TEXT("Teardown leaves a clean admission state"),
		ProjectResult.IsAccepted());
	TestEqual(TEXT("Project channel priority reaches native submission"),
		Backend.LastNamedRequest.Options.Priority,
		EOpenMobileHapticChannelPriority::High);
	TestEqual(TEXT("Project playback can release its only active slot"),
		Replacement->StopPlayback(ProjectResult.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	Backend.bFailNamedSubmissions = true;
	const FOpenMobileHapticPlaybackResult FailedSubmission =
		Replacement->SubmitNamedPattern(ProjectRequest);
	TestFalse(TEXT("Rejected backend submission is not accepted"),
		FailedSubmission.IsAccepted());
	Backend.bFailNamedSubmissions = false;
	TestTrue(TEXT("Rejected native submission releases channel capacity"),
		Replacement->SubmitNamedPattern(ProjectRequest).IsAccepted());

	Replacement->Deinitialize();
	UGameInstance* ReentrantGameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Reentrant =
		NewObject<UOpenMobileHapticsSubsystem>(ReentrantGameInstance);
	FOpenMobileHapticNamedPatternRequest ReentrantRepeatRequest = ProjectRequest;
	ReentrantRepeatRequest.PatternName = TEXT("ReentrantRepeat");
	ReentrantRepeatRequest.Options.Loop.bLoop = true;
	const FOpenMobileHapticPlaybackResult ReentrantRepeat =
		Reentrant->SubmitNamedPattern(ReentrantRepeatRequest);
	TestTrue(TEXT("Reentrant teardown setup owns repeated work"),
		ReentrantRepeat.IsAccepted());
	bool bToreDownDuringPreemption = false;
	Reentrant->OnPlaybackEventNative().AddLambda(
		[Reentrant, &ReentrantRepeat, &bToreDownDuringPreemption](
			const FOpenMobileHapticPlaybackEvent& Event
		)
		{
			if (Event.Handle == ReentrantRepeat.Handle
				&& Event.State
					== EOpenMobileHapticPlaybackState::Interrupted)
			{
				bToreDownDuringPreemption = true;
				Reentrant->Deinitialize();
			}
		}
	);
	const int32 OneShotsBeforeReentrantTeardown =
		Backend.OneShotSubmissionCount;
	const FOpenMobileHapticPlaybackResult ReentrantRejection =
		Reentrant->SubmitOneShot(CriticalRequest);
	TestTrue(TEXT("Preemption tolerates synchronous subsystem teardown"),
		bToreDownDuringPreemption);
	TestEqual(TEXT("Teardown rejects the pending incoming request"),
		ReentrantRejection.Error.Code,
		EOpenMobileHapticErrorCode::BackendUnavailable);
	TestEqual(TEXT("Teardown prevents late native submission"),
		Backend.OneShotSubmissionCount,
		OneShotsBeforeReentrantTeardown);

	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	Settings->Channels = SavedChannels;
	Settings->MaximumActiveHandles = SavedMaximumActiveHandles;
	Settings->MaximumQueuedHandles = SavedMaximumQueuedHandles;
	Settings->MaximumQueueDepthPerChannel =
		SavedMaximumQueueDepthPerChannel;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsOverlapSubsystemTest,
	"OpenMobile.Haptics.Overlap.SubsystemPolicies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsOverlapSubsystemTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	const TArray<FOpenMobileHapticChannelSettings> SavedChannels =
		Settings->Channels;
	const int32 SavedMaximumActiveHandles = Settings->MaximumActiveHandles;
	const int32 SavedMaximumQueuedHandles = Settings->MaximumQueuedHandles;
	const int32 SavedMaximumQueueDepthPerChannel =
		Settings->MaximumQueueDepthPerChannel;
	const float SavedMaximumQueuedRequestAgeSeconds =
		Settings->MaximumQueuedRequestAgeSeconds;
	Settings->NamedLibraries.Reset();
	Settings->MaximumActiveHandles = 16;
	Settings->MaximumQueuedHandles = 8;
	Settings->MaximumQueueDepthPerChannel = 4;
	Settings->MaximumQueuedRequestAgeSeconds = 1.0f;
	for (FOpenMobileHapticChannelSettings& Channel : Settings->Channels)
	{
		Channel.Priority = EOpenMobileHapticChannelPriority::Low;
		Channel.MaximumActiveHandles = 8;
		Channel.MaximumQueueDepth = 4;
		Channel.MinimumIntervalSeconds = 0.0f;
		Channel.UnsupportedMixFallbackPolicy =
			EOpenMobileHapticOverlapPolicy::Replace;
	}

	FMockBackend Backend(TEXT("Overlap"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::BasicVibration;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Mixing =
		EOpenMobileHapticSupportState::Supported;
	Backend.ControlSupport.bStop = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	auto NewSubsystem = []()
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		return NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	};
	auto MakeRequest = [](
		FName Channel,
		EOpenMobileHapticOverlapPolicy Policy,
		EOpenMobileHapticChannelPriority Priority
	)
	{
		FOpenMobileHapticOneShotRequest Request;
		Request.DurationSeconds = 0.03f;
		Request.Options.Channel = Channel;
		Request.Options.OverlapPolicy = Policy;
		Request.Options.Priority = Priority;
		return Request;
	};

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const FOpenMobileHapticPlaybackResult Gameplay = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
				EOpenMobileHapticChannelPriority::Normal)
		);
		const FOpenMobileHapticPlaybackResult Independent = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("UI"), EOpenMobileHapticOverlapPolicy::Ignore,
				EOpenMobileHapticChannelPriority::Low)
		);
		TestTrue(TEXT("Ignore does not collide across channels"),
			Gameplay.IsAccepted() && Independent.IsAccepted());
		Subsystem->Deinitialize();
	}

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const int32 SubmissionsBefore = Backend.OneShotSubmissionCount;
		const FOpenMobileHapticPlaybackResult Existing = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
				EOpenMobileHapticChannelPriority::Normal)
		);
		const FOpenMobileHapticPlaybackResult Ignored = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Ignore,
				EOpenMobileHapticChannelPriority::Critical)
		);
		TestEqual(TEXT("Ignore suppresses the simultaneous request"),
			Ignored.Outcome, EOpenMobileHapticPlaybackOutcome::Suppressed);
		TestFalse(TEXT("Ignored work has no playback handle"),
			Ignored.Handle.IsValid());
		TestEqual(TEXT("Ignored work never reaches native submission"),
			Backend.OneShotSubmissionCount, SubmissionsBefore + 1);
		TestTrue(TEXT("Ignore preserves existing plugin-owned work"),
			Subsystem->GetPlaybackState(Existing.Handle)
				!= EOpenMobileHapticPlaybackState::Interrupted);
		Subsystem->Deinitialize();
	}

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const int32 StopsBefore = Backend.StopPlaybackCount;
		const FOpenMobileHapticPlaybackResult Existing = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
				EOpenMobileHapticChannelPriority::Normal)
		);
		const FOpenMobileHapticPlaybackResult Replacement =
			Subsystem->SubmitOneShot(MakeRequest(
				TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
				EOpenMobileHapticChannelPriority::Low));
		TestTrue(TEXT("Replace accepts the incoming request"),
			Replacement.IsAccepted());
		TestEqual(TEXT("Replace interrupts existing work before submission"),
			Subsystem->GetPlaybackState(Existing.Handle),
			EOpenMobileHapticPlaybackState::Interrupted);
		TestEqual(TEXT("Replace stops exactly one owned native request"),
			Backend.StopPlaybackCount, StopsBefore + 1);
		Subsystem->Deinitialize();
	}

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const int32 StopsBefore = Backend.StopPlaybackCount;
		const FOpenMobileHapticPlaybackResult Lower = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
				EOpenMobileHapticChannelPriority::Low)
		);
		const FOpenMobileHapticPlaybackResult Higher = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"),
				EOpenMobileHapticOverlapPolicy::InterruptLowerPriority,
				EOpenMobileHapticChannelPriority::High)
		);
		TestTrue(TEXT("Higher priority interrupts and submits"),
			Higher.IsAccepted());
		TestEqual(TEXT("Lower priority receives an interrupted outcome"),
			Subsystem->GetPlaybackState(Lower.Handle),
			EOpenMobileHapticPlaybackState::Interrupted);
		const FOpenMobileHapticPlaybackResult Equal = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"),
				EOpenMobileHapticOverlapPolicy::InterruptLowerPriority,
				EOpenMobileHapticChannelPriority::High)
		);
		TestEqual(TEXT("Equal priority suppresses atomically"), Equal.Outcome,
			EOpenMobileHapticPlaybackOutcome::Suppressed);
		TestEqual(TEXT("Blocked interruption does not stop the equal request"),
			Backend.StopPlaybackCount, StopsBefore + 1);
		Subsystem->Deinitialize();
	}

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const int32 StopsBefore = Backend.StopPlaybackCount;
		const FOpenMobileHapticPlaybackResult Existing = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
				EOpenMobileHapticChannelPriority::Normal)
		);
		const FOpenMobileHapticPlaybackResult Mixed = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"),
				EOpenMobileHapticOverlapPolicy::MixWhenSupported,
				EOpenMobileHapticChannelPriority::Normal)
		);
		TestTrue(TEXT("Supported plugin-owned mixing submits concurrently"),
			Mixed.IsAccepted());
		TestEqual(TEXT("Mixing leaves existing native work untouched"),
			Backend.StopPlaybackCount, StopsBefore);
		TestTrue(TEXT("Mixed work retains both active handles"),
			Subsystem->GetDiagnostics().ActivePlaybackCount == 2
				&& Existing.Handle.IsValid() && Mixed.Handle.IsValid());
		Subsystem->Deinitialize();
	}

	{
		Backend.Capabilities.Mixing =
			EOpenMobileHapticSupportState::Unsupported;
		FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
		FOpenMobileHapticChannelSettings* GameplayChannel =
			Settings->Channels.FindByPredicate(
				[](const FOpenMobileHapticChannelSettings& Channel)
				{
					return Channel.Name == TEXT("Gameplay");
				}
			);
		TestNotNull(TEXT("Gameplay overlap settings are available"),
			GameplayChannel);
		if (GameplayChannel)
		{
			GameplayChannel->UnsupportedMixFallbackPolicy =
				EOpenMobileHapticOverlapPolicy::Queue;
			GameplayChannel->MaximumQueueDepth = 2;
		}
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const int32 ActiveCallback = Backend.GetPendingCallbackCount();
		const int32 SubmissionsBefore = Backend.OneShotSubmissionCount;
		const FOpenMobileHapticPlaybackResult Active = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
				EOpenMobileHapticChannelPriority::Normal)
		);
		const FOpenMobileHapticPlaybackResult QueuedLow = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Queue,
				EOpenMobileHapticChannelPriority::Low)
		);
		const FOpenMobileHapticPlaybackResult QueuedHigh = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"),
				EOpenMobileHapticOverlapPolicy::MixWhenSupported,
				EOpenMobileHapticChannelPriority::High)
		);
		TestTrue(TEXT("Queue returns stable accepted handles"),
			QueuedLow.IsAccepted() && QueuedLow.Handle.IsValid()
				&& QueuedHigh.IsAccepted() && QueuedHigh.Handle.IsValid());
		TestEqual(TEXT("Unsupported mix reports the configured fallback"),
			QueuedHigh.Outcome, EOpenMobileHapticPlaybackOutcome::Fallback);
		TestEqual(TEXT("Queued requests do not reach native code early"),
			Backend.OneShotSubmissionCount, SubmissionsBefore + 1);
		TestEqual(TEXT("Diagnostics separate active and queued work"),
			Subsystem->GetDiagnostics().ActivePlaybackCount, 1);
		TestEqual(TEXT("Diagnostics report both overlap queue entries"),
			Subsystem->GetDiagnostics().QueuedPlaybackCount, 2);
		TestEqual(TEXT("Diagnostics retain peak overlap queue depth"),
			Subsystem->GetDiagnostics().Performance.PeakQueuedPlaybackCount, 2);
		const FOpenMobileHapticPlaybackResult Overflow = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Queue,
				EOpenMobileHapticChannelPriority::Critical)
		);
		TestEqual(TEXT("Overlap queue depth is bounded"), Overflow.Error.Code,
			EOpenMobileHapticErrorCode::ChannelBusy);
		TestEqual(TEXT("Queue overflow increments dropped requests"),
			Subsystem->GetDiagnostics().Performance.DroppedRequestCount,
			static_cast<int64>(1));

		Backend.Emit(ActiveCallback,
			EOpenMobileHapticPlaybackState::Completed, 1);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(
			ENamedThreads::GameThread);
		TestEqual(TEXT("Highest priority queued request starts first"),
			Backend.LastToken.PlaybackHandle, QueuedHigh.Handle);
		TestEqual(TEXT("One queued request remains after promotion"),
			Subsystem->GetDiagnostics().QueuedPlaybackCount, 1);
		const int32 HighCallback = Backend.GetPendingCallbackCount() - 1;
		Backend.Emit(HighCallback,
			EOpenMobileHapticPlaybackState::Completed, 1);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(
			ENamedThreads::GameThread);
		TestEqual(TEXT("Equal-channel queue eventually promotes older work"),
			Backend.LastToken.PlaybackHandle, QueuedLow.Handle);
		TestEqual(TEXT("Promoted queue is no longer counted as queued"),
			Subsystem->GetDiagnostics().QueuedPlaybackCount, 0);
		TestEqual(TEXT("The original active request completed cleanly"),
			Subsystem->GetPlaybackState(Active.Handle),
			EOpenMobileHapticPlaybackState::Completed);
		Subsystem->Deinitialize();
	}

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const int32 NativeStopsBefore = Backend.StopPlaybackCount;
		Subsystem->SubmitOneShot(MakeRequest(
			TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
			EOpenMobileHapticChannelPriority::Normal));
		const FOpenMobileHapticPlaybackResult Queued = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Queue,
				EOpenMobileHapticChannelPriority::Normal)
		);
		TestEqual(TEXT("Queued work can be cancelled locally"),
			Subsystem->CancelPlayback(Queued.Handle).Outcome,
			EOpenMobileHapticControlOutcome::Accepted);
		TestEqual(TEXT("Queue cancellation has a clear terminal state"),
			Subsystem->GetPlaybackState(Queued.Handle),
			EOpenMobileHapticPlaybackState::Cancelled);
		TestEqual(TEXT("Queue cancellation never stops unrelated native work"),
			Backend.StopPlaybackCount, NativeStopsBefore);
		TestEqual(TEXT("Cancelled queue capacity is released"),
			Subsystem->GetDiagnostics().QueuedPlaybackCount, 0);
		Subsystem->Deinitialize();
	}

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const FOpenMobileHapticPlaybackResult Active = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
				EOpenMobileHapticChannelPriority::Normal)
		);
		const FOpenMobileHapticPlaybackResult Queued = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Queue,
				EOpenMobileHapticChannelPriority::Normal)
		);
		const FOpenMobileHapticPlaybackResult Replacement =
			Subsystem->SubmitOneShot(MakeRequest(
				TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
				EOpenMobileHapticChannelPriority::Low));
		TestTrue(TEXT("Replace submits after clearing active and queued work"),
			Replacement.IsAccepted());
		TestEqual(TEXT("Replace interrupts active same-channel work"),
			Subsystem->GetPlaybackState(Active.Handle),
			EOpenMobileHapticPlaybackState::Interrupted);
		TestEqual(TEXT("Replace cancels deferred same-channel work"),
			Subsystem->GetPlaybackState(Queued.Handle),
			EOpenMobileHapticPlaybackState::Cancelled);
		TestEqual(TEXT("Replace leaves no deferred same-channel work"),
			Subsystem->GetDiagnostics().QueuedPlaybackCount, 0);
		Subsystem->Deinitialize();
	}

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		Subsystem->SubmitOneShot(MakeRequest(
			TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
			EOpenMobileHapticChannelPriority::Normal));
		const FOpenMobileHapticPlaybackResult Queued = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Queue,
				EOpenMobileHapticChannelPriority::Normal)
		);
		Subsystem->DrainOverlapQueues(
			std::numeric_limits<double>::max() / 2.0);
		TestEqual(TEXT("Stale overlap queues expire deterministically"),
			Subsystem->GetPlaybackState(Queued.Handle),
			EOpenMobileHapticPlaybackState::Cancelled);
		TestEqual(TEXT("Expired queues release their bounded reservation"),
			Subsystem->GetDiagnostics().QueuedPlaybackCount, 0);
		Subsystem->Deinitialize();
	}

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		Subsystem->SubmitOneShot(MakeRequest(
			TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
			EOpenMobileHapticChannelPriority::Normal));
		const FOpenMobileHapticPlaybackResult Queued = Subsystem->SubmitOneShot(
			MakeRequest(TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Queue,
				EOpenMobileHapticChannelPriority::Normal)
		);
		FOpenMobileHapticUserPolicy DisabledPolicy = Subsystem->GetUserPolicy();
		DisabledPolicy.bEnabled = false;
		TestEqual(TEXT("User policy changes are accepted while work is queued"),
			Subsystem->SetUserPolicy(DisabledPolicy).Outcome,
			EOpenMobileHapticControlOutcome::Accepted);
		TestEqual(TEXT("Disabling haptics cancels queued overlap work"),
			Subsystem->GetPlaybackState(Queued.Handle),
			EOpenMobileHapticPlaybackState::Cancelled);
		TestEqual(TEXT("Disabled policy leaves no queued work"),
			Subsystem->GetDiagnostics().QueuedPlaybackCount, 0);
		Subsystem->Deinitialize();
	}

	{
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const int32 ActiveCallback = Backend.GetPendingCallbackCount();
		Subsystem->SubmitOneShot(MakeRequest(
			TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
			EOpenMobileHapticChannelPriority::Normal));
		UOpenMobileHapticPatternAsset* QueuedAsset =
			NewObject<UOpenMobileHapticPatternAsset>(
				GetTransientPackage(),
				TEXT("QueuedOverlapAsset")
			);
		FOpenMobileHapticNamedPatternRequest NamedRequest;
		NamedRequest.PatternName = TEXT("QueuedAssetPattern");
		NamedRequest.PatternAsset = FSoftObjectPath(QueuedAsset);
		NamedRequest.Options.Channel = TEXT("Gameplay");
		NamedRequest.Options.OverlapPolicy =
			EOpenMobileHapticOverlapPolicy::Queue;
		const int32 NamedSubmissionsBefore = Backend.NamedSubmissionCount;
		const FOpenMobileHapticPlaybackResult Queued =
			Subsystem->SubmitNamedPattern(NamedRequest);
		TestTrue(TEXT("Loaded direct assets can enter the overlap queue"),
			Queued.IsAccepted());
		QueuedAsset->Rename(
			TEXT("RenamedQueuedOverlapAsset"),
			GetTransientPackage(),
			REN_DontCreateRedirectors | REN_NonTransactional
		);
		Backend.Emit(ActiveCallback,
			EOpenMobileHapticPlaybackState::Completed, 1);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(
			ENamedThreads::GameThread);
		TestEqual(TEXT("Unloaded queued assets fail before native submission"),
			Subsystem->GetPlaybackState(Queued.Handle),
			EOpenMobileHapticPlaybackState::Failed);
		TestEqual(TEXT("Invalidated queued assets never reach native code"),
			Backend.NamedSubmissionCount, NamedSubmissionsBefore);
		Subsystem->Deinitialize();
	}

	{
		Backend.Capabilities.SemanticEffects =
			EOpenMobileHapticSupportState::Supported;
		FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
		UOpenMobileHapticsSubsystem* Subsystem = NewSubsystem();
		const int32 ActiveCallback = Backend.GetPendingCallbackCount();
		Subsystem->SubmitOneShot(MakeRequest(
			TEXT("Gameplay"), EOpenMobileHapticOverlapPolicy::Replace,
			EOpenMobileHapticChannelPriority::Normal));
		FOpenMobileHapticSemanticRequest SemanticRequest;
		SemanticRequest.Effect = EOpenMobileHapticSemanticEffect::Click;
		SemanticRequest.Options.Channel = TEXT("Gameplay");
		SemanticRequest.Options.OverlapPolicy =
			EOpenMobileHapticOverlapPolicy::Queue;
		const int32 SemanticSubmissionsBefore =
			Backend.SemanticSubmissionCount;
		const FOpenMobileHapticPlaybackResult Queued =
			Subsystem->SubmitSemantic(SemanticRequest);
		TestTrue(TEXT("Fire-and-forget semantics receive a queued handle"),
			Queued.IsAccepted() && Queued.Handle.IsValid());
		Backend.Emit(ActiveCallback,
			EOpenMobileHapticPlaybackState::Completed, 1);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(
			ENamedThreads::GameThread);
		TestEqual(TEXT("Queued semantic feedback submits after promotion"),
			Backend.SemanticSubmissionCount, SemanticSubmissionsBefore + 1);
		TestEqual(TEXT("Promoted fire-and-forget work terminates clearly"),
			Subsystem->GetPlaybackState(Queued.Handle),
			EOpenMobileHapticPlaybackState::Completed);
		Subsystem->Deinitialize();
	}

	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	Settings->Channels = SavedChannels;
	Settings->MaximumActiveHandles = SavedMaximumActiveHandles;
	Settings->MaximumQueuedHandles = SavedMaximumQueuedHandles;
	Settings->MaximumQueueDepthPerChannel =
		SavedMaximumQueueDepthPerChannel;
	Settings->MaximumQueuedRequestAgeSeconds =
		SavedMaximumQueuedRequestAgeSeconds;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsApplicationLifecycleTest,
	"OpenMobile.Haptics.Lifecycle.ProcessTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsApplicationLifecycleTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const EOpenMobileHapticBackgroundPolicy SavedBackgroundPolicy =
		Settings->BackgroundPolicy;
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::CriticalOnly;
	Settings->NamedLibraries.Reset();

	FMockBackend Backend(TEXT("ApplicationLifecycle"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::BasicVibration;
	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.PredefinedEffects =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.Scheduling =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.BackgroundAlerts =
		EOpenMobileHapticSupportState::Supported;
	Backend.ControlSupport.bStop = true;
	Backend.ControlSupport.bStopAll = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* FirstGameInstance = NewObject<UGameInstance>();
	UGameInstance* SecondGameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* First =
		NewObject<UOpenMobileHapticsSubsystem>(FirstGameInstance);
	UOpenMobileHapticsSubsystem* Second =
		NewObject<UOpenMobileHapticsSubsystem>(SecondGameInstance);
	const FOpenMobileHapticPlaybackResult Active =
		First->PlayNamedPattern(TEXT("ActiveBeforeDeactivate"));
	FOpenMobileHapticNamedPatternRequest ScheduledRequest;
	ScheduledRequest.PatternName = TEXT("QueuedBeforeDeactivate");
	ScheduledRequest.Options.Schedule.Mode =
		EOpenMobileHapticScheduleMode::Relative;
	ScheduledRequest.Options.Schedule.TimeSeconds = 10.0;
	const FOpenMobileHapticPlaybackResult Scheduled =
		Second->SubmitNamedPattern(ScheduledRequest);
	TestTrue(TEXT("First PIE instance owns active work"), Active.IsAccepted());
	TestTrue(TEXT("Second PIE instance owns queued work"),
		Scheduled.IsAccepted());

	bool bPublicStatePrecedesNativeCleanup = false;
	Backend.OnHandleApplicationLifecycle = [&]()
	{
		bPublicStatePrecedesNativeCleanup =
			First->GetPlaybackState(Active.Handle)
				== EOpenMobileHapticPlaybackState::Interrupted
			&& Second->GetPlaybackState(Scheduled.Handle)
				== EOpenMobileHapticPlaybackState::Interrupted;
	};
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::WillDeactivate
	);
	TestTrue(TEXT("Every PIE instance terminates before native cleanup"),
		bPublicStatePrecedesNativeCleanup);
	TestEqual(TEXT("The process invokes one native deactivate transition"),
		Backend.LifecycleTransitionCount, 1);
	TestFalse(TEXT("Queued playback cannot start after deactivation"),
		Backend.LastNamedPlaybackParameters.ScheduledStartGuard->CanStart(
			FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()
		));
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::WillDeactivate
	);
	TestEqual(TEXT("Duplicate deactivate is ignored"),
		Backend.LifecycleTransitionCount, 1);

	const int32 SemanticBeforeInactive = Backend.SemanticSubmissionCount;
	const FOpenMobileHapticPlaybackResult InactiveGameplay =
		First->PlaySemanticFeedback(EOpenMobileHapticSemanticEffect::Damage);
	TestEqual(TEXT("Inactive gameplay is suppressed"),
		InactiveGameplay.Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	TestEqual(TEXT("Inactive gameplay never reaches native code"),
		Backend.SemanticSubmissionCount, SemanticBeforeInactive);

	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::WillEnterBackground
	);
	FOpenMobileHapticPlaybackOptions AlertOptions;
	AlertOptions.Channel = TEXT("CriticalAlerts");
	AlertOptions.Category = TEXT("Alerts");
	AlertOptions.Priority = EOpenMobileHapticChannelPriority::Critical;
	const FOpenMobileHapticPlaybackResult BackgroundAlert =
		First->SubmitSemantic({
			EOpenMobileHapticSemanticEffect::NotificationWarning,
			1.0f,
			AlertOptions
		});
	TestTrue(TEXT("Supported critical alert intent can run in background"),
		BackgroundAlert.IsAccepted());
	TestNotEqual(TEXT("Background alert avoids view-only semantic feedback"),
		Backend.LastSemanticResolution.Path,
		EOpenMobileHapticsSemanticPath::SystemSemantic);
	const FOpenMobileHapticPlaybackResult BackgroundGameplay =
		First->PlayNamedPattern(TEXT("BackgroundGameplay"));
	TestEqual(TEXT("Named gameplay is suppressed in background"),
		BackgroundGameplay.Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);
	UOpenMobileHapticPatternAsset* AlertPattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	AlertPattern->bSuitableForBackgroundPlayback = true;
	FOpenMobileHapticNamedPatternRequest BackgroundPatternRequest;
	BackgroundPatternRequest.PatternName = TEXT("BackgroundAlertPattern");
	BackgroundPatternRequest.PatternAsset = FSoftObjectPath(AlertPattern);
	BackgroundPatternRequest.Options = AlertOptions;
	BackgroundPatternRequest.Options.Channel = TEXT("CriticalPatterns");
	const FOpenMobileHapticPlaybackResult BackgroundPattern =
		First->SubmitNamedPattern(BackgroundPatternRequest);
	TestTrue(TEXT("Marked critical alert assets can run in background"),
		BackgroundPattern.IsAccepted());

	FOpenMobileHapticOneShotRequest AlertOneShot;
	AlertOneShot.DurationSeconds = 0.1f;
	AlertOneShot.Options = AlertOptions;
	const FOpenMobileHapticPlaybackResult BackgroundOneShot =
		Second->SubmitOneShot(AlertOneShot);
	TestTrue(TEXT("Critical alert one-shot is accepted in background"),
		BackgroundOneShot.IsAccepted());
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::HasEnteredForeground
	);
	TestEqual(TEXT("Foreground entry stops remaining background work"),
		Second->GetPlaybackState(BackgroundOneShot.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	TestEqual(TEXT("Foreground entry stops background alert patterns"),
		First->GetPlaybackState(BackgroundPattern.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	const int32 TransitionsAtForeground = Backend.LifecycleTransitionCount;
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::HasEnteredForeground
	);
	TestEqual(TEXT("Duplicate foreground entry is ignored"),
		Backend.LifecycleTransitionCount, TransitionsAtForeground);
	const int32 OneShotsBeforeReactivation = Backend.OneShotSubmissionCount;
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::HasReactivated
	);
	TestTrue(TEXT("Reactivation refreshes native services once"),
		Backend.LastLifecycleTransition.bRefreshesNativeServices);
	TestEqual(TEXT("Reactivation does not replay consumed one-shots"),
		Backend.OneShotSubmissionCount, OneShotsBeforeReactivation);
	TestEqual(TEXT("Reactivation restores active process state"),
		FOpenMobileHapticsBackendRegistry::GetApplicationState(),
		EOpenMobileHapticsApplicationState::Active);
	const int32 TransitionsAtReactivation = Backend.LifecycleTransitionCount;
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::HasReactivated
	);
	TestEqual(TEXT("Duplicate reactivation is ignored"),
		Backend.LifecycleTransitionCount, TransitionsAtReactivation);

	const FOpenMobileHapticPlaybackResult BeforeTermination =
		First->PlayNamedPattern(
			TEXT("BeforeTermination"),
			1.0f,
			TEXT("ForegroundTermination")
		);
	TestTrue(TEXT("Foreground playback resumes normally"),
		BeforeTermination.IsAccepted());
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::WillTerminate
	);
	TestEqual(TEXT("Termination interrupts accepted work"),
		First->GetPlaybackState(BeforeTermination.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	const int32 TransitionsAtTermination = Backend.LifecycleTransitionCount;
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::HasReactivated
	);
	TestEqual(TEXT("Termination rejects late lifecycle changes"),
		Backend.LifecycleTransitionCount, TransitionsAtTermination);
	TestEqual(TEXT("Termination rejects new playback"),
		First->PlaySelectionFeedback().Outcome,
		EOpenMobileHapticPlaybackOutcome::Suppressed);

	First->Deinitialize();
	Second->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->BackgroundPolicy = SavedBackgroundPolicy;
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsLifecyclePreparedAssetsTest,
	"OpenMobile.Haptics.Lifecycle.LazyPreparedAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsLifecyclePreparedAssetsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;

	FMockBackend Backend(TEXT("LifecyclePreparedAssets"));
	Backend.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Backend.Capabilities.RichHaptics =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.WaveformTiming =
		EOpenMobileHapticSupportState::Supported;
	Backend.Capabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Supported;
	Backend.ControlSupport.bStop = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UOpenMobileHapticPatternAsset* Pattern =
		NewObject<UOpenMobileHapticPatternAsset>();
	Pattern->SourcePattern.Events.AddDefaulted();
	TArray<FString> PatternErrors;
	TestTrue(TEXT("Lifecycle pattern builds"),
		Pattern->RebuildDerivedData(PatternErrors));
	UOpenMobileHapticLibrary* Library = NewObject<UOpenMobileHapticLibrary>();
	Library->Patterns = {{TEXT("LifecyclePrepared"), Pattern}};
	FOpenMobileHapticNamedLibrarySettings LibrarySettings;
	LibrarySettings.Name = TEXT("Lifecycle");
	LibrarySettings.Asset = FSoftObjectPath(Library);
	Settings->NamedLibraries = {LibrarySettings};

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	TArray<FString> Errors;
	TestTrue(TEXT("Lifecycle library prepares"),
		Subsystem->PrepareLoadedNamedLibraries({Library}, Errors));
	TestEqual(TEXT("Initial lifecycle preparation compiles once"),
		Backend.PrepareResourcesCount, 1);
	const FOpenMobileHapticPlaybackResult Original =
		Subsystem->PlayNamedPattern(TEXT("LifecyclePrepared"));
	TestTrue(TEXT("Prepared lifecycle playback is accepted"),
		Original.IsAccepted());

	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::WillDeactivate
	);
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::WillEnterBackground
	);
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::HasEnteredForeground
	);
	FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
		EOpenMobileHapticsLifecycleEvent::HasReactivated
	);
	TestEqual(TEXT("Lifecycle transition interrupts the old handle"),
		Subsystem->GetPlaybackState(Original.Handle),
		EOpenMobileHapticPlaybackState::Interrupted);
	TestTrue(TEXT("Background transition releases native prepared state"),
		Backend.ReleasePreparedResourcesCount >= 1);
	TestEqual(TEXT("Foreground refresh does not eagerly compile assets"),
		Backend.PrepareResourcesCount, 1);
	TestEqual(TEXT("Foreground refresh does not replay named requests"),
		Backend.NamedSubmissionCount, 1);
	TestEqual(TEXT("Resolved library remains loaded across background"),
		Subsystem->GetNamedPatternStatus(TEXT("LifecyclePrepared")),
		EOpenMobileHapticNamedPatternStatus::Loaded);

	const FOpenMobileHapticPlaybackResult AfterForeground =
		Subsystem->PlayNamedPattern(
			TEXT("LifecyclePrepared"),
			1.0f,
			TEXT("LifecyclePreparedForeground")
		);
	TestTrue(TEXT("Next foreground request is accepted"),
		AfterForeground.IsAccepted());
	TestEqual(TEXT("Next foreground request restores native preparation"),
		Backend.PrepareResourcesCount, 2);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsGameInstanceLifecycleTest,
	"OpenMobile.Haptics.Lifecycle.GameInstanceIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsGameInstanceLifecycleTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTests;
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UOpenMobileHapticsSettings* Settings =
		GetMutableDefault<UOpenMobileHapticsSettings>();
	const TArray<FOpenMobileHapticNamedLibrarySettings> SavedLibraries =
		Settings->NamedLibraries;
	Settings->NamedLibraries.Reset();
	FMockBackend Backend(TEXT("GameInstanceIsolation"));
	Backend.ControlSupport.bStop = true;
	Backend.ControlSupport.bStopAll = true;
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Backend);

	UGameInstance* FirstGameInstance = NewObject<UGameInstance>();
	UGameInstance* SecondGameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* First =
		NewObject<UOpenMobileHapticsSubsystem>(FirstGameInstance);
	UOpenMobileHapticsSubsystem* Second =
		NewObject<UOpenMobileHapticsSubsystem>(SecondGameInstance);
	const FOpenMobileHapticPlaybackResult FirstPlayback =
		First->PlayNamedPattern(TEXT("FirstPIE"));
	const FOpenMobileHapticPlaybackResult SecondPlayback =
		Second->PlayNamedPattern(TEXT("SecondPIE"));
	TestTrue(TEXT("First PIE request is accepted"),
		FirstPlayback.IsAccepted());
	TestTrue(TEXT("Second PIE request is accepted"),
		SecondPlayback.IsAccepted());

	First->Deinitialize();
	TestEqual(TEXT("Game Instance teardown stops only owned handles"),
		Backend.StopPlaybackCount, 1);
	TestEqual(TEXT("Teardown does not call process-wide stop-all"),
		Backend.StopAllCount, 0);
	TestEqual(TEXT("The stopped token belongs to the torn-down instance"),
		Backend.LastStoppedToken.PlaybackHandle,
		FirstPlayback.Handle);
	TestEqual(TEXT("The other PIE state remains accepted"),
		Second->GetPlaybackState(SecondPlayback.Handle),
		EOpenMobileHapticPlaybackState::Accepted);

	if (SecondPlayback.IsAccepted())
	{
		Backend.Emit(1, EOpenMobileHapticPlaybackState::Started, 1,
			EOpenMobileHapticEventEvidence::NativeConfirmed);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(
			ENamedThreads::GameThread
		);
	}
	TestEqual(TEXT("The surviving PIE callback remains current"),
		Second->GetPlaybackState(SecondPlayback.Handle),
		EOpenMobileHapticPlaybackState::Started);
	Second->Deinitialize();
	TestEqual(TEXT("Second teardown stops its own handle"),
		Backend.StopPlaybackCount, 2);

	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	Settings->NamedLibraries = SavedLibraries;
	return true;
}

#endif

#endif
