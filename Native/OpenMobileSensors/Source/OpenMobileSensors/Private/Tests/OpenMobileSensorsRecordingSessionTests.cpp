#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorBlueprintLibrary.h"
#include "OpenMobileSensorListener.h"
#include "OpenMobileSensorRecordingSession.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsRecordingService.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"
#include "UObject/Package.h"

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
	for (const FName FunctionName : {
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorRecordingSession,
			StartRecordingSensor),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorRecordingSession,
			StartRecordingListeners),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorRecordingSession,
			StartRecordingActiveSensors),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorRecordingSession,
			GetAppliedRecordingOptions)})
	{
		TestNotNull(TEXT("The preferred recording helper is reflected"),
			UOpenMobileSensorRecordingSession::StaticClass()->
				FindFunctionByName(FunctionName));
	}
	TestNotNull(TEXT("The project recording defaults are reflected"),
		UOpenMobileSensorBlueprintLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorBlueprintLibrary,
				GetDefaultSensorRecordingOptions)));
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
	FOpenMobileSensorsRecordingSessionDefaultsTest,
	"OpenMobile.Sensors.Blueprint.RecordingSession.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingSessionDefaultsTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const double PreviousDuration =
		Settings->MaximumRecordingDurationSeconds;
	const int64 PreviousBytes = Settings->MaximumRecordingBytes;
	Settings->MaximumRecordingDurationSeconds = 42.0;
	Settings->MaximumRecordingBytes = 8ll * 1024 * 1024;

	const FOpenMobileSensorRecordingOptions Defaults =
		UOpenMobileSensorBlueprintLibrary::
			GetDefaultSensorRecordingOptions();
	TestEqual(TEXT("Recording defaults use the project duration policy"),
		Defaults.MaximumDurationSeconds, 42.0);
	TestEqual(TEXT("Recording defaults use the project size policy"),
		Defaults.MaximumBytes, 8ll * 1024 * 1024);

	UOpenMobileSensorRecordingSession* Session =
		UOpenMobileSensorRecordingSession::StartRecordingSensor(
			GetTransientPackage(),
			EOpenMobileSensorType::Accelerometer,
			nullptr);
	const FOpenMobileSensorRecordingOptions Applied =
		Session->GetAppliedRecordingOptions();
	TestEqual(TEXT("The preferred factory selects one sensor"),
		Applied.Sensors.Num(), 1);
	if (Applied.Sensors.Num() == 1)
	{
		TestEqual(TEXT("The preferred factory keeps the sensor type"),
			Applied.Sensors[0].Type,
			EOpenMobileSensorType::Accelerometer);
	}
	TestEqual(TEXT("The session reports its applied duration policy"),
		Applied.MaximumDurationSeconds, 42.0);
	TestEqual(TEXT("The session reports its applied size policy"),
		Applied.MaximumBytes, 8ll * 1024 * 1024);
	FOpenMobileSensorStreamOptions ListenerOptions;
	UOpenMobileAccelerometerListener* Listener =
		UOpenMobileAccelerometerListener::ListenForAccelerometer(
			GetTransientPackage(),
			ListenerOptions,
			EOpenMobileSensorRatePreset::UI,
			EOpenMobileSensorCoordinateSpace::DeviceFixed,
			false,
			nullptr);
	UOpenMobileSensorRecordingSession* ListenerSession =
		UOpenMobileSensorRecordingSession::StartRecordingListeners(
			GetTransientPackage(),
			{Listener},
			nullptr);
	const FOpenMobileSensorRecordingOptions ListenerApplied =
		ListenerSession->GetAppliedRecordingOptions();
	TestEqual(TEXT("The listener factory selects one distinct sensor"),
		ListenerApplied.Sensors.Num(), 1);
	if (ListenerApplied.Sensors.Num() == 1)
	{
		TestEqual(TEXT("The listener factory uses its listener sensor"),
			ListenerApplied.Sensors[0].Type,
			EOpenMobileSensorType::Accelerometer);
	}
	const FOpenMobileSensorRecordingOptions SafeRawDefaults;
	TestEqual(TEXT("Raw recording options fit every valid duration policy"),
		SafeRawDefaults.MaximumDurationSeconds, 1.0);
	TestEqual(TEXT("Raw recording options fit every valid size policy"),
		SafeRawDefaults.MaximumBytes, 1024ll * 1024);

	Settings->MaximumRecordingDurationSeconds = PreviousDuration;
	Settings->MaximumRecordingBytes = PreviousBytes;
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
	FOpenMobileSensorSubscriptionRequest ActiveRequest;
	ActiveRequest.Sensor.Type = EOpenMobileSensorType::Accelerometer;
	const FOpenMobileSensorSubscriptionResult ActiveSubscription =
		Subsystem->StartSubscriptionNative(ActiveRequest);
	TestTrue(TEXT("The active-sensor fixture is accepted"),
		ActiveSubscription.Operation.IsSuccess());

	UOpenMobileSensorRecordingSession* Session =
		UOpenMobileSensorRecordingSession::StartRecordingActiveSensors(
			World,
			World);
	Session->Activate();
	TestEqual(TEXT("The active-sensor factory selects the running sensor"),
		Session->GetAppliedRecordingOptions().Sensors.Num(), 1);
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
	TestTrue(TEXT("The active-sensor fixture stops cleanly"),
		Subsystem->StopSubscriptionNative(
			ActiveSubscription.Handle).IsSuccess());

	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingEligibilityTest,
	"OpenMobile.Sensors.Blueprint.RecordingSession.Eligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingEligibilityTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileSensorsRecordingSessionTestsPrivate;
	ResetServices();
	FOpenMobileSensorsRecordingService::Start();
	FOpenMobileSensorRecordingOptions Options = MakeOptions(1.0);
	FOpenMobileSensorIdentifier& Attitude = Options.Sensors.AddDefaulted_GetRef();
	Attitude.Type = EOpenMobileSensorType::Attitude;
	Attitude.InstanceId = TEXT("Default");
	bool bCompleted = false;
	FOpenMobileSensorRecordingResult Result;
	FOpenMobileSensorsRecordingService::StartRecording(FGuid::NewGuid(), Options,
		[&](const FOpenMobileSensorRecordingResult& InResult)
		{
			Result = InResult;
			bCompleted = true;
		});
	TestTrue(TEXT("Mixed selection fails before opening any file"), bCompleted);
	TestFalse(TEXT("Unsupported families are rejected"), Result.Operation.IsSuccess());
	TestTrue(TEXT("Failure identifies the unsupported sensor"),
		Result.Operation.Error.Message.Contains(TEXT("Attitude")));
	TestTrue(TEXT("Failure explains the supported family"),
		Result.Operation.Error.Message.Contains(TEXT("vector")));
	const UFunction* Query = UOpenMobileSensorBlueprintLibrary::StaticClass()->
		FindFunctionByName(TEXT("IsSensorRecordable"));
	TestNotNull(TEXT("Blueprints can check recording eligibility"), Query);
	ResetServices();
	return true;
}

#endif
