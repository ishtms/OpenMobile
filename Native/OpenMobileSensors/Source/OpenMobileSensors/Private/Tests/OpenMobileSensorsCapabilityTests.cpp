#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"
#include "OpenMobileSensorsTrueHeadingService.h"

namespace OpenMobileSensorsCapabilityTestsPrivate
{
	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type,
		EOpenMobileSensorAvailabilitySource Source =
			EOpenMobileSensorAvailabilitySource::Native,
		EOpenMobileCapabilityState State =
			EOpenMobileCapabilityState::Available
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = State;
		Capability.Source = Source;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 200.0;
		Capability.bSupportsNativeBatching = true;
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Supported;
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

	const FOpenMobileSensorBackgroundCapability* FindBackgroundOperation(
		const FOpenMobileSensorCapability& Capability,
		EOpenMobileSensorBackgroundOperation Operation
	)
	{
		return Capability.BackgroundOperations.FindByPredicate(
			[Operation](
				const FOpenMobileSensorBackgroundCapability& Background
			)
			{
				return Background.Operation == Operation;
			}
		);
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsTrueHeadingService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsTrueHeadingService::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBackgroundOperationMatrixTest,
	"OpenMobile.Sensors.Capabilities.Background.OperationMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBackgroundOperationMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bPreviousBackgroundOptIn =
		Settings->bAllowBackgroundSensorDelivery;
	Settings->bAllowBackgroundSensorDelivery = false;
	FOpenMobileSensorsMockBackend Backend(TEXT("BackgroundOperations"));
	FOpenMobileSensorCapability Accelerometer = MakeCapability(
		EOpenMobileSensorType::Accelerometer
	);
	Accelerometer.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::Suspended;
	FOpenMobileSensorCapability Transition = MakeCapability(
		EOpenMobileSensorType::ActivityTransition
	);
	Transition.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::EventDriven;
	FOpenMobileSensorCapability Steps = MakeCapability(
		EOpenMobileSensorType::StepCounter
	);
	Steps.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::EventDriven;
	Backend.SetSensorCapabilities({Accelerometer, Transition, Steps});
	Backend.SetNativeStepCountQueryBackgroundSupportForTests(
		EOpenMobileSensorBackgroundSupport::Limited
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* ReportedAccelerometer =
		FindCapability(Snapshot, EOpenMobileSensorType::Accelerometer);
	const FOpenMobileSensorCapability* ReportedTransition =
		FindCapability(Snapshot, EOpenMobileSensorType::ActivityTransition);
	const FOpenMobileSensorCapability* ReportedSteps =
		FindCapability(Snapshot, EOpenMobileSensorType::StepCounter);
	TestNotNull(
		TEXT("Accelerometer background operations are reported"),
		ReportedAccelerometer
	);
	TestNotNull(
		TEXT("Activity-transition background operations are reported"),
		ReportedTransition
	);
	TestNotNull(
		TEXT("Step background operations are reported"),
		ReportedSteps
	);
	if (ReportedAccelerometer && ReportedTransition && ReportedSteps)
	{
		const FOpenMobileSensorBackgroundCapability* RawStream =
			FindBackgroundOperation(
				*ReportedAccelerometer,
				EOpenMobileSensorBackgroundOperation::Stream
			);
		const FOpenMobileSensorBackgroundCapability* Recording =
			FindBackgroundOperation(
				*ReportedAccelerometer,
				EOpenMobileSensorBackgroundOperation::Recording
			);
		const FOpenMobileSensorBackgroundCapability* TransitionStream =
			FindBackgroundOperation(
				*ReportedTransition,
				EOpenMobileSensorBackgroundOperation::Stream
			);
		const FOpenMobileSensorBackgroundCapability* UnsupportedRecording =
			FindBackgroundOperation(
				*ReportedTransition,
				EOpenMobileSensorBackgroundOperation::Recording
			);
		const FOpenMobileSensorBackgroundCapability* StepQuery =
			FindBackgroundOperation(
				*ReportedSteps,
				EOpenMobileSensorBackgroundOperation::NativeStepCountQuery
			);
		TestNotNull(TEXT("Raw streaming has a distinct report"), RawStream);
		TestNotNull(TEXT("Recording has a distinct report"), Recording);
		TestNotNull(
			TEXT("Activity transitions have a stream report"),
			TransitionStream
		);
		TestNull(
			TEXT("Unsupported recording pairs are not advertised"),
			UnsupportedRecording
		);
		TestNotNull(
			TEXT("Native step queries have a distinct report"),
			StepQuery
		);
		if (RawStream && Recording && TransitionStream && StepQuery)
		{
			TestEqual(
				TEXT("Raw platform behavior is suspended"),
				RawStream->PlatformBehavior,
				EOpenMobileSensorBackgroundSupport::Suspended
			);
			TestEqual(
				TEXT("Raw effective behavior remains suspended"),
				RawStream->ExpectedBehavior,
				EOpenMobileSensorBackgroundSupport::Suspended
			);
			TestEqual(
				TEXT("Raw suspension has a structured reason"),
				RawStream->Reason,
				EOpenMobileSensorFailureReason::BackgroundRestricted
			);
			TestEqual(
				TEXT("Recording follows its input stream behavior"),
				Recording->ExpectedBehavior,
				EOpenMobileSensorBackgroundSupport::Suspended
			);
			TestEqual(
				TEXT("Transition platform delivery is event-driven"),
				TransitionStream->PlatformBehavior,
				EOpenMobileSensorBackgroundSupport::EventDriven
			);
			TestEqual(
				TEXT("Project policy suspends transition delivery"),
				TransitionStream->ExpectedBehavior,
				EOpenMobileSensorBackgroundSupport::Suspended
			);
			TestEqual(
				TEXT("Project policy has a structured reason"),
				TransitionStream->Reason,
				EOpenMobileSensorFailureReason::ConfigurationBlocked
			);
			TestTrue(
				TEXT("Stream continuation reports its project opt-in"),
				TransitionStream->bProjectOptInRequired
					&& !TransitionStream->bProjectOptInEnabled
			);
			TestEqual(
				TEXT("Historical step queries report limited behavior"),
				StepQuery->ExpectedBehavior,
				EOpenMobileSensorBackgroundSupport::Limited
			);
			TestFalse(
				TEXT("Historical queries do not claim continuous-delivery policy"),
				StepQuery->bProjectOptInRequired
			);
		}
	}
	TestEqual(
		TEXT("Capability inspection starts no sensor stream"),
		Backend.GetStartSensorStreamCount(),
		0
	);
	Settings->bAllowBackgroundSensorDelivery = bPreviousBackgroundOptIn;
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBackgroundDynamicStateTest,
	"OpenMobile.Sensors.Capabilities.Background.DynamicState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBackgroundDynamicStateTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bPreviousBackgroundOptIn =
		Settings->bAllowBackgroundSensorDelivery;
	Settings->bAllowBackgroundSensorDelivery = true;
	FOpenMobileSensorsMockBackend Backend(TEXT("BackgroundDynamicState"));
	FOpenMobileSensorCapability Transition = MakeCapability(
		EOpenMobileSensorType::ActivityTransition
	);
	Transition.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::EventDriven;
	Transition.RequiredPermission =
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::ActivityRecognition
		);
	Backend.SetSensorCapabilities({Transition});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Transition.RequiredPermission,
		EOpenMobilePermissionStatus::Granted
	);
	int32 ChangeCount = 0;
	const FDelegateHandle ChangeHandle =
		FOpenMobileSensorsCapabilityService::OnChanged().AddLambda(
			[&](const FOpenMobileSensorCapabilitySnapshot&)
			{
				++ChangeCount;
			}
		);
	auto GetTransitionBackground = [this]()
	{
		const FOpenMobileSensorCapabilitySnapshot Snapshot =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		const FOpenMobileSensorCapability* Capability = FindCapability(
			Snapshot,
			EOpenMobileSensorType::ActivityTransition
		);
		TestNotNull(
			TEXT("The transition capability remains present"),
			Capability
		);
		const FOpenMobileSensorBackgroundCapability* Background = Capability
			? FindBackgroundOperation(
				*Capability,
				EOpenMobileSensorBackgroundOperation::Stream
			)
			: nullptr;
		TestNotNull(
			TEXT("The transition stream background report remains present"),
			Background
		);
		return Background
			? *Background
			: FOpenMobileSensorBackgroundCapability{};
	};
	FOpenMobileSensorBackgroundCapability Background =
		GetTransitionBackground();
	TestEqual(
		TEXT("Granted provider transitions are event-driven"),
		Background.ExpectedBehavior,
		EOpenMobileSensorBackgroundSupport::EventDriven
	);
	TestEqual(
		TEXT("Foreground transitions have no active restriction"),
		Background.ActiveRestriction,
		EOpenMobileSensorRestriction::None
	);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	Background = GetTransitionBackground();
	TestEqual(
		TEXT("Event-driven transitions remain available in background"),
		Background.ActiveRestriction,
		EOpenMobileSensorRestriction::None
	);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Transition.RequiredPermission,
		EOpenMobilePermissionStatus::Denied
	);
	Background = GetTransitionBackground();
	TestEqual(
		TEXT("Denied authorization removes effective background support"),
		Background.ExpectedBehavior,
		EOpenMobileSensorBackgroundSupport::Unsupported
	);
	TestEqual(
		TEXT("Denied authorization is structured"),
		Background.Reason,
		EOpenMobileSensorFailureReason::PermissionDenied
	);
	TestEqual(
		TEXT("Denied authorization is the active restriction"),
		Background.ActiveRestriction,
		EOpenMobileSensorRestriction::Permission
	);

	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Transition.RequiredPermission,
		EOpenMobilePermissionStatus::Granted
	);
	Transition.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::Suspended;
	Backend.SetSensorCapabilities({Transition});
	FOpenMobileSensorsCapabilityService::HandleBackendGenerationChanged();
	Background = GetTransitionBackground();
	TestEqual(
		TEXT("A provider downgrade is reflected"),
		Background.ExpectedBehavior,
		EOpenMobileSensorBackgroundSupport::Suspended
	);
	TestEqual(
		TEXT("A suspended provider is restricted while inactive"),
		Background.ActiveRestriction,
		EOpenMobileSensorRestriction::Background
	);

	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	Background = GetTransitionBackground();
	TestEqual(
		TEXT("Foreground restores a suspended operation"),
		Background.ActiveRestriction,
		EOpenMobileSensorRestriction::None
	);
	Transition.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::EventDriven;
	Backend.SetSensorCapabilities({Transition});
	FOpenMobileSensorsCapabilityService::HandleBackendGenerationChanged();
	Settings->bAllowBackgroundSensorDelivery = false;
	Background = GetTransitionBackground();
	TestEqual(
		TEXT("A runtime project-policy change is reevaluated"),
		Background.ExpectedBehavior,
		EOpenMobileSensorBackgroundSupport::Suspended
	);
	TestEqual(
		TEXT("A project-policy change reports configuration"),
		Background.Reason,
		EOpenMobileSensorFailureReason::ConfigurationBlocked
	);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	Background = GetTransitionBackground();
	TestEqual(
		TEXT("Disabled project policy restricts the inactive operation"),
		Background.ActiveRestriction,
		EOpenMobileSensorRestriction::Background
	);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	TestTrue(
		TEXT("Material background changes are broadcast"),
		ChangeCount >= 7
	);
	TestEqual(
		TEXT("Capability changes do not start provider work"),
		Backend.GetStartSensorStreamCount(),
		0
	);
	FOpenMobileSensorsCapabilityService::OnChanged().Remove(ChangeHandle);
	Settings->bAllowBackgroundSensorDelivery = bPreviousBackgroundOptIn;
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBackgroundDeliveryContractTest,
	"OpenMobile.Sensors.Capabilities.Background.DeliveryContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBackgroundDeliveryContractTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsSampleService::ResetForTests();
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const bool bPreviousBackgroundOptIn =
		Settings->bAllowBackgroundSensorDelivery;
	Settings->bAllowBackgroundSensorDelivery = true;
	FOpenMobileSensorsMockBackend Backend(TEXT("BackgroundDeliveryContract"));
	FOpenMobileSensorCapability Accelerometer = MakeCapability(
		EOpenMobileSensorType::Accelerometer
	);
	Accelerometer.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::Suspended;
	FOpenMobileSensorCapability Transition = MakeCapability(
		EOpenMobileSensorType::ActivityTransition
	);
	Transition.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::EventDriven;
	Backend.SetSensorCapabilities({Accelerometer, Transition});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	auto Start = [&Owner](EOpenMobileSensorType Type)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = Type;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 10.0;
		Request.Options.MaximumCallbackFrequencyHz = 10.0;
		Request.Options.LifecyclePolicy =
			EOpenMobileSensorLifecyclePolicy::ContinueWhenSupported;
		return FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			Request
		);
	};
	const FOpenMobileSensorSubscriptionResult Raw = Start(
		EOpenMobileSensorType::Accelerometer
	);
	const FOpenMobileSensorSubscriptionResult Events = Start(
		EOpenMobileSensorType::ActivityTransition
	);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* RawCapability = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Accelerometer
	);
	const FOpenMobileSensorCapability* EventCapability = FindCapability(
		Snapshot,
		EOpenMobileSensorType::ActivityTransition
	);
	const FOpenMobileSensorBackgroundCapability* RawBackground =
		RawCapability
			? FindBackgroundOperation(
				*RawCapability,
				EOpenMobileSensorBackgroundOperation::Stream
			)
			: nullptr;
	const FOpenMobileSensorBackgroundCapability* EventBackground =
		EventCapability
			? FindBackgroundOperation(
				*EventCapability,
				EOpenMobileSensorBackgroundOperation::Stream
			)
			: nullptr;
	TestTrue(
		TEXT("Capability reports raw suspension before lifecycle change"),
		RawBackground
			&& RawBackground->ExpectedBehavior ==
				EOpenMobileSensorBackgroundSupport::Suspended
	);
	TestTrue(
		TEXT("Capability reports event-driven transition delivery"),
		EventBackground
			&& EventBackground->ExpectedBehavior ==
				EOpenMobileSensorBackgroundSupport::EventDriven
	);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	FOpenMobileSensorSubscriptionStateSnapshot RawState;
	FOpenMobileSensorSubscriptionStateSnapshot EventState;
	TestTrue(
		TEXT("The raw handle remains queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Raw.Handle,
			RawState
		)
	);
	TestTrue(
		TEXT("The transition handle remains queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Events.Handle,
			EventState
		)
	);
	TestEqual(
		TEXT("Reported raw suspension matches delivery behavior"),
		RawState.State,
		EOpenMobileSensorSubscriptionState::Paused
	);
	TestEqual(
		TEXT("Reported event delivery matches delivery behavior"),
		EventState.State,
		EOpenMobileSensorSubscriptionState::Active
	);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(Owner);
	Settings->bAllowBackgroundSensorDelivery = bPreviousBackgroundOptIn;
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsSampleService::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsFullySupportedMatrixTest,
	"OpenMobile.Sensors.Capabilities.FullySupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsFullySupportedMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Full"));
	TArray<FOpenMobileSensorCapability> Capabilities;
	for (EOpenMobileSensorType Type : FOpenMobileSensorTypes::GetAll())
	{
		Capabilities.Add(MakeCapability(Type));
	}
	Backend.SetSensorCapabilities(MoveTemp(Capabilities));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(true);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Matrix includes every stable sensor type"),
		Snapshot.Sensors.Num(), FOpenMobileSensorTypes::GetAll().Num());
	TSet<FName> StableNames;
	for (const FOpenMobileSensorCapability& Capability : Snapshot.Sensors)
	{
		TestEqual(TEXT("Fully supported sensor is available"),
			Capability.Availability.State,
			EOpenMobileCapabilityState::Available);
		if (Capability.Sensor.Type == EOpenMobileSensorType::Shake)
		{
			TestEqual(TEXT("Shake remains a derived logical event"),
				Capability.Source,
				EOpenMobileSensorAvailabilitySource::Derived);
			TestTrue(TEXT("Shake fallback is available"),
				Capability.Fallback.bAvailable);
			TestFalse(TEXT("Shake does not claim native batching"),
				Capability.bSupportsNativeBatching);
		}
		else
		{
			TestEqual(TEXT("Fully supported sensor is native"),
				Capability.Source,
				EOpenMobileSensorAvailabilitySource::Native);
			TestTrue(TEXT("Native batching is reported"),
				Capability.bSupportsNativeBatching);
		}
		TestTrue(TEXT("Supported rates are ordered"),
			Capability.MinimumFrequencyHz > 0.0
				&& Capability.MaximumFrequencyHz >=
					Capability.MinimumFrequencyHz);
		StableNames.Add(Capability.Availability.Name);
	}
	TestEqual(TEXT("Stable capability names are unique"),
		StableNames.Num(), Snapshot.Sensors.Num());
	TestEqual(TEXT("Backend matrix is queried once"),
		Backend.GetSensorCapabilityQueryCount(), 1);
	FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Repeated snapshots reuse the cached backend matrix"),
		Backend.GetSensorCapabilityQueryCount(), 1);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPartiallySupportedMatrixTest,
	"OpenMobile.Sensors.Capabilities.PartiallySupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPartiallySupportedMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Partial"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Accelerometer = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Accelerometer
	);
	const FOpenMobileSensorCapability* Gyroscope = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Gyroscope
	);
	TestNotNull(TEXT("Supported sensor remains in the matrix"), Accelerometer);
	TestNotNull(TEXT("Missing sensor remains in the matrix"), Gyroscope);
	TestEqual(TEXT("Supported sensor remains available"),
		Accelerometer->Availability.State,
		EOpenMobileCapabilityState::Available);
	TestEqual(TEXT("Missing hardware is unavailable"),
		Gyroscope->Availability.State,
		EOpenMobileCapabilityState::Unavailable);
	TestEqual(TEXT("Missing hardware has an explicit restriction"),
		Gyroscope->ActiveRestriction,
		EOpenMobileSensorRestriction::MissingHardware);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPermissionBlockedMatrixTest,
	"OpenMobile.Sensors.Capabilities.PermissionBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPermissionBlockedMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Permission"));
	FOpenMobileSensorCapability Activity = MakeCapability(
		EOpenMobileSensorType::MotionActivity
	);
	Activity.RequiredPermission =
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity
		);
	Backend.SetSensorCapabilities({Activity});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Denied
	);
	FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Blocked = FindCapability(
		Snapshot,
		EOpenMobileSensorType::MotionActivity
	);
	TestEqual(TEXT("Denied permission blocks the capability"),
		Blocked->Availability.State,
		EOpenMobileCapabilityState::Denied);
	TestEqual(TEXT("Permission restriction remains explicit"),
		Blocked->ActiveRestriction,
		EOpenMobileSensorRestriction::Permission);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Granted
	);
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Granted permission restores availability"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::MotionActivity
		)->Availability.State,
		EOpenMobileCapabilityState::Available);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsDerivedOnlyMatrixTest,
	"OpenMobile.Sensors.Capabilities.DerivedOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsDerivedOnlyMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Derived"));
	FOpenMobileSensorCapability Gravity = MakeCapability(
		EOpenMobileSensorType::Gravity,
		EOpenMobileSensorAvailabilitySource::Derived
	);
	FOpenMobileSensorCapability TrueHeading = MakeCapability(
		EOpenMobileSensorType::TrueHeading,
		EOpenMobileSensorAvailabilitySource::Derived
	);
	Backend.SetSensorCapabilities({Gravity, TrueHeading});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Derived source remains explicit"),
		FindCapability(Snapshot, EOpenMobileSensorType::Gravity)->Source,
		EOpenMobileSensorAvailabilitySource::Derived);
	TestEqual(TEXT("True heading starts with missing input"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::TrueHeading
		)->ActiveRestriction,
		EOpenMobileSensorRestriction::MissingInput);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	FOpenMobileSensorLocationInput LocationInput;
	LocationInput.HorizontalAccuracyMeters = 10.0;
	LocationInput.TimestampSeconds = static_cast<double>(
		FDateTime::UtcNow().ToUnixTimestamp()
	);
	TestTrue(TEXT("Current location input is accepted"),
		Subsystem->SetTrueHeadingLocationInputNative(
			LocationInput
		).IsSuccess());
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Location input restores derived true heading"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::TrueHeading
		)->Availability.State,
		EOpenMobileCapabilityState::Available);
	Subsystem->Deinitialize();
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Owner teardown removes true-heading location input"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::TrueHeading
		)->ActiveRestriction,
		EOpenMobileSensorRestriction::MissingInput);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTrueHeadingCapabilityOwnerIsolationTest,
	"OpenMobile.Sensors.Capabilities.TrueHeadingOwnerIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTrueHeadingCapabilityOwnerIsolationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("TrueHeadingOwners"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::MagneticHeading)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* FirstGameInstance = NewObject<UGameInstance>();
	UGameInstance* SecondGameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* First =
		NewObject<UOpenMobileSensorsSubsystem>(FirstGameInstance);
	UOpenMobileSensorsSubsystem* Second =
		NewObject<UOpenMobileSensorsSubsystem>(SecondGameInstance);
	FOpenMobileSensorLocationInput Location;
	Location.HorizontalAccuracyMeters = 10.0;
	Location.TimestampSeconds = static_cast<double>(
		FDateTime::UtcNow().ToUnixTimestamp()
	);
	TestTrue(TEXT("First owner accepts current location"),
		First->SetTrueHeadingLocationInputNative(Location).IsSuccess());
	const FOpenMobileSensorCapabilitySnapshot FirstSnapshot =
		First->GetCapabilitySnapshotNative();
	const FOpenMobileSensorCapabilitySnapshot SecondSnapshot =
		Second->GetCapabilitySnapshotNative();
	const FOpenMobileSensorCapability* FirstHeading = FindCapability(
		FirstSnapshot,
		EOpenMobileSensorType::TrueHeading
	);
	const FOpenMobileSensorCapability* SecondHeading = FindCapability(
		SecondSnapshot,
		EOpenMobileSensorType::TrueHeading
	);
	TestNotNull(TEXT("First owner has true-heading capability"), FirstHeading);
	TestNotNull(TEXT("Second owner has true-heading capability"), SecondHeading);
	if (FirstHeading && SecondHeading)
	{
		TestEqual(TEXT("Supplying owner sees true heading available"),
			FirstHeading->Availability.State,
			EOpenMobileCapabilityState::Available);
		TestEqual(TEXT("Other owner still sees missing input"),
			SecondHeading->ActiveRestriction,
			EOpenMobileSensorRestriction::MissingInput);
		const FOpenMobileSensorBackgroundCapability* FirstBackground =
			FindBackgroundOperation(
				*FirstHeading,
				EOpenMobileSensorBackgroundOperation::Stream
			);
		const FOpenMobileSensorBackgroundCapability* SecondBackground =
			FindBackgroundOperation(
				*SecondHeading,
				EOpenMobileSensorBackgroundOperation::Stream
			);
		TestNotNull(
			TEXT("Supplying owner has a background report"),
			FirstBackground
		);
		TestNotNull(
			TEXT("Other owner has a background report"),
			SecondBackground
		);
		if (FirstBackground && SecondBackground)
		{
			TestTrue(
				TEXT("Supplying owner is not blocked by missing input"),
				FirstBackground->Reason !=
					EOpenMobileSensorFailureReason::MissingLocationInput
			);
			TestEqual(
				TEXT("Other owner reports its missing input"),
				SecondBackground->Reason,
				EOpenMobileSensorFailureReason::MissingLocationInput
			);
			TestEqual(
				TEXT("Missing input removes effective background support"),
				SecondBackground->ExpectedBehavior,
				EOpenMobileSensorBackgroundSupport::Unsupported
			);
		}
	}
	First->Deinitialize();
	Second->Deinitialize();
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRateLimitedMatrixTest,
	"OpenMobile.Sensors.Capabilities.RateLimited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRateLimitedMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RateLimited"));
	FOpenMobileSensorCapability Accelerometer = MakeCapability(
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorAvailabilitySource::Native,
		EOpenMobileCapabilityState::TemporarilyUnavailable
	);
	Accelerometer.ActiveRestriction =
		EOpenMobileSensorRestriction::RateLimited;
	Accelerometer.MaximumFrequencyHz = 50.0;
	Backend.SetSensorCapabilities({Accelerometer});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Capability = FindCapability(
		Snapshot,
		EOpenMobileSensorType::Accelerometer
	);
	TestEqual(TEXT("Rate limit remains a distinct restriction"),
		Capability->ActiveRestriction,
		EOpenMobileSensorRestriction::RateLimited);
	TestEqual(TEXT("Rate-limited maximum is preserved"),
		Capability->MaximumFrequencyHz, 50.0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMockAndReplayMatrixTest,
	"OpenMobile.Sensors.Capabilities.MockAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMockAndReplayMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("DevelopmentSources"));
	Backend.SetSensorCapabilities({
		MakeCapability(
			EOpenMobileSensorType::Accelerometer,
			EOpenMobileSensorAvailabilitySource::Mock
		),
		MakeCapability(
			EOpenMobileSensorType::Gyroscope,
			EOpenMobileSensorAvailabilitySource::Replay
		)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Mock source is visible"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::Accelerometer
		)->Source,
		EOpenMobileSensorAvailabilitySource::Mock);
	TestEqual(TEXT("Replay source is visible"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::Gyroscope
		)->Source,
		EOpenMobileSensorAvailabilitySource::Replay);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsUnsupportedEditorMatrixTest,
	"OpenMobile.Sensors.Capabilities.UnsupportedEditor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsUnsupportedEditorMatrixTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Unsupported host still returns the complete matrix"),
		Snapshot.Sensors.Num(), FOpenMobileSensorTypes::GetAll().Num());
	TestEqual(TEXT("Unsupported host reports no backend"),
		Snapshot.BackendAvailability.State,
		EOpenMobileCapabilityState::NotSupported);
	for (const FOpenMobileSensorCapability& Capability : Snapshot.Sensors)
	{
		TestEqual(TEXT("Unsupported host marks every sensor unsupported"),
			Capability.Availability.State,
			EOpenMobileCapabilityState::NotSupported);
	}
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMaterialChangeEventsTest,
	"OpenMobile.Sensors.Capabilities.MaterialChangeEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMaterialChangeEventsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsCapabilityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsCapabilityService::GetSnapshot();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	int32 ServiceEvents = 0;
	int32 SubsystemEvents = 0;
	const FDelegateHandle ServiceHandle =
		FOpenMobileSensorsCapabilityService::OnChanged().AddLambda(
			[&ServiceEvents](const FOpenMobileSensorCapabilitySnapshot&)
			{
				++ServiceEvents;
			}
		);
	const FDelegateHandle SubsystemHandle =
		Subsystem->OnCapabilitiesChangedNative().AddLambda(
			[&SubsystemEvents](const FOpenMobileSensorCapabilitySnapshot&)
			{
				++SubsystemEvents;
			}
		);

	FOpenMobileSensorsMockBackend Backend(TEXT("Events"));
	FOpenMobileSensorCapability Accelerometer = MakeCapability(
		EOpenMobileSensorType::Accelerometer
	);
	Accelerometer.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::Suspended;
	FOpenMobileSensorCapability Activity = MakeCapability(
		EOpenMobileSensorType::MotionActivity
	);
	Activity.RequiredPermission =
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity
		);
	FOpenMobileSensorCapability TrueHeading = MakeCapability(
		EOpenMobileSensorType::TrueHeading,
		EOpenMobileSensorAvailabilitySource::Derived
	);
	Backend.SetSensorCapabilities({Accelerometer, Activity, TrueHeading});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	TestEqual(TEXT("Backend registration emits one material event"),
		ServiceEvents, 1);
	FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Repeated snapshot emits no event"), ServiceEvents, 1);

	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Denied
	);
	TestEqual(TEXT("Permission change emits one event"), ServiceEvents, 2);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Denied
	);
	TestEqual(TEXT("Duplicate permission state is coalesced"), ServiceEvents, 2);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		Activity.RequiredPermission,
		EOpenMobilePermissionStatus::Granted
	);
	TestEqual(TEXT("Permission recovery emits one event"), ServiceEvents, 3);

	FOpenMobileSensorsCapabilityService::SetApplicationActive(false);
	TestEqual(TEXT("Lifecycle restriction emits one event"), ServiceEvents, 4);
	FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Inactive lifecycle suspends unsupported background delivery"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::Accelerometer
		)->ActiveRestriction,
		EOpenMobileSensorRestriction::Background);
	FOpenMobileSensorsCapabilityService::SetApplicationActive(false);
	TestEqual(TEXT("Duplicate lifecycle state is coalesced"), ServiceEvents, 4);
	FOpenMobileSensorsCapabilityService::SetApplicationActive(true);
	TestEqual(TEXT("Foreground recovery emits one event"), ServiceEvents, 5);
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Foreground restores sensor availability"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::Accelerometer
		)->Availability.State,
		EOpenMobileCapabilityState::Available);
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(true);
	TestEqual(TEXT("Location input emits one event"), ServiceEvents, 6);
	Snapshot = FOpenMobileSensorsCapabilityService::GetSnapshot();
	TestEqual(TEXT("Location input restores true heading"),
		FindCapability(
			Snapshot,
			EOpenMobileSensorType::TrueHeading
		)->Availability.State,
		EOpenMobileCapabilityState::Available);
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(true);
	TestEqual(TEXT("Duplicate location input is coalesced"), ServiceEvents, 6);
	TestEqual(TEXT("Subsystem receives every material service event"),
		SubsystemEvents, ServiceEvents);

	FOpenMobileSensorsCapabilityService::OnChanged().Remove(ServiceHandle);
	Subsystem->OnCapabilitiesChangedNative().Remove(SubsystemHandle);
	Subsystem->Deinitialize();
	FinishBackend(Backend);
	return true;
}

#endif
