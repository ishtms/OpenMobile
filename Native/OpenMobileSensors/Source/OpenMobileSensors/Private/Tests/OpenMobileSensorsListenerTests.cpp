#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorBlueprintLibrary.h"
#include "OpenMobileSensorListener.h"
#include "OpenMobileSensorPoseEnvironmentListeners.h"
#include "OpenMobileSensorActivityListeners.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsBackendTypes.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace OpenMobileSensorsListenerTestsPrivate
{
	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	FOpenMobileSensorCapability MakeGyroscopeCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::Gyroscope;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("Gyroscope");
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Mock;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 200.0;
		return Capability;
	}

	FOpenMobileSensorCapability MakeCapability(EOpenMobileSensorType Type)
	{
		FOpenMobileSensorCapability Capability = MakeGyroscopeCapability();
		Capability.Sensor.Type = Type;
		Capability.Availability.Name = FOpenMobileSensorTypes::GetStableName(Type);
		return Capability;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGyroscopeListenerTest,
	"OpenMobile.Sensors.Blueprint.Listener.GyroscopeLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGyroscopeListenerTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsListenerTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("GyroscopeListener"));
	Backend.SetSensorCapabilities({MakeGyroscopeCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileSensorsListenerTest"));
	UWorld* World = GameInstance->GetWorld();
	USceneComponent* Owner = NewObject<USceneComponent>(World);
	UOpenMobileSensorsSubsystem* Subsystem =
		GameInstance->GetSubsystem<UOpenMobileSensorsSubsystem>();
	int32 SensorErrorCount = 0;
	FOpenMobileSensorRuntimeError SubsystemError;
	const FDelegateHandle SensorErrorHandle =
		Subsystem->OnSensorErrorNative().AddLambda(
			[&](const FOpenMobileSensorSubscriptionHandle& InHandle,
				const FOpenMobileSensorRuntimeError& Error)
			{
				static_cast<void>(InHandle);
				++SensorErrorCount;
				SubsystemError = Error;
			});

	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const FOpenMobileSensorStreamOptions SavedDefaults =
		Settings->DefaultStreamOptions;
	Settings->DefaultStreamOptions.Filters.bEnableLowPass = true;
	Settings->DefaultStreamOptions.Filters.LowPassTimeConstantSeconds = 0.25;
	Settings->DefaultStreamOptions.BufferCapacitySamples = 1;
	Settings->DefaultStreamOptions.OverflowPolicy =
		EOpenMobileSensorOverflowPolicy::RejectNewest;

	const FOpenMobileSensorStreamOptions EmptyAdvancedOptions;
	UOpenMobileGyroscopeListener* Listener =
		UOpenMobileGyroscopeListener::ListenForGyroscope(
			World,
			EmptyAdvancedOptions,
			EOpenMobileSensorRatePreset::Game,
			EOpenMobileSensorCoordinateSpace::DeviceFixed,
			false,
			Owner
		);
	TestNotNull(TEXT("The gyroscope listener is created"), Listener);
	if (!Listener)
	{
		Settings->DefaultStreamOptions = SavedDefaults;
		return false;
	}
	Listener->Activate();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("The gyroscope listener becomes active"), Listener->IsActive());
	TestEqual(
		TEXT("The listener event path selects event delivery"),
		Listener->GetAppliedOptions().DeliveryMode,
		EOpenMobileSensorDeliveryMode::EventBatches
	);
	TestTrue(
		TEXT("The listener starts from project filter defaults"),
		Listener->GetAppliedOptions().Filters.bEnableLowPass
	);
	TestEqual(
		TEXT("The listener keeps the project filter constant"),
		Listener->GetAppliedOptions().Filters.LowPassTimeConstantSeconds,
		0.25
	);
	TestTrue(TEXT("A focused rate update succeeds"),
		Listener->SetSensorRatePreset(
			EOpenMobileSensorRatePreset::UI, 15.0).IsSuccess());
	TestEqual(TEXT("The focused rate update changes only the preset"),
		Listener->GetAppliedOptions().RatePreset,
		EOpenMobileSensorRatePreset::UI);
	TestTrue(TEXT("A focused rate update preserves filters"),
		Listener->GetAppliedOptions().Filters.bEnableLowPass);
	FOpenMobileSensorFilterOptions UpdatedFilters =
		Listener->GetAppliedOptions().Filters;
	UpdatedFilters.LowPassTimeConstantSeconds = 0.5;
	TestTrue(TEXT("A focused filter update succeeds"),
		Listener->SetSensorFilterOptions(UpdatedFilters).IsSuccess());
	TestEqual(TEXT("The focused filter update preserves the rate preset"),
		Listener->GetAppliedOptions().RatePreset,
		EOpenMobileSensorRatePreset::UI);
	TestEqual(TEXT("The focused filter update changes its constant"),
		Listener->GetAppliedOptions().Filters.LowPassTimeConstantSeconds,
		0.5);

	FOpenMobileVectorSensorSample Sample;
	Sample.Header.Sensor = MakeGyroscopeCapability().Sensor;
	Sample.Header.TimestampSeconds = 1.0;
	Sample.Header.bValid = true;
	Sample.Value = FVector(1.0, 2.0, 3.0);
	FOpenMobileSensorsSampleService::PublishVector(Sample);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FOpenMobileVectorSensorSample OverflowSample = Sample;
		OverflowSample.Header.TimestampSeconds = 1.01 + Index * 0.01;
		OverflowSample.Value.X += Index + 1.0;
		FOpenMobileSensorsSampleService::PublishVector(OverflowSample);
	}
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.01);
	FVector AngularVelocity;
	FOpenMobileSensorSampleInfo SampleInfo;
	TestTrue(
		TEXT("The listener caches its latest angular velocity"),
		Listener->GetLatestAngularVelocity(
			AngularVelocity,
			SampleInfo
		)
	);
	TestEqual(
		TEXT("The listener exposes angular velocity in radians per second"),
		AngularVelocity,
		FVector(1.0, 2.0, 3.0)
	);
	TestEqual(TEXT("The compact sample info keeps sequence"), SampleInfo.Sequence, 1ll);
	FOpenMobileSensorDropInfo DropInfo;
	TestTrue(TEXT("The listener caches its latest drop notification"),
		Listener->GetLastSampleDrop(DropInfo));
	TestEqual(TEXT("The listener reports dropped samples"),
		DropInfo.DroppedSamples,
		2ll);
	TestTrue(TEXT("The active listener accepts a transient backend failure"),
		FOpenMobileSensorsSubscriptionService::FailPhysicalStreamFromBackend(
			FOpenMobileSensorsBackendRegistry::CaptureToken(),
			Backend.GetLastStartedPhysicalHandle(),
			FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::TemporarilyUnavailable,
				TEXT("Test"),
				TEXT("SensorDisconnected"))));
	FOpenMobileSensorRuntimeError ListenerError;
	TestTrue(TEXT("The listener caches its scoped runtime error"),
		Listener->GetLastSensorError(ListenerError));
	TestEqual(TEXT("The listener error preserves its portable reason"),
		ListenerError.Reason,
		EOpenMobileSensorFailureReason::TemporarilyUnavailable);
	TestTrue(TEXT("A temporary listener failure is retryable"),
		ListenerError.bRetryable);
	TestEqual(TEXT("The subsystem broadcasts one compact sensor error"),
		SensorErrorCount, 1);
	TestEqual(TEXT("The subsystem error matches the listener error"),
		SubsystemError.Reason, ListenerError.Reason);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests(
			std::numeric_limits<double>::max());
	TestTrue(TEXT("The listener recovers without diagnostics polling"),
		Listener->IsActive());

	Owner->MarkAsGarbage();
	TestFalse(
		TEXT("The owner check completes after an owner is destroyed"),
		Listener->TickOwnerForTests()
	);
	TestFalse(TEXT("Owner destruction deactivates the listener"), Listener->IsActive());
	TestEqual(
		TEXT("Failure recovery and owner destruction stop both streams"),
		Backend.GetStopSensorStreamCount(),
		2
	);

	Settings->DefaultStreamOptions = SavedDefaults;
	Subsystem->OnSensorErrorNative().Remove(SensorErrorHandle);
	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsListenerCollectionCleanupTest,
	"OpenMobile.Sensors.Blueprint.Listener.CollectionCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsListenerCollectionCleanupTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsListenerTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ListenerCollectionCleanup"));
	Backend.SetSensorCapabilities({MakeGyroscopeCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(
		TEXT("OpenMobileSensorsListenerCollectionCleanup"));
	UWorld* World = GameInstance->GetWorld();
	USceneComponent* Owner = NewObject<USceneComponent>(World);
	UOpenMobileGyroscopeListener* Listener =
		UOpenMobileGyroscopeListener::ListenForGyroscope(
			World,
			FOpenMobileSensorStreamOptions{},
			EOpenMobileSensorRatePreset::Game,
			EOpenMobileSensorCoordinateSpace::DeviceFixed,
			false,
			Owner
		);
	Listener->Activate();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	UOpenMobileSensorsSubsystem* Subsystem =
		GameInstance->GetSubsystem<UOpenMobileSensorsSubsystem>();
	const TArray<UOpenMobileSensorListener*> ManagedListeners =
		Subsystem->GetManagedSensorListenersNative();
	TestEqual(TEXT("The Game Instance exposes one managed listener"),
		ManagedListeners.Num(), 1);
	TestEqual(TEXT("The managed listener keeps its typed object"),
		ManagedListeners[0],
		static_cast<UOpenMobileSensorListener*>(Listener));

	EOpenMobileSensorListenerCleanupOutcome Outcome =
		EOpenMobileSensorListenerCleanupOutcome::NothingToStop;
	TArray<UOpenMobileSensorListener*> StoppedListeners;
	TArray<FOpenMobileSensorListenerCleanupFailure> Failures;
	UOpenMobileSensorBlueprintLibrary::StopSensorListeners(
		{Listener, nullptr},
		Outcome,
		StoppedListeners,
		Failures
	);
	TestEqual(TEXT("Mixed cleanup reports partial failure"), Outcome,
		EOpenMobileSensorListenerCleanupOutcome::SomeFailed);
	TestEqual(TEXT("Cleanup returns the stopped typed listener"),
		StoppedListeners.Num(), 1);
	TestEqual(TEXT("Cleanup reports the invalid collection entry"),
		Failures.Num(), 1);
	TestFalse(TEXT("The stopped listener is no longer active"),
		Listener->IsActive());
	TestEqual(TEXT("Finished listeners leave the managed collection"),
		Subsystem->GetManagedSensorListenersNative().Num(), 0);
	UOpenMobileSensorBlueprintLibrary::StopSensorListeners(
		{Listener},
		Outcome,
		StoppedListeners,
		Failures
	);
	TestEqual(TEXT("Repeated typed cleanup is idempotent"), Outcome,
		EOpenMobileSensorListenerCleanupOutcome::NothingToStop);
	TestTrue(TEXT("Repeated cleanup has no false failures"),
		Failures.IsEmpty());

#if WITH_METADATA
	const UFunction* RawStopAll =
		UOpenMobileSensorsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorsSubsystem,
				StopAllSubscriptionsNative));
	TestNotNull(TEXT("The raw stop-all node is reflected"), RawStopAll);
	if (RawStopAll)
	{
		TestTrue(TEXT("Raw stop-all is in the Advanced palette"),
			RawStopAll->GetMetaData(TEXT("Category")).Contains(
				TEXT("Advanced")));
		TestTrue(TEXT("Raw stop-all names the Game Instance boundary"),
			RawStopAll->GetMetaData(TEXT("DisplayName")).Contains(
				TEXT("Game Instance")));
	}
	const UFunction* TypedCleanup =
		UOpenMobileSensorBlueprintLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorBlueprintLibrary,
				StopSensorListeners));
	TestNotNull(TEXT("Typed listener cleanup is reflected"), TypedCleanup);
	if (TypedCleanup)
	{
		TestEqual(TEXT("Cleanup outcomes become execution pins"),
			TypedCleanup->GetMetaData(TEXT("ExpandEnumAsExecs")),
			FString(TEXT("Outcome")));
	}
