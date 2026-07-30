#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenMobileSensorRecordingCodec.h"
#include "OpenMobileSensorReplaySession.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsRecordingService.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsReplaySessionTestsPrivate
{
	FString WriteFixture()
	{
		FOpenMobileSensorRecordingDocument Document;
		Document.Header.PluginVersion = TEXT("0.1.0");
		Document.Header.PlatformName = TEXT("Test");
		Document.Header.UnitsConvention = TEXT("SI");
		Document.Header.CoordinateConvention = TEXT("Unreal device-fixed");
		FOpenMobileSensorRecordingStreamDescriptor& Stream =
			Document.Header.Streams.AddDefaulted_GetRef();
		Stream.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Stream.Sensor.InstanceId = TEXT("Default");
		Stream.Family = EOpenMobileSensorSampleFamily::Vector;
		Stream.Units = TEXT("m/s^2");
		Stream.Capability.Sensor = Stream.Sensor;
		Stream.Capability.Availability.State =
			EOpenMobileCapabilityState::Available;
		FOpenMobileVectorSensorBatch& Batch =
			Document.VectorBatches.AddDefaulted_GetRef();
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FOpenMobileVectorSensorSample& Sample =
				Batch.Samples.AddDefaulted_GetRef();
			Sample.Header.Sensor = Stream.Sensor;
			Sample.Header.TimestampSeconds = Index * 0.01;
			Sample.Header.bValid = true;
			Sample.Value = FVector(Index + 1.0, 0.0, 9.81);
		}
		TArray<uint8> Bytes;
		FString Error;
		FOpenMobileSensorRecordingCodec::EncodeComplete(
			Document, Bytes, Error);
		const FString FilePath = FPaths::CreateTempFilename(
			*FPaths::ProjectSavedDir(),
			TEXT("omsensors-session-replay-"),
			TEXT(".bin"));
		FFileHelper::SaveArrayToFile(Bytes, *FilePath);
		return FilePath;
	}

	bool WaitUntil(
		UOpenMobileSensorReplaySession* Session,
		TFunctionRef<bool()> Predicate)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (!Predicate() && FPlatformTime::Seconds() < Deadline)
		{
			FOpenMobileSensorsRecordingService::TickForTests(
				FPlatformTime::Seconds());
			Session->TickForTests();
			FPlatformProcess::Sleep(0.001f);
		}
		return Predicate();
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
	FOpenMobileSensorsReplaySessionReflectionTest,
	"OpenMobile.Sensors.Blueprint.ReplaySession.Reflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsReplaySessionReflectionTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);
#if WITH_METADATA
	TestNotNull(TEXT("The typed replay factory is reflected"),
		UOpenMobileSensorReplaySession::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorReplaySession,
				ReplaySensorFile)));
	for (const FName FunctionName : {
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorReplaySession, PauseReplay),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorReplaySession, ResumeReplay),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorReplaySession, SeekReplayTo),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorReplaySession,
			SetReplaySpeed),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorReplaySession,
			SetReplayLooping),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorReplaySession,
			AdvanceReplayBy),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorReplaySession, StopReplay)})
	{
		TestNotNull(TEXT("The typed replay control is reflected"),
			UOpenMobileSensorReplaySession::StaticClass()->FindFunctionByName(
				FunctionName));
	}
	for (const FName EventName : {
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorReplaySession, ReplayStarted),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorReplaySession, StateChanged),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorReplaySession, Looped),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorReplaySession, Paused),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorReplaySession, Resumed),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorReplaySession, Completed),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorReplaySession, Failed),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorReplaySession, Cancelled)})
	{
		TestNotNull(TEXT("The replay lifecycle event is reflected"),
			FindFProperty<FMulticastDelegateProperty>(
				UOpenMobileSensorReplaySession::StaticClass(),
				EventName));
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsReplaySessionLifecycleTest,
	"OpenMobile.Sensors.Blueprint.ReplaySession.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsReplaySessionLifecycleTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsReplaySessionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("TypedReplaySession"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FString FilePath = WriteFixture();
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileSensorsReplaySession"));
	UWorld* World = GameInstance->GetWorld();
	UOpenMobileSensorsSubsystem* Subsystem =
		GameInstance->GetSubsystem<UOpenMobileSensorsSubsystem>();
	FOpenMobileSensorReplayOptions Options;
	Options.ClockMode = EOpenMobileSensorReplayClockMode::Manual;
	Options.bStartPaused = true;
	Options.bLoop = true;
	UOpenMobileSensorReplaySession* Session =
		UOpenMobileSensorReplaySession::ReplaySensorFile(
			World, FilePath, Options, World);
	Session->Activate();
	TestTrue(TEXT("The typed replay loads in its requested paused state"),
		WaitUntil(Session, [&]()
		{
			return Session->GetReplayState() ==
				EOpenMobileSensorReplayState::Paused;
		}));
	TestTrue(TEXT("The subsystem lists its active replay session"),
		Subsystem->GetActiveReplaySessionsNative().Contains(Session));
	TestTrue(TEXT("The typed replay resumes"),
		Session->ResumeReplay().IsSuccess());
	TestTrue(TEXT("The typed replay accepts a timespan seek"),
		Session->SeekReplayTo(FTimespan::Zero()).IsSuccess());
	TestTrue(TEXT("The typed replay accepts a speed change"),
		Session->SetReplaySpeed(2.0).IsSuccess());
	TestTrue(TEXT("The typed replay can advance through a loop"),
		Session->AdvanceReplayBy(
			FTimespan::FromSeconds(0.02)).IsSuccess());
	TestFalse(TEXT("A looping typed replay stays active"),
		Session->IsFinished());
	TestTrue(TEXT("The typed replay can disable looping"),
		Session->SetReplayLooping(false).IsSuccess());
	TestTrue(TEXT("The typed replay can seek into its final pass"),
		Session->SeekReplayTo(FTimespan::Zero()).IsSuccess());
	TestTrue(TEXT("The typed replay advances with a timespan"),
		Session->AdvanceReplayBy(FTimespan::FromSeconds(1.0)).IsSuccess());
	TestTrue(TEXT("The replay session reaches its terminal event"),
		Session->IsFinished());
	TestEqual(TEXT("A complete replay has explicit state"),
		Session->GetReplayState(),
		EOpenMobileSensorReplayState::Completed);

	IFileManager::Get().Delete(*FilePath);
	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

#endif
