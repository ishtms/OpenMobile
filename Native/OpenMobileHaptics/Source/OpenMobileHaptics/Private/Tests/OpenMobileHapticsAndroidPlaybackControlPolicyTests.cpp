#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsAndroidPlaybackControlPolicy.h"

namespace OpenMobileHapticsAndroidPlaybackControlPolicyTests
{
	FOpenMobileHapticsRepeatPlan FinitePlan()
	{
		FOpenMobileHapticsRepeatPlan Plan;
		Plan.bLoop = true;
		Plan.RepeatCount = 2;
		Plan.TotalIterationCount = 3;
		Plan.PatternDurationSeconds = 2.0;
		Plan.RepeatStartTimeSeconds = 0.5;
		Plan.RepeatDurationSeconds = 1.5;
		Plan.TotalDurationSeconds = 5.0;
		Plan.MaximumDurationSeconds = 5.0;
		return Plan;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAndroidPlaybackControlSliceTest,
	"OpenMobile.Haptics.Android.Controls.WaveformSlice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAndroidPlaybackControlSliceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAndroidPlaybackControlPolicyTests;
	const TArray<int64> Timings = {500, 500, 1000};
	const TArray<int32> Amplitudes = {10, 20, 30};
	const FOpenMobileHapticsAndroidPlaybackControlResolution Resolution =
		FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve(
			Timings,
			Amplitudes,
			FinitePlan(),
			0.75,
			1,
			4.25,
			16
		);
	TestTrue(TEXT("Finite waveform suffix compiles"),
		Resolution.IsSuccess());
	TestEqual(TEXT("Current segment is shortened at the seek position"),
		Resolution.TimingsMilliseconds,
		TArray<int64>({250, 1000, 500, 1000}));
	TestEqual(TEXT("Suffix amplitudes retain segment ownership"),
		Resolution.Amplitudes,
		TArray<int32>({20, 30, 20, 30}));
	TestEqual(TEXT("Finite remaining loops are unrolled"),
		Resolution.RepeatIndex, INDEX_NONE);
	TestEqual(TEXT("Finite completion matches remaining native output"),
		Resolution.CompletionDurationMilliseconds,
		static_cast<int64>(2750));

	FOpenMobileHapticsRepeatPlan Infinite = FinitePlan();
	Infinite.bRepeatUntilStopped = true;
	Infinite.RepeatCount = 0;
	Infinite.TotalIterationCount = 0;
	Infinite.TotalDurationSeconds = 30.0;
	Infinite.MaximumDurationSeconds = 30.0;
	const FOpenMobileHapticsAndroidPlaybackControlResolution Repeating =
		FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve(
			Timings,
			Amplitudes,
			Infinite,
			0.75,
			7,
			3.25,
			16
		);
	TestTrue(TEXT("Indefinite suffix compiles"), Repeating.IsSuccess());
	TestEqual(TEXT("Indefinite suffix appends one repeat body"),
		Repeating.TimingsMilliseconds,
		TArray<int64>({250, 1000, 500, 1000}));
	TestEqual(TEXT("Indefinite playback loops at the appended body"),
		Repeating.RepeatIndex, 2);
	TestEqual(TEXT("Safety duration bounds indefinite playback"),
		Repeating.CompletionDurationMilliseconds,
		static_cast<int64>(3250));

	const FOpenMobileHapticsAndroidPlaybackControlResolution InitialRepeating =
		FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve(
			Timings,
			Amplitudes,
			Infinite,
			0.0,
			0,
			30.0,
			16
		);
	TestTrue(TEXT("Initial indefinite waveform compiles"),
		InitialRepeating.IsSuccess());
	TestEqual(TEXT("Initial indefinite waveform keeps the reusable base"),
		InitialRepeating.TimingsMilliseconds, Timings);
	TestEqual(TEXT("Initial indefinite waveform loops at the authored suffix"),
		InitialRepeating.RepeatIndex, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAndroidPlaybackControlBoundsTest,
	"OpenMobile.Haptics.Android.Controls.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAndroidPlaybackControlBoundsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAndroidPlaybackControlPolicyTests;
	const TArray<int64> Timings = {500, 500, 1000};
	const TArray<int32> Amplitudes = {10, 20, 30};
	FOpenMobileHapticsRepeatPlan NoLoop = FinitePlan();
	NoLoop.bLoop = false;
	NoLoop.RepeatCount = 0;
	NoLoop.TotalIterationCount = 1;
	NoLoop.RepeatStartTimeSeconds = 0.0;
	NoLoop.RepeatDurationSeconds = 0.0;
	NoLoop.TotalDurationSeconds = 2.0;
	NoLoop.MaximumDurationSeconds = 30.0;
	const FOpenMobileHapticsAndroidPlaybackControlResolution ExactBoundary =
		FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve(
			Timings, Amplitudes, NoLoop, 0.5, 0, 10.0, 8);
	TestTrue(TEXT("Exact segment boundary compiles"),
		ExactBoundary.IsSuccess());
	TestEqual(TEXT("Exact boundary starts the next segment"),
		ExactBoundary.TimingsMilliseconds,
		TArray<int64>({500, 1000}));
	TestEqual(TEXT("Finite output beats the larger safety bound"),
		ExactBoundary.CompletionDurationMilliseconds,
		static_cast<int64>(1500));

	TestFalse(TEXT("Pattern-end position is rejected"),
		FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve(
			Timings, Amplitudes, NoLoop, 2.0, 0, 1.0, 8
		).IsSuccess());
	TestFalse(TEXT("Nonpositive active time is rejected"),
		FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve(
			Timings, Amplitudes, NoLoop, 0.0, 0, 0.0, 8
		).IsSuccess());
	TestFalse(TEXT("Output segment capacity is enforced"),
		FOpenMobileHapticsAndroidPlaybackControlPolicy::Resolve(
			Timings, Amplitudes, FinitePlan(), 0.0, 0, 5.0, 3
		).IsSuccess());
	return true;
}

#endif