#endif

	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsGyroscopeListenerReflectionTest,
	"OpenMobile.Sensors.Blueprint.Listener.GyroscopeReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsGyroscopeListenerReflectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
#if WITH_METADATA
	const UFunction* Factory =
		UOpenMobileGyroscopeListener::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileGyroscopeListener,
				ListenForGyroscope
			)
		);
	TestNotNull(TEXT("The gyroscope listener factory is reflected"), Factory);
	if (Factory)
	{
		TestTrue(
			TEXT("The gyroscope listener is searchable by common terms"),
			Factory->GetMetaData(TEXT("Keywords")).Contains(TEXT("gyro"))
		);
		TestTrue(
			TEXT("The full stream options pin is advanced"),
			Factory->GetMetaData(TEXT("AdvancedDisplay")).Contains(
				TEXT("AdvancedOptions")
			)
		);
	}
	const FMulticastDelegateProperty* SampleEvent =
		FindFProperty<FMulticastDelegateProperty>(
			UOpenMobileGyroscopeListener::StaticClass(),
			GET_MEMBER_NAME_CHECKED(UOpenMobileGyroscopeListener, Sample)
		);
	TestNotNull(TEXT("The listener sample event is reflected"), SampleEvent);
	if (SampleEvent)
	{
		const FProperty* Value = FindFProperty<FProperty>(
			SampleEvent->SignatureFunction,
			TEXT("AngularVelocityRadiansPerSecond")
		);
		TestNotNull(TEXT("The event exposes angular velocity"), Value);
		if (Value)
		{
			TestEqual(
				TEXT("The angular velocity pin states its public unit"),
				Value->GetMetaData(TEXT("DisplayName")),
				FString(TEXT("Angular Velocity (rad/s)"))
			);
		}
	}
	const FMulticastDelegateProperty* DroppedEvent =
		FindFProperty<FMulticastDelegateProperty>(
			UOpenMobileSensorListener::StaticClass(),
			GET_MEMBER_NAME_CHECKED(
				UOpenMobileSensorListener,
				SamplesDropped
			)
		);
	TestNotNull(TEXT("Listeners expose scoped sample loss"), DroppedEvent);
	const FMulticastDelegateProperty* ErrorEvent =
		FindFProperty<FMulticastDelegateProperty>(
			UOpenMobileSensorListener::StaticClass(),
			GET_MEMBER_NAME_CHECKED(
				UOpenMobileSensorListener, SensorError));
	TestNotNull(TEXT("Listeners expose scoped runtime errors"), ErrorEvent);
	TestNotNull(TEXT("The subsystem exposes one compact sensor error event"),
		FindFProperty<FMulticastDelegateProperty>(
			UOpenMobileSensorsSubsystem::StaticClass(),
			GET_MEMBER_NAME_CHECKED(
				UOpenMobileSensorsSubsystem, OnSensorError)));
	const FMulticastDelegateProperty* StartedEvent =
		FindFProperty<FMulticastDelegateProperty>(
			UOpenMobileSensorListener::StaticClass(),
			GET_MEMBER_NAME_CHECKED(UOpenMobileSensorListener, Started));
	TestNotNull(TEXT("The listener started event is reflected"), StartedEvent);
	if (StartedEvent)
	{
		TestNotNull(TEXT("Started exposes every resolved stream option"),
			FindFProperty<FProperty>(
				StartedEvent->SignatureFunction,
				TEXT("AppliedOptions")));
		TestNotNull(TEXT("Started exposes applied background behavior"),
			FindFProperty<FProperty>(
				StartedEvent->SignatureFunction,
				TEXT("BackgroundBehavior")));
	}
	TestNotNull(TEXT("Listeners expose shared-rate power warnings"),
		FindFProperty<FMulticastDelegateProperty>(
			UOpenMobileSensorListener::StaticClass(),
			TEXT("SharedStreamRateRaised")));
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMotionListenerReflectionTest,
	"OpenMobile.Sensors.Blueprint.Listener.MotionReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMotionListenerReflectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
