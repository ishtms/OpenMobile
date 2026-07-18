#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileSensorRecordingCodec.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsRecordingService.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace OpenMobileSensorsRecordingTestsPrivate
{
	FOpenMobileSensorRecordingDocument MakeDocument()
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
		Stream.Capability.MinimumFrequencyHz = 1.0;
		Stream.Capability.MaximumFrequencyHz = 200.0;
		Stream.Capability.bSupportsNativeBatching = true;

		FOpenMobileVectorSensorBatch& Batch =
			Document.VectorBatches.AddDefaulted_GetRef();
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FOpenMobileVectorSensorSample& Sample =
				Batch.Samples.AddDefaulted_GetRef();
			Sample.Header.Sensor = Stream.Sensor;
			Sample.Header.TimestampSeconds = 10.0 + Index * 0.01;
			Sample.Header.Sequence = Index + 7;
			Sample.Header.bUnitsNormalized = true;
			Sample.Header.bCoordinatesNormalized = true;
			Sample.Header.bValid = true;
			Sample.Header.Accuracy = EOpenMobileSensorAccuracy::High;
			Sample.Header.SourceFlags = static_cast<int32>(
				EOpenMobileSensorSourceFlags::CalibratedNative
			);
			Sample.Header.Fusion.Quality =
				EOpenMobileSensorFusionQuality::Nominal;
			Sample.Header.Fusion.bHasEstimatedLag = true;
			Sample.Header.Fusion.EstimatedLagSeconds = 0.5;
			Sample.Value = FVector(1.0 + Index, 2.0, 9.81);
		}
		return Document;
	}

	bool WaitUntil(TFunctionRef<bool()> Predicate)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (!Predicate() && FPlatformTime::Seconds() < Deadline)
		{
			FOpenMobileSensorsRecordingService::TickForTests(
				FPlatformTime::Seconds());
			FOpenMobileSensorsSubscriptionService::
				ProcessPendingBackendOperationsForTests();
			FOpenMobileSensorsSampleService::DrainPendingEventsForTests(
				FPlatformTime::Seconds());
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

	void FinishServices(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsRecordingService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingCodecRoundTripTest,
	"OpenMobile.Sensors.Accelerometer.Recording.CodecRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingCodecRoundTripTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	FOpenMobileSensorRecordingDocument Source = MakeDocument();
	Source.Header.bHasSensitiveLocationContext = true;
	Source.Header.SensitiveLocationContext.LatitudeDegrees = 51.5;
	Source.Header.SensitiveLocationContext.LongitudeDegrees = -0.12;
	Source.Header.SensitiveLocationContext.AltitudeMeters = 18.0;
	Source.Header.SensitiveLocationContext.HorizontalAccuracyMeters = 3.0;
	Source.Header.SensitiveLocationContext.TimestampSeconds = 1700000000.0;
	FOpenMobileSensorRecordingStreamDescriptor& GyroscopeStream =
		Source.Header.Streams.AddDefaulted_GetRef();
	GyroscopeStream.Sensor.Type = EOpenMobileSensorType::Gyroscope;
	GyroscopeStream.Sensor.InstanceId = TEXT("Default");
	GyroscopeStream.Family = EOpenMobileSensorSampleFamily::Vector;
	GyroscopeStream.Units = TEXT("rad/s");
	GyroscopeStream.Capability.Sensor = GyroscopeStream.Sensor;
	GyroscopeStream.Capability.MinimumFrequencyHz = 1.0;
	GyroscopeStream.Capability.MaximumFrequencyHz = 200.0;
	FOpenMobileVectorSensorBatch& GyroscopeBatch =
		Source.VectorBatches.AddDefaulted_GetRef();
	FOpenMobileVectorSensorSample& GyroscopeSample =
		GyroscopeBatch.Samples.AddDefaulted_GetRef();
	GyroscopeSample = Source.VectorBatches[0].Samples[0];
	GyroscopeSample.Header.Sensor = GyroscopeStream.Sensor;
	GyroscopeSample.Header.TimestampSeconds = 10.005;
	GyroscopeSample.Value = FVector(0.1, 0.2, 0.3);
	TArray<uint8> Bytes;
	FString Error;
	TestTrue(TEXT("A complete recording can be encoded"),
		FOpenMobileSensorRecordingCodec::EncodeComplete(
			Source, Bytes, Error));
	TestTrue(TEXT("The recording contains data"), !Bytes.IsEmpty());

	FOpenMobileSensorRecordingDocument Decoded;
	EOpenMobileSensorRecordingDecodeStatus Status =
		EOpenMobileSensorRecordingDecodeStatus::InvalidData;
	TestTrue(TEXT("The encoded recording can be decoded"),
		FOpenMobileSensorRecordingCodec::DecodeComplete(
			Bytes, Decoded, Status, Error));
	TestEqual(TEXT("The format version is retained"),
		Decoded.Header.FormatVersion,
		FOpenMobileSensorRecordingCodec::CurrentFormatVersion);
	TestEqual(TEXT("The plugin version is retained"),
		Decoded.Header.PluginVersion, Source.Header.PluginVersion);
	TestEqual(TEXT("Mixed stream descriptors are retained"),
		Decoded.Header.Streams.Num(), 2);
	if (Decoded.Header.Streams.Num() == 2)
	{
		TestEqual(TEXT("The stream capability is retained"),
			Decoded.Header.Streams[0].Capability.MaximumFrequencyHz, 200.0);
		TestEqual(TEXT("The second stream units are retained"),
			Decoded.Header.Streams[1].Units, FString(TEXT("rad/s")));
	}
	TestTrue(TEXT("Explicit sensitive context is retained"),
		Decoded.Header.bHasSensitiveLocationContext);
	TestEqual(TEXT("Sensitive latitude is retained"),
		Decoded.Header.SensitiveLocationContext.LatitudeDegrees, 51.5);
	TestEqual(TEXT("Mixed sample batches are retained"),
		Decoded.VectorBatches.Num(), 2);
	TestEqual(TEXT("Every sample is retained"),
		Decoded.VectorBatches[0].Samples.Num(), 2);
	if (Decoded.VectorBatches.Num() == 1
		&& Decoded.VectorBatches[0].Samples.Num() == 2)
	{
		const FOpenMobileVectorSensorSample& Sample =
			Decoded.VectorBatches[0].Samples[1];
		TestEqual(TEXT("The timestamp is retained"),
			Sample.Header.TimestampSeconds, 10.01);
		TestEqual(TEXT("The source flags are retained"),
			Sample.Header.SourceFlags,
			static_cast<int32>(
				EOpenMobileSensorSourceFlags::CalibratedNative));
		TestTrue(TEXT("The estimated lag remains present"),
			Sample.Header.Fusion.bHasEstimatedLag);
		TestEqual(TEXT("The estimated lag is retained"),
			Sample.Header.Fusion.EstimatedLagSeconds, 0.5);
		TestEqual(TEXT("The vector is retained"),
			Sample.Value, FVector(2.0, 2.0, 9.81));
	}
	TArray<uint8> IncompatibleBytes = Bytes;
	IncompatibleBytes[8] = 0xff;
	IncompatibleBytes[9] = 0xff;
	TestFalse(TEXT("An incompatible format version is rejected"),
		FOpenMobileSensorRecordingCodec::DecodeComplete(
			IncompatibleBytes,
			Decoded,
			Status,
			Error
		));
	TestEqual(TEXT("Version failure has a distinct decode status"),
		Status,
		EOpenMobileSensorRecordingDecodeStatus::IncompatibleVersion);
	FOpenMobileSensorRecordingDocument InvalidLag = MakeDocument();
	InvalidLag.VectorBatches[0].Samples[0]
		.Header.Fusion.EstimatedLagSeconds =
		std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("A nonfinite fusion lag cannot be recorded"),
		FOpenMobileSensorRecordingCodec::EncodeComplete(
			InvalidLag,
			Bytes,
			Error
		));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingTruncationTest,
	"OpenMobile.Sensors.Accelerometer.Recording.Truncation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingTruncationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	TArray<uint8> Bytes;
	FString Error;
	FOpenMobileSensorRecordingCodec::EncodeComplete(
		MakeDocument(), Bytes, Error);
	Bytes.SetNum(FMath::Max(0, Bytes.Num() - 5));

	FOpenMobileSensorRecordingDocument Decoded;
	EOpenMobileSensorRecordingDecodeStatus Status =
		EOpenMobileSensorRecordingDecodeStatus::Success;
	TestFalse(TEXT("A recording without its full footer is rejected"),
		FOpenMobileSensorRecordingCodec::DecodeComplete(
			Bytes, Decoded, Status, Error));
	TestEqual(TEXT("Truncation has a distinct decode status"),
		Status,
		EOpenMobileSensorRecordingDecodeStatus::Truncated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingChecksumTest,
	"OpenMobile.Sensors.Accelerometer.Recording.Checksum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingChecksumTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	TArray<uint8> Bytes;
	FString Error;
	FOpenMobileSensorRecordingCodec::EncodeComplete(
		MakeDocument(), Bytes, Error);
	if (Bytes.Num() > 32)
	{
		Bytes[32] ^= 0x40;
	}

	FOpenMobileSensorRecordingDocument Decoded;
	EOpenMobileSensorRecordingDecodeStatus Status =
		EOpenMobileSensorRecordingDecodeStatus::Success;
	TestFalse(TEXT("Corrupted recording data is rejected"),
		FOpenMobileSensorRecordingCodec::DecodeComplete(
			Bytes, Decoded, Status, Error));
	TestEqual(TEXT("Checksum failure has a distinct decode status"),
		Status,
		EOpenMobileSensorRecordingDecodeStatus::ChecksumMismatch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccelerometerRecordingLifecycleTest,
	"OpenMobile.Sensors.Accelerometer.Recording.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccelerometerRecordingLifecycleTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RecordingLifecycle"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	FOpenMobileSensorRecordingOptions Options;
	FOpenMobileSensorIdentifier& Sensor =
		Options.Sensors.AddDefaulted_GetRef();
	Sensor.Type = EOpenMobileSensorType::Accelerometer;
	Sensor.InstanceId = TEXT("Default");
	Options.MaximumDurationSeconds = 30.0;
	Options.MaximumBytes = 1024ll * 1024;
	FOpenMobileSensorSubscriptionRequest PublicRequest;
	PublicRequest.Sensor = Sensor;
	PublicRequest.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
	PublicRequest.Options.CustomFrequencyHz = 60.0;
	PublicRequest.Options.MaximumCallbackFrequencyHz = 60.0;
	PublicRequest.Options.DeliveryMode =
		EOpenMobileSensorDeliveryMode::EventBatches;
	const FOpenMobileSensorSubscriptionResult PublicSubscription =
		Subsystem->StartSubscriptionNative(PublicRequest);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	int32 PublicBatchCount = 0;
	FOpenMobileSensorSubscriptionHandle PublicCallbackHandle;
	Subsystem->OnVectorSamplesNative().AddLambda(
		[&](
			FOpenMobileSensorSubscriptionHandle Handle,
			const FOpenMobileVectorSensorBatch&)
		{
			++PublicBatchCount;
			PublicCallbackHandle = Handle;
		});

	bool bStarted = false;
	FOpenMobileSensorRecordingResult StartResult;
	const FGuid RequestId =
		Subsystem->StartRecordingNative(
			Options,
			FOnOpenMobileSensorRecordingComplete::CreateLambda(
				[&](const FOpenMobileSensorRecordingResult& Result)
				{
					StartResult = Result;
					bStarted = true;
				})
		);
	TestTrue(TEXT("The recording request has an identifier"),
		RequestId.IsValid());
	TestTrue(TEXT("Recording initialization completes"),
		WaitUntil([&]() { return bStarted; }));
	TestTrue(TEXT("Recording starts successfully"),
		StartResult.Operation.IsSuccess());
	TestEqual(TEXT("The recording enters the recording state"),
		StartResult.Recording.State,
		EOpenMobileSensorRecordingState::Recording);
	TestEqual(TEXT("Recording shares the existing physical stream"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(), 1);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	EOpenMobileSensorRecordingState RecordingState =
		EOpenMobileSensorRecordingState::Idle;
	TestTrue(
		TEXT("A suspended recording remains queryable"),
		FOpenMobileSensorsRecordingService::GetRecordingStateForTests(
			RequestId,
			RecordingState
		)
	);
	TestEqual(
		TEXT("The default lifecycle keeps recording state across a gap"),
		RecordingState,
		EOpenMobileSensorRecordingState::Recording
	);
	TestEqual(
		TEXT("Suspending every owner releases the shared native stream"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		0
	);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(
		TEXT("Resume restores one renegotiated shared native stream"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		1
	);

	FOpenMobileVectorSensorBatch Batch;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FOpenMobileVectorSensorSample& Sample =
			Batch.Samples.AddDefaulted_GetRef();
		Sample.Header.Sensor = Sensor;
		Sample.Header.TimestampSeconds = 20.0 + Index * 0.01;
		Sample.Header.bValid = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative);
		Sample.Value = FVector(Index + 1.0, 0.0, 9.81);
	}
	TestTrue(TEXT("The common service accepts the accelerometer batch"),
		FOpenMobileSensorsSampleService::PublishVectorBatch(Batch));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(
		FPlatformTime::Seconds());
	TestEqual(TEXT("The hidden recording stream is not broadcast publicly"),
		PublicBatchCount, 1);
	TestTrue(TEXT("The public callback keeps the caller's handle"),
		PublicCallbackHandle == PublicSubscription.Handle);

	bool bStopped = false;
	FOpenMobileSensorRecordingResult StopResult;
	Subsystem->StopRecordingNative(
		RequestId,
		FOnOpenMobileSensorRecordingComplete::CreateLambda(
			[&](const FOpenMobileSensorRecordingResult& Result)
			{
				StopResult = Result;
				bStopped = true;
			})
	);
	TestTrue(TEXT("Recording finalization completes"),
		WaitUntil([&]() { return bStopped; }));
	TestTrue(TEXT("Recording stops successfully"),
		StopResult.Operation.IsSuccess());
	TestEqual(TEXT("The recording enters the completed state"),
		StopResult.Recording.State,
		EOpenMobileSensorRecordingState::Completed);
	TestEqual(TEXT("Only the public stream remains after recording"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(), 1);

	TArray<uint8> Bytes;
	TestTrue(TEXT("The finalized recording exists"),
		FFileHelper::LoadFileToArray(
			Bytes, *StopResult.Recording.FilePath));
	FOpenMobileSensorRecordingDocument Decoded;
	EOpenMobileSensorRecordingDecodeStatus Status =
		EOpenMobileSensorRecordingDecodeStatus::InvalidData;
	FString Error;
	TestTrue(TEXT("The finalized recording is valid"),
		FOpenMobileSensorRecordingCodec::DecodeComplete(
			Bytes, Decoded, Status, Error));
	TestEqual(TEXT("The recording uses the plugin descriptor version"),
		Decoded.Header.PluginVersion, FString(TEXT("0.1.0")));
	TestEqual(TEXT("Both accelerometer samples were recorded"),
		Decoded.Footer.SampleCount, 2ll);
	if (Decoded.VectorBatches.Num() == 1
		&& Decoded.VectorBatches[0].Samples.Num() == 2)
	{
		TestEqual(TEXT("Recording uses normalized device coordinates"),
			Decoded.VectorBatches[0].Samples[1].Header.CoordinateSpace,
			EOpenMobileSensorCoordinateSpace::DeviceFixed);
		TestTrue(TEXT("Recording preserves normalized units"),
			Decoded.VectorBatches[0].Samples[1]
				.Header.bUnitsNormalized);
	}
	IFileManager::Get().Delete(*StopResult.Recording.FilePath);
	Subsystem->StopSubscriptionNative(PublicSubscription.Handle);
	Subsystem->Deinitialize();
	FinishServices(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccelerometerReplayLifecycleTest,
	"OpenMobile.Sensors.Accelerometer.Replay.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccelerometerReplayLifecycleTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ReplayLifecycle"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
	Request.Sensor.InstanceId = TEXT("Default");
	Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
	Request.Options.CustomFrequencyHz = 100.0;
	Request.Options.MaximumCallbackFrequencyHz = 100.0;
	Request.Options.DeliveryMode = EOpenMobileSensorDeliveryMode::Buffered;
	Request.Options.BufferCapacitySamples = 16;
	const FOpenMobileSensorSubscriptionResult Subscription =
		Subsystem->StartSubscriptionNative(Request);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("The replay consumer starts"),
		Subscription.Operation.IsSuccess());

	TArray<uint8> Bytes;
	FString Error;
	FOpenMobileSensorRecordingCodec::EncodeComplete(
		MakeDocument(), Bytes, Error);
	const FString FilePath = FPaths::CreateTempFilename(
		*FPaths::ProjectSavedDir(), TEXT("omsensors-replay-"), TEXT(".bin"));
	TestTrue(TEXT("The replay fixture is written"),
		FFileHelper::SaveArrayToFile(Bytes, *FilePath));

	FOpenMobileSensorReplayOptions Options;
	Options.PlaybackSpeed = 2.0;
	Options.bLoop = true;
	Options.bStartPaused = true;
	Options.ClockMode = EOpenMobileSensorReplayClockMode::Manual;
	bool bCompleted = false;
	FOpenMobileSensorReplayResult ReplayResult;
	const FGuid ReplayId = Subsystem->ReplayRecordingNative(
		FilePath,
		Options,
		FOnOpenMobileSensorReplayComplete::CreateLambda(
			[&](const FOpenMobileSensorReplayResult& Result)
			{
				ReplayResult = Result;
				bCompleted = true;
			})
	);
	TestTrue(TEXT("The replay request has an identifier"),
		ReplayId.IsValid());
	FOpenMobileSensorReplaySnapshot ReplaySnapshot;
	TestTrue(TEXT("The manual replay loads without advancing"),
		WaitUntil([&]()
		{
			return Subsystem->GetReplayStateNative(
				ReplayId,
				ReplaySnapshot
			) && ReplaySnapshot.State ==
				EOpenMobileSensorReplayState::Paused;
		}));
	TestFalse(TEXT("A paused replay has no terminal result"), bCompleted);
	TestTrue(TEXT("A paused replay accepts a seek"),
		Subsystem->SeekReplayNative(ReplayId, 0.0).IsSuccess());
	TestTrue(TEXT("A paused replay accepts a speed change"),
		Subsystem->SetReplaySpeedNative(ReplayId, 2.0).IsSuccess());
	TestTrue(TEXT("A paused replay resumes"),
		Subsystem->ResumeReplayNative(ReplayId).IsSuccess());
	TestTrue(TEXT("A running replay pauses again"),
		Subsystem->PauseReplayNative(ReplayId).IsSuccess());
	TestTrue(TEXT("The paused replay resumes again"),
		Subsystem->ResumeReplayNative(ReplayId).IsSuccess());
	TestTrue(TEXT("Manual time advances deterministically"),
		Subsystem->AdvanceReplayNative(ReplayId, 0.005).IsSuccess());
	TestFalse(TEXT("A looping replay stays active"), bCompleted);
	TestTrue(TEXT("Looping can be disabled while replay is active"),
		Subsystem->SetReplayLoopingNative(ReplayId, false).IsSuccess());
	TestTrue(TEXT("The replay can seek into its next pass"),
		Subsystem->SeekReplayNative(ReplayId, 0.0).IsSuccess());
	TestTrue(TEXT("The final non-looping pass advances"),
		Subsystem->AdvanceReplayNative(ReplayId, 0.005).IsSuccess());
	TestTrue(TEXT("Manual replay completes at the final sample"), bCompleted);
	TestTrue(TEXT("Replay succeeds"), ReplayResult.Operation.IsSuccess());
	TestEqual(TEXT("Replay reports a valid decoded file"),
		ReplayResult.FileStatus,
		EOpenMobileSensorRecordingDecodeStatus::Success);

	FOpenMobileSensorBufferReadResult ReadResult;
	FOpenMobileVectorSensorBatch Replayed;
	Subsystem->GetBufferedVectorSamplesNative(
		Subscription.Handle,
		16,
		ReadResult,
		Replayed);
	TestEqual(TEXT("Both replay passes publish every sample"),
		Replayed.Samples.Num(), 4);
	if (Replayed.Samples.Num() == 4)
	{
		TestTrue(TEXT("Replay is visibly tagged"),
			(Replayed.Samples[0].Header.SourceFlags
				& static_cast<int32>(
					EOpenMobileSensorSourceFlags::Replay)) != 0);
		TestTrue(TEXT("Original source provenance is preserved"),
			(Replayed.Samples[0].Header.SourceFlags
				& static_cast<int32>(
					EOpenMobileSensorSourceFlags::CalibratedNative)) != 0);
		TestTrue(TEXT("Replay remaps timestamps monotonically"),
			Replayed.Samples[1].Header.TimestampSeconds
				> Replayed.Samples[0].Header.TimestampSeconds);
		TestTrue(TEXT("Playback speed scales sample timing"),
			FMath::IsNearlyEqual(
				Replayed.Samples[1].Header.TimestampSeconds
					- Replayed.Samples[0].Header.TimestampSeconds,
				0.005,
				0.00001));
	}
	Subsystem->StopSubscriptionNative(Subscription.Handle);
	IFileManager::Get().Delete(*FilePath);
	Subsystem->Deinitialize();
	FinishServices(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccelerometerRecordingSizeLimitTest,
	"OpenMobile.Sensors.Accelerometer.Recording.SizeLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccelerometerRecordingSizeLimitTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RecordingSizeLimit"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorRecordingOptions Options;
	FOpenMobileSensorIdentifier& Sensor =
		Options.Sensors.AddDefaulted_GetRef();
	Sensor.Type = EOpenMobileSensorType::Accelerometer;
	Sensor.InstanceId = TEXT("Default");
	Options.MaximumDurationSeconds = 30.0;
	Options.MaximumBytes = 1024ll * 1024;
	bool bStarted = false;
	const FGuid RequestId =
		FOpenMobileSensorsRecordingService::StartRecording(
			Owner,
			Options,
			[&](const FOpenMobileSensorRecordingResult&)
			{
				bStarted = true;
			}
		);
	TestTrue(TEXT("Bounded recording initialization completes"),
		WaitUntil([&]() { return bStarted; }));

	int32 NextSample = 1;
	for (int32 BatchIndex = 0; BatchIndex < 10; ++BatchIndex)
	{
		FOpenMobileVectorSensorBatch Batch;
		Batch.Samples.Reserve(1000);
		for (int32 Index = 0; Index < 1000; ++Index)
		{
			FOpenMobileVectorSensorSample& Sample =
				Batch.Samples.AddDefaulted_GetRef();
			Sample.Header.Sensor = Sensor;
			Sample.Header.TimestampSeconds = NextSample * 0.001;
			Sample.Header.bValid = true;
			Sample.Header.SourceFlags = static_cast<int32>(
				EOpenMobileSensorSourceFlags::CalibratedNative);
			Sample.Value = FVector(NextSample, 0.0, 9.81);
			++NextSample;
		}
		FOpenMobileSensorsSampleService::PublishVectorBatch(Batch);
		FOpenMobileSensorsSampleService::DrainPendingEventsForTests(
			FPlatformTime::Seconds());
	}

	bool bStopped = false;
	FOpenMobileSensorRecordingResult StopResult;
	FOpenMobileSensorsRecordingService::StopRecording(
		Owner,
		RequestId,
		[&](const FOpenMobileSensorRecordingResult& Result)
		{
			StopResult = Result;
			bStopped = true;
		}
	);
	TestTrue(TEXT("Bounded recording finalization completes"),
		WaitUntil([&]() { return bStopped; }));
	TestTrue(TEXT("The size-limited recording remains valid"),
		StopResult.Operation.IsSuccess());
	TestTrue(TEXT("The final file respects the byte limit"),
		StopResult.Recording.BytesWritten <= Options.MaximumBytes);
	TestTrue(TEXT("Samples beyond the limit are reported as dropped"),
		StopResult.Recording.DroppedSamples > 0);

	TArray<uint8> Bytes;
	FFileHelper::LoadFileToArray(Bytes, *StopResult.Recording.FilePath);
	FOpenMobileSensorRecordingDocument Decoded;
	EOpenMobileSensorRecordingDecodeStatus Status =
		EOpenMobileSensorRecordingDecodeStatus::InvalidData;
	FString Error;
	TestTrue(TEXT("A size-limited recording has a complete footer"),
		FOpenMobileSensorRecordingCodec::DecodeComplete(
			Bytes, Decoded, Status, Error));
	TestEqual(TEXT("The footer retains recording drops"),
		Decoded.Footer.DroppedSamples,
		StopResult.Recording.DroppedSamples);
	IFileManager::Get().Delete(*StopResult.Recording.FilePath);
	FinishServices(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingStopInBackgroundTest,
	"OpenMobile.Sensors.Accelerometer.Recording.StopInBackground",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingStopInBackgroundTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RecordingLifecycleStop"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorRecordingOptions Options;
	FOpenMobileSensorIdentifier& Sensor =
		Options.Sensors.AddDefaulted_GetRef();
	Sensor.Type = EOpenMobileSensorType::Accelerometer;
	Sensor.InstanceId = TEXT("Default");
	Options.MaximumDurationSeconds = 30.0;
	Options.MaximumBytes = 1024ll * 1024;
	Options.LifecyclePolicy =
		EOpenMobileSensorLifecyclePolicy::StopInBackground;
	bool bStarted = false;
	FOpenMobileSensorRecordingResult StartResult;
	const FGuid RequestId =
		FOpenMobileSensorsRecordingService::StartRecording(
			Owner,
			Options,
			[&](const FOpenMobileSensorRecordingResult& Result)
			{
				StartResult = Result;
				bStarted = true;
			}
		);
	TestTrue(
		TEXT("The lifecycle recording starts"),
		WaitUntil([&]() { return bStarted; })
	);
	TestTrue(
		TEXT("The lifecycle recording is active"),
		StartResult.Operation.IsSuccess()
	);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	EOpenMobileSensorRecordingState RecordingState =
		EOpenMobileSensorRecordingState::Idle;
	TestTrue(
		TEXT("The recording remains queryable during finalization"),
		FOpenMobileSensorsRecordingService::GetRecordingStateForTests(
			RequestId,
			RecordingState
		)
	);
	TestTrue(
		TEXT("Stop-in-background begins recording finalization"),
		RecordingState == EOpenMobileSensorRecordingState::Stopping
			|| RecordingState == EOpenMobileSensorRecordingState::Completed
	);
	TestTrue(
		TEXT("Background finalization completes"),
		WaitUntil([&]()
		{
			return FOpenMobileSensorsRecordingService::
				GetRecordingStateForTests(RequestId, RecordingState)
				&& RecordingState ==
					EOpenMobileSensorRecordingState::Completed;
		})
	);

	bool bCollected = false;
	FOpenMobileSensorRecordingResult FinalResult;
	FOpenMobileSensorsRecordingService::StopRecording(
		Owner,
		RequestId,
		[&](const FOpenMobileSensorRecordingResult& Result)
		{
			FinalResult = Result;
			bCollected = true;
		}
	);
	TestTrue(
		TEXT("The finalized result is immediately collectable"),
		bCollected
	);
	TestTrue(
		TEXT("Lifecycle finalization produces a valid recording"),
		FinalResult.Operation.IsSuccess()
	);
	TestEqual(
		TEXT("Lifecycle finalization reports completed state"),
		FinalResult.Recording.State,
		EOpenMobileSensorRecordingState::Completed
	);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	IFileManager::Get().Delete(*FinalResult.Recording.FilePath);
	FinishServices(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingStopWhileStartingInBackgroundTest,
	"OpenMobile.Sensors.Accelerometer.Recording.StopWhileStartingInBackground",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingStopWhileStartingInBackgroundTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RecordingStartLifecycleStop"));
	bool bInterrupted = false;
	Backend.SetStartSensorStreamHookForTests([&]()
	{
		if (!bInterrupted)
		{
			bInterrupted = true;
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
		}
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorRecordingOptions Options;
	FOpenMobileSensorIdentifier& Sensor =
		Options.Sensors.AddDefaulted_GetRef();
	Sensor.Type = EOpenMobileSensorType::Accelerometer;
	Sensor.InstanceId = TEXT("Default");
	Options.MaximumDurationSeconds = 30.0;
	Options.MaximumBytes = 1024ll * 1024;
	Options.LifecyclePolicy =
		EOpenMobileSensorLifecyclePolicy::StopInBackground;
	bool bCompleted = false;
	int32 CompletionCount = 0;
	FOpenMobileSensorRecordingResult Result;
	FOpenMobileSensorsRecordingService::StartRecording(
		FGuid::NewGuid(),
		Options,
		[&](const FOpenMobileSensorRecordingResult& InResult)
		{
			Result = InResult;
			++CompletionCount;
			bCompleted = true;
		}
	);
	TestTrue(
		TEXT("The native start is interrupted"),
		WaitUntil([&]() { return bInterrupted; })
	);
	TestTrue(
		TEXT("The interrupted start completes"),
		WaitUntil([&]() { return bCompleted; })
	);
	TestEqual(
		TEXT("The interrupted start completes once"),
		CompletionCount,
		1
	);
	TestFalse(
		TEXT("The interrupted recording is not accepted"),
		Result.Operation.IsSuccess()
	);
	TestEqual(
		TEXT("The interrupted start reports the lifecycle restriction"),
		Result.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::BackgroundRestricted
	);
	TestEqual(
		TEXT("The raced native stream is released"),
		Backend.GetStopSensorStreamCount(),
		1
	);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	if (!Result.Recording.FilePath.IsEmpty())
	{
		IFileManager::Get().Delete(*Result.Recording.FilePath);
	}
	FinishServices(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAccelerometerRecordingNativeStartFailureTest,
	"OpenMobile.Sensors.Accelerometer.Recording.NativeStartFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAccelerometerRecordingNativeStartFailureTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RecordingStartFailure"));
	Backend.SetStartSensorStreamResult(
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::OperationalFailure));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorRecordingOptions Options;
	FOpenMobileSensorIdentifier& Sensor =
		Options.Sensors.AddDefaulted_GetRef();
	Sensor.Type = EOpenMobileSensorType::Accelerometer;
	Sensor.InstanceId = TEXT("Default");
	Options.MaximumDurationSeconds = 30.0;
	Options.MaximumBytes = 1024ll * 1024;
	bool bCompleted = false;
	FOpenMobileSensorRecordingResult Result;
	FOpenMobileSensorsRecordingService::StartRecording(
		FGuid::NewGuid(),
		Options,
		[&](const FOpenMobileSensorRecordingResult& InResult)
		{
			Result = InResult;
			bCompleted = true;
		});
	TestTrue(TEXT("A failed native start completes"),
		WaitUntil([&]() { return bCompleted; }));
	TestFalse(TEXT("A failed native start is not reported as recording"),
		Result.Operation.IsSuccess());
	TestEqual(TEXT("A failed native start has failed state"),
		Result.Recording.State,
		EOpenMobileSensorRecordingState::Failed);
	TestEqual(TEXT("A failed native start releases its hidden handle"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(), 0);
	TestFalse(TEXT("A failed native start leaves no final file"),
		IFileManager::Get().FileExists(*Result.Recording.FilePath));
	FinishServices(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingShutdownCancellationTest,
	"OpenMobile.Sensors.Accelerometer.Recording.ShutdownCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingShutdownCancellationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRecordingTestsPrivate;
	ResetServices();
	FOpenMobileSensorRecordingOptions RecordingOptions;
	FOpenMobileSensorIdentifier& Sensor =
		RecordingOptions.Sensors.AddDefaulted_GetRef();
	Sensor.Type = EOpenMobileSensorType::Accelerometer;
	Sensor.InstanceId = TEXT("Default");
	RecordingOptions.MaximumDurationSeconds = 30.0;
	RecordingOptions.MaximumBytes = 1024ll * 1024;
	int32 RecordingCompletionCount = 0;
	FOpenMobileSensorRecordingResult RecordingResult;
	FOpenMobileSensorsRecordingService::StartRecording(
		FGuid::NewGuid(),
		RecordingOptions,
		[&](const FOpenMobileSensorRecordingResult& Result)
		{
			++RecordingCompletionCount;
			RecordingResult = Result;
		}
	);

	int32 ReplayCompletionCount = 0;
	FOpenMobileSensorReplayResult ReplayResult;
	FOpenMobileSensorsRecordingService::ReplayRecording(
		FGuid::NewGuid(),
		FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("missing-shutdown-replay.omsensors")
		),
		{},
		[&](const FOpenMobileSensorReplayResult& Result)
		{
			++ReplayCompletionCount;
			ReplayResult = Result;
		}
	);

	FOpenMobileSensorsRecordingService::BeginShutdown();
	TestEqual(TEXT("Shutdown completes a pending recording once"),
		RecordingCompletionCount, 1);
	TestEqual(TEXT("Shutdown cancels the pending recording"),
		RecordingResult.Operation.Code,
		EOpenMobileSensorResultCode::Cancelled);
	TestEqual(TEXT("Shutdown completes a pending replay once"),
		ReplayCompletionCount, 1);
	TestEqual(TEXT("Shutdown cancels the pending replay"),
		ReplayResult.Operation.Code,
		EOpenMobileSensorResultCode::Cancelled);
	FOpenMobileSensorsRecordingService::BeginShutdown();
	TestEqual(TEXT("Repeated shutdown does not repeat recording completion"),
		RecordingCompletionCount, 1);
	TestEqual(TEXT("Repeated shutdown does not repeat replay completion"),
		ReplayCompletionCount, 1);
	FOpenMobileSensorsRecordingService::ResetForTests();
	return true;
}

#endif
