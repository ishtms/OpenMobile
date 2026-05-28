#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileSensors.h"

class FOpenMobileSensorsConsumerTestsModule final : public IModuleInterface
{
};

IMPLEMENT_MODULE(
	FOpenMobileSensorsConsumerTestsModule,
	OpenMobileSensorsConsumerTests
)

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPublicConsumerCompileTest,
	"OpenMobile.Sensors.API.PublicConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPublicConsumerCompileTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	FOpenMobileSensorIdentifier Identifier;
	FOpenMobileSensorCapability Capability;
	FOpenMobileSensorCapabilitySnapshot Capabilities;
	FOpenMobileSensorMetadata Metadata;
	FOpenMobileSensorStreamOptions Options;
	FOpenMobileSensorSubscriptionRequest Request;
	FOpenMobileSensorSubscriptionHandle Handle;
	FOpenMobileSensorSubscriptionStateSnapshot State;
	FOpenMobileVectorSensorSample Vector;
	FOpenMobileAttitudeSensorSample Attitude;
	FOpenMobileScalarSensorSample Scalar;
	FOpenMobileHeadingSensorSample Heading;
	FOpenMobileStepsSensorSample Steps;
	FOpenMobileActivitySensorSample Activity;
	FOpenMobileOrientationSensorSample Orientation;
	FOpenMobileProximitySensorSample Proximity;
	FOpenMobileSensorPermissionDescriptor Permission;
	FOpenMobileSensorLocationInput LocationInput;
	FOpenMobileSensorDiagnosticsSnapshot Diagnostics;
	FOpenMobileSensorRecordingOptions Recording;
	FOpenMobileSensorReplayOptions Replay;
	FOpenMobileSensorFailureDetails Failure;
	FOpenMobileSensorOperationResult Operation;
	FOpenMobileSensorSubscriptionResult Subscription;
	FOpenMobileSensorReadResult Read;
	FOpenMobileSensorBufferReadResult BufferRead;
	FOpenMobileSensorFlushResult Flush;
	FOpenMobileSensorRecenterResult Recenter;
	static_cast<void>(Identifier);
	static_cast<void>(Capability);
	static_cast<void>(Capabilities);
	static_cast<void>(Metadata);
	static_cast<void>(Options);
	static_cast<void>(Request);
	static_cast<void>(Handle);
	static_cast<void>(State);
	static_cast<void>(Vector);
	static_cast<void>(Attitude);
	static_cast<void>(Scalar);
	static_cast<void>(Heading);
	static_cast<void>(Steps);
	static_cast<void>(Activity);
	static_cast<void>(Orientation);
	static_cast<void>(Proximity);
	static_cast<void>(Permission);
	static_cast<void>(LocationInput);
	static_cast<void>(Diagnostics);
	static_cast<void>(Recording);
	static_cast<void>(Replay);
	static_cast<void>(Failure);
	static_cast<void>(Operation);
	static_cast<void>(Subscription);
	static_cast<void>(Read);
	static_cast<void>(BufferRead);
	static_cast<void>(Flush);
	static_cast<void>(Recenter);

	using FStartSubscription = FOpenMobileSensorSubscriptionResult (
		UOpenMobileSensorsSubsystem::*
	)(const FOpenMobileSensorSubscriptionRequest&);
	using FGetCapabilities = FOpenMobileSensorCapabilitySnapshot (
		UOpenMobileSensorsSubsystem::*
	)() const;
	using FGetLatestVector = bool (UOpenMobileSensorsSubsystem::*)(
		const FOpenMobileSensorSubscriptionHandle&,
		int64,
		FOpenMobileSensorReadResult&,
		FOpenMobileVectorSensorSample&
	) const;
	using FGetBufferedVector = bool (UOpenMobileSensorsSubsystem::*)(
		const FOpenMobileSensorSubscriptionHandle&,
		int32,
		FOpenMobileSensorBufferReadResult&,
		FOpenMobileVectorSensorBatch&
	);

	FStartSubscription StartSubscription =
		&UOpenMobileSensorsSubsystem::StartSubscriptionNative;
	FGetCapabilities GetCapabilities =
		&UOpenMobileSensorsSubsystem::GetCapabilitySnapshotNative;
	FGetLatestVector GetLatestVector =
		&UOpenMobileSensorsSubsystem::GetLatestVectorSampleNative;
	FGetBufferedVector GetBufferedVector =
		&UOpenMobileSensorsSubsystem::GetBufferedVectorSamplesNative;
	TestTrue(TEXT("Subscription API is public"), StartSubscription != nullptr);
	TestTrue(TEXT("Discovery API is public"), GetCapabilities != nullptr);
	TestTrue(TEXT("Latest-value API is public"), GetLatestVector != nullptr);
	TestTrue(TEXT("Buffered API is public"), GetBufferedVector != nullptr);
	TestNotNull(
		TEXT("Sensors subsystem is reflected"),
		UOpenMobileSensorsSubsystem::StaticClass()
	);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	TestNotNull(TEXT("Sensors subsystem can be created by a host"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	const FOpenMobileSensorCapabilitySnapshot UnsupportedCapabilities =
		Subsystem->GetCapabilitySnapshotNative();
	TestEqual(
		TEXT("Editor host reports no native backend"),
		UnsupportedCapabilities.BackendAvailability.State,
		EOpenMobileCapabilityState::NotSupported
	);

	Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
	const FOpenMobileSensorSubscriptionResult UnsupportedSubscription =
		Subsystem->StartSubscriptionNative(Request);
	TestEqual(
		TEXT("Streaming without a backend is explicit"),
		UnsupportedSubscription.Operation.Code,
		EOpenMobileSensorResultCode::NotSupported
	);
	TestEqual(
		TEXT("No backend has a typed sensor reason"),
		UnsupportedSubscription.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::UnsupportedPlatform
	);

	FOpenMobileSensorReadResult UnsupportedRead;
	FOpenMobileVectorSensorSample UnsupportedSample;
	TestFalse(
		TEXT("Invalid latest-value read fails"),
		Subsystem->GetLatestVectorSampleNative(
			Handle,
			0,
			UnsupportedRead,
			UnsupportedSample
		)
	);
	TestEqual(
		TEXT("Invalid latest-value read is typed"),
		UnsupportedRead.Status,
		EOpenMobileSensorReadStatus::InvalidHandle
	);

	int32 FlushCompletionCount = 0;
	FOpenMobileSensorFlushResult FlushResult;
	const FGuid FlushRequestId = Subsystem->FlushNative(
		Handle,
		FOnOpenMobileSensorFlushComplete::CreateLambda(
			[&FlushCompletionCount, &FlushResult](
				const FOpenMobileSensorFlushResult& Result
			)
			{
				++FlushCompletionCount;
				FlushResult = Result;
			}
		)
	);
	TestTrue(TEXT("Flush has request identity"), FlushRequestId.IsValid());
	TestEqual(TEXT("Unsupported flush completes once"), FlushCompletionCount, 1);
	TestEqual(
		TEXT("Unsupported flush has an invalid-handle result"),
		FlushResult.Operation.Code,
		EOpenMobileSensorResultCode::InvalidHandle
	);
	return true;
}

#endif