#if WITH_METADATA
	struct FMotionListenerContract
	{
		UClass* Class;
		FName FactoryName;
		FName SampleName;
		const TCHAR* SearchTerm;
		const TCHAR* ValueParameter;
		const TCHAR* ValueDisplayName;
	};
	const FMotionListenerContract Contracts[] = {
		{UOpenMobileAccelerometerListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileAccelerometerListener,
				ListenForAccelerometer),
			GET_MEMBER_NAME_CHECKED(UOpenMobileAccelerometerListener, Sample),
			TEXT("accelerometer"),
			TEXT("AccelerationMetresPerSecondSquared"),
			TEXT("Acceleration (m/s2)")},
		{UOpenMobileMagnetometerListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileMagnetometerListener,
				ListenForMagnetometer),
			GET_MEMBER_NAME_CHECKED(UOpenMobileMagnetometerListener, Sample),
			TEXT("magnetometer"),
			TEXT("MagneticFieldMicroteslas"),
			TEXT("Magnetic Field (uT)")},
		{UOpenMobileGravityListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileGravityListener,
				ListenForGravity),
			GET_MEMBER_NAME_CHECKED(UOpenMobileGravityListener, Sample),
			TEXT("gravity"),
			TEXT("GravityMetresPerSecondSquared"),
			TEXT("Gravity (m/s2)")},
		{UOpenMobileLinearAccelerationListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileLinearAccelerationListener,
				ListenForLinearAcceleration),
			GET_MEMBER_NAME_CHECKED(
				UOpenMobileLinearAccelerationListener,
				Sample),
			TEXT("linear acceleration"),
			TEXT("LinearAccelerationMetresPerSecondSquared"),
			TEXT("Linear Acceleration (m/s2)")},
		{UOpenMobileShakeListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileShakeListener,
				ListenForShake),
			GET_MEMBER_NAME_CHECKED(UOpenMobileShakeListener, Sample),
			TEXT("shake"),
			TEXT("StrengthMetresPerSecondSquared"),
			TEXT("Strength (m/s2)")}
	};
	for (const FMotionListenerContract& Contract : Contracts)
	{
		const UFunction* Factory =
			Contract.Class->FindFunctionByName(Contract.FactoryName);
		TestNotNull(TEXT("The motion listener factory is reflected"), Factory);
		if (Factory)
		{
			TestTrue(TEXT("The motion listener is searchable by feature name"),
				Factory->GetMetaData(TEXT("Keywords")).Contains(
					Contract.SearchTerm));
		}
		const FMulticastDelegateProperty* SampleEvent =
			FindFProperty<FMulticastDelegateProperty>(
				Contract.Class,
				Contract.SampleName
			);
		TestNotNull(TEXT("The motion sample event is reflected"), SampleEvent);
		if (SampleEvent)
		{
			const FProperty* Value = FindFProperty<FProperty>(
				SampleEvent->SignatureFunction,
				Contract.ValueParameter
			);
			TestNotNull(TEXT("The motion event exposes its primary value"), Value);
			if (Value)
			{
				TestEqual(TEXT("The motion value pin states its unit"),
					Value->GetMetaData(TEXT("DisplayName")),
					FString(Contract.ValueDisplayName));
			}
		}
	}
	for (const FName FunctionName : {
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorListener, SetSensorRatePreset),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorListener, SetSensorCoordinateSpace),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorListener, SetSensorLifecyclePolicy),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileSensorListener, SetSensorFilterOptions)})
	{
		TestNotNull(TEXT("The focused listener update is reflected"),
			UOpenMobileSensorListener::StaticClass()->FindFunctionByName(
				FunctionName));
	}
	TestNotNull(TEXT("Magnetometer calibration stays on its listener"),
		UOpenMobileMagnetometerListener::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileMagnetometerListener,
				RequestMagnetometerCalibration)));
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPoseEnvironmentListenerReflectionTest,
	"OpenMobile.Sensors.Blueprint.Listener.PoseEnvironmentReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPoseEnvironmentListenerReflectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
