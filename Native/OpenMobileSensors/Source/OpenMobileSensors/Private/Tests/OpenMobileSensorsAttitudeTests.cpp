#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorCoordinates.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsAttitudeTestsPrivate
{
	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 15.0;
		Capability.MaximumFrequencyHz = 200.0;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileAttitudeReferenceFrame ReferenceFrame =
			EOpenMobileAttitudeReferenceFrame::GameRelative
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Attitude;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		Request.Options.AttitudeReferenceFrame = ReferenceFrame;
		return Request;
	}

	FOpenMobileAttitudeSensorSample MakeSample(
		double TimestampSeconds,
		const FQuat& Quaternion,
		EOpenMobileSensorAccuracy Accuracy,
		EOpenMobileAttitudeReferenceFrame ReferenceFrame =
			EOpenMobileAttitudeReferenceFrame::GameRelative,
		bool bCalibrationRequired = false
	)
	{
		FOpenMobileAttitudeSensorSample Sample;
		Sample.Header.Sensor = MakeRequest().Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Header.Accuracy = Accuracy;
		Sample.Header.bCalibrationRequired = bCalibrationRequired;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::NativeFused
		);
		Sample.Quaternion = Quaternion;
		Sample.ReferenceFrame = ReferenceFrame;
		return Sample;
	}

	bool SameRotation(const FQuat& Left, const FQuat& Right)
	{
		return FMath::Abs(Left | Right) >= 1.0 - 1.e-9;
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
	FOpenMobileSensorsAttitudeNativeSelectionTest,
	"OpenMobile.Sensors.Attitude.NativeSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeNativeSelectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend NativeBackend(TEXT("NativeAttitude"));
	NativeBackend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::Gyroscope),
		MakeCapability(EOpenMobileSensorType::Magnetometer),
		MakeCapability(EOpenMobileSensorType::Attitude)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(NativeBackend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Native =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Attitude uses the native fused stream"),
		NativeBackend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::Attitude);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Native.Handle
	);
	FinishBackend(NativeBackend);

	FOpenMobileSensorsMockBackend RawOnlyBackend(TEXT("RawOnlyAttitude"));
	RawOnlyBackend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::Gyroscope),
		MakeCapability(EOpenMobileSensorType::Magnetometer)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(RawOnlyBackend);
	const FOpenMobileSensorSubscriptionResult UnsupportedFallback =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest()
		);
	TestEqual(TEXT("Undocumented plugin fusion is not selected"),
		UnsupportedFallback.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	TestEqual(TEXT("Raw inputs do not start hidden fusion streams"),
		RawOnlyBackend.GetStartSensorStreamCount(), 0);
	FinishBackend(RawOnlyBackend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeNormalizationQualityTest,
	"OpenMobile.Sensors.Attitude.NormalizationQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeNormalizationQualityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	FOpenMobileAttitudeSensorSample Normalized = MakeSample(
		1.0,
		FQuat(0.0, 0.0, 0.0, 2.0),
		EOpenMobileSensorAccuracy::High
	);
	Normalized.Header.bUnitsNormalized = false;
	TestTrue(TEXT("A finite native quaternion normalizes"),
		FOpenMobileSensorUnitConverter::NormalizeAttitudeSample(
			EOpenMobileSensorNativePlatform::Android,
			Normalized
		));
	TestTrue(TEXT("The canonical quaternion has unit length"),
		FMath::IsNearlyEqual(Normalized.Quaternion.SizeSquared(), 1.0));
	const FQuat QuarterTurn(FVector::UpVector, UE_DOUBLE_PI * 0.5);
	const FQuat Negated(
		-QuarterTurn.X,
		-QuarterTurn.Y,
		-QuarterTurn.Z,
		-QuarterTurn.W
	);
	TestTrue(TEXT("Quaternion sign does not change orientation"),
		SameRotation(QuarterTurn, Negated));
	FQuat LastRotation = FQuat::Identity;
	bool bContinuousNormalized = true;
	bool bContinuousWithoutJumps = true;
	for (int32 Degrees = 1; Degrees <= 360; ++Degrees)
	{
		FOpenMobileAttitudeSensorSample Continuous = MakeSample(
			Degrees / 60.0,
			FQuat(
				FVector::UpVector,
				FMath::DegreesToRadians(static_cast<double>(Degrees))
			),
			EOpenMobileSensorAccuracy::High
		);
		Continuous.Header.bUnitsNormalized = false;
		FOpenMobileSensorUnitConverter::NormalizeAttitudeSample(
			EOpenMobileSensorNativePlatform::Android,
			Continuous
		);
		bContinuousNormalized &= FMath::IsNearlyEqual(
				Continuous.Quaternion.SizeSquared(),
				1.0
			);
		bContinuousWithoutJumps &=
			FMath::Abs(LastRotation | Continuous.Quaternion) > 0.99;
		LastRotation = Continuous.Quaternion;
	}
	TestTrue(TEXT("Continuous rotations remain normalized"),
		bContinuousNormalized);
	TestTrue(TEXT("Continuous rotations do not jump orientation"),
		bContinuousWithoutJumps);
	TestTrue(TEXT("A full turn returns to the identity orientation"),
		SameRotation(LastRotation, FQuat::Identity));

	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeQuality"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Attitude)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(EOpenMobileAttitudeReferenceFrame::MagneticNorth)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileAttitudeSensorBatch Batch;
	Batch.Samples.Add(MakeSample(
		1.0,
		QuarterTurn,
		EOpenMobileSensorAccuracy::Low,
		EOpenMobileAttitudeReferenceFrame::MagneticNorth,
		true
	));
	FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		Batch
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileAttitudeSensorSample Output;
	TestTrue(TEXT("A calibrated native sample remains observable"),
		FOpenMobileSensorsSampleService::ReadLatestAttitude(
			Owner,
			Subscription.Handle,
			0,
			1.1,
			Read,
			Output
		));
	TestTrue(TEXT("The delivered quaternion keeps its rotation"),
		SameRotation(Output.Quaternion, QuarterTurn));
	TestEqual(TEXT("Native fusion provenance remains explicit"),
		Output.Header.SourceFlags,
		static_cast<int32>(EOpenMobileSensorSourceFlags::NativeFused));
	TestEqual(TEXT("Magnetic calibration trouble degrades attitude"),
		Output.Header.Fusion.Quality,
		EOpenMobileSensorFusionQuality::Degraded);
	TestTrue(TEXT("Native quality remains distinguishable"),
		Output.Header.Fusion.bHasNativeQualityReport);
	TestEqual(TEXT("No plugin-derived drift is introduced"),
		Output.Header.SourceFlags
			& static_cast<int32>(EOpenMobileSensorSourceFlags::PluginDerived),
		0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeReferenceMismatchTest,
	"OpenMobile.Sensors.Attitude.ReferenceMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeReferenceMismatchTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeFrame"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Attitude)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileAttitudeSensorBatch WrongFrame;
	WrongFrame.Samples.Add(MakeSample(
		1.0,
		FQuat::Identity,
		EOpenMobileSensorAccuracy::High,
		EOpenMobileAttitudeReferenceFrame::MagneticNorth
	));
	FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		WrongFrame
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileAttitudeSensorSample Output;
	TestFalse(TEXT("A mismatched reference frame is not delivered"),
		FOpenMobileSensorsSampleService::ReadLatestAttitude(
			Owner,
			Subscription.Handle,
			0,
			1.1,
			Read,
			Output
		));
	FOpenMobileAttitudeSensorBatch MatchingFrame;
	MatchingFrame.Samples.Add(MakeSample(
		2.0,
		FQuat::Identity,
		EOpenMobileSensorAccuracy::High
	));
	FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		MatchingFrame
	);
	TestTrue(TEXT("A matching reference frame is delivered"),
		FOpenMobileSensorsSampleService::ReadLatestAttitude(
			Owner,
			Subscription.Handle,
			0,
			2.1,
			Read,
			Output
		));
	TestTrue(TEXT("Rejected frame data resets the next sample"),
		Output.Header.bStatefulProcessingReset);
	FinishBackend(Backend);
	return true;
}

#endif
