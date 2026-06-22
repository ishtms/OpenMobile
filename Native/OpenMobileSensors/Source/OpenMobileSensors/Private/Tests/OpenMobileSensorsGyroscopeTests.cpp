#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorCoordinates.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorValidity.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsGyroscopeTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Gyroscope;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 100.0;
		return Request;
	}

	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type,
		double MaximumFrequencyHz = 400.0
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("Gyroscope");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = MaximumFrequencyHz;
		return Capability;
	}

	FOpenMobileVectorSensorSample MakeSample(
		double TimestampSeconds,
		const FVector& AngularVelocity
	)
	{
		FOpenMobileVectorSensorSample Sample;
		Sample.Header.Sensor = MakeRequest().Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative
		);
		Sample.Value = AngularVelocity;
		return Sample;
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
	FOpenMobileSensorsGyroscopeAngularVelocityTest,
	"OpenMobile.Sensors.Gyroscope.AngularVelocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGyroscopeAngularVelocityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	for (const EOpenMobileSensorNativePlatform Platform : {
		EOpenMobileSensorNativePlatform::Android,
		EOpenMobileSensorNativePlatform::IOS})
	{
		FOpenMobileVectorSensorSample Still;
		Still.Header.Sensor =
			OpenMobileSensorsGyroscopeTestsPrivate::MakeRequest().Sensor;
		Still.Header.bValid = true;
		Still.Value = FVector::ZeroVector;
		FOpenMobileSensorUnitConverter::NormalizeVectorSample(Platform, Still);
		FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
			Platform,
			Still
		);
		TestEqual(TEXT("Stillness remains zero angular velocity"),
			Still.Value, FVector::ZeroVector);

		FOpenMobileVectorSensorSample Rotation;
		Rotation.Header.Sensor = Still.Header.Sensor;
		Rotation.Header.bValid = true;
		Rotation.Value = FVector(2.0, 0.0, 0.0);
		FOpenMobileSensorUnitConverter::NormalizeVectorSample(
			Platform,
			Rotation
		);
		FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
			Platform,
			Rotation
		);
		TestEqual(TEXT("Native positive X rotation maps to device negative Y"),
			Rotation.Value, FVector(0.0, -2.0, 0.0));
		TestTrue(TEXT("Angular velocity reports normalized radians per second"),
			Rotation.Header.bUnitsNormalized);
		TestTrue(TEXT("Angular velocity reports normalized device axes"),
			Rotation.Header.bCoordinatesNormalized);
		TestFalse(TEXT("The standard gyro does not invent a bias"),
			Rotation.bHasBias);
	}
	TestEqual(TEXT("Android calibrated gyro keeps calibrated provenance"),
		FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
			EOpenMobileSensorType::Gyroscope),
		static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative));
	TestEqual(TEXT("Core Motion raw gyro keeps raw provenance"),
		FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
			EOpenMobileSensorType::Gyroscope),
		static_cast<int32>(EOpenMobileSensorSourceFlags::Raw));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGyroscopeSaturationTest,
	"OpenMobile.Sensors.Gyroscope.Saturation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGyroscopeSaturationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGyroscopeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("GyroscopeSaturation"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsSampleService::PublishVector(
		MakeSample(1.0, FVector(9.0, 0.0, 0.0))
	);
	FOpenMobileVectorSensorSample Saturated =
		MakeSample(2.0, FVector(10.01, 0.0, 0.0));
	Saturated.Header.bValid &= FOpenMobileSensorValidity::IsWithinMaximumRange(
		Saturated.Value,
		10.0
	);
	FOpenMobileSensorsSampleService::PublishVector(Saturated);
	FOpenMobileSensorReadResult Read;
	FOpenMobileVectorSensorSample Latest;
	TestTrue(TEXT("The last valid gyro sample remains readable"),
		FOpenMobileSensorsSampleService::ReadLatestVector(
			Owner,
			Subscription.Handle,
			0,
			2.0,
			Read,
			Latest
		));
	TestEqual(TEXT("A saturated sample does not replace the latest value"),
		Latest.Value, FVector(9.0, 0.0, 0.0));
	TestEqual(TEXT("A rejected sample does not advance the sequence"),
		Latest.Header.Sequence, 1ll);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGyroscopeUnavailableTest,
	"OpenMobile.Sensors.Gyroscope.Unavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGyroscopeUnavailableTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGyroscopeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("NoGyroscope"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Accelerometer)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Gyroscope = Snapshot.Sensors.FindByPredicate(
		[](const FOpenMobileSensorCapability& Capability)
		{
			return Capability.Sensor.Type == EOpenMobileSensorType::Gyroscope;
		}
	);
	TestNotNull(TEXT("The capability matrix retains a missing gyroscope"),
		Gyroscope);
	if (Gyroscope)
	{
		TestEqual(TEXT("Missing gyro hardware is unavailable"),
			Gyroscope->Availability.State,
			EOpenMobileCapabilityState::Unavailable);
		TestEqual(TEXT("Missing gyro hardware has a typed restriction"),
			Gyroscope->ActiveRestriction,
			EOpenMobileSensorRestriction::MissingHardware);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGyroscopeDriftDiagnosticsTest,
	"OpenMobile.Sensors.Gyroscope.DriftDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGyroscopeDriftDiagnosticsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGyroscopeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("GyroscopeDrift"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FVector StationaryOffset(0.01, -0.02, 0.005);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FOpenMobileSensorsSampleService::PublishVector(
			MakeSample(1.0 + Index * 0.01, StationaryOffset)
		);
	}
	FOpenMobileSensorStreamDiagnostics Diagnostics;
	TestTrue(TEXT("Gyroscope diagnostics are available"),
		FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
			Owner,
			Subscription.Handle,
			2.0,
			Diagnostics
		));
	TestEqual(TEXT("The bounded drift window counts samples"),
		Diagnostics.GyroscopeDrift.SampleCount, 3);
	TestEqual(TEXT("The mean exposes a stationary angular-rate offset"),
		Diagnostics.GyroscopeDrift.MeanAngularVelocityRadiansPerSecond,
		StationaryOffset);
	TestTrue(TEXT("The RMS angular speed uses radians per second"),
		FMath::IsNearlyEqual(
			Diagnostics.GyroscopeDrift.
				RootMeanSquareAngularSpeedRadiansPerSecond,
			StationaryOffset.Size(),
			1.e-9
		));
	double ExpectedSquareSum = 0.0;
	for (int32 Index = 0; Index < 70; ++Index)
	{
		const double Value = static_cast<double>(Index);
		FOpenMobileSensorsSampleService::PublishVector(
			MakeSample(2.0 + Index * 0.01, FVector(Value, 0.0, 0.0))
		);
		if (Index >= 6)
		{
			ExpectedSquareSum += Value * Value;
		}
	}
	FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
		Owner,
		Subscription.Handle,
		3.0,
		Diagnostics
	);
	TestEqual(TEXT("The drift window remains bounded"),
		Diagnostics.GyroscopeDrift.SampleCount, 64);
	TestEqual(TEXT("The rolling mean evicts the oldest samples"),
		Diagnostics.GyroscopeDrift.MeanAngularVelocityRadiansPerSecond,
		FVector(37.5, 0.0, 0.0));
	TestTrue(TEXT("The rolling RMS evicts the oldest samples"),
		FMath::IsNearlyEqual(
			Diagnostics.GyroscopeDrift.
				RootMeanSquareAngularSpeedRadiansPerSecond,
			FMath::Sqrt(ExpectedSquareSum / 64.0),
			1.e-9
		));
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGyroscopeHighRateLifecycleTest,
	"OpenMobile.Sensors.Gyroscope.HighRateLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGyroscopeHighRateLifecycleTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsGyroscopeTestsPrivate;
	ResetServices();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bOriginalHighRate = Settings->bAllowHighSamplingRate;
	Settings->bAllowHighSamplingRate = true;
	FOpenMobileSensorsMockBackend Backend(TEXT("GyroscopeHighRate"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Gyroscope)}
	);
	Backend.SetHighSamplingRateDeclarationForTests(true, true);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorSubscriptionRequest Request = MakeRequest();
	Request.Options.CustomFrequencyHz = 300.0;
	Request.Options.MaximumCallbackFrequencyHz = 60.0;
	Request.Options.bAllowHighSamplingRate = true;
	Request.Options.bLowLatency = true;
	Request.Options.MaximumDeliveryLatencySeconds = 0.5;
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("The opted-in gyro reaches the requested high rate"),
		Backend.GetLastStartedPhysicalRequest().RequestedFrequencyHz, 300.0);
	TestFalse(TEXT("Low-latency gyro avoids native batching"),
		Backend.GetLastStartedPhysicalRequest().bNativeBatchingRequested);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeSample(1.0, FVector(0.01, 0.0, 0.0))
	);
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Paused
	);
	FOpenMobileSensorStreamDiagnostics Diagnostics;
	FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
		Owner,
		Subscription.Handle,
		1.1,
		Diagnostics
	);
	TestEqual(TEXT("Pausing resets gyro drift observations"),
		Diagnostics.GyroscopeDrift.SampleCount, 0);
	FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
		Subscription.Handle,
		EOpenMobileSensorSubscriptionState::Active
	);
	FOpenMobileSensorsSampleService::PublishVector(
		MakeSample(2.0, FVector(1.0, 0.0, 0.0))
	);
	FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
		Owner,
		Subscription.Handle,
		2.1,
		Diagnostics
	);
	TestEqual(TEXT("Resume starts a new gyro diagnostic window"),
		Diagnostics.GyroscopeDrift.SampleCount, 1);
	const FOpenMobileSensorOperationResult Stop =
		FOpenMobileSensorsSubscriptionService::StopSubscription(
			Owner,
			Subscription.Handle
		);
	TestTrue(TEXT("The gyro stream stops cleanly"), Stop.IsSuccess());
	TestEqual(TEXT("The final gyro subscriber releases native hardware"),
		Backend.GetStopSensorStreamCount(), 1);
	Settings->bAllowHighSamplingRate = bOriginalHighRate;
	FinishBackend(Backend);
	return true;
}

#endif