#if WITH_METADATA
	struct FListenerContract
	{
		UClass* Class;
		FName FactoryName;
		FName SampleName;
		const TCHAR* SearchTerm;
		const TCHAR* ValueParameter;
		const TCHAR* ValueDisplayName;
	};
	const FListenerContract Contracts[] = {
		{UOpenMobileAttitudeListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileAttitudeListener,
				ListenForAttitude),
			GET_MEMBER_NAME_CHECKED(UOpenMobileAttitudeListener, Sample),
			TEXT("attitude"), TEXT("Rotation"), TEXT("Rotation")},
		{UOpenMobileMagneticHeadingListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileMagneticHeadingListener,
				ListenForMagneticHeading),
			GET_MEMBER_NAME_CHECKED(UOpenMobileMagneticHeadingListener, Sample),
			TEXT("magnetic heading"),
			TEXT("HeadingDegrees"), TEXT("Heading (degrees)")},
		{UOpenMobileTrueHeadingListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileTrueHeadingListener,
				ListenForTrueHeading),
			GET_MEMBER_NAME_CHECKED(UOpenMobileTrueHeadingListener, Sample),
			TEXT("true heading"),
			TEXT("HeadingDegrees"), TEXT("Heading (degrees)")},
		{UOpenMobilePressureListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobilePressureListener,
				ListenForPressure),
			GET_MEMBER_NAME_CHECKED(UOpenMobilePressureListener, Sample),
			TEXT("pressure"),
			TEXT("PressureHectopascals"), TEXT("Pressure (hPa)")},
		{UOpenMobileRelativeAltitudeListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileRelativeAltitudeListener,
				ListenForRelativeAltitude),
			GET_MEMBER_NAME_CHECKED(UOpenMobileRelativeAltitudeListener, Sample),
			TEXT("relative altitude"),
			TEXT("AltitudeMetres"), TEXT("Altitude (m)")},
		{UOpenMobileAbsoluteAltitudeListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileAbsoluteAltitudeListener,
				ListenForAbsoluteAltitude),
			GET_MEMBER_NAME_CHECKED(UOpenMobileAbsoluteAltitudeListener, Sample),
			TEXT("absolute altitude"),
			TEXT("AltitudeMetres"), TEXT("Altitude (m)")},
		{UOpenMobileAmbientLightListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileAmbientLightListener,
				ListenForAmbientLight),
			GET_MEMBER_NAME_CHECKED(UOpenMobileAmbientLightListener, Sample),
			TEXT("ambient light"),
			TEXT("IlluminanceLux"), TEXT("Illuminance (lux)")},
		{UOpenMobileProximityListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileProximityListener,
				ListenForProximity),
			GET_MEMBER_NAME_CHECKED(UOpenMobileProximityListener, Sample),
			TEXT("proximity"), TEXT("bIsNear"), TEXT("Is Near")},
		{UOpenMobilePhysicalOrientationListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobilePhysicalOrientationListener,
				ListenForPhysicalOrientation),
			GET_MEMBER_NAME_CHECKED(
				UOpenMobilePhysicalOrientationListener,
				Sample),
			TEXT("physical orientation"),
			TEXT("Orientation"), TEXT("Orientation")}
	};
	for (const FListenerContract& Contract : Contracts)
	{
		const UFunction* Factory =
			Contract.Class->FindFunctionByName(Contract.FactoryName);
		TestNotNull(TEXT("The typed listener factory is reflected"), Factory);
		if (Factory)
		{
			TestTrue(TEXT("The typed listener is searchable by feature name"),
				Factory->GetMetaData(TEXT("Keywords")).Contains(
					Contract.SearchTerm));
		}
		const FMulticastDelegateProperty* SampleEvent =
			FindFProperty<FMulticastDelegateProperty>(
				Contract.Class,
				Contract.SampleName
			);
		TestNotNull(TEXT("The typed sample event is reflected"), SampleEvent);
		if (SampleEvent)
		{
			const FProperty* Value = FindFProperty<FProperty>(
				SampleEvent->SignatureFunction,
				Contract.ValueParameter
			);
			TestNotNull(TEXT("The typed event exposes its primary value"), Value);
			if (Value)
			{
				TestEqual(TEXT("The typed value pin states its meaning and unit"),
					Value->GetMetaData(TEXT("DisplayName")),
					FString(Contract.ValueDisplayName));
			}
		}
	}
	for (const FName FunctionName : {
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileAttitudeListener, SetAttitudeReferenceFrame),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileAttitudeListener, RecenterAttitude),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileAttitudeListener, ClearAttitudeRecenter),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileAttitudeListener, RequestAttitudeCalibration),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileMagneticHeadingListener,
			RequestHeadingCalibration)})
	{
		TestNotNull(TEXT("The typed pose control is reflected"),
			UOpenMobileAttitudeListener::StaticClass()->FindFunctionByName(
				FunctionName)
				? UOpenMobileAttitudeListener::StaticClass()->
					FindFunctionByName(FunctionName)
				: UOpenMobileMagneticHeadingListener::StaticClass()->
					FindFunctionByName(FunctionName));
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsListenerSampleFamiliesTest,
	"OpenMobile.Sensors.Blueprint.Listener.SampleFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsListenerSampleFamiliesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsListenerTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("ListenerSampleFamilies"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Attitude),
		MakeCapability(EOpenMobileSensorType::BarometricPressure),
		MakeCapability(EOpenMobileSensorType::MagneticHeading),
		MakeCapability(EOpenMobileSensorType::Proximity),
		MakeCapability(EOpenMobileSensorType::PhysicalOrientation),
		MakeCapability(EOpenMobileSensorType::StepCounter),
		MakeCapability(EOpenMobileSensorType::MotionActivity)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileSensorsFamilyListenerTest"));
	UWorld* World = GameInstance->GetWorld();
	USceneComponent* Owner = NewObject<USceneComponent>(World);
	const FOpenMobileSensorStreamOptions AdvancedOptions;
	UOpenMobileAttitudeListener* Attitude =
		UOpenMobileAttitudeListener::ListenForAttitude(
			World, AdvancedOptions, EOpenMobileSensorRatePreset::Game,
			EOpenMobileSensorCoordinateSpace::DeviceFixed, false, Owner);
	UOpenMobilePressureListener* Pressure =
		UOpenMobilePressureListener::ListenForPressure(
			World, AdvancedOptions, EOpenMobileSensorRatePreset::UI,
			false, Owner);
	UOpenMobileMagneticHeadingListener* Heading =
		UOpenMobileMagneticHeadingListener::ListenForMagneticHeading(
			World, AdvancedOptions, EOpenMobileSensorRatePreset::UI,
			EOpenMobileSensorCoordinateSpace::DeviceFixed, false, Owner);
	UOpenMobileProximityListener* Proximity =
		UOpenMobileProximityListener::ListenForProximity(
			World, AdvancedOptions, EOpenMobileSensorRatePreset::UI,
			false, Owner);
	UOpenMobilePhysicalOrientationListener* Orientation =
		UOpenMobilePhysicalOrientationListener::ListenForPhysicalOrientation(
			World, AdvancedOptions, EOpenMobileSensorRatePreset::UI,
			false, Owner);
	UOpenMobileStepCountListener* StepCount =
		UOpenMobileStepCountListener::ListenForStepCount(
			World, AdvancedOptions, EOpenMobileSensorRatePreset::UI,
			false, Owner);
	UOpenMobileMotionActivityListener* MotionActivity =
		UOpenMobileMotionActivityListener::ListenForMotionActivity(
			World, AdvancedOptions, EOpenMobileSensorRatePreset::UI,
			false, Owner);
	const TArray<UOpenMobileSensorListener*> Listeners = {
		Attitude, Pressure, Heading, Proximity, Orientation,
		StepCount, MotionActivity
	};
	for (UOpenMobileSensorListener* Listener : Listeners)
	{
		Listener->Activate();
	}
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	for (UOpenMobileSensorListener* Listener : Listeners)
	{
		TestTrue(TEXT("Each sample-family listener becomes active"),
			Listener->IsActive());
	}
	TestTrue(TEXT("Attitude reference updates stay on the typed listener"),
		Attitude->SetAttitudeReferenceFrame(
			EOpenMobileAttitudeReferenceFrame::ArbitraryVertical).IsSuccess());
	TestEqual(TEXT("The attitude listener reports its applied reference"),
		Attitude->GetAppliedOptions().AttitudeReferenceFrame,
		EOpenMobileAttitudeReferenceFrame::ArbitraryVertical);
	TestTrue(TEXT("Activity thresholds update without replacing options"),
		MotionActivity->SetActivityThresholds(
			EOpenMobileActivityConfidence::High, 0.0).IsSuccess());
	TestEqual(TEXT("The activity listener reports applied confidence"),
		MotionActivity->GetAppliedOptions().MinimumActivityConfidence,
		EOpenMobileActivityConfidence::High);
	EOpenMobileSensorControlOutcome RecenterOutcome =
		EOpenMobileSensorControlOutcome::Failed;
	FText ControlMessage;
	FText ControlCorrection;
	FOpenMobileSensorOperationResult ControlDetails;
	Attitude->RecenterAttitude(
		EOpenMobileSensorRecenterMode::YawOnly,
		RecenterOutcome,
		ControlMessage,
		ControlCorrection,
		ControlDetails);
	TestEqual(TEXT("Recentering before the first pose fails explicitly"),
		RecenterOutcome,
		EOpenMobileSensorControlOutcome::Failed);
	Attitude->RequestAttitudeCalibration(
		RecenterOutcome,
		ControlMessage,
		ControlCorrection,
		ControlDetails);
	TestEqual(TEXT("Unsupported calibration has its own execution outcome"),
		RecenterOutcome,
		EOpenMobileSensorControlOutcome::NotSupported);

	FOpenMobileAttitudeSensorSample AttitudeSample;
	AttitudeSample.Header.Sensor =
		MakeCapability(EOpenMobileSensorType::Attitude).Sensor;
	AttitudeSample.Header.TimestampSeconds = 1.0;
	AttitudeSample.Header.bValid = true;
	AttitudeSample.Quaternion = FQuat(FVector::UpVector, 0.5);
	AttitudeSample.ReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::ArbitraryVertical;
	FOpenMobileSensorsSampleService::PublishAttitude(AttitudeSample);
	FOpenMobileScalarSensorSample PressureSample;
	PressureSample.Header.Sensor =
		MakeCapability(EOpenMobileSensorType::BarometricPressure).Sensor;
	PressureSample.Header.TimestampSeconds = 1.0;
	PressureSample.Header.bValid = true;
	PressureSample.Value = 1001.25;
	FOpenMobileSensorsSampleService::PublishScalar(PressureSample);
	FOpenMobileHeadingSensorSample HeadingSample;
	HeadingSample.Header.Sensor =
		MakeCapability(EOpenMobileSensorType::MagneticHeading).Sensor;
	HeadingSample.Header.TimestampSeconds = 1.0;
	HeadingSample.Header.bValid = true;
	HeadingSample.HeadingDegrees = 42.0;
	FOpenMobileSensorsSampleService::PublishHeading(HeadingSample);
	FOpenMobileProximitySensorSample ProximitySample;
	ProximitySample.Header.Sensor =
		MakeCapability(EOpenMobileSensorType::Proximity).Sensor;
	ProximitySample.Header.TimestampSeconds = 1.0;
	ProximitySample.Header.bValid = true;
	ProximitySample.bNear = true;
	FOpenMobileSensorsSampleService::PublishProximity(ProximitySample);
	FOpenMobileOrientationSensorSample OrientationSample;
	OrientationSample.Header.Sensor =
		MakeCapability(EOpenMobileSensorType::PhysicalOrientation).Sensor;
	OrientationSample.Header.TimestampSeconds = 1.0;
	OrientationSample.Header.bValid = true;
	OrientationSample.Orientation = EOpenMobilePhysicalOrientation::FaceUp;
	OrientationSample.Confidence = 0.8;
	FOpenMobileSensorsSampleService::PublishOrientation(OrientationSample);
	FOpenMobileStepsSensorSample StepsSample;
	StepsSample.Header.Sensor =
		MakeCapability(EOpenMobileSensorType::StepCounter).Sensor;
	StepsSample.Header.TimestampSeconds = 1.0;
	StepsSample.Header.bValid = true;
	StepsSample.Count = 123;
	FOpenMobileSensorsSampleService::PublishSteps(StepsSample);
	FOpenMobileActivitySensorSample ActivitySample;
	ActivitySample.Header.Sensor =
		MakeCapability(EOpenMobileSensorType::MotionActivity).Sensor;
	ActivitySample.Header.TimestampSeconds = 1.0;
	ActivitySample.Header.bValid = true;
	ActivitySample.Activity = EOpenMobileMotionActivity::Walking;
	ActivitySample.Confidence = EOpenMobileActivityConfidence::High;
	FOpenMobileSensorsSampleService::PublishActivity(ActivitySample);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.0);
	Attitude->RecenterAttitude(
		EOpenMobileSensorRecenterMode::YawOnly,
		RecenterOutcome,
		ControlMessage,
		ControlCorrection,
		ControlDetails);
	TestEqual(TEXT("Attitude recentering succeeds after a pose sample"),
		RecenterOutcome,
		EOpenMobileSensorControlOutcome::Succeeded);

	FOpenMobileSensorSampleInfo SampleInfo;
	FQuat Rotation;
	bool bHasEuler = false;
	FRotator Euler;
	TestTrue(TEXT("Attitude listeners receive attitude batches"),
		Attitude->GetLatestAttitude(
			Rotation, bHasEuler, Euler, SampleInfo));
	TestTrue(TEXT("Attitude listeners keep the rotation"),
		Rotation.Equals(AttitudeSample.Quaternion));
	double ScalarValue = 0.0;
	TestTrue(TEXT("Scalar listeners receive scalar batches"),
		Pressure->GetLatestPressure(ScalarValue, SampleInfo));
	TestEqual(TEXT("Scalar listeners keep the value"), ScalarValue, 1001.25);
	double HeadingValue = 0.0;
	TestTrue(TEXT("Heading listeners receive heading batches"),
		Heading->GetLatestHeading(HeadingValue, SampleInfo));
	TestEqual(TEXT("Heading listeners keep degrees"), HeadingValue, 42.0);
	bool bNear = false;
	bool bHasDistance = false;
	double Distance = 0.0;
	TestTrue(TEXT("Proximity listeners receive proximity batches"),
		Proximity->GetLatestProximity(
			bNear, bHasDistance, Distance, SampleInfo));
	TestTrue(TEXT("Proximity listeners keep the near state"), bNear);
	EOpenMobilePhysicalOrientation PhysicalOrientation;
	double Confidence = 0.0;
	TestTrue(TEXT("Orientation listeners receive orientation batches"),
		Orientation->GetLatestOrientation(
			PhysicalOrientation, Confidence, SampleInfo));
	TestEqual(TEXT("Orientation listeners keep the classified pose"),
		PhysicalOrientation,
		EOpenMobilePhysicalOrientation::FaceUp);
	TestEqual(TEXT("Orientation listeners keep confidence"), Confidence, 0.8);
	int64 Steps = 0;
	TestTrue(TEXT("Steps listeners receive steps batches"),
		StepCount->GetLatestSteps(Steps, SampleInfo));
	TestEqual(TEXT("Steps listeners keep the count"), Steps, 123ll);
	EOpenMobileMotionActivity Activity;
	EOpenMobileActivityConfidence ActivityConfidence;
	TestTrue(TEXT("Activity listeners receive activity batches"),
		MotionActivity->GetLatestActivity(
			Activity, ActivityConfidence, SampleInfo));
	TestEqual(TEXT("Activity listeners keep the classification"),
		Activity,
		EOpenMobileMotionActivity::Walking);
	TestEqual(TEXT("Activity listeners keep confidence"),
		ActivityConfidence,
		EOpenMobileActivityConfidence::High);

	for (UOpenMobileSensorListener* Listener : Listeners)
	{
		Listener->Stop();
	}
	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTypedSessionControlsTest,
	"OpenMobile.Sensors.Blueprint.Listener.TypedSessionControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTypedSessionControlsTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsListenerTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("TypedSessionControls"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::StepCounter),
		MakeCapability(EOpenMobileSensorType::RelativeAltitude)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("OpenMobileSensorsSessionListenerTest"));
	UWorld* World = GameInstance->GetWorld();
	USceneComponent* Owner = NewObject<USceneComponent>(World);
	const FOpenMobileSensorStreamOptions AdvancedOptions;
	UOpenMobileStepCountListener* Steps =
		UOpenMobileStepCountListener::ListenForStepCount(
			World, AdvancedOptions, EOpenMobileSensorRatePreset::UI,
			false, Owner);
	UOpenMobileRelativeAltitudeListener* Altitude =
		UOpenMobileRelativeAltitudeListener::ListenForRelativeAltitude(
			World, AdvancedOptions, EOpenMobileSensorRatePreset::UI,
			false, Owner);
	Steps->Activate();
	Altitude->Activate();
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("Typed step session becomes active"), Steps->IsActive());
	TestTrue(TEXT("Typed altitude session becomes active"), Altitude->IsActive());

	FOpenMobileStepsSensorSample StepSample;
	StepSample.Header.Sensor =
		MakeCapability(EOpenMobileSensorType::StepCounter).Sensor;
	StepSample.Header.TimestampSeconds = 1.0;
	StepSample.Header.bValid = true;
	StepSample.Count = 100;
	StepSample.Origin = EOpenMobileStepCountOrigin::DeviceBoot;
	StepSample.OriginIdentifier = FGuid::NewGuid();
	FOpenMobileSensorsSampleService::PublishSteps(StepSample);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(1.0);
	int64 StepCount = -1;
	FOpenMobileSensorSampleInfo SampleInfo;
	TestTrue(TEXT("Typed step session receives its baseline"),
		Steps->GetLatestSteps(StepCount, SampleInfo));
	TestEqual(TEXT("Typed step session starts from zero"), StepCount, 0LL);
	EOpenMobileSensorControlOutcome Outcome =
		EOpenMobileSensorControlOutcome::Failed;
	FText Message;
	FText Correction;
	FOpenMobileSensorOperationResult Details;
	Steps->ResetStepCount(Outcome, Message, Correction, Details);
	TestEqual(TEXT("Typed step reset succeeds"),
		Outcome, EOpenMobileSensorControlOutcome::Succeeded);
	StepSample.Header.TimestampSeconds = 2.0;
	StepSample.Count = 110;
	FOpenMobileSensorsSampleService::PublishSteps(StepSample);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(2.0);
	Steps->GetLatestSteps(StepCount, SampleInfo);
	TestEqual(TEXT("Typed step reset establishes a new zero"), StepCount, 0LL);
	StepSample.Header.TimestampSeconds = 3.0;
	StepSample.Count = 115;
	FOpenMobileSensorsSampleService::PublishSteps(StepSample);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(3.0);
	Steps->GetLatestSteps(StepCount, SampleInfo);
	TestEqual(TEXT("Typed step session continues from its reset"),
		StepCount, 5LL);

	FOpenMobileScalarSensorSample AltitudeSample;
	AltitudeSample.Header.Sensor =
		MakeCapability(EOpenMobileSensorType::RelativeAltitude).Sensor;
	AltitudeSample.Header.TimestampSeconds = 1.0;
	AltitudeSample.Header.bValid = true;
	AltitudeSample.Value = 50.0;
	FOpenMobileSensorsSampleService::PublishScalar(AltitudeSample);
	AltitudeSample.Header.TimestampSeconds = 2.0;
	AltitudeSample.Value = 52.0;
	FOpenMobileSensorsSampleService::PublishScalar(AltitudeSample);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(3.0);
	double RelativeAltitude = 0.0;
	TestTrue(TEXT("Typed altitude session receives samples"),
		Altitude->GetLatestAltitude(RelativeAltitude, SampleInfo));
	TestEqual(TEXT("Typed altitude is relative to its first sample"),
		RelativeAltitude, 2.0);
	Altitude->RecenterAltitudeBaseline(
		Outcome, Message, Correction, Details);
	TestEqual(TEXT("Typed altitude recenter succeeds"),
		Outcome, EOpenMobileSensorControlOutcome::Succeeded);
	AltitudeSample.Header.TimestampSeconds = 4.0;
	AltitudeSample.Value = 54.0;
	FOpenMobileSensorsSampleService::PublishScalar(AltitudeSample);
	FOpenMobileSensorsSampleService::DrainPendingEventsForTests(4.0);
	Altitude->GetLatestAltitude(RelativeAltitude, SampleInfo);
	TestEqual(TEXT("Typed altitude recenter establishes a new zero"),
		RelativeAltitude, 0.0);
