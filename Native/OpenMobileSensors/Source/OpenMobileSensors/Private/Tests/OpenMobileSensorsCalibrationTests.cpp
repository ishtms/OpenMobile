#if WITH_DEV_AUTOMATION_TESTS

#include "Internationalization/Text.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorCalibration.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsCalibrationTestsPrivate
{
	FOpenMobileSensorSubscriptionRequest MakeRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Magnetometer;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 30.0;
		Request.Options.MaximumCallbackFrequencyHz = 30.0;
		return Request;
	}

	FOpenMobileSensorAccuracySnapshot MakeAccuracy(
		const FOpenMobileSensorIdentifier& Sensor,
		EOpenMobileSensorAccuracy Accuracy,
		double TimestampSeconds,
		bool bCalibrationRequired = false
	)
	{
		FOpenMobileSensorAccuracySnapshot Snapshot;
		Snapshot.Sensor = Sensor;
		Snapshot.Accuracy = Accuracy;
		Snapshot.TimestampSeconds = TimestampSeconds;
		Snapshot.bCalibrationRequired = bCalibrationRequired;
		return Snapshot;
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
	FOpenMobileSensorsCalibrationEventLifecycleTest,
	"OpenMobile.Sensors.Calibration.EventLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCalibrationEventLifecycleTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCalibrationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("CalibrationEvents"));
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
	TArray<FOpenMobileSensorCalibrationEvent> Events;
	FOpenMobileSensorsSampleService::OnCalibrationChanged().AddLambda(
		[&](
			const FGuid& EventOwner,
			const FOpenMobileSensorSubscriptionHandle& EventHandle,
			const FOpenMobileSensorCalibrationEvent& Event
		)
		{
			if (EventOwner == Owner && EventHandle == Subscription.Handle)
			{
				Events.Add(Event);
			}
		}
	);

	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor,
		EOpenMobileSensorAccuracy::Low,
		1.0
	));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.0);
	TestEqual(TEXT("Low quality does not request calibration"),
		Events.Num(), 0);

	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor,
		EOpenMobileSensorAccuracy::Unreliable,
		2.0
	));
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor,
		EOpenMobileSensorAccuracy::Unreliable,
		3.0
	));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(3.0);
	TestEqual(TEXT("Repeated poor quality is deduplicated"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		TestEqual(TEXT("Required event identifies the sensor"),
			Events[0].Sensor, Request.Sensor);
		TestEqual(TEXT("Required state is explicit"),
			Events[0].State,
			EOpenMobileSensorCalibrationState::Required);
		TestEqual(TEXT("Magnetic interference is the reason"),
			Events[0].Reason,
			EOpenMobileSensorCalibrationReason::MagneticInterference);
		TestEqual(TEXT("Current quality is retained"),
			Events[0].Accuracy,
			EOpenMobileSensorAccuracy::Unreliable);
		TestFalse(TEXT("Guidance is present"), Events[0].Guidance.IsEmpty());
		TestTrue(TEXT("Guidance has a localization namespace"),
			FTextInspector::GetNamespace(Events[0].Guidance).IsSet());
		TestTrue(TEXT("Guidance has a localization key"),
			FTextInspector::GetKey(Events[0].Guidance).IsSet());
	}

	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor,
		EOpenMobileSensorAccuracy::Medium,
		4.0
	));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(4.0);
	TestEqual(TEXT("Recovery emits one resolution"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		TestEqual(TEXT("Resolution state is explicit"),
			Events[1].State,
			EOpenMobileSensorCalibrationState::Resolved);
		TestEqual(TEXT("Resolution carries recovered quality"),
			Events[1].Accuracy,
			EOpenMobileSensorAccuracy::Medium);
		TestEqual(TEXT("Resolution reason is explicit"),
			Events[1].Reason,
			EOpenMobileSensorCalibrationReason::QualityRecovered);
	}

	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor,
		EOpenMobileSensorAccuracy::Unreliable,
		5.0
	));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(5.0);
	TestEqual(TEXT("Recurrence inside cooldown is suppressed"),
		Events.Num(), 2);
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor,
		EOpenMobileSensorAccuracy::Unreliable,
		32.1
	));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(32.1);
	TestEqual(TEXT("Persistent recurrence emits after cooldown"),
		Events.Num(), 3);
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor,
		EOpenMobileSensorAccuracy::High,
		33.0
	));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(33.0);
	TestEqual(TEXT("Recovered recurrence resolves once"), Events.Num(), 4);
	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		TestEqual(TEXT("Event sequences are contiguous"),
			Events[Index].Sequence,
			static_cast<int64>(Index + 1));
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsCalibrationPromptIsExplicitTest,
	"OpenMobile.Sensors.Calibration.PromptIsExplicit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsCalibrationPromptIsExplicitTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCalibrationTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("CalibrationPrompt"));
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
	FOpenMobileSensorsSampleService::PublishAccuracy(MakeAccuracy(
		Request.Sensor,
		EOpenMobileSensorAccuracy::Unreliable,
		1.0,
		true
	));
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.0);
	TestEqual(TEXT("Informational events do not launch native UI"),
		Backend.GetCalibrationPromptCount(), 0);
	const FOpenMobileSensorOperationResult Prompt =
		FOpenMobileSensorsSubscriptionService::RequestNativeCalibrationPrompt(
			Owner,
			Subscription.Handle
		);
	TestEqual(TEXT("Unsupported prompt returns not supported"),
		Prompt.Code, EOpenMobileSensorResultCode::NotSupported);
	TestEqual(TEXT("Unsupported prompt has a portable reason"),
		Prompt.Failure.Reason,
		EOpenMobileSensorFailureReason::UnsupportedOperation);
	TestEqual(TEXT("Only the explicit operation reaches the backend"),
		Backend.GetCalibrationPromptCount(), 1);
	FinishBackend(Backend);
	return true;
}

#endif
