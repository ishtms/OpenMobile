#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorFusionQuality.h"
#include "OpenMobileSensorProvenanceCodec.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsSourceQualityTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Attitude;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		Request.Options.MaximumCallbackFrequencyHz = 60.0;
		Request.Options.DeliveryMode = DeliveryMode;
		Request.Options.BufferCapacitySamples = 16;
		return Request;
	}

	FOpenMobileSensorSubscriptionResult StartActive(
		const FGuid& Owner,
		EOpenMobileSensorDeliveryMode DeliveryMode
	)
	{
		const FOpenMobileSensorSubscriptionResult Result =
			FOpenMobileSensorsSubscriptionService::StartSubscription(
				Owner,
				MakeRequest(DeliveryMode)
			);
		FOpenMobileSensorsSubscriptionService::
			ProcessPendingBackendOperationsForTests();
		return Result;
	}

	FOpenMobileSensorFusionInputObservation MakeInput(
		EOpenMobileSensorType Sensor,
		bool bAvailable,
		bool bValid,
		bool bContributed,
		EOpenMobileSensorAccuracy Accuracy,
		bool bCalibrationRequired = false
	)
	{
		FOpenMobileSensorFusionInputObservation Input;
		Input.Sensor = Sensor;
		Input.bExpected = true;
		Input.bAvailable = bAvailable;
		Input.bValid = bValid;
		Input.bContributed = bContributed;
		Input.Accuracy = Accuracy;
		Input.bCalibrationRequired = bCalibrationRequired;
		return Input;
	}

	FOpenMobileSensorFusionContext MakeNominalPluginFusion()
	{
		const TArray<FOpenMobileSensorFusionInputObservation> Inputs = {
			MakeInput(
				EOpenMobileSensorType::Accelerometer,
				true,
				true,
				true,
				EOpenMobileSensorAccuracy::High
			),
			MakeInput(
				EOpenMobileSensorType::Gyroscope,
				true,
				true,
				true,
				EOpenMobileSensorAccuracy::Unknown
			)
		};
		return FOpenMobileSensorFusionQualityEvaluator::Evaluate(Inputs);
	}

	FOpenMobileAttitudeSensorSample MakeAttitude(
		double TimestampSeconds,
		int32 SourceFlags,
		const FOpenMobileSensorFusionContext& Fusion
	)
	{
		FOpenMobileAttitudeSensorSample Sample;
		Sample.Header.Sensor = MakeRequest().Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Header.SourceFlags = SourceFlags;
		Sample.Header.Fusion = Fusion;
		Sample.Quaternion = FQuat::Identity;
		return Sample;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPlatformSourceMappingsTest,
	"OpenMobile.Sensors.SourceQuality.PlatformSourceMappings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPlatformSourceMappingsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const int32 Raw = static_cast<int32>(EOpenMobileSensorSourceFlags::Raw);
	const int32 Calibrated = static_cast<int32>(
		EOpenMobileSensorSourceFlags::CalibratedNative);
	const int32 NativeFused = static_cast<int32>(
		EOpenMobileSensorSourceFlags::NativeFused);
	TestEqual(TEXT("Android uncalibrated motion is raw"),
		FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
			EOpenMobileSensorType::GyroscopeUncalibrated), Raw);
	TestEqual(TEXT("Android calibrated motion stays identified"),
		FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
			EOpenMobileSensorType::Gyroscope), Calibrated);
	TestEqual(TEXT("Android gravity is native fused"),
		FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
			EOpenMobileSensorType::Gravity), NativeFused);
	TestEqual(TEXT("iOS raw accelerometer is not called calibrated"),
		FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
			EOpenMobileSensorType::Accelerometer), Raw);
	TestEqual(TEXT("iOS device-motion gravity is native fused"),
		FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
			EOpenMobileSensorType::Gravity), NativeFused);
	const int32 TrueHeading =
		FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
			EOpenMobileSensorType::TrueHeading);
	TestTrue(TEXT("True heading is native fused"),
		(TrueHeading & NativeFused) != 0);
	TestTrue(TEXT("True heading declares its north reference"),
		(TrueHeading & static_cast<int32>(
			EOpenMobileSensorSourceFlags::TrueNorthReferenced)) != 0);
	TestTrue(TEXT("A replay overlay preserves a valid native source"),
		FOpenMobileSensorSourcePolicy::ValidateSourceFlags(
			Calibrated
			| static_cast<int32>(EOpenMobileSensorSourceFlags::Replay)));
	TestFalse(TEXT("Raw and plugin-derived are contradictory"),
		FOpenMobileSensorSourcePolicy::ValidateSourceFlags(
			Raw
			| static_cast<int32>(
				EOpenMobileSensorSourceFlags::PluginDerived)));
	TestFalse(TEXT("Two north references are contradictory"),
		FOpenMobileSensorSourcePolicy::ValidateSourceFlags(
			static_cast<int32>(
				EOpenMobileSensorSourceFlags::MagneticNorthReferenced)
			| static_cast<int32>(
				EOpenMobileSensorSourceFlags::TrueNorthReferenced)));
	TestFalse(TEXT("A raw field cannot claim a north reference"),
		FOpenMobileSensorSourcePolicy::ValidateSourceFlags(
			Raw
			| static_cast<int32>(
				EOpenMobileSensorSourceFlags::MagneticNorthReferenced)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMixedQualityInputsTest,
	"OpenMobile.Sensors.SourceQuality.MixedQualityInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMixedQualityInputsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSourceQualityTestsPrivate;
	TArray<FOpenMobileSensorFusionInputObservation> Inputs = {
		MakeInput(
			EOpenMobileSensorType::Accelerometer,
			true,
			true,
			true,
			EOpenMobileSensorAccuracy::High
		),
		MakeInput(
			EOpenMobileSensorType::Gyroscope,
			true,
			true,
			true,
			EOpenMobileSensorAccuracy::Low
		),
		MakeInput(
			EOpenMobileSensorType::Magnetometer,
			false,
			false,
			false,
			EOpenMobileSensorAccuracy::Unknown
		)
	};
	const FOpenMobileSensorFusionContext Mixed =
		FOpenMobileSensorFusionQualityEvaluator::Evaluate(Inputs);
	TestEqual(TEXT("A mixed input set is degraded"), Mixed.Quality,
		EOpenMobileSensorFusionQuality::Degraded);
	TestTrue(TEXT("The valid low-quality gyro still contributed"),
		UOpenMobileSensorQualityLibrary::ContainsSensor(
			Mixed.ContributingInputMask,
			EOpenMobileSensorType::Gyroscope));
	TestTrue(TEXT("The low-quality gyro is explicitly degraded"),
		UOpenMobileSensorQualityLibrary::ContainsSensor(
			Mixed.DegradedInputMask,
			EOpenMobileSensorType::Gyroscope));
	TestTrue(TEXT("The absent magnetometer is explicitly missing"),
		UOpenMobileSensorQualityLibrary::ContainsSensor(
			Mixed.MissingInputMask,
			EOpenMobileSensorType::Magnetometer));
	const TArray<EOpenMobileSensorType> MissingInputs =
		UOpenMobileSensorQualityLibrary::GetMissingInputs(Mixed);
	TestEqual(TEXT("Blueprint inspection returns one missing input"),
		MissingInputs.Num(), 1);
	if (MissingInputs.Num() == 1)
	{
		TestEqual(TEXT("Blueprint inspection identifies the magnetometer"),
			MissingInputs[0], EOpenMobileSensorType::Magnetometer);
	}

	Inputs[1].Accuracy = EOpenMobileSensorAccuracy::Unknown;
	Inputs[2].bAvailable = true;
	Inputs[2].bValid = true;
	Inputs[2].bContributed = true;
	const FOpenMobileSensorFusionContext ObservableNominal =
		FOpenMobileSensorFusionQualityEvaluator::Evaluate(
			Inputs,
			EOpenMobileSensorFusionQuality::Nominal
		);
	TestEqual(TEXT("Valid complete inputs can be nominal"),
		ObservableNominal.Quality,
		EOpenMobileSensorFusionQuality::Nominal);
	TestTrue(TEXT("The native quality report remains explicit"),
		ObservableNominal.bHasNativeQualityReport);
	Inputs[0].bValid = false;
	Inputs[0].Accuracy = EOpenMobileSensorAccuracy::High;
	const FOpenMobileSensorFusionContext InvalidHighAccuracy =
		FOpenMobileSensorFusionQualityEvaluator::Evaluate(Inputs);
	TestEqual(TEXT("High accuracy cannot hide invalid input"),
		InvalidHighAccuracy.Quality,
		EOpenMobileSensorFusionQuality::Degraded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNativeToDerivedFallbackTest,
	"OpenMobile.Sensors.SourceQuality.NativeToDerivedFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNativeToDerivedFallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSourceQualityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("SourceFallback"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription = StartActive(
		Owner,
		EOpenMobileSensorDeliveryMode::LatestValue
	);
	FOpenMobileSensorFusionContext NativeFusion;
	NativeFusion.bHasNativeQualityReport = true;
	NativeFusion.NativeQuality = EOpenMobileSensorFusionQuality::Nominal;
	NativeFusion.Quality = EOpenMobileSensorFusionQuality::Nominal;
	const int32 NativeFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::CalibratedNative)
		| static_cast<int32>(EOpenMobileSensorSourceFlags::NativeFused);
	FOpenMobileSensorsSampleService::PublishAttitude(
		MakeAttitude(1.0, NativeFlags, NativeFusion));
	const int32 InvalidFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::Raw)
		| static_cast<int32>(
			EOpenMobileSensorSourceFlags::PluginDerived);
	FOpenMobileSensorsSampleService::PublishAttitude(
		MakeAttitude(1.5, InvalidFlags, MakeNominalPluginFusion()));
	const int32 DerivedFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::PluginDerived);
	FOpenMobileSensorsSampleService::PublishAttitude(
		MakeAttitude(2.0, DerivedFlags, MakeNominalPluginFusion()));
	FOpenMobileSensorReadResult Read;
	FOpenMobileAttitudeSensorSample Result;
	TestTrue(TEXT("The derived fallback becomes the latest sample"),
		FOpenMobileSensorsSampleService::ReadLatestAttitude(
			Owner, Subscription.Handle, 0, 2.0, Read, Result));
	TestEqual(TEXT("The fallback source is exact"),
		Result.Header.SourceFlags, DerivedFlags);
	TestTrue(TEXT("The native-to-derived discontinuity is visible"),
		Result.Header.bSourceChanged);
	TestTrue(TEXT("A source change resets stateful processing"),
		Result.Header.bStatefulProcessingReset);
	TestEqual(TEXT("The invalid combination consumes no sequence"),
		Result.Header.Sequence, 2ll);
	TestEqual(TEXT("The derived quality uses observable inputs"),
		Result.Header.Fusion.Quality,
		EOpenMobileSensorFusionQuality::Nominal);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSourceTransportPreservationTest,
	"OpenMobile.Sensors.SourceQuality.TransportPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSourceTransportPreservationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSourceQualityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("SourceTransport"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Latest = StartActive(
		Owner, EOpenMobileSensorDeliveryMode::LatestValue);
	const FOpenMobileSensorSubscriptionResult Events = StartActive(
		Owner, EOpenMobileSensorDeliveryMode::EventBatches);
	const FOpenMobileSensorSubscriptionResult Buffered = StartActive(
		Owner, EOpenMobileSensorDeliveryMode::Buffered);
	FOpenMobileAttitudeSensorBatch EventBatch;
	FOpenMobileSensorsSampleService::OnAttitudeBatch().AddLambda(
		[&](
			const FGuid&,
			const FOpenMobileSensorSubscriptionHandle& Handle,
			const FOpenMobileAttitudeSensorBatch& Batch
		)
		{
			if (Handle == Events.Handle)
			{
				EventBatch = Batch;
			}
		}
	);
	const int32 SourceFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::PluginDerived)
		| static_cast<int32>(EOpenMobileSensorSourceFlags::Mock);
	const FOpenMobileSensorFusionContext Fusion = MakeNominalPluginFusion();
	FOpenMobileSensorsSampleService::PublishAttitude(
		MakeAttitude(1.0, SourceFlags, Fusion));
	FOpenMobileSensorReadResult LatestRead;
	FOpenMobileAttitudeSensorSample LatestSample;
	FOpenMobileSensorsSampleService::ReadLatestAttitude(
		Owner, Latest.Handle, 0, 1.0, LatestRead, LatestSample);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.0);
	FOpenMobileSensorBufferReadResult BufferRead;
	FOpenMobileAttitudeSensorBatch BufferedBatch;
	FOpenMobileSensorsSampleService::DrainBufferedAttitude(
		Owner, Buffered.Handle, 8, BufferRead, BufferedBatch);
	TestEqual(TEXT("Latest polling preserves source flags"),
		LatestSample.Header.SourceFlags, SourceFlags);
	TestEqual(TEXT("Event delivery preserves one sample"),
		EventBatch.Samples.Num(), 1);
	TestEqual(TEXT("Buffered delivery preserves one sample"),
		BufferedBatch.Samples.Num(), 1);
	if (EventBatch.Samples.Num() == 1 && BufferedBatch.Samples.Num() == 1)
	{
		TestEqual(TEXT("Events preserve source flags"),
			EventBatch.Samples[0].Header.SourceFlags, SourceFlags);
		TestEqual(TEXT("Buffers preserve source flags"),
			BufferedBatch.Samples[0].Header.SourceFlags, SourceFlags);
		TestEqual(TEXT("Latest polling preserves contributors"),
			LatestSample.Header.Fusion.ContributingInputMask,
			Fusion.ContributingInputMask);
		TestEqual(TEXT("Events preserve degraded inputs"),
			EventBatch.Samples[0].Header.Fusion.DegradedInputMask,
			Fusion.DegradedInputMask);
		TestEqual(TEXT("Buffers preserve expected inputs"),
			BufferedBatch.Samples[0].Header.Fusion.ExpectedInputMask,
			Fusion.ExpectedInputMask);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRecordingReplayRoundTripTest,
	"OpenMobile.Sensors.SourceQuality.RecordingReplayRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRecordingReplayRoundTripTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSourceQualityTestsPrivate;
	FOpenMobileSensorSampleHeader Original;
	Original.SourceFlags = static_cast<int32>(
		EOpenMobileSensorSourceFlags::PluginDerived)
		| static_cast<int32>(EOpenMobileSensorSourceFlags::Mock);
	Original.Fusion = MakeNominalPluginFusion();
	Original.Fusion.bHasEstimatedLag = true;
	Original.Fusion.EstimatedLagSeconds = 0.5;
	TArray<uint8> Encoded;
	TestTrue(TEXT("Recording provenance encodes"),
		FOpenMobileSensorProvenanceCodec::Encode(Original, Encoded));
	FOpenMobileSensorSampleHeader Restored;
	TestTrue(TEXT("Recorded provenance decodes"),
		FOpenMobileSensorProvenanceCodec::Decode(
			Encoded, false, Restored));
	TestEqual(TEXT("Recording preserves exact source flags"),
		Restored.SourceFlags, Original.SourceFlags);
	TestEqual(TEXT("Recording preserves fusion quality"),
		Restored.Fusion.Quality, Original.Fusion.Quality);
	TestEqual(TEXT("Recording preserves contributors"),
		Restored.Fusion.ContributingInputMask,
		Original.Fusion.ContributingInputMask);
	TestTrue(TEXT("Recording preserves estimated lag presence"),
		Restored.Fusion.bHasEstimatedLag);
	TestEqual(TEXT("Recording preserves estimated lag"),
		Restored.Fusion.EstimatedLagSeconds,
		Original.Fusion.EstimatedLagSeconds);
	FOpenMobileSensorSampleHeader Replayed;
	TestTrue(TEXT("Replay provenance decodes"),
		FOpenMobileSensorProvenanceCodec::Decode(
			Encoded, true, Replayed));
	TestTrue(TEXT("Replay preserves the original derived source"),
		(Replayed.SourceFlags
			& static_cast<int32>(
				EOpenMobileSensorSourceFlags::PluginDerived)) != 0);
	TestTrue(TEXT("Replay is visibly tagged"),
		(Replayed.SourceFlags
			& static_cast<int32>(EOpenMobileSensorSourceFlags::Replay)) != 0);
	Encoded.Pop();
	TestFalse(TEXT("Truncated provenance is rejected"),
		FOpenMobileSensorProvenanceCodec::Decode(
			Encoded, false, Restored));
	return true;
}

#endif