#if WITH_METADATA
	const UFunction* ResetFunction =
		UOpenMobileStepCountListener::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileStepCountListener, ResetStepCount));
	const UFunction* RecenterFunction =
		UOpenMobileRelativeAltitudeListener::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileRelativeAltitudeListener,
				RecenterAltitudeBaseline));
	TestNotNull(TEXT("Typed step reset is reflected"), ResetFunction);
	TestNotNull(TEXT("Typed altitude recenter is reflected"), RecenterFunction);
	if (ResetFunction)
	{
		TestEqual(TEXT("Step reset exposes explicit outcomes"),
			ResetFunction->GetMetaData(TEXT("ExpandEnumAsExecs")),
			FString(TEXT("Outcome")));
	}
	if (RecenterFunction)
	{
		TestEqual(TEXT("Altitude recenter exposes explicit outcomes"),
			RecenterFunction->GetMetaData(TEXT("ExpandEnumAsExecs")),
			FString(TEXT("Outcome")));
	}
	const UFunction* LegacyStepSession =
		UOpenMobileSensorsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorsSubsystem,
				BeginStepCountSessionNative));
	TestNotNull(TEXT("The legacy step-session function remains reflected"),
		LegacyStepSession);
	if (LegacyStepSession)
	{
		TestTrue(TEXT("The legacy step-session function is advanced"),
			LegacyStepSession->GetMetaData(TEXT("Category"))
				.StartsWith(TEXT("OpenMobile|Sensors|Advanced")));
		TestTrue(TEXT("The legacy step-session function directs migration"),
			LegacyStepSession->HasMetaData(TEXT("DeprecatedFunction")));
	}
