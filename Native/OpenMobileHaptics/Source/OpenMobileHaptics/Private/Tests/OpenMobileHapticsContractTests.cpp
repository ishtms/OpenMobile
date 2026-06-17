#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "IOpenMobileHapticsBackend.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/CoreRedirects.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsDurationPolicy.h"
#include "OpenMobileHapticsEnvelopePolicy.h"
#include "OpenMobileHapticsErrorMapper.h"
#include "OpenMobileHapticsFallbackPolicy.h"
#include "OpenMobileHapticsIntensityPolicy.h"
#include "OpenMobileHapticLibrary.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsLibraryResolver.h"
#include "OpenMobileHapticsOneShotPolicy.h"
#include "OpenMobileHapticsPlatformOverridePolicy.h"
#include "OpenMobileHapticsPrimitiveCompositionPolicy.h"
#include "OpenMobileHapticsPatternCompiler.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsAndroidFallbackPolicy.h"
#include "OpenMobileHapticsAndroidWaveformPolicy.h"
#include "OpenMobileHapticsRateLimiter.h"
#include "OpenMobileHapticsRepeatPolicy.h"
#include "OpenMobileHapticsSemanticPolicy.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsSubsystem.h"
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
		virtual void HandleLifecycleChange() override
		{
			++LifecycleChangeCount;
			if (bApplyCapabilitiesAfterLifecycle)
			{
				Capabilities = CapabilitiesAfterLifecycle;
			}
		}
		virtual EOpenMobileHapticsBackendPreparationState
		GetPreparationState() const override
		{
			return PreparationState;
		}
		virtual FOpenMobileHapticsBackendControlSupport
		GetControlSupport() const override
		{
			return ControlSupport;
		}

		virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
			const FOpenMobileHapticSemanticRequest& Request,
			const FOpenMobileHapticsSemanticResolution& Resolution,
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			LastSemanticRequest = Request;
			LastSemanticResolution = Resolution;
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
			return MakeSubmission(false, false, MoveTemp(Callback));
		}

		virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
			const FOpenMobileHapticOneShotRequest& Request,
			const FOpenMobileHapticsOneShotResolution& Resolution,
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			LastOneShotRequest = Request;
			LastOneShotResolution = Resolution;
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
			const FOpenMobileHapticsBackendRequestToken& Token,
			FOpenMobileHapticsBackendEventCallback Callback
		) override
		{
			LastNamedRequest = Request;
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
			LastStoppedToken = Token;
			FOpenMobileHapticControlResult Result;
			Result.Outcome = ControlSupport.bStop
				? EOpenMobileHapticControlOutcome::Accepted
				: EOpenMobileHapticControlOutcome::Unsupported;
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
			uint64 Sequence
		)
		{
			FOpenMobileHapticsBackendCallback Callback;
			Callback.Token = PendingCallbacks[PendingIndex].Token;
			Callback.Sequence = Sequence;
			Callback.Event.Handle = Callback.Token.PlaybackHandle;
			Callback.Event.State = State;
			Callback.Event.TimestampSeconds = CurrentTimeSeconds;
			PendingCallbacks[PendingIndex].Callback(Callback);
		}

		FOpenMobileHapticCapabilities Capabilities;
		FOpenMobileHapticCapabilities CapabilitiesAfterLifecycle;
		EOpenMobileHapticsBackendPreparationState PreparationState =
			EOpenMobileHapticsBackendPreparationState::Unprepared;
		FOpenMobileHapticsBackendControlSupport ControlSupport;
		bool bAvailable = true;
		bool bFailSubmissions = false;
		bool bFailSubmissionsWithoutError = false;
		bool bNativePolicySuppressesSemantic = false;
		bool bFailNamedSubmissions = false;
		bool bBusyOneShot = false;
		bool bOneShotControllable = true;
		bool bApplyCapabilitiesAfterLifecycle = false;
		double CurrentTimeSeconds = 0.0;
		int32 SemanticSubmissionCount = 0;
		int32 OneShotSubmissionCount = 0;
		int32 NamedSubmissionCount = 0;
		int32 ShutdownCount = 0;
		int32 LifecycleChangeCount = 0;
		int32 StopChannelCount = 0;
		int32 StopAllCount = 0;
		FOpenMobileHapticsBackendRequestToken LastToken;
		FOpenMobileHapticsBackendRequestToken LastStoppedToken;
		FName LastStoppedChannel;
		FOpenMobileHapticSemanticRequest LastSemanticRequest;
		FOpenMobileHapticOneShotRequest LastOneShotRequest;
		FOpenMobileHapticNamedPatternRequest LastNamedRequest;
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
			if (bExpectsCallbacks)
			{
				PendingCallbacks.Add({LastToken, MoveTemp(Callback)});
			}
			return Submission;
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
	Limits.MaximumDurationSeconds = 1.0;
	Limits.MaximumEventDurationSeconds = 0.5;
	Limits.MinimumGranularitySeconds = 0.01;
	FOpenMobileHapticCapabilities NativeLimits;
	NativeLimits.MaximumEventCount = {true, 2};
	NativeLimits.MaximumDurationSeconds = {true, 0.5};
	NativeLimits.MinimumTimingGranularitySeconds = {true, 0.02};
	const FOpenMobileHapticsPatternCompileLimits ResolvedLimits =
		FOpenMobileHapticsPatternCompiler::MakeLimits(
			*GetDefault<UOpenMobileHapticsSettings>(),
			NativeLimits
		);
	TestEqual(TEXT("Native event limit narrows project settings"),
		ResolvedLimits.MaximumEventCount, 2);
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
	FMockBackend Backend(TEXT("NamedLibrary"));
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
	Library->Patterns = {{TEXT("Weapon_Recoil"), Pattern}};
	FOpenMobileHapticNamedLibrarySettings LibrarySettings;
	LibrarySettings.Name = TEXT("Gameplay");
	LibrarySettings.Asset = FSoftObjectPath(Library);
	Settings->NamedLibraries = {LibrarySettings};

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);
	TestEqual(TEXT("Configured library starts unprepared"),
		Subsystem->GetNamedPatternStatus(TEXT("Weapon_Recoil")),
		EOpenMobileHapticNamedPatternStatus::Unprepared);
	const FOpenMobileHapticPlaybackResult Unprepared =
		Subsystem->PlayNamedPattern(TEXT("Weapon_Recoil"));
	TestEqual(TEXT("Unprepared configured request is rejected"),
		Unprepared.Error.Code, EOpenMobileHapticErrorCode::NotConfigured);
	TestEqual(TEXT("Unprepared request never reaches the backend"),
		Backend.NamedSubmissionCount, 0);

	TArray<FString> Errors;
	TestTrue(TEXT("Loaded libraries can complete preparation"),
		Subsystem->PrepareLoadedNamedLibraries({Library}, Errors));
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

	Subsystem->ReleaseNamedLibraries();
	TestEqual(TEXT("Release unloads the prepared registry"),
		Subsystem->GetNamedPatternStatus(TEXT("Weapon_Recoil")),
		EOpenMobileHapticNamedPatternStatus::Unprepared);
	const FOpenMobileHapticLibraryPreloadHandle LoadHandle =
		Subsystem->PreloadNamedLibraries();
	TestTrue(TEXT("Async preload returns a stable handle"),
		LoadHandle.IsValid());
	TestEqual(TEXT("Async preload enters loading state"),
		Subsystem->GetNamedPatternStatus(TEXT("Weapon_Recoil")),
		EOpenMobileHapticNamedPatternStatus::Loading);
	TestEqual(TEXT("Active async preload can be cancelled"),
		Subsystem->CancelNamedLibraryPreload(LoadHandle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Cancelled preload cannot publish loaded state"),
		Subsystem->GetNamedPatternStatus(TEXT("Weapon_Recoil")),
		EOpenMobileHapticNamedPatternStatus::Unprepared);

	Settings->NamedLibraries = SavedLibraries;
	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
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
	IOS->AHAPJson = TEXT(
		"{\"Version\":1.0,\"Pattern\":[{\"Event\":"
		"{\"EventType\":\"HapticTransient\",\"Time\":0}}]}"
	);
	TestTrue(TEXT("Valid AHAP asset validates"), IOS->Validate(Errors));
	TestTrue(TEXT("iOS override cooks only for iOS"),
		IOS->ShouldCookForPlatform(TEXT("IOS")));
	TestFalse(TEXT("iOS override is filtered from Android cooks"),
		IOS->ShouldCookForPlatform(TEXT("Android")));
	IOS->AHAPJson = TEXT("{invalid");
	TestFalse(TEXT("Malformed AHAP JSON is invalid"), IOS->Validate(Errors));
	IOS->AHAPJson = TEXT(
		"{\"Version\":1.0,\"Pattern\":[{\"Event\":"
		"{\"EventType\":\"HapticTransient\",\"Time\":0}}]}"
	);

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

	Backend.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Unsupported;
	FOpenMobileHapticsBackendRegistry::RefreshCapabilities();
	const FOpenMobileHapticPlaybackResult Unsupported =
		Subsystem->Vibrate(0.2f, 1.0f, TEXT("UnsupportedPulse"));
	TestEqual(TEXT("Unsupported pulse has a typed error"),
		Unsupported.Error.Code,
		EOpenMobileHapticErrorCode::UnsupportedFeature);

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
			0.0, 0.0, 2));
	TestFalse(TEXT("Second burst event is allowed"),
		BurstLimiter.ShouldSuppress(TEXT("B"), false, 30.1,
			0.0, 0.0, 2));
	TestTrue(TEXT("Submission cap suppresses the next event"),
		BurstLimiter.ShouldSuppress(TEXT("C"), false, 30.2,
			0.0, 0.0, 2));
	TestFalse(TEXT("Old burst events expire at one second"),
		BurstLimiter.ShouldSuppress(TEXT("D"), false, 31.0,
			0.0, 0.0, 2));

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

	const FOpenMobileHapticUserPolicy Policy;
	TestTrue(TEXT("Haptics are enabled by default"), Policy.bEnabled);
	TestEqual(TEXT("Master intensity defaults to one"), Policy.MasterIntensity, 1.0f);
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
	TestTrue(TEXT("Haptics are enabled by default"), Settings->bEnabledByDefault);
	TestEqual(
		TEXT("Default project intensity is one"),
		Settings->DefaultMasterIntensity,
		1.0f
	);
	TestEqual(TEXT("Five default channels are present"), Settings->Channels.Num(), 5);
	TestEqual(
		TEXT("Default channel is Gameplay"),
		Settings->DefaultChannel,
		FName(TEXT("Gameplay"))
	);
	TestTrue(
		TEXT("Custom Android vibration packaging is enabled by default"),
		Settings->Android.bPackageCustomVibration
	);
	TestEqual(TEXT("Portable patterns have a bounded event count"),
		Settings->MaximumPatternEventCount, 128);
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

	Settings->BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::AllowAll;
	TestFalse(TEXT("Unrestricted background haptics are invalid"), Settings->Validate(Errors));
	Settings->BackgroundPolicy = EOpenMobileHapticBackgroundPolicy::StopAll;

	Settings->bEnableCustomPlayback = false;
	TestFalse(TEXT("Disabled custom playback cannot package Android vibration"), Settings->Validate(Errors));
	Settings->Android.bPackageCustomVibration = false;
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
	Settings->DefaultMasterIntensity = 0.75f;
	Settings->DefaultChannel = TEXT("UI");
	Settings->MaximumQueuedHandles = 12;
	Settings->SelectionDebounceSeconds = 0.06f;
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
	TestEqual(
		TEXT("Rate limit survives editor restart serialization"),
		Loaded->SelectionDebounceSeconds,
		0.06f
	);
	TestFalse(
		TEXT("Android packaging override survives serialization"),
		Loaded->Android.bPackageCustomVibration
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
		EOpenMobileHapticsBackendPreparationState::Preparing;
	FMockBackend High(TEXT("High"), 10);
	High.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	High.Capabilities.SemanticEffects =
		EOpenMobileHapticSupportState::Supported;
	High.PreparationState =
		EOpenMobileHapticsBackendPreparationState::Prepared;
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
		EOpenMobileHapticsBackendPreparationState::Prepared
	);

	int32 EventCount = 0;
	EOpenMobileHapticPlaybackState LastState =
		EOpenMobileHapticPlaybackState::Invalid;
	double LastTimestamp = 0.0;
	Subsystem->OnPlaybackEventNative().AddLambda(
		[&EventCount, &LastState, &LastTimestamp](
			const FOpenMobileHapticPlaybackEvent& Event
		)
		{
			++EventCount;
			LastState = Event.State;
			LastTimestamp = Event.TimestampSeconds;
		}
	);
	const FOpenMobileHapticPlaybackResult Named =
		Subsystem->PlayNamedPattern(TEXT("UI_Confirm"));
	TestTrue(TEXT("Named request is accepted"), Named.IsAccepted());
	TestTrue(TEXT("Controllable request has a handle"), Named.Handle.IsValid());
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
	TestEqual(TEXT("Stop broadcasts one terminal event"), EventCount, 1);
	TestEqual(TEXT("Stop event is distinct from cancellation"), LastState,
		EOpenMobileHapticPlaybackState::Stopped);
	TestEqual(TEXT("Repeated stop is idempotent"),
		Subsystem->StopPlayback(Named.Handle).Outcome,
		EOpenMobileHapticControlOutcome::Accepted);
	TestEqual(TEXT("Repeated stop does not duplicate events"), EventCount, 1);

	High.CurrentTimeSeconds = 12.5;
	High.Emit(0, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Callback queued before stop is invalidated"), EventCount, 1);
	TestEqual(
		TEXT("Stopped state cannot regress"),
		LastState,
		EOpenMobileHapticPlaybackState::Stopped
	);
	High.Emit(0, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Duplicate callback is ignored"), EventCount, 1);

	High.Emit(0, EOpenMobileHapticPlaybackState::Completed, 2);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Post-stop terminal callback is ignored"), EventCount, 1);
	TestEqual(
		TEXT("Stopped state remains queryable"),
		Subsystem->GetPlaybackState(Named.Handle),
		EOpenMobileHapticPlaybackState::Stopped
	);
	High.Emit(0, EOpenMobileHapticPlaybackState::Completed, 3);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Post-terminal callback is ignored"), EventCount, 1);

	const FOpenMobileHapticPlaybackResult Stale =
		Subsystem->PlayNamedPattern(TEXT("Stale"));
	FMockBackend Newest(TEXT("Newest"), 20);
	FOpenMobileHapticsBackendRegistry::RegisterBackend(Newest);
	High.Emit(1, EOpenMobileHapticPlaybackState::Started, 1);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Stale backend callback is ignored"), EventCount, 1);
	TestTrue(TEXT("Stale request originally had a handle"), Stale.Handle.IsValid());

	Newest.bFailSubmissions = true;
	const FOpenMobileHapticPlaybackResult Failed =
		Subsystem->PlayNamedPattern(TEXT("Failure"));
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
	Settings->NamedLibraries.Reset();
	FMockBackend Backend(TEXT("Control"));
	Backend.ControlSupport.bStop = true;
	Backend.ControlSupport.bStopChannel = true;
	Backend.ControlSupport.bStopAll = true;
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

	Subsystem->Deinitialize();
	TestEqual(TEXT("Shutdown stops native playback once more"),
		Backend.StopAllCount, 2);
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
		Subsystem->PlayNamedPattern(TEXT("MissingSubmissionError"));
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
		Subsystem->PlayNamedPattern(TEXT("MissingCallbackError"));
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

#endif
