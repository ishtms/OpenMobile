#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsSubsystem.h"
#include "OpenMobileHapticsTimingPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAudioClockConversionTest,
	"OpenMobile.Haptics.Timing.AudioClockConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAudioClockConversionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	FOpenMobileHapticsTimingPolicy Policy;
	const FOpenMobileHapticTimingCalibrationResult Calibration =
		Policy.Calibrate(
			EOpenMobileHapticTimingClock::Audio,
			120.0,
			50.0,
			0.004,
			7
		);
	TestTrue(TEXT("The audio clock anchor is accepted"), Calibration.bAccepted);
	TestEqual(TEXT("The first calibration has revision one"),
		Calibration.Anchor.CalibrationRevision, static_cast<int64>(1));

	FOpenMobileHapticSchedule Schedule;
	Schedule.Mode = EOpenMobileHapticScheduleMode::AbsoluteAudioTime;
	Schedule.TimeSeconds = 120.25;
	Schedule.LatencyOffsetSeconds = -0.01;
	const FOpenMobileHapticsTimingResolution Resolution = Policy.Resolve(
		Schedule,
		50.1,
		7,
		EOpenMobileHapticSynchronizationMode::BestEffort
	);

	TestEqual(TEXT("The audio schedule resolves"),
		Resolution.Outcome, EOpenMobileHapticsTimingOutcome::Ready);
	TestEqual(TEXT("The source clock remains explicit"),
		Resolution.Diagnostics.Clock, EOpenMobileHapticTimingClock::Audio);
	TestEqual(TEXT("Android-style scheduling is reported as best effort"),
		Resolution.Diagnostics.Mode,
		EOpenMobileHapticSynchronizationMode::BestEffort);
	TestTrue(TEXT("The absolute clock target converts once"),
		FMath::IsNearlyEqual(
			Resolution.Diagnostics.ResolvedPlatformTimeSeconds,
			50.24
		));
	TestTrue(TEXT("The native delay is based on monotonic now"),
		FMath::IsNearlyEqual(Resolution.StartDelaySeconds, 0.14));
	TestEqual(TEXT("Calibration precision reaches the result contract"),
		Resolution.Diagnostics.EstimatedPrecisionSeconds, 0.004);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsTimingCalibrationAPITest,
	"OpenMobile.Haptics.Timing.CalibrationAPI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsTimingCalibrationAPITest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileHapticsSubsystem* Subsystem =
		NewObject<UOpenMobileHapticsSubsystem>(GameInstance);

	const FOpenMobileHapticTimingCalibrationResult First =
		Subsystem->CalibrateTimingClock(
			EOpenMobileHapticTimingClock::Audio,
			12.0,
			0.003
		);
	TestTrue(TEXT("The subsystem accepts an audio clock sample"),
		First.bAccepted);
	TestTrue(TEXT("The returned timing anchor is valid"),
		First.Anchor.IsValid());
	TestTrue(TEXT("The subsystem captures a monotonic timestamp"),
		First.Anchor.PlatformMonotonicTimeSeconds > 0.0);

	const FOpenMobileHapticTimingCalibrationResult Invalid =
		Subsystem->CalibrateTimingClock(
			EOpenMobileHapticTimingClock::None,
			12.0,
			0.003
		);
	TestFalse(TEXT("A clockless sample is rejected"), Invalid.bAccepted);

	FOpenMobileHapticsBackendRegistry::SetApplicationActive(false);
	const FOpenMobileHapticTimingCalibrationResult AfterLifecycle =
		Subsystem->CalibrateTimingClock(
			EOpenMobileHapticTimingClock::Audio,
			13.0,
			0.003
		);
	TestTrue(TEXT("A fresh post-lifecycle anchor is accepted"),
		AfterLifecycle.bAccepted);
	TestNotEqual(TEXT("Lifecycle changes produce a fresh generation"),
		AfterLifecycle.Anchor.LifecycleGeneration,
		First.Anchor.LifecycleGeneration);

	Subsystem->Deinitialize();
	FOpenMobileHapticsBackendRegistry::SetApplicationActive(true);
	FOpenMobileHapticsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsClockInvalidationTest,
	"OpenMobile.Haptics.Timing.ClockInvalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsClockInvalidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsTimingPolicy Policy;
	const FOpenMobileHapticTimingCalibrationResult First = Policy.Calibrate(
		EOpenMobileHapticTimingClock::Game,
		20.0,
		100.0,
		0.002,
		3
	);
	TestEqual(TEXT("A valid sample reports accepted status"),
		First.Status, EOpenMobileHapticTimingCalibrationStatus::Accepted);

	const FOpenMobileHapticTimingCalibrationResult Drift = Policy.Calibrate(
		EOpenMobileHapticTimingClock::Game,
		20.1,
		100.3,
		0.002,
		3
	);
	TestFalse(TEXT("Excessive clock drift invalidates calibration"),
		Drift.bAccepted);
	TestEqual(TEXT("Clock drift has a typed result"),
		Drift.Status,
		EOpenMobileHapticTimingCalibrationStatus::ClockDiscontinuity);

	FOpenMobileHapticSchedule Absolute;
	Absolute.Mode = EOpenMobileHapticScheduleMode::AbsoluteGameTime;
	Absolute.TimeSeconds = 20.2;
	TestEqual(TEXT("The invalidated anchor cannot resolve"),
		Policy.Resolve(
			Absolute,
			100.2,
			3,
			EOpenMobileHapticSynchronizationMode::None
		).Outcome,
		EOpenMobileHapticsTimingOutcome::MissingCalibration);

	Policy.Calibrate(
		EOpenMobileHapticTimingClock::Game,
		30.0,
		200.0,
		0.002,
		4
	);
	Absolute.TimeSeconds = 30.0;
	TestEqual(TEXT("A prior lifecycle anchor becomes stale"),
		Policy.Resolve(
			Absolute,
			200.0,
			5,
			EOpenMobileHapticSynchronizationMode::None
		).Outcome,
		EOpenMobileHapticsTimingOutcome::StaleCalibration);

	FOpenMobileHapticSchedule Late;
	Late.Mode = EOpenMobileHapticScheduleMode::Relative;
	Late.LatencyOffsetSeconds = -0.05;
	const FOpenMobileHapticsTimingResolution Boundary = Policy.Resolve(
		Late,
		300.0,
		5,
		EOpenMobileHapticSynchronizationMode::BestEffort
	);
	TestEqual(TEXT("The exact late boundary starts immediately"),
		Boundary.Outcome, EOpenMobileHapticsTimingOutcome::Ready);
	TestTrue(TEXT("The late boundary reports its lateness"),
		FMath::IsNearlyEqual(Boundary.Diagnostics.LatenessSeconds, 0.05));
	Late.LatencyOffsetSeconds = -0.051;
	TestEqual(TEXT("A request beyond the late boundary is rejected"),
		Policy.Resolve(
			Late,
			300.0,
			5,
			EOpenMobileHapticSynchronizationMode::BestEffort
		).Outcome,
		EOpenMobileHapticsTimingOutcome::TooLate);
	return true;
}

#endif
