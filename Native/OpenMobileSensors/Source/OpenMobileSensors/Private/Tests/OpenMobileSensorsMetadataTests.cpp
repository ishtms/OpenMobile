#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsBackendTypes.h"
#include "OpenMobileSensorsMetadataService.h"
#include "OpenMobileSensorsMockBackend.h"

namespace OpenMobileSensorsMetadataTestsPrivate
{
	FOpenMobileSensorBackendMetadata MakeMetadata(
		EOpenMobileSensorType Type,
		const TCHAR* NativeIdentifier,
		const TCHAR* InstanceId = nullptr
	)
	{
		FOpenMobileSensorBackendMetadata Candidate;
		Candidate.Metadata.Sensor.Type = Type;
		if (InstanceId)
		{
			Candidate.Metadata.Sensor.InstanceId = InstanceId;
		}
		Candidate.NativeIdentifier = NativeIdentifier;
		return Candidate;
	}

	FOpenMobileSensorOptionalNumber Number(double Value)
	{
		FOpenMobileSensorOptionalNumber Result;
		Result.bAvailable = true;
		Result.Value = Value;
		return Result;
	}

	const FOpenMobileSensorMetadata* FindMetadata(
		const TArray<FOpenMobileSensorMetadata>& Metadata,
		EOpenMobileSensorType Type
	)
	{
		return Metadata.FindByPredicate(
			[Type](const FOpenMobileSensorMetadata& Candidate)
			{
				return Candidate.Sensor.Type == Type;
			}
		);
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsMetadataService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsMetadataService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMetadataMissingFieldsTest,
	"OpenMobile.Sensors.Metadata.MissingFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMetadataMissingFieldsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMetadataTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("MissingFields"));
	Backend.SetSensorMetadata({
		MakeMetadata(EOpenMobileSensorType::Accelerometer, TEXT("accel-0"))
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const TArray<FOpenMobileSensorMetadata> Metadata =
		FOpenMobileSensorsMetadataService::GetMetadata();
	TestEqual(TEXT("One discovered sensor is returned"), Metadata.Num(), 1);
	if (Metadata.Num() == 1)
	{
		const FOpenMobileSensorMetadata& Entry = Metadata[0];
		TestFalse(TEXT("Vendor stays unavailable"), Entry.Vendor.bAvailable);
		TestFalse(TEXT("Native name stays unavailable"), Entry.NativeName.bAvailable);
		TestFalse(TEXT("Version stays unavailable"), Entry.Version.bAvailable);
		TestFalse(TEXT("Maximum range stays unavailable"),
			Entry.MaximumRange.bAvailable);
		TestFalse(TEXT("Resolution stays unavailable"),
			Entry.Resolution.bAvailable);
		TestFalse(TEXT("Power stays unavailable"),
			Entry.EstimatedPowerMilliwatts.bAvailable);
		TestFalse(TEXT("Minimum interval stays unavailable"),
			Entry.MinimumIntervalSeconds.bAvailable);
		TestFalse(TEXT("Maximum interval stays unavailable"),
			Entry.MaximumIntervalSeconds.bAvailable);
		TestFalse(TEXT("FIFO capacity stays unavailable"),
			Entry.FifoCapacitySamples.bAvailable);
		TestFalse(TEXT("Wake-up behavior stays unavailable"),
			Entry.WakeUpBehavior.bAvailable);
		TestFalse(TEXT("Reporting mode stays unavailable"),
			Entry.bReportingModeAvailable);
		TestFalse(
			*FString::Printf(
				TEXT("A portable identifier is assigned: %s"),
				*Entry.Sensor.InstanceId.ToString()
			),
			Entry.Sensor.InstanceId.IsNone()
		);
		TestTrue(TEXT("The sole sensor is preferred"), Entry.bPreferred);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMetadataUnitNormalizationTest,
	"OpenMobile.Sensors.Metadata.UnitNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMetadataUnitNormalizationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMetadataTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Units"));
	FOpenMobileSensorBackendMetadata Acceleration = MakeMetadata(
		EOpenMobileSensorType::Accelerometer,
		TEXT("acceleration")
	);
	Acceleration.MeasurementUnit =
		EOpenMobileSensorMetadataUnit::StandardGravity;
	Acceleration.IntervalUnit =
		EOpenMobileSensorMetadataTimeUnit::Milliseconds;
	Acceleration.Metadata.MaximumRange = Number(2.0);
	Acceleration.Metadata.Resolution = Number(0.001);
	Acceleration.Metadata.MinimumIntervalSeconds = Number(10.0);
	Acceleration.Metadata.MaximumIntervalSeconds = Number(1000.0);
	FOpenMobileSensorBackendMetadata Rotation = MakeMetadata(
		EOpenMobileSensorType::Gyroscope,
		TEXT("rotation")
	);
	Rotation.MeasurementUnit =
		EOpenMobileSensorMetadataUnit::DegreesPerSecond;
	Rotation.Metadata.MaximumRange = Number(180.0);
	FOpenMobileSensorBackendMetadata Magnetic = MakeMetadata(
		EOpenMobileSensorType::Magnetometer,
		TEXT("magnetic")
	);
	Magnetic.MeasurementUnit = EOpenMobileSensorMetadataUnit::Tesla;
	Magnetic.Metadata.MaximumRange = Number(0.00005);
	FOpenMobileSensorBackendMetadata Pressure = MakeMetadata(
		EOpenMobileSensorType::BarometricPressure,
		TEXT("pressure")
	);
	Pressure.MeasurementUnit = EOpenMobileSensorMetadataUnit::Pascal;
	Pressure.Metadata.MaximumRange = Number(101325.0);
	FOpenMobileSensorBackendMetadata Proximity = MakeMetadata(
		EOpenMobileSensorType::Proximity,
		TEXT("proximity")
	);
	Proximity.MeasurementUnit = EOpenMobileSensorMetadataUnit::Centimeters;
	Proximity.Metadata.MaximumRange = Number(25.0);
	FOpenMobileSensorBackendMetadata Altitude = MakeMetadata(
		EOpenMobileSensorType::AbsoluteAltitude,
		TEXT("altitude")
	);
	Altitude.MeasurementUnit = EOpenMobileSensorMetadataUnit::Millimeters;
	Altitude.Metadata.Resolution = Number(500.0);
	Backend.SetSensorMetadata({
		Acceleration,
		Rotation,
		Magnetic,
		Pressure,
		Proximity,
		Altitude
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const TArray<FOpenMobileSensorMetadata> Metadata =
		FOpenMobileSensorsMetadataService::GetMetadata();
	const FOpenMobileSensorMetadata* NormalizedAcceleration = FindMetadata(
		Metadata,
		EOpenMobileSensorType::Accelerometer
	);
	TestTrue(TEXT("Gravity units become metres per second squared"),
		FMath::IsNearlyEqual(
			NormalizedAcceleration->MaximumRange.Value,
			19.6133,
			1.e-9
		));
	TestTrue(TEXT("Gravity resolution is normalized"),
		FMath::IsNearlyEqual(
			NormalizedAcceleration->Resolution.Value,
			0.00980665,
			1.e-12
		));
	TestTrue(TEXT("Milliseconds become seconds"),
		FMath::IsNearlyEqual(
			NormalizedAcceleration->MinimumIntervalSeconds.Value,
			0.01,
			1.e-12
		)
		&& FMath::IsNearlyEqual(
			NormalizedAcceleration->MaximumIntervalSeconds.Value,
			1.0,
			1.e-12
		));
	TestTrue(TEXT("Degrees per second become radians per second"),
		FMath::IsNearlyEqual(
			FindMetadata(Metadata, EOpenMobileSensorType::Gyroscope)
				->MaximumRange.Value,
			UE_DOUBLE_PI,
			1.e-12
		));
	TestTrue(TEXT("Tesla become microtesla"),
		FMath::IsNearlyEqual(
			FindMetadata(Metadata, EOpenMobileSensorType::Magnetometer)
				->MaximumRange.Value,
			50.0,
			1.e-9
		));
	TestTrue(TEXT("Pascals become hectopascals"),
		FMath::IsNearlyEqual(
			FindMetadata(Metadata, EOpenMobileSensorType::BarometricPressure)
				->MaximumRange.Value,
			1013.25,
			1.e-9
		));
	TestTrue(TEXT("Centimetres become metres"),
		FMath::IsNearlyEqual(
			FindMetadata(Metadata, EOpenMobileSensorType::Proximity)
				->MaximumRange.Value,
			0.25,
			1.e-12
		));
	TestTrue(TEXT("Millimetres become metres"),
		FMath::IsNearlyEqual(
			FindMetadata(Metadata, EOpenMobileSensorType::AbsoluteAltitude)
				->Resolution.Value,
			0.5,
			1.e-12
		));
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMetadataExtremeValuesTest,
	"OpenMobile.Sensors.Metadata.ExtremeValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMetadataExtremeValuesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMetadataTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Extreme"));
	FOpenMobileSensorBackendMetadata Candidate = MakeMetadata(
		EOpenMobileSensorType::Magnetometer,
		TEXT("extreme")
	);
	Candidate.MeasurementUnit = EOpenMobileSensorMetadataUnit::Tesla;
	Candidate.Metadata.MaximumRange = Number(
		std::numeric_limits<double>::max()
	);
	Candidate.Metadata.Resolution = Number(
		std::numeric_limits<double>::quiet_NaN()
	);
	Candidate.Metadata.EstimatedPowerMilliwatts = Number(-1.0);
	Candidate.Metadata.MinimumIntervalSeconds = Number(
		std::numeric_limits<double>::infinity()
	);
	Candidate.Metadata.FifoCapacitySamples.bAvailable = true;
	Candidate.Metadata.FifoCapacitySamples.Value = -1;
	Backend.SetSensorMetadata({Candidate});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorMetadata Metadata =
		FOpenMobileSensorsMetadataService::GetMetadata()[0];
	TestFalse(TEXT("Overflowed range becomes unavailable"),
		Metadata.MaximumRange.bAvailable);
	TestFalse(TEXT("Non-finite resolution becomes unavailable"),
		Metadata.Resolution.bAvailable);
	TestFalse(TEXT("Negative power becomes unavailable"),
		Metadata.EstimatedPowerMilliwatts.bAvailable);
	TestFalse(TEXT("Infinite interval becomes unavailable"),
		Metadata.MinimumIntervalSeconds.bAvailable);
	TestFalse(TEXT("Negative FIFO capacity becomes unavailable"),
		Metadata.FifoCapacitySamples.bAvailable);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMetadataDuplicateSelectionTest,
	"OpenMobile.Sensors.Metadata.DuplicateSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMetadataDuplicateSelectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMetadataTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Duplicates"));
	FOpenMobileSensorBackendMetadata Beta = MakeMetadata(
		EOpenMobileSensorType::Accelerometer,
		TEXT("native-beta"),
		TEXT("Beta")
	);
	FOpenMobileSensorBackendMetadata PointerLike = MakeMetadata(
		EOpenMobileSensorType::Accelerometer,
		TEXT("native-pointer"),
		TEXT("0xCAFE")
	);
	FOpenMobileSensorBackendMetadata Alpha = MakeMetadata(
		EOpenMobileSensorType::Accelerometer,
		TEXT("native-alpha"),
		TEXT("Alpha")
	);
	Backend.SetSensorMetadata({Beta, PointerLike, Alpha});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const TArray<FOpenMobileSensorMetadata> First =
		FOpenMobileSensorsMetadataService::GetMetadata();
	TestEqual(TEXT("All duplicate sensors are preserved"), First.Num(), 3);
	int32 PreferredCount = 0;
	for (const FOpenMobileSensorMetadata& Entry : First)
	{
		PreferredCount += Entry.bPreferred ? 1 : 0;
		TestFalse(TEXT("Pointer-like identifiers are never public"),
			Entry.Sensor.InstanceId.ToString().StartsWith(TEXT("0x")));
	}
	TestEqual(TEXT("Exactly one duplicate is preferred"), PreferredCount, 1);
	TestEqual(TEXT("Lexical portable identity wins deterministic ties"),
		First[0].Sensor.InstanceId, FName(TEXT("Alpha")));
	TestTrue(TEXT("The deterministic first sensor is preferred"),
		First[0].bPreferred);
	const TArray<FOpenMobileSensorMetadata> Second =
		FOpenMobileSensorsMetadataService::GetMetadata();
	for (int32 Index = 0; Index < First.Num(); ++Index)
	{
		TestEqual(TEXT("Repeated reads preserve sensor ordering"),
			Second[Index].Sensor.InstanceId,
			First[Index].Sensor.InstanceId);
	}
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMetadataMutableRefreshTest,
	"OpenMobile.Sensors.Metadata.MutableRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMetadataMutableRefreshTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMetadataTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Mutable"));
	FOpenMobileSensorBackendMetadata Immutable = MakeMetadata(
		EOpenMobileSensorType::Accelerometer,
		TEXT("immutable"),
		TEXT("Immutable")
	);
	Immutable.Metadata.EstimatedPowerMilliwatts = Number(1.0);
	FOpenMobileSensorBackendMetadata Mutable = MakeMetadata(
		EOpenMobileSensorType::Gyroscope,
		TEXT("mutable"),
		TEXT("Mutable")
	);
	Mutable.bMutable = true;
	Mutable.Metadata.EstimatedPowerMilliwatts = Number(2.0);
	FOpenMobileSensorBackendMetadata Refreshed = Mutable;
	Refreshed.Metadata.EstimatedPowerMilliwatts = Number(3.0);
	Backend.SetSensorMetadata({Immutable, Mutable});
	Backend.SetRefreshedMutableSensorMetadata({Refreshed});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	TArray<FOpenMobileSensorMetadata> Metadata =
		FOpenMobileSensorsMetadataService::GetMetadata();
	TestEqual(TEXT("Discovery is queried once"),
		Backend.GetSensorMetadataQueryCount(), 1);
	TestEqual(TEXT("Initial discovery does not refresh mutable data"),
		Backend.GetMutableSensorMetadataRefreshCount(), 0);
	Metadata = FOpenMobileSensorsMetadataService::GetMetadata();
	TestEqual(TEXT("Repeated reads reuse immutable discovery"),
		Backend.GetSensorMetadataQueryCount(), 1);
	TestEqual(TEXT("Only one mutable refresh is requested"),
		Backend.GetMutableSensorMetadataRefreshCount(), 1);
	TestEqual(TEXT("Only mutable candidates reach refresh"),
		Backend.GetLastMutableSensorMetadataRefreshCount(), 1);
	TestEqual(TEXT("Immutable metadata remains cached"),
		FindMetadata(Metadata, EOpenMobileSensorType::Accelerometer)
			->EstimatedPowerMilliwatts.Value,
		1.0);
	TestEqual(TEXT("Mutable metadata is refreshed"),
		FindMetadata(Metadata, EOpenMobileSensorType::Gyroscope)
			->EstimatedPowerMilliwatts.Value,
		3.0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMetadataBackendGenerationRefreshTest,
	"OpenMobile.Sensors.Metadata.BackendGenerationRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMetadataBackendGenerationRefreshTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMetadataTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend FirstBackend(TEXT("First"));
	FirstBackend.SetSensorMetadata({MakeMetadata(
		EOpenMobileSensorType::Accelerometer,
		TEXT("first"),
		TEXT("First")
	)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(FirstBackend);
	TestEqual(TEXT("First backend metadata is discovered"),
		FOpenMobileSensorsMetadataService::GetMetadata()[0].Sensor.InstanceId,
		FName(TEXT("First")));
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(FirstBackend);
	FOpenMobileSensorsMockBackend SecondBackend(TEXT("Second"));
	SecondBackend.SetSensorMetadata({MakeMetadata(
		EOpenMobileSensorType::Accelerometer,
		TEXT("second"),
		TEXT("Second")
	)});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(SecondBackend);
	TestEqual(TEXT("Replacement backend metadata is discovered"),
		FOpenMobileSensorsMetadataService::GetMetadata()[0].Sensor.InstanceId,
		FName(TEXT("Second")));
	TestEqual(TEXT("First backend was queried once"),
		FirstBackend.GetSensorMetadataQueryCount(), 1);
	TestEqual(TEXT("Second backend was queried once"),
		SecondBackend.GetSensorMetadataQueryCount(), 1);
	FinishBackend(SecondBackend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMetadataNativeMetadataDiagnosticsTest,
	"OpenMobile.Sensors.Metadata.NativeMetadataDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMetadataNativeMetadataDiagnosticsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMetadataTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("Diagnostics"));
	Backend.SetSensorMetadata({
		MakeMetadata(
			EOpenMobileSensorType::Accelerometer,
			TEXT("safe.native-1"),
			TEXT("Safe")
		),
		MakeMetadata(
			EOpenMobileSensorType::Gyroscope,
			TEXT("unsafe\nidentifier"),
			TEXT("Unsafe")
		)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorsMetadataService::GetMetadata();
	const TArray<FString> Diagnostics =
		FOpenMobileSensorsMetadataService::
			GetVerboseNativeMetadataForDiagnostics();
#if UE_BUILD_SHIPPING
	TestTrue(TEXT("Shipping diagnostics omit native metadata"),
		Diagnostics.IsEmpty());
#else
	TestEqual(TEXT("Development diagnostics retain discovered entries"),
		Diagnostics.Num(), 2);
	TestTrue(TEXT("Safe native metadata is retained"),
		Diagnostics.ContainsByPredicate(
			[](const FString& Entry)
			{
				return Entry.Contains(TEXT("safe.native-1"));
			}
		));
	TestTrue(TEXT("Unsafe native metadata is redacted"),
		Diagnostics.ContainsByPredicate(
			[](const FString& Entry)
			{
				return Entry.Contains(TEXT("redacted"));
			}
		));
#endif
	FinishBackend(Backend);
	return true;
}

#endif
