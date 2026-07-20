#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorQuality.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsDiagnosticsOutput.h"
#include "OpenMobileSensorsDiagnosticsService.h"
#include "OpenMobileSensorsEditorMockBackend.h"
#include "OpenMobileSensorsMetadataService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDiagnosticsTest,
	"OpenMobile.Sensors.Diagnostics.Panel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDiagnosticsTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsMetadataService::ResetForTests();

	FOpenMobileSensorsEditorMockBackend Backend;
	Backend.Activate();
	TestTrue(TEXT("Diagnostics backend registers"),
		FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend));
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
	Request.Sensor.InstanceId = TEXT("Default");
	Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
	Request.Options.CustomFrequencyHz = 120.0;
	Request.Options.MaximumCallbackFrequencyHz = 60.0;
	const FOpenMobileSensorSubscriptionResult First =
		Subsystem->StartSubscriptionNative(Request);
	const FOpenMobileSensorSubscriptionResult Second =
		Subsystem->StartSubscriptionNative(Request);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsMockInput Input;
	Input.AccelerationMetresPerSecondSquared = FVector(1.0, 2.0, 9.0);
	TestTrue(TEXT("Live mock sample publishes"),
		Backend.ApplyInput(Input).IsSuccess());

	FOpenMobileSensorDiagnosticsSnapshot Snapshot =
		Subsystem->GetDiagnosticsSnapshotNative();
	TestFalse(TEXT("Diagnostics capture time is present"),
		Snapshot.CapturedAtUtc.IsEmpty());
	TestEqual(TEXT("Diagnostics report the active backend"),
		Snapshot.BackendName, Backend.GetBackendName());
	TestEqual(TEXT("Two logical streams are visible"),
		Snapshot.Streams.Num(), 2);
	TestEqual(TEXT("Shared subscriptions have one physical stream"),
		Snapshot.PhysicalStreams.Num(), 1);
	if (Snapshot.PhysicalStreams.Num() == 1)
	{
		TestEqual(TEXT("Physical sharing count is visible"),
			Snapshot.PhysicalStreams[0].SubscriberCount, 2);
		TestEqual(TEXT("Physical applied rate is visible"),
			Snapshot.PhysicalStreams[0].AppliedFrequencyHz, 120.0);
	}
	for (const FOpenMobileSensorStreamDiagnostics& Stream : Snapshot.Streams)
	{
		TestTrue(TEXT("Latest sample presence is visible"), Stream.bHasSample);
		TestTrue(TEXT("Mock source is visible"),
			(Stream.SourceFlags & static_cast<int32>(
				EOpenMobileSensorSourceFlags::Mock)) != 0);
		TestTrue(TEXT("Latest accuracy is visible"), Stream.bHasAccuracy);
	}
	TestEqual(TEXT("All sensor permissions are visible"),
		Snapshot.Permissions.Num(), 3);

	const double CaptureStartSeconds = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < 32; ++Index)
	{
		FOpenMobileSensorsDiagnosticsService::Capture();
	}
	TestTrue(TEXT("Bounded refresh capture stays inexpensive"),
		FPlatformTime::Seconds() - CaptureStartSeconds < 1.0);

	Snapshot.RecentErrors.Add(FOpenMobileError::Make(
		EOpenMobileErrorCode::NativeFailure,
		TEXT("/Users/private/sensor.log?latitude=1"),
		TEXT("secret-native-code"),
		TEXT("/Users/private/provider")
	));
	FString Json;
	TestTrue(TEXT("Redacted JSON serializes"),
		FOpenMobileSensorsDiagnosticsOutput::Serialize(
			Snapshot,
			Json
		).IsSuccess());
	TestTrue(TEXT("Export labels redacted content"),
		Json.Contains(TEXT("<redacted>")));
	TestFalse(TEXT("Export omits native codes"),
		Json.Contains(TEXT("secret-native-code")));
	TestFalse(TEXT("Export omits private paths"),
		Json.Contains(TEXT("/Users/private")));

	TestTrue(TEXT("Injected stream error is accepted"),
		Backend.InjectError(
			EOpenMobileSensorType::Accelerometer,
			EOpenMobileSensorFailureReason::OperationalFailure,
			TEXT("MockDiagnosticsFailure")
		).IsSuccess());
	const FOpenMobileSensorDiagnosticsSnapshot FailedSnapshot =
		Subsystem->GetDiagnosticsSnapshotNative();
	TestFalse(TEXT("Recent stream errors remain visible"),
		FailedSnapshot.RecentErrors.IsEmpty());

	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	const FOpenMobileSensorDiagnosticsSnapshot LostSnapshot =
		FOpenMobileSensorsDiagnosticsService::Capture();
	TestTrue(TEXT("Backend loss clears physical streams"),
		LostSnapshot.PhysicalStreams.IsEmpty());
	TestTrue(TEXT("Backend loss is reflected"),
		LostSnapshot.BackendName.IsNone());

	static_cast<void>(First);
	static_cast<void>(Second);
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsMetadataService::ResetForTests();
	return true;
}

#endif
