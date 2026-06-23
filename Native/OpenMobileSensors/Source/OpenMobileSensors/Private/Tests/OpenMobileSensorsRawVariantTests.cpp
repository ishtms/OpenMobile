#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorRecordingCodec.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

#include <limits>

namespace OpenMobileSensorsRawVariantTestsPrivate
{
	FOpenMobileSensorRecordingDocument MakeRecording(
		bool bHasBias,
		const FVector& Bias
	)
	{
		FOpenMobileSensorRecordingDocument Document;
		Document.Header.PluginVersion = TEXT("0.1.0");
		Document.Header.PlatformName = TEXT("Test");
		Document.Header.UnitsConvention = TEXT("SI");
		Document.Header.CoordinateConvention = TEXT("Unreal device-fixed");
		FOpenMobileSensorRecordingStreamDescriptor& Stream =
			Document.Header.Streams.AddDefaulted_GetRef();
		Stream.Sensor.Type = EOpenMobileSensorType::GyroscopeUncalibrated;
		Stream.Sensor.InstanceId = TEXT("Default");
		Stream.Family = EOpenMobileSensorSampleFamily::Vector;
		Stream.Units = TEXT("rad/s");
		Stream.Capability.Sensor = Stream.Sensor;
		Stream.Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Stream.Sensor.Type);
		Stream.Capability.Availability.State =
			EOpenMobileCapabilityState::Available;
		FOpenMobileVectorSensorSample& Sample = Document.VectorBatches.
			AddDefaulted_GetRef().Samples.AddDefaulted_GetRef();
		Sample.Header.Sensor = Stream.Sensor;
		Sample.Header.TimestampSeconds = 1.0;
		Sample.Header.bValid = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Raw
		);
		Sample.Value = FVector(1.0, 2.0, 3.0);
		Sample.bHasBias = bHasBias;
		Sample.Bias = Bias;
		return Document;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorType Type,
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = Type;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 100.0;
		Request.Options.DeliveryMode = DeliveryMode;
		Request.Options.BufferCapacitySamples = 8;
		return Request;
	}

	FOpenMobileSensorCapability MakeCapability(EOpenMobileSensorType Type)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = MakeRequest(Type).Sensor;
		Capability.Availability.Name = FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 200.0;
		return Capability;
	}

	const FOpenMobileSensorCapability* FindCapability(
		const FOpenMobileSensorCapabilitySnapshot& Snapshot,
		EOpenMobileSensorType Type
	)
	{
		return Snapshot.Sensors.FindByPredicate(
			[Type](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type == Type;
			}
		);
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
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
	FOpenMobileSensorsRawVariantSourceLabelsTest,
	"OpenMobile.Sensors.RawVariants.SourceLabels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRawVariantSourceLabelsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const int32 Raw = static_cast<int32>(EOpenMobileSensorSourceFlags::Raw);
	const int32 Calibrated = static_cast<int32>(
		EOpenMobileSensorSourceFlags::CalibratedNative
	);
	for (const EOpenMobileSensorType Type : {
		EOpenMobileSensorType::AccelerometerUncalibrated,
		EOpenMobileSensorType::GyroscopeUncalibrated,
		EOpenMobileSensorType::MagnetometerUncalibrated})
	{
		TestEqual(TEXT("Android uncalibrated variants are raw"),
			FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(Type),
			Raw);
	}
	for (const EOpenMobileSensorType Type : {
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorType::Gyroscope,
		EOpenMobileSensorType::Magnetometer})
	{
		TestEqual(TEXT("Android calibrated variants remain distinct"),
			FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(Type),
			Calibrated);
	}
	for (const EOpenMobileSensorType Type : {
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorType::Gyroscope,
		EOpenMobileSensorType::MagnetometerUncalibrated})
	{
		TestEqual(TEXT("Direct Core Motion streams are raw"),
			FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(Type),
			Raw);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRawVariantCapabilitySeparationTest,
	"OpenMobile.Sensors.RawVariants.CapabilitySeparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRawVariantCapabilitySeparationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRawVariantTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RawCapabilities"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Gyroscope),
		MakeCapability(EOpenMobileSensorType::MagnetometerUncalibrated)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* CalibratedGyroscope = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Gyroscope
	);
	const FOpenMobileSensorCapability* RawGyroscope = FindCapability(
		Snapshot,
		EOpenMobileSensorType::GyroscopeUncalibrated
	);
	const FOpenMobileSensorCapability* RawMagnetometer = FindCapability(
		Snapshot,
		EOpenMobileSensorType::MagnetometerUncalibrated
	);
	TestNotNull(TEXT("The matrix retains the calibrated gyro"),
		CalibratedGyroscope);
	TestNotNull(TEXT("The matrix retains the absent raw gyro"),
		RawGyroscope);
	TestNotNull(TEXT("The matrix retains the raw magnetic field"),
		RawMagnetometer);
	if (CalibratedGyroscope && RawGyroscope && RawMagnetometer)
	{
		TestEqual(TEXT("A calibrated gyro can exist without a raw variant"),
			CalibratedGyroscope->Availability.State,
			EOpenMobileCapabilityState::Available);
		TestEqual(TEXT("An absent raw gyro remains unavailable"),
			RawGyroscope->Availability.State,
			EOpenMobileCapabilityState::Unavailable);
		TestEqual(TEXT("An independently discovered raw field is available"),
			RawMagnetometer->Availability.State,
			EOpenMobileCapabilityState::Available);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRawVariantBiasFanoutTest,
	"OpenMobile.Sensors.RawVariants.BiasFanout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRawVariantBiasFanoutTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRawVariantTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RawBiasFanout"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest(
		EOpenMobileSensorType::GyroscopeUncalibrated,
		EOpenMobileSensorDeliveryMode::Buffered
	);
	TestFalse(TEXT("Raw streams do not enable low-pass filtering by default"),
		Request.Options.Filters.bEnableLowPass);
	TestFalse(TEXT("Raw streams do not enable high-pass filtering by default"),
		Request.Options.Filters.bEnableHighPass);
	TestFalse(TEXT("Raw streams do not enable smoothing by default"),
		Request.Options.Filters.bEnableExponentialSmoothing);
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileVectorSensorBatch Published;
	FOpenMobileVectorSensorSample& WithBias =
		Published.Samples.AddDefaulted_GetRef();
	WithBias.Header.Sensor = Request.Sensor;
	WithBias.Header.TimestampSeconds = 1.0;
	WithBias.Header.bValid = true;
	WithBias.Header.SourceFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::Raw
	);
	WithBias.Value = FVector(1.0, 2.0, 3.0);
	WithBias.bHasBias = true;
	WithBias.Bias = FVector(0.1, 0.2, 0.3);
	FOpenMobileVectorSensorSample& WithoutBias =
		Published.Samples.AddDefaulted_GetRef();
	WithoutBias.Header.Sensor = Request.Sensor;
	WithoutBias.Header.TimestampSeconds = 2.0;
	WithoutBias.Header.bValid = true;
	WithoutBias.Header.SourceFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::Raw
	);
	WithoutBias.Value = FVector(4.0, 5.0, 6.0);
	FOpenMobileSensorsSampleService::PublishVectorBatch(Published);
	FOpenMobileSensorBufferReadResult Read;
	FOpenMobileVectorSensorBatch Drained;
	TestTrue(TEXT("Raw variants use normal buffered retrieval"),
		FOpenMobileSensorsSampleService::DrainBufferedVector(
			Owner,
			Subscription.Handle,
			8,
			Read,
			Drained
		));
	TestEqual(TEXT("Both raw samples are retained"),
		Drained.Samples.Num(), 2);
	if (Drained.Samples.Num() == 2)
	{
		TestTrue(TEXT("A supplied native bias remains present"),
			Drained.Samples[0].bHasBias);
		TestEqual(TEXT("A supplied native bias remains exact"),
			Drained.Samples[0].Bias, FVector(0.1, 0.2, 0.3));
		TestFalse(TEXT("An absent native bias remains absent"),
			Drained.Samples[1].bHasBias);
		TestEqual(TEXT("Default filters preserve raw sample values"),
			Drained.Samples[1].Value, FVector(4.0, 5.0, 6.0));
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRawVariantOptionalBiasRecordingTest,
	"OpenMobile.Sensors.RawVariants.OptionalBiasRecording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRawVariantOptionalBiasRecordingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRawVariantTestsPrivate;
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const FOpenMobileSensorRecordingDocument Source = MakeRecording(
		false,
		FVector(NaN, NaN, NaN)
	);
	TArray<uint8> Bytes;
	FString Error;
	TestTrue(TEXT("Absent optional bias does not invalidate recording"),
		FOpenMobileSensorRecordingCodec::EncodeComplete(
			Source,
			Bytes,
			Error
		));
	FOpenMobileSensorRecordingDocument Decoded;
	EOpenMobileSensorRecordingDecodeStatus Status =
		EOpenMobileSensorRecordingDecodeStatus::InvalidData;
	TestTrue(TEXT("The recording with absent bias decodes"),
		FOpenMobileSensorRecordingCodec::DecodeComplete(
			Bytes,
			Decoded,
			Status,
			Error
		));
	if (Decoded.VectorBatches.Num() == 1
		&& Decoded.VectorBatches[0].Samples.Num() == 1)
	{
		const FOpenMobileVectorSensorSample& Sample =
			Decoded.VectorBatches[0].Samples[0];
		TestFalse(TEXT("Absent bias remains absent"), Sample.bHasBias);
		TestEqual(TEXT("Absent bias has canonical storage"),
			Sample.Bias, FVector::ZeroVector);
	}

	const FVector NativeBias(0.1, -0.2, 0.3);
	const FOpenMobileSensorRecordingDocument BiasedSource = MakeRecording(
		true,
		NativeBias
	);
	TestTrue(TEXT("A present native bias can be recorded"),
		FOpenMobileSensorRecordingCodec::EncodeComplete(
			BiasedSource,
			Bytes,
			Error
		));
	TestTrue(TEXT("A recording with native bias decodes"),
		FOpenMobileSensorRecordingCodec::DecodeComplete(
			Bytes,
			Decoded,
			Status,
			Error
		));
	if (Decoded.VectorBatches.Num() == 1
		&& Decoded.VectorBatches[0].Samples.Num() == 1)
	{
		const FOpenMobileVectorSensorSample& Sample =
			Decoded.VectorBatches[0].Samples[0];
		TestTrue(TEXT("Recorded native bias remains present"),
			Sample.bHasBias);
		TestEqual(TEXT("Recorded native bias remains exact"),
			Sample.Bias, NativeBias);
	}
	return true;
}

#endif