#endif

	Steps->Stop();
	Altitude->Stop();
	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	ResetServices();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsActivityListenerReflectionTest,
	"OpenMobile.Sensors.Blueprint.Listener.ActivityReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsActivityListenerReflectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
#if WITH_METADATA
	struct FActivityListenerContract
	{
		UClass* Class;
		FName FactoryName;
		FName SampleName;
		const TCHAR* SearchTerm;
		const TCHAR* ValueParameter;
		const TCHAR* ValueDisplayName;
	};
	const FActivityListenerContract Contracts[] = {
		{UOpenMobileStepCountListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileStepCountListener,
				ListenForStepCount),
			GET_MEMBER_NAME_CHECKED(UOpenMobileStepCountListener, Sample),
			TEXT("step count"), TEXT("Steps"), TEXT("Steps")},
		{UOpenMobileStepEventListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileStepEventListener,
				ListenForStepEvents),
			GET_MEMBER_NAME_CHECKED(UOpenMobileStepEventListener, Sample),
			TEXT("step event"),
			TEXT("DetectedStepDelta"), TEXT("Detected Step Delta")},
		{UOpenMobilePedometerListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobilePedometerListener,
				ListenForPedometer),
			GET_MEMBER_NAME_CHECKED(UOpenMobilePedometerListener, Sample),
			TEXT("pedometer"), TEXT("Steps"), TEXT("Steps")},
		{UOpenMobileMotionActivityListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileMotionActivityListener,
				ListenForMotionActivity),
			GET_MEMBER_NAME_CHECKED(UOpenMobileMotionActivityListener, Sample),
			TEXT("motion activity"),
			TEXT("Activity"), TEXT("Activity")},
		{UOpenMobileActivityTransitionListener::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileActivityTransitionListener,
				ListenForActivityTransitions),
			GET_MEMBER_NAME_CHECKED(
				UOpenMobileActivityTransitionListener,
				Sample),
			TEXT("activity transition"),
			TEXT("Activity"), TEXT("Activity")}
	};
	for (const FActivityListenerContract& Contract : Contracts)
	{
		const UFunction* Factory =
			Contract.Class->FindFunctionByName(Contract.FactoryName);
		TestNotNull(TEXT("The activity listener factory is reflected"), Factory);
		if (Factory)
		{
			TestTrue(TEXT("The activity listener is searchable by feature name"),
				Factory->GetMetaData(TEXT("Keywords")).Contains(
					Contract.SearchTerm));
		}
		const FMulticastDelegateProperty* SampleEvent =
			FindFProperty<FMulticastDelegateProperty>(
				Contract.Class,
				Contract.SampleName
			);
		TestNotNull(TEXT("The activity sample event is reflected"), SampleEvent);
		if (SampleEvent)
		{
			const FProperty* Value = FindFProperty<FProperty>(
				SampleEvent->SignatureFunction,
				Contract.ValueParameter
			);
			TestNotNull(TEXT("The activity event exposes its primary value"), Value);
			if (Value)
			{
				TestEqual(TEXT("The activity value pin states its meaning"),
					Value->GetMetaData(TEXT("DisplayName")),
					FString(Contract.ValueDisplayName));
			}
		}
	}
#endif
	return true;
}

#endif
