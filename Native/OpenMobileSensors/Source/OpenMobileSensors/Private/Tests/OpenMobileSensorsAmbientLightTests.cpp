#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorValidity.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsAmbientLightTestsPrivate
{
	FOpenMobileSensorIdentifier MakeSensor(EOpenMobileSensorType Type)
	{
		FOpenMobileSensorIdentifier Sensor;
		Sensor.Type = Type;
		Sensor.InstanceId = TEXT("Default");
		return Sensor;
	}

	FOpenMobileScalarSensorSample MakeSample(double Lux)
	{
		FOpenMobileScalarSensorSample Sample;
		Sample.Header.Sensor = MakeSensor(EOpenMobileSensorType::AmbientLight);
		Sample.Header.TimestampSeconds = 1.0;
		Sample.Header.bValid = true;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Raw
		);
		Sample.Value = Lux;
		return Sample;
	}

	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type,
		EOpenMobileCapabilityState State =
			EOpenMobileCapabilityState::Available
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = MakeSensor(Type);
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = State;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 50.0;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor = MakeSensor(EOpenMobileSensorType::AmbientLight);
		return Request;
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
	FOpenMobileSensorsAmbientLightUnitsTest,
	"OpenMobile.Sensors.AmbientLight.Units",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAmbientLightUnitsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAmbientLightTestsPrivate;
	FOpenMobileScalarSensorSample Darkness = MakeSample(0.0);
	TestTrue(TEXT("Zero lux is valid darkness"),
		FOpenMobileSensorUnitConverter::NormalizeScalarSample(
			EOpenMobileSensorNativePlatform::Android,
			Darkness
		));
	TestEqual(TEXT("Darkness stays zero lux"), Darkness.Value, 0.0);
	FOpenMobileScalarSensorSample Bright = MakeSample(120000.0);
	TestTrue(TEXT("Finite bright light is valid"),
		FOpenMobileSensorUnitConverter::NormalizeScalarSample(
			EOpenMobileSensorNativePlatform::Android,
			Bright
		));
	TestEqual(TEXT("Android light values stay in lux"),
		Bright.Value, 120000.0);
	FOpenMobileScalarSensorSample Negative = MakeSample(-0.01);
	TestFalse(TEXT("Negative lux is rejected"),
		FOpenMobileSensorUnitConverter::NormalizeScalarSample(
			EOpenMobileSensorNativePlatform::Android,
			Negative
		));
	TestFalse(TEXT("Rejected lux is marked invalid"),
		Negative.Header.bValid);
	TestEqual(TEXT("Rejected lux is not replaced"), Negative.Value, -0.01);
	TestTrue(TEXT("Darkness is within the hardware range"),
		FOpenMobileSensorValidity::IsWithinMaximumRange(0.0, 120000.0));
	TestTrue(TEXT("Maximum reported lux is inclusive"),
		FOpenMobileSensorValidity::IsWithinMaximumRange(
			120000.0,
			120000.0
		));
	TestFalse(TEXT("Lux above the hardware maximum is rejected"),
		FOpenMobileSensorValidity::IsWithinMaximumRange(
			120000.01,
			120000.0
		));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAmbientLightAvailabilityAndRateTest,
	"OpenMobile.Sensors.AmbientLight.AvailabilityAndRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAmbientLightAvailabilityAndRateTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAmbientLightTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AmbientLightAvailable"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::AmbientLight)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Default =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	TestTrue(TEXT("Default ambient-light polling starts"),
		Default.Operation.IsSuccess());
	TestEqual(TEXT("Ambient light defaults to one hertz"),
		Default.AppliedOptions.CustomFrequencyHz, 1.0);
	TestEqual(TEXT("Ambient-light callback cap defaults to one hertz"),
		Default.AppliedOptions.MaximumCallbackFrequencyHz, 1.0);
	FOpenMobileSensorSubscriptionRequest FastRequest = MakeRequest();
	FastRequest.Options.RatePreset = EOpenMobileSensorRatePreset::Fast;
	const FOpenMobileSensorSubscriptionResult Fast =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			FastRequest
		);
	TestEqual(TEXT("Fast ambient-light requests obey the policy ceiling"),
		Fast.AppliedOptions.CustomFrequencyHz, 5.0);
	TestEqual(TEXT("Ambient-light ceiling is reported as project policy"),
		Fast.RateResolution.AdjustmentReason,
		EOpenMobileSensorRateAdjustmentReason::ProjectPolicy);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileScalarSensorSample Bright = MakeSample(85000.0);
	Bright.Header.TimestampSeconds = 2.0;
	Bright.Header.bUnitsNormalized = true;
	FOpenMobileSensorsSampleService::PublishScalar(Bright);
	FOpenMobileSensorReadResult Read;
	FOpenMobileScalarSensorSample Latest;
	TestTrue(TEXT("Ambient light uses latest-value polling"),
		FOpenMobileSensorsSampleService::ReadLatestScalar(
			Owner,
			Default.Handle,
			0,
			2.0,
			Read,
			Latest
		));
	TestEqual(TEXT("Latest ambient-light value remains lux"),
		Latest.Value, 85000.0);
	TestEqual(TEXT("Ambient light retains native raw source"),
		Latest.Header.SourceFlags,
		FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
			EOpenMobileSensorType::AmbientLight
		));
	FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(Owner);
	FinishBackend(Backend);

	ResetServices();
	FOpenMobileSensorsMockBackend MinimumRate(
		TEXT("AmbientLightMinimumRate")
	);
	FOpenMobileSensorCapability MinimumRateCapability =
		MakeCapability(EOpenMobileSensorType::AmbientLight);
	MinimumRateCapability.MinimumFrequencyHz = 10.0;
	MinimumRate.SetSensorCapabilities({MinimumRateCapability});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(MinimumRate);
	FOpenMobileSensorSubscriptionRequest MinimumRateRequest = MakeRequest();
	MinimumRateRequest.Options.RatePreset =
		EOpenMobileSensorRatePreset::Fast;
	const FOpenMobileSensorSubscriptionResult MinimumRateResult =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MinimumRateRequest
		);
	TestEqual(TEXT("Hardware minimum remains the physical rate"),
		MinimumRateResult.AppliedOptions.CustomFrequencyHz, 10.0);
	TestEqual(TEXT("Callbacks retain the ambient-light ceiling"),
		MinimumRateResult.AppliedOptions.MaximumCallbackFrequencyHz, 5.0);
	FinishBackend(MinimumRate);

	ResetServices();
	FOpenMobileSensorsMockBackend Missing(TEXT("AmbientLightMissing"));
	Missing.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::BarometricPressure)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Missing);
	const FOpenMobileSensorSubscriptionResult Unavailable =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest()
		);
	TestEqual(TEXT("Absent Android light hardware is rejected"),
		Unavailable.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	TestEqual(TEXT("Absent light hardware has a typed reason"),
		Unavailable.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::MissingHardware);
	TestEqual(TEXT("Missing light hardware starts no native stream"),
		Missing.GetStartSensorStreamCount(), 0);
	FinishBackend(Missing);

	ResetServices();
	FOpenMobileSensorsMockBackend Unsupported(TEXT("AmbientLightUnsupported"));
	Unsupported.SetSensorCapabilities({
		MakeCapability(
			EOpenMobileSensorType::AmbientLight,
			EOpenMobileCapabilityState::NotSupported
		)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Unsupported);
	const FOpenMobileSensorSubscriptionResult NotSupported =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest()
		);
	TestEqual(TEXT("Explicit unsupported ambient light is deterministic"),
		NotSupported.Operation.Code,
		EOpenMobileSensorResultCode::NotSupported);
	TestEqual(TEXT("Unsupported ambient light starts no native stream"),
		Unsupported.GetStartSensorStreamCount(), 0);
	FinishBackend(Unsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAmbientLightEventThresholdTest,
	"OpenMobile.Sensors.AmbientLight.EventThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAmbientLightEventThresholdTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAmbientLightTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AmbientLightThreshold"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::AmbientLight)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorSubscriptionRequest ThresholdRequest = MakeRequest();
	ThresholdRequest.Options.DeliveryMode =
		EOpenMobileSensorDeliveryMode::EventBatches;
	ThresholdRequest.Options.MinimumScalarEventChange = 10.0;
	const FOpenMobileSensorSubscriptionResult Threshold =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			ThresholdRequest
		);
	FOpenMobileSensorSubscriptionRequest UnfilteredRequest = MakeRequest();
	UnfilteredRequest.Options.DeliveryMode =
		EOpenMobileSensorDeliveryMode::EventBatches;
	const FOpenMobileSensorSubscriptionResult Unfiltered =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			UnfilteredRequest
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Threshold subscribers share one light stream"),
		Backend.GetStartSensorStreamCount(), 1);
	TArray<double> ThresholdValues;
	TArray<double> UnfilteredValues;
	FOpenMobileSensorsSampleService::OnScalarBatch().AddLambda(
		[&](
			const FGuid&,
			const FOpenMobileSensorSubscriptionHandle& Handle,
			const FOpenMobileScalarSensorBatch& Batch
		)
		{
			TArray<double>* Values = Handle == Threshold.Handle
				? &ThresholdValues
				: Handle == Unfiltered.Handle
					? &UnfilteredValues
					: nullptr;
			if (!Values)
			{
				return;
			}
			for (const FOpenMobileScalarSensorSample& Sample : Batch.Samples)
			{
				Values->Add(Sample.Value);
			}
		}
	);
	for (const TPair<double, double>& Fixture : {
		TPair<double, double>(1.0, 100.0),
		TPair<double, double>(2.0, 105.0),
		TPair<double, double>(3.0, 109.99),
		TPair<double, double>(4.0, 110.0)
	})
	{
		FOpenMobileScalarSensorSample Sample = MakeSample(Fixture.Value);
		Sample.Header.TimestampSeconds = Fixture.Key;
		Sample.Header.bUnitsNormalized = true;
		FOpenMobileSensorsSampleService::PublishScalar(Sample);
		if (Fixture.Key == 2.0)
		{
			FOpenMobileSensorReadResult Read;
			FOpenMobileScalarSensorSample Latest;
			FOpenMobileSensorsSampleService::ReadLatestScalar(
				Owner,
				Threshold.Handle,
				0,
				2.0,
				Read,
				Latest
			);
			TestEqual(TEXT("Polling ignores the event threshold"),
				Latest.Value, 105.0);
		}
	}
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(4.0);
	TestEqual(TEXT("Threshold emits the first and boundary values"),
		ThresholdValues.Num(), 2);
	if (ThresholdValues.Num() == 2)
	{
		TestEqual(TEXT("Threshold emits the first lux value"),
			ThresholdValues[0], 100.0);
		TestEqual(TEXT("Threshold equality emits the new lux value"),
			ThresholdValues[1], 110.0);
	}
	TestEqual(TEXT("Unfiltered subscriber receives every light change"),
		UnfilteredValues.Num(), 4);
	FOpenMobileSensorSubscriptionRequest InvalidRequest = MakeRequest();
	InvalidRequest.Options.DeliveryMode =
		EOpenMobileSensorDeliveryMode::EventBatches;
	InvalidRequest.Options.MinimumScalarEventChange = -0.01;
	const FOpenMobileSensorSubscriptionResult Invalid =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			InvalidRequest
		);
	TestEqual(TEXT("Negative event thresholds are rejected"),
		Invalid.Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(Owner);
	FinishBackend(Backend);
	return true;
}

#endif
