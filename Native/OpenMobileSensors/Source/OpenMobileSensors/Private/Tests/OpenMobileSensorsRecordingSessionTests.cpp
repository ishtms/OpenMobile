#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorRecordingSession.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsRecordingService.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsRecordingSessionTestsPrivate
{
	bool WaitUntil(TFunctionRef<bool()> Predicate)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (!Predicate() && FPlatformTime::Seconds() < Deadline)
		{
			FOpenMobileSensorsRecordingService::TickForTests(
				FPlatformTime::Seconds());
			FOpenMobileSensorsSubscriptionService::
				ProcessPendingBackendOperationsForTests();
			FPlatformProcess::Sleep(0.001f);
		}
		return Predicate();
	}

	FOpenMobileSensorRecordingOptions MakeOptions(double MaximumDurationSeconds)
	{
		FOpenMobileSensorRecordingOptions Options;
		FOpenMobileSensorIdentifier& Sensor =
			Options.Sensors.AddDefaulted_GetRef();
		Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Sensor.InstanceId = TEXT("Default");
		Options.MaximumDurationSeconds = MaximumDurationSeconds;
		Options.MaximumBytes = 1024ll * 1024;
		return Options;
	}

	void ResetServices()
	{
		FOpenMobileSensorsRecordingService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsRecordingService::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingSessionReflectionTest,
	"OpenMobile.Sensors.Blueprint.RecordingSession.Reflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingSessionReflectionTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);
#if WITH_METADATA
	const UFunction* Factory =
		UOpenMobileSensorRecordingSession::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorRecordingSession,
				RecordSensors));
	TestNotNull(TEXT("The recording session factory is reflected"), Factory);
	if (Factory)
	{
		TestTrue(TEXT("The recording node explains that start is not final"),
			Factory->GetMetaData(TEXT("ToolTip")).Contains(
				TEXT("Recording Started")));
	}
	for (const FName EventName : {
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorRecordingSession,
			RecordingStarted),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorRecordingSession,
			Finalized),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorRecordingSession,
			LimitReached),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorRecordingSession, Failed),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorRecordingSession, Cancelled)})
	{
		TestNotNull(TEXT("The recording lifecycle event is reflected"),
			FindFProperty<FMulticastDelegateProperty>(
				UOpenMobileSensorRecordingSession::StaticClass(),
				EventName));
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingSessionLifecycleTest,
	"OpenMobile.Sensors.Blueprint.RecordingSession.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingSessionLifecycleTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingSessionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RecordingSession"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileSensorsRecordingSession"));
	UWorld* World = GameInstance->GetWorld();
	UOpenMobileSensorsSubsystem* Subsystem =
		GameInstance->GetSubsystem<UOpenMobileSensorsSubsystem>();

	UOpenMobileSensorRecordingSession* Session =
		UOpenMobileSensorRecordingSession::RecordSensors(
			World,
			MakeOptions(30.0),
			World);
	Session->Activate();
	TestTrue(TEXT("The typed recording session reaches Recording"),
		WaitUntil([&]()
		{
			return Session->GetRecordingState() ==
				EOpenMobileSensorRecordingState::Recording;
		}));
	TestTrue(TEXT("The subsystem lists its active recording session"),
		Subsystem->GetActiveRecordingSessionsNative().Contains(Session));
	Session->FinalizeRecording();
	TestTrue(TEXT("Finalizing completes the recording session"),
		WaitUntil([&]() { return Session->IsFinished(); }));
	TestEqual(TEXT("Finalize produces a complete recording"),
		Session->GetRecordingState(),
		EOpenMobileSensorRecordingState::Completed);
	TestTrue(TEXT("A finalized recording reports its file"),
		!Session->GetRecordingSnapshot().FilePath.IsEmpty());
	IFileManager::Get().Delete(*Session->GetRecordingSnapshot().FilePath);

	UOpenMobileSensorRecordingSession* Limited =
		UOpenMobileSensorRecordingSession::RecordSensors(
			World,
			MakeOptions(1.0),
			World);
	Limited->Activate();
	TestTrue(TEXT("The bounded recording starts"),
		WaitUntil([&]()
		{
			return Limited->GetRecordingState() ==
				EOpenMobileSensorRecordingState::Recording;
		}));
	FOpenMobileSensorsRecordingService::TickForTests(
		FPlatformTime::Seconds() + 2.0);
	TestTrue(TEXT("The duration limit finalizes the session"),
		WaitUntil([&]() { return Limited->IsFinished(); }));
	TestEqual(TEXT("The session identifies its duration limit"),
		Limited->GetLimitReason(),
		EOpenMobileSensorRecordingLimitReason::Duration);
	IFileManager::Get().Delete(*Limited->GetRecordingSnapshot().FilePath);

	UOpenMobileSensorRecordingSession* Discarded =
		UOpenMobileSensorRecordingSession::RecordSensors(
			World,
			MakeOptions(30.0),
			World);
	Discarded->Activate();
	TestTrue(TEXT("The discard fixture starts"),
		WaitUntil([&]()
		{
			return Discarded->GetRecordingState() ==
				EOpenMobileSensorRecordingState::Recording;
		}));
	const FString DiscardedPath =
		Discarded->GetRecordingSnapshot().FilePath;
	Discarded->DiscardRecording();
	TestTrue(TEXT("Discard completes the recording session"),
		Discarded->IsFinished());
	TestEqual(TEXT("Discard reports the cancelled state"),
		Discarded->GetRecordingState(),
		EOpenMobileSensorRecordingState::Cancelled);
	TestFalse(TEXT("Discard does not leave a replayable file"),
		IFileManager::Get().FileExists(*DiscardedPath));

	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

#endif
