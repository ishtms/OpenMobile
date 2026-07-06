#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorRelativeAltitudeEstimator.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsRelativeAltitudeTestsPrivate
{
	FOpenMobileSensorIdentifier MakeSensor(EOpenMobileSensorType Type)
	{
		FOpenMobileSensorIdentifier Sensor;
		Sensor.Type = Type;
		Sensor.InstanceId = TEXT("Default");
		return Sensor;
	}

	FOpenMobileScalarSensorSample MakeScalar(
		EOpenMobileSensorType Type,
		double TimestampSeconds,
		double Value
	)
	{
		FOpenMobileScalarSensorSample Sample;
		Sample.Header.Sensor = MakeSensor(Type);
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.SourceFlags = Type ==
			EOpenMobileSensorType::BarometricPressure
			? static_cast<int32>(EOpenMobileSensorSourceFlags::Raw)
			: static_cast<int32>(EOpenMobileSensorSourceFlags::NativeFused);
		Sample.Value = Value;
		return Sample;
	}

	FOpenMobileSensorCapability MakePressureCapability(
		EOpenMobileCapabilityState State =
			EOpenMobileCapabilityState::Available
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = MakeSensor(
			EOpenMobileSensorType::BarometricPressure
		);
		Capability.Availability.Name = TEXT("BarometricPressure");
		Capability.Availability.State = State;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 5.0;
		return Capability;
	}

	FOpenMobileSensorCapability MakeNativeRelativeAltitudeCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = MakeSensor(
			EOpenMobileSensorType::RelativeAltitude
		);
		Capability.Availability.Name = TEXT("RelativeAltitude");
		Capability.Availability.State =
			EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 1.0;
		return Capability;
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
	FOpenMobileSensorsRelativeAltitudeEstimatorTest,
	"OpenMobile.Sensors.RelativeAltitude.Estimator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRelativeAltitudeEstimatorTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRelativeAltitudeTestsPrivate;
	FOpenMobileSensorRelativeAltitudeEstimator Estimator;
	const FOpenMobileSensorIdentifier OutputSensor = MakeSensor(
		EOpenMobileSensorType::RelativeAltitude
	);
	FOpenMobileScalarSensorSample Output;
	TestTrue(TEXT("Pressure baseline is accepted"), Estimator.Process(
		MakeScalar(EOpenMobileSensorType::BarometricPressure,
			10.0, 1013.25),
		OutputSensor,
		Output
	));
	TestEqual(TEXT("Pressure baseline starts at zero metres"),
		Output.Value, 0.0);
	TestEqual(TEXT("Pressure derivation source is explicit"),
		Output.RelativeAltitude.Source,
		EOpenMobileRelativeAltitudeSource::PressureBaseline);
	TestEqual(TEXT("Baseline time is retained"),
		Output.RelativeAltitude.BaselineTimestampSeconds, 10.0);
	TestTrue(TEXT("Baseline pressure is available"),
		Output.RelativeAltitude.bHasBaselinePressure);
	TestEqual(TEXT("Baseline pressure is retained"),
		Output.RelativeAltitude.BaselinePressureHectopascals, 1013.25);
	TestTrue(TEXT("Standard atmosphere assumption is explicit"),
		Output.RelativeAltitude.bUsesStandardAtmosphereModel);
	TestTrue(TEXT("Weather sensitivity is reported"),
		(Output.RelativeAltitude.QualityLimitationFlags
			& static_cast<int32>(
				EOpenMobileRelativeAltitudeQualityLimitation::WeatherSensitive
			)) != 0);
	TestTrue(TEXT("Pressure-derived sample is marked derived"),
		(Output.Header.SourceFlags
			& static_cast<int32>(
				EOpenMobileSensorSourceFlags::PluginDerived
			)) != 0);
	Estimator.Process(
		MakeScalar(EOpenMobileSensorType::BarometricPressure,
			10.5, 1013.13),
		OutputSensor,
		Output
	);
	TestTrue(TEXT("Small pressure drift remains visible"),
		Output.Value > 0.0 && Output.Value < 2.0);

	TestTrue(TEXT("Pressure change is converted"), Estimator.Process(
		MakeScalar(EOpenMobileSensorType::BarometricPressure,
			11.0, 1000.0),
		OutputSensor,
		Output
	));
	TestTrue(TEXT("Standard atmosphere fixture matches"),
		FMath::IsNearlyEqual(Output.Value, 110.901045, 1.e-5));
	Estimator.Reset();
	Estimator.Process(
		MakeScalar(EOpenMobileSensorType::BarometricPressure,
			20.0, 1000.0),
		OutputSensor,
		Output
	);
	TestEqual(TEXT("Reset pressure baseline returns to zero"),
		Output.Value, 0.0);

	Estimator.Reset();
	Estimator.Process(
		MakeScalar(EOpenMobileSensorType::RelativeAltitude,
			30.0, 12.5),
		OutputSensor,
		Output
	);
	TestEqual(TEXT("Native session rebases its first value"),
		Output.Value, 0.0);
	TestEqual(TEXT("Native source is explicit"),
		Output.RelativeAltitude.Source,
		EOpenMobileRelativeAltitudeSource::NativePlatform);
	TestFalse(TEXT("Native source does not invent baseline pressure"),
		Output.RelativeAltitude.bHasBaselinePressure);
	Estimator.Process(
		MakeScalar(EOpenMobileSensorType::RelativeAltitude,
			31.0, 15.5),
		OutputSensor,
		Output
	);
	TestEqual(TEXT("Native change is relative to the session"),
		Output.Value, 3.0);
	FOpenMobileScalarSensorSample NativeRestart = MakeScalar(
		EOpenMobileSensorType::RelativeAltitude,
		40.0,
		20.0
	);
	NativeRestart.Header.bStatefulProcessingReset = true;
	Estimator.Process(NativeRestart, OutputSensor, Output);
	TestEqual(TEXT("Native restart establishes a new baseline"),
		Output.Value, 0.0);
	TestEqual(TEXT("Native restart updates baseline time"),
		Output.RelativeAltitude.BaselineTimestampSeconds, 40.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsRelativeAltitudeSessionsTest,
	"OpenMobile.Sensors.RelativeAltitude.Sessions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsRelativeAltitudeSessionsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsRelativeAltitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("RelativeAltitudeSessions"));
	Backend.SetSensorCapabilities({MakePressureCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Capabilities =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Relative =
		Capabilities.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::RelativeAltitude;
			}
		);
	TestNotNull(TEXT("Relative altitude capability is present"), Relative);
	if (Relative)
	{
		TestEqual(TEXT("Android-style fallback is derived"),
			Relative->Source,
			EOpenMobileSensorAvailabilitySource::Derived);
		TestTrue(TEXT("Pressure fallback is available"),
			Relative->Fallback.bAvailable);
	}

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	FOpenMobileSensorStreamOptions Options;
	const FOpenMobileSensorSubscriptionResult First =
		Subsystem->BeginRelativeAltitudeSessionNative(Options);
	const FOpenMobileSensorSubscriptionResult Second =
		Subsystem->BeginRelativeAltitudeSessionNative(Options);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("First session begins"), First.Operation.IsSuccess());
	TestTrue(TEXT("Second session begins"), Second.Operation.IsSuccess());
	TestEqual(TEXT("Concurrent sessions share one pressure stream"),
		Backend.GetStartSensorStreamCount(), 1);

	FOpenMobileSensorsSampleService::PublishScalar(MakeScalar(
		EOpenMobileSensorType::BarometricPressure,
		1.0,
		1013.25
	));
	FOpenMobileSensorReadResult Read;
	FOpenMobileScalarSensorSample FirstSample;
	FOpenMobileScalarSensorSample SecondSample;
	TestTrue(TEXT("First session reads"),
		Subsystem->ReadRelativeAltitudeSessionNative(
			First.Handle, 0, Read, FirstSample));
	TestTrue(TEXT("Second session reads"),
		Subsystem->ReadRelativeAltitudeSessionNative(
			Second.Handle, 0, Read, SecondSample));
	TestEqual(TEXT("First baseline starts at zero"), FirstSample.Value, 0.0);
	TestEqual(TEXT("Second baseline starts at zero"), SecondSample.Value, 0.0);

	FOpenMobileSensorsSampleService::PublishScalar(MakeScalar(
		EOpenMobileSensorType::BarometricPressure,
		2.0,
		1012.88971156
	));
	Subsystem->ReadRelativeAltitudeSessionNative(
		First.Handle, 0, Read, FirstSample);
	TestTrue(TEXT("Stairs fixture rises three metres"),
		FMath::IsNearlyEqual(FirstSample.Value, 3.0, 1.e-6));
	TestTrue(TEXT("One session recenters"),
		Subsystem->RecenterRelativeAltitudeBaselineNative(
			First.Handle).IsSuccess());
	FOpenMobileSensorsSampleService::PublishScalar(MakeScalar(
		EOpenMobileSensorType::BarometricPressure,
		3.0,
		1012.7
	));
	Subsystem->ReadRelativeAltitudeSessionNative(
		First.Handle, 0, Read, FirstSample);
	Subsystem->ReadRelativeAltitudeSessionNative(
		Second.Handle, 0, Read, SecondSample);
	TestEqual(TEXT("Recentered session gets a new zero"),
		FirstSample.Value, 0.0);
	TestTrue(TEXT("Other session keeps its original baseline"),
		SecondSample.Value > 3.0);

	TestTrue(TEXT("First session stops explicitly"),
		Subsystem->StopRelativeAltitudeSessionNative(
			First.Handle).IsSuccess());
	TestEqual(TEXT("Shared pressure stream remains for second session"),
		Backend.GetStopSensorStreamCount(), 0);
	TestTrue(TEXT("Second session stops explicitly"),
		Subsystem->StopRelativeAltitudeSessionNative(
			Second.Handle).IsSuccess());
	TestEqual(TEXT("Last session stops the pressure stream"),
		Backend.GetStopSensorStreamCount(), 1);

	const FOpenMobileSensorSubscriptionResult Restarted =
		Subsystem->BeginRelativeAltitudeSessionNative(Options);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorsSampleService::PublishScalar(MakeScalar(
		EOpenMobileSensorType::BarometricPressure,
		4.0,
		1000.0
	));
	FOpenMobileScalarSensorSample RestartedSample;
	Subsystem->ReadRelativeAltitudeSessionNative(
		Restarted.Handle, 0, Read, RestartedSample);
	TestEqual(TEXT("Restart creates a new baseline"),
		RestartedSample.Value, 0.0);
	Subsystem->StopRelativeAltitudeSessionNative(Restarted.Handle);
	Subsystem->Deinitialize();
	FinishBackend(Backend);

	ResetServices();
	FOpenMobileSensorsMockBackend Native(TEXT("RelativeAltitudeNative"));
	Native.SetSensorCapabilities({
		MakeNativeRelativeAltitudeCapability(),
		MakePressureCapability()
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Native);
	UGameInstance* NativeGameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* NativeSubsystem =
		NewObject<UOpenMobileSensorsSubsystem>(NativeGameInstance);
	const FOpenMobileSensorSubscriptionResult NativeSession =
		NativeSubsystem->BeginRelativeAltitudeSessionNative(Options);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("Native relative-altitude session begins"),
		NativeSession.Operation.IsSuccess());
	TestEqual(TEXT("Native capability avoids pressure fallback"),
		Native.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::RelativeAltitude);
	FOpenMobileSensorsSampleService::PublishScalar(MakeScalar(
		EOpenMobileSensorType::RelativeAltitude,
		5.0,
		50.0
	));
	FOpenMobileSensorsSampleService::PublishScalar(MakeScalar(
		EOpenMobileSensorType::RelativeAltitude,
		6.0,
		52.0
	));
	FOpenMobileScalarSensorSample NativeSample;
	TestTrue(TEXT("Native relative-altitude sample reads"),
		NativeSubsystem->ReadRelativeAltitudeSessionNative(
			NativeSession.Handle, 0, Read, NativeSample));
	TestEqual(TEXT("Native session rebases platform output"),
		NativeSample.Value, 2.0);
	TestEqual(TEXT("Native session reports native source"),
		NativeSample.RelativeAltitude.Source,
		EOpenMobileRelativeAltitudeSource::NativePlatform);
	NativeSubsystem->StopRelativeAltitudeSessionNative(NativeSession.Handle);
	NativeSubsystem->Deinitialize();
	FinishBackend(Native);

	ResetServices();
	FOpenMobileSensorsMockBackend Missing(TEXT("RelativeAltitudeMissing"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Missing);
	UGameInstance* MissingGameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* MissingSubsystem =
		NewObject<UOpenMobileSensorsSubsystem>(MissingGameInstance);
	const FOpenMobileSensorSubscriptionResult Unavailable =
		MissingSubsystem->BeginRelativeAltitudeSessionNative(Options);
	TestEqual(TEXT("Missing pressure rejects derived altitude"),
		Unavailable.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::DerivedInputUnavailable);
	MissingSubsystem->Deinitialize();
	FinishBackend(Missing);
	return true;
}

#endif
