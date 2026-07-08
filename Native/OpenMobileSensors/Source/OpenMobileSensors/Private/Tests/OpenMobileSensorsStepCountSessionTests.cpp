#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"
#include "OpenMobileStepCountSessionTracker.h"

namespace OpenMobileSensorsStepCountSessionTestsPrivate
{
	FOpenMobileStepsSensorSample MakeNativeStepSample(
		int64 Count,
		double TimestampSeconds,
		const FGuid& OriginIdentifier,
		EOpenMobileStepCountDiscontinuity Discontinuity =
			EOpenMobileStepCountDiscontinuity::None
	)
	{
		FOpenMobileStepsSensorSample Sample;
		Sample.Header.Sensor.Type = EOpenMobileSensorType::StepCounter;
		Sample.Header.Sensor.InstanceId = TEXT("Default");
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::NativeFused
		);
		Sample.Count = Count;
		Sample.Origin = EOpenMobileStepCountOrigin::DeviceBoot;
		Sample.OriginIdentifier = OriginIdentifier;
		Sample.Discontinuity = Discontinuity;
		return Sample;
	}

	FOpenMobileSensorCapability MakeStepCounterCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::StepCounter;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("StepCounter");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.RequiredPermission =
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::MotionActivity
			);
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 10.0;
		return Capability;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	void GrantMotionPermission()
	{
		FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::MotionActivity),
			EOpenMobilePermissionStatus::Granted);
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsStepCountSessionCounterTest,
	"OpenMobile.Sensors.Steps.Session.Counter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsStepCountSessionCounterTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsStepCountSessionTestsPrivate;
	const FGuid SessionIdentifier = FGuid::NewGuid();
	const FGuid NativeOrigin = FGuid::NewGuid();
	FOpenMobileStepCountSessionTracker Tracker(SessionIdentifier);
	FOpenMobileStepsSensorSample Output;
	TestTrue(TEXT("The first native total establishes the baseline"),
		Tracker.Process(
			MakeNativeStepSample(4000000000LL, 1.0, NativeOrigin),
			Output));
	TestEqual(TEXT("The session begins at zero"), Output.Count, 0LL);
	TestEqual(TEXT("The session origin is explicit"),
		Output.Origin, EOpenMobileStepCountOrigin::Session);
	TestEqual(TEXT("The session identifier is stable"),
		Output.OriginIdentifier, SessionIdentifier);
	TestEqual(TEXT("The first sample reports a stream start"),
		Output.Discontinuity,
		EOpenMobileStepCountDiscontinuity::StreamStarted);
	TestTrue(TEXT("A later native total is accepted"),
		Tracker.Process(
			MakeNativeStepSample(4000000025LL, 2.0, NativeOrigin),
			Output));
	TestEqual(TEXT("Wide native totals produce a wide session count"),
		Output.Count, 25LL);
	TestFalse(TEXT("An ordinary count is not saturated"),
		Output.bCountSaturated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsStepCountConcurrentSessionResetTest,
	"OpenMobile.Sensors.Steps.Session.ConcurrentReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsStepCountConcurrentSessionResetTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsStepCountSessionTestsPrivate;
	const FGuid NativeOrigin = FGuid::NewGuid();
	FOpenMobileStepCountSessionTracker First(FGuid::NewGuid());
	FOpenMobileStepCountSessionTracker Second(FGuid::NewGuid());
	FOpenMobileStepsSensorSample FirstOutput;
	FOpenMobileStepsSensorSample SecondOutput;
	First.Process(MakeNativeStepSample(100, 1.0, NativeOrigin), FirstOutput);
	First.Process(MakeNativeStepSample(110, 2.0, NativeOrigin), FirstOutput);
	Second.Process(MakeNativeStepSample(110, 2.0, NativeOrigin), SecondOutput);
	First.Process(MakeNativeStepSample(130, 3.0, NativeOrigin), FirstOutput);
	Second.Process(MakeNativeStepSample(130, 3.0, NativeOrigin), SecondOutput);
	TestEqual(TEXT("The older session keeps its own baseline"),
		FirstOutput.Count, 30LL);
	TestEqual(TEXT("The newer session keeps its own baseline"),
		SecondOutput.Count, 20LL);

	First.ResetBaseline();
	First.Process(MakeNativeStepSample(140, 4.0, NativeOrigin), FirstOutput);
	Second.Process(MakeNativeStepSample(140, 4.0, NativeOrigin), SecondOutput);
	TestEqual(TEXT("Reset affects only the selected session"),
		FirstOutput.Count, 0LL);
	TestEqual(TEXT("Reset is explicit"),
		FirstOutput.Discontinuity,
		EOpenMobileStepCountDiscontinuity::SessionReset);
	TestEqual(TEXT("The other session remains continuous"),
		SecondOutput.Count, 30LL);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsStepCountSessionRollbackOverflowTest,
	"OpenMobile.Sensors.Steps.Session.RollbackOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsStepCountSessionRollbackOverflowTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsStepCountSessionTestsPrivate;
	const FGuid FirstOrigin = FGuid::NewGuid();
	const FGuid SecondOrigin = FGuid::NewGuid();
	FOpenMobileStepCountSessionTracker Tracker(FGuid::NewGuid());
	FOpenMobileStepsSensorSample Output;
	Tracker.Process(MakeNativeStepSample(100, 1.0, FirstOrigin), Output);
	Tracker.Process(MakeNativeStepSample(150, 2.0, FirstOrigin), Output);
	Tracker.Process(MakeNativeStepSample(
		10,
		3.0,
		SecondOrigin,
		EOpenMobileStepCountDiscontinuity::NativeCounterReset), Output);
	TestEqual(TEXT("A native reset retains the confirmed session count"),
		Output.Count, 50LL);
	TestEqual(TEXT("A native reset remains explicit"),
		Output.Discontinuity,
		EOpenMobileStepCountDiscontinuity::NativeCounterReset);
	Tracker.Process(MakeNativeStepSample(25, 4.0, SecondOrigin), Output);
	TestEqual(TEXT("Counting continues from the new native baseline"),
		Output.Count, 65LL);

	FOpenMobileStepCountSessionTracker Overflow(FGuid::NewGuid());
	const FGuid OverflowOrigin = FGuid::NewGuid();
	const FGuid RestartedOverflowOrigin = FGuid::NewGuid();
	Overflow.Process(MakeNativeStepSample(0, 1.0, OverflowOrigin), Output);
	Overflow.Process(MakeNativeStepSample(
		TNumericLimits<int64>::Max() - 5,
		2.0,
		OverflowOrigin), Output);
	Overflow.Process(MakeNativeStepSample(
		0,
		3.0,
		RestartedOverflowOrigin,
		EOpenMobileStepCountDiscontinuity::NativeCounterReset), Output);
	Overflow.Process(MakeNativeStepSample(
		10,
		4.0,
		RestartedOverflowOrigin), Output);
	TestEqual(TEXT("A long-running session saturates instead of wrapping"),
		Output.Count, TNumericLimits<int64>::Max());
	TestTrue(TEXT("Saturation remains explicit"), Output.bCountSaturated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsOwnedStepCountSessionsTest,
	"OpenMobile.Sensors.Steps.Session.OwnedSessions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsOwnedStepCountSessionsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsStepCountSessionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("StepCountSessions"));
	Backend.SetSensorCapabilities({MakeStepCounterCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	GrantMotionPermission();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	FOpenMobileSensorStreamOptions Options;
	const FOpenMobileSensorSubscriptionResult First =
		Subsystem->BeginStepCountSessionNative(Options);
	FOpenMobileSensorSubscriptionRequest InvalidSessionRequest;
	InvalidSessionRequest.Sensor.Type = EOpenMobileSensorType::Accelerometer;
	InvalidSessionRequest.Sensor.InstanceId = TEXT("Default");
	InvalidSessionRequest.bResettableStepCountSession = true;
	const FOpenMobileSensorSubscriptionResult InvalidSession =
		Subsystem->StartSubscriptionNative(InvalidSessionRequest);
	TestEqual(TEXT("Session mode is valid only for Step Counter"),
		InvalidSession.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::InvalidRequest);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("The first session begins"), First.Operation.IsSuccess());
	TestTrue(TEXT("Reset before the first sample is accepted"),
		Subsystem->ResetStepCountSessionNative(First.Handle).IsSuccess());
	const FGuid NativeOrigin = FGuid::NewGuid();
	FOpenMobileSensorsSampleService::PublishSteps(
		MakeNativeStepSample(100, 1.0, NativeOrigin));
	FOpenMobileSensorReadResult Read;
	FOpenMobileStepsSensorSample FirstSample;
	TestTrue(TEXT("The first session establishes a delayed baseline"),
		Subsystem->ReadStepCountSessionNative(
			First.Handle,
			0,
			Read,
			FirstSample));
	TestEqual(TEXT("The delayed baseline starts at zero"),
		FirstSample.Count, 0LL);
	TestEqual(TEXT("The pre-baseline reset remains explicit"),
		FirstSample.Discontinuity,
		EOpenMobileStepCountDiscontinuity::SessionReset);

	const FOpenMobileSensorSubscriptionResult Second =
		Subsystem->BeginStepCountSessionNative(Options);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("The second session begins"), Second.Operation.IsSuccess());
	TestEqual(TEXT("Concurrent sessions share one native stream"),
		Backend.GetStartSensorStreamCount(), 1);
	FOpenMobileSensorSubscriptionRequest NativeRequest;
	NativeRequest.Sensor.Type = EOpenMobileSensorType::StepCounter;
	NativeRequest.Sensor.InstanceId = TEXT("Default");
	const FOpenMobileSensorSubscriptionResult NativeSubscription =
		Subsystem->StartSubscriptionNative(NativeRequest);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("A native-total subscription can share the stream"),
		NativeSubscription.Operation.IsSuccess());
	TestEqual(TEXT("A native-total handle cannot be reset as a session"),
		Subsystem->ResetStepCountSessionNative(
			NativeSubscription.Handle).Failure.Reason,
		EOpenMobileSensorFailureReason::UnsupportedOperation);
	FOpenMobileSensorsSampleService::PublishSteps(
		MakeNativeStepSample(110, 2.0, NativeOrigin));
	FOpenMobileStepsSensorSample SecondSample;
	Subsystem->ReadStepCountSessionNative(
		First.Handle, 0, Read, FirstSample);
	Subsystem->ReadStepCountSessionNative(
		Second.Handle, 0, Read, SecondSample);
	TestEqual(TEXT("The first session keeps its earlier baseline"),
		FirstSample.Count, 10LL);
	TestEqual(TEXT("The second session starts independently"),
		SecondSample.Count, 0LL);

	TestTrue(TEXT("One session resets explicitly"),
		Subsystem->ResetStepCountSessionNative(First.Handle).IsSuccess());
	FOpenMobileStepsSensorSample Native =
		MakeNativeStepSample(140, 3.0, NativeOrigin);
	FOpenMobileSensorsSampleService::PublishSteps(Native);
	Subsystem->ReadStepCountSessionNative(
		First.Handle, 0, Read, FirstSample);
	Subsystem->ReadStepCountSessionNative(
		Second.Handle, 0, Read, SecondSample);
	TestEqual(TEXT("Reset session returns to zero"), FirstSample.Count, 0LL);
	TestEqual(TEXT("The other session remains continuous"),
		SecondSample.Count, 30LL);
	TestEqual(TEXT("The platform native total is untouched"),
		Native.Count, 140LL);
	FOpenMobileStepsSensorSample NativeSample;
	TestTrue(TEXT("The native-total subscription remains readable"),
		Subsystem->GetLatestStepsSampleNative(
			NativeSubscription.Handle, 0, Read, NativeSample));
	TestEqual(TEXT("The native-total subscription is not reset"),
		NativeSample.Count, 140LL);
	TestEqual(TEXT("The native-total origin remains unchanged"),
		NativeSample.Origin, EOpenMobileStepCountOrigin::DeviceBoot);
	UGameInstance* OtherGameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* OtherOwner =
		NewObject<UOpenMobileSensorsSubsystem>(OtherGameInstance);
	const FOpenMobileSensorSubscriptionResult OtherSession =
		OtherOwner->BeginStepCountSessionNative(Options);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("Another owner has an independent session"),
		OtherSession.Operation.IsSuccess());
	TestEqual(TEXT("Another owner cannot reset the session"),
		OtherOwner->ResetStepCountSessionNative(
			First.Handle).Failure.Reason,
		EOpenMobileSensorFailureReason::StaleHandle);
	TestFalse(TEXT("Another owner cannot read the session"),
		OtherOwner->ReadStepCountSessionNative(
			First.Handle, 0, Read, FirstSample));
	OtherOwner->Deinitialize();

	TestTrue(TEXT("The first session stops"),
		Subsystem->StopStepCountSessionNative(First.Handle).IsSuccess());
	TestEqual(TEXT("The shared native stream stays active"),
		Backend.GetStopSensorStreamCount(), 0);
	TestTrue(TEXT("The second session stops"),
		Subsystem->StopStepCountSessionNative(Second.Handle).IsSuccess());
	TestEqual(TEXT("The native-total subscription keeps the stream active"),
		Backend.GetStopSensorStreamCount(), 0);
	TestTrue(TEXT("The native-total subscription stops normally"),
		Subsystem->StopSubscriptionNative(
			NativeSubscription.Handle).IsSuccess());
	TestEqual(TEXT("The final consumer releases the native stream"),
		Backend.GetStopSensorStreamCount(), 1);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsStepCountSessionPersistenceTest,
	"OpenMobile.Sensors.Steps.Session.Persistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsStepCountSessionPersistenceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsStepCountSessionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("StepSessionPersistence"));
	Backend.SetSensorCapabilities({MakeStepCounterCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	GrantMotionPermission();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	const FOpenMobileStepCountSessionPolicy Policy =
		Subsystem->GetStepCountSessionPolicyNative();
	TestTrue(TEXT("Confirmed count persists across app pause"),
		Policy.bPersistsAcrossApplicationPause);
	TestFalse(TEXT("Paused steps are not promised without native continuity"),
		Policy.bBackfillsStepsWhilePaused);
	TestFalse(TEXT("Game Instance recreation ends ownership"),
		Policy.bPersistsAcrossGameInstanceRecreation);
	TestFalse(TEXT("Process restart does not restore a session"),
		Policy.bPersistsAcrossProcessRestart);
	TestFalse(TEXT("Unrecoverable permission loss ends a session"),
		Policy.bPersistsAcrossPermissionLoss);
	TestTrue(TEXT("Native reset keeps the confirmed total"),
		Policy.bCarriesConfirmedCountAcrossNativeReset);

	FOpenMobileSensorStreamOptions Options;
	const FOpenMobileSensorSubscriptionResult Session =
		Subsystem->BeginStepCountSessionNative(Options);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FGuid FirstOrigin = FGuid::NewGuid();
	FOpenMobileSensorsSampleService::PublishSteps(
		MakeNativeStepSample(100, 1.0, FirstOrigin));
	FOpenMobileSensorsSampleService::PublishSteps(
		MakeNativeStepSample(150, 2.0, FirstOrigin));
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Session.Handle,
		EOpenMobileSensorSubscriptionState::Paused);
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Session.Handle,
		EOpenMobileSensorSubscriptionState::Active);
	const FGuid SecondOrigin = FGuid::NewGuid();
	FOpenMobileSensorsSampleService::PublishSteps(MakeNativeStepSample(
		10,
		3.0,
		SecondOrigin,
		EOpenMobileStepCountDiscontinuity::NativeCounterReset));
	FOpenMobileSensorReadResult Read;
	FOpenMobileStepsSensorSample Sample;
	Subsystem->ReadStepCountSessionNative(
		Session.Handle, 0, Read, Sample);
	TestEqual(TEXT("Pause and native reset retain the confirmed count"),
		Sample.Count, 50LL);
	FOpenMobileSensorsSampleService::PublishSteps(
		MakeNativeStepSample(25, 4.0, SecondOrigin));
	Subsystem->ReadStepCountSessionNative(
		Session.Handle, 0, Read, Sample);
	TestEqual(TEXT("The resumed origin continues counting"),
		Sample.Count, 65LL);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity),
		EOpenMobilePermissionStatus::Denied);
	FOpenMobileSensorsSubscriptionService::
		InvalidateForUnrecoverablePermissionLoss(
			EOpenMobileSensorType::StepCounter);
	TestFalse(TEXT("Unrecoverable permission loss ends the session"),
		Subsystem->ReadStepCountSessionNative(
			Session.Handle, 0, Read, Sample));
	GrantMotionPermission();
	const FOpenMobileSensorSubscriptionResult AfterPermissionRestore =
		Subsystem->BeginStepCountSessionNative(Options);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsSampleService::PublishSteps(
		MakeNativeStepSample(30, 5.0, FGuid::NewGuid()));
	TestTrue(TEXT("A new session can begin after permission returns"),
		Subsystem->ReadStepCountSessionNative(
			AfterPermissionRestore.Handle, 0, Read, Sample));
	TestEqual(TEXT("Permission restoration creates a fresh baseline"),
		Sample.Count, 0LL);

	Subsystem->Deinitialize();
	UGameInstance* ReplacementGameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Replacement =
		NewObject<UOpenMobileSensorsSubsystem>(ReplacementGameInstance);
	TestFalse(TEXT("A replacement Game Instance cannot read the old session"),
		Replacement->ReadStepCountSessionNative(
			AfterPermissionRestore.Handle, 0, Read, Sample));
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity),
		EOpenMobilePermissionStatus::Denied);
	const FOpenMobileSensorSubscriptionResult Denied =
		Replacement->BeginStepCountSessionNative(Options);
	TestEqual(TEXT("Denied motion access blocks a new session"),
		Denied.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::PermissionDenied);
	Replacement->Deinitialize();
	FinishBackend(Backend);
	return true;
}

#endif
