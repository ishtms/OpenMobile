#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileNativeStepCount.h"
#include "OpenMobileSensorBlueprintLibrary.h"
#include "OpenMobileSensorOptionalValueLibrary.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintOperationHelperTest,
	"OpenMobile.Sensors.Blueprint.Helpers.OperationAndHandle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintOperationHelperTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorOperationResult Result;
	Result.Code = EOpenMobileSensorResultCode::Success;
	TestTrue(
		TEXT("Success is successful"),
		UOpenMobileSensorBlueprintLibrary::WasSensorOperationSuccessful(Result)
	);
	Result.Code = EOpenMobileSensorResultCode::Accepted;
	TestTrue(
		TEXT("Accepted asynchronous work is successful"),
		UOpenMobileSensorBlueprintLibrary::WasSensorOperationSuccessful(Result)
	);
	Result.Code = EOpenMobileSensorResultCode::Failed;
	TestFalse(
		TEXT("Failure is not successful"),
		UOpenMobileSensorBlueprintLibrary::WasSensorOperationSuccessful(Result)
	);
	FOpenMobileSensorOperationResult BranchResult;
	BranchResult.Code = EOpenMobileSensorResultCode::Accepted;
	TestTrue(
		TEXT("The branch helper returns the same success rule"),
		UOpenMobileSensorBlueprintLibrary::BranchOnSensorOperationResult(
			BranchResult
		)
	);

	FOpenMobileSensorSubscriptionHandle Handle =
		UOpenMobileSensorBlueprintLibrary::MakeInvalidSensorSubscriptionHandle();
	TestFalse(
		TEXT("The intentional invalid handle is invalid"),
		UOpenMobileSensorBlueprintLibrary::IsSensorSubscriptionHandleValid(
			Handle
		)
	);
	TestTrue(
		TEXT("Two invalid handles compare equal"),
		UOpenMobileSensorBlueprintLibrary::AreSensorSubscriptionHandlesEqual(
			Handle,
			FOpenMobileSensorSubscriptionHandle()
		)
	);
	UOpenMobileSensorBlueprintLibrary::InvalidateSensorSubscriptionHandle(
		Handle
	);
	TestFalse(
		TEXT("Invalidating an invalid handle stays safe"),
		Handle.IsValid()
	);
	const FOpenMobileSensorFlushHandle FlushHandle(FGuid::NewGuid());
	TestTrue(TEXT("A typed flush handle reports validity"),
		UOpenMobileSensorBlueprintLibrary::IsSensorFlushHandleValid(
			FlushHandle));
	TestTrue(TEXT("Typed flush handles compare within their own type"),
		UOpenMobileSensorBlueprintLibrary::AreSensorFlushHandlesEqual(
			FlushHandle, FlushHandle));
	const FOpenMobileNativeStepCountQueryHandle QueryHandle(FGuid::NewGuid());
	TestTrue(TEXT("A typed historical query handle reports validity"),
		UOpenMobileSensorBlueprintLibrary::
			IsNativeStepCountQueryHandleValid(QueryHandle));
	TestTrue(TEXT("Typed historical query handles compare within their type"),
		UOpenMobileSensorBlueprintLibrary::
			AreNativeStepCountQueryHandlesEqual(QueryHandle, QueryHandle));
	TestNull(TEXT("Raw flush GUIDs are not Blueprint properties"),
		FindFProperty<FProperty>(
			FOpenMobileSensorFlushResult::StaticStruct(),
			TEXT("RequestId")));
	TestNotNull(TEXT("The typed flush request is a Blueprint property"),
		FindFProperty<FProperty>(
			FOpenMobileSensorFlushResult::StaticStruct(),
			GET_MEMBER_NAME_CHECKED(
				FOpenMobileSensorFlushResult, Request)));
	TestNull(TEXT("Raw historical-query GUIDs are not Blueprint properties"),
		FindFProperty<FProperty>(
			FOpenMobileNativeStepCountQueryResult::StaticStruct(),
			TEXT("RequestId")));
	TestNotNull(TEXT("The typed historical request is a Blueprint property"),
		FindFProperty<FProperty>(
			FOpenMobileNativeStepCountQueryResult::StaticStruct(),
			GET_MEMBER_NAME_CHECKED(
				FOpenMobileNativeStepCountQueryResult, Request)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintReadOutcomeHelperTest,
	"OpenMobile.Sensors.Blueprint.Helpers.ReadOutcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintReadOutcomeHelperTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorReadResult Read;
	EOpenMobileSensorReadOutcome Outcome =
		EOpenMobileSensorReadOutcome::NewSample;
	Read.Status = EOpenMobileSensorReadStatus::InvalidHandle;
	UOpenMobileSensorBlueprintLibrary::BranchOnSensorReadResult(Read, Outcome);
	TestEqual(TEXT("Invalid handles use the invalid listener branch"),
		Outcome,
		EOpenMobileSensorReadOutcome::InvalidListener);
	Read.Status = EOpenMobileSensorReadStatus::NoSample;
	UOpenMobileSensorBlueprintLibrary::BranchOnSensorReadResult(Read, Outcome);
	TestEqual(TEXT("Missing samples use the no sample branch"),
		Outcome,
		EOpenMobileSensorReadOutcome::NoSample);
	Read.Status = EOpenMobileSensorReadStatus::Valid;
	Read.Sequence = 4;
	Read.bHasNewerSample = false;
	UOpenMobileSensorBlueprintLibrary::BranchOnSensorReadResult(Read, Outcome);
	TestEqual(TEXT("An unchanged cached sample uses the same sample branch"),
		Outcome,
		EOpenMobileSensorReadOutcome::SameSample);
	Read.bHasNewerSample = true;
	UOpenMobileSensorBlueprintLibrary::BranchOnSensorReadResult(Read, Outcome);
	TestEqual(TEXT("A newer cached sample uses the new sample branch"),
		Outcome,
		EOpenMobileSensorReadOutcome::NewSample);

#if WITH_METADATA
	const UFunction* BranchFunction =
		UOpenMobileSensorBlueprintLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorBlueprintLibrary,
				BranchOnSensorReadResult
			)
		);
	TestNotNull(TEXT("The read branch helper is reflected"), BranchFunction);
	if (BranchFunction)
	{
		TestEqual(TEXT("The read outcome expands into execution pins"),
			BranchFunction->GetMetaData(TEXT("ExpandEnumAsExecs")),
			FString(TEXT("Outcome")));
	}
	for (const FName FunctionName : {
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorsSubsystem,
			GetLatestVectorSampleNative),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorsSubsystem,
			GetLatestAttitudeSampleNative),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorsSubsystem,
			GetLatestScalarSampleNative),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorsSubsystem,
			GetLatestHeadingSampleNative),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorsSubsystem,
			GetLatestStepsSampleNative),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorsSubsystem,
			GetLatestActivitySampleNative),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorsSubsystem,
			GetLatestOrientationSampleNative),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileSensorsSubsystem,
			GetLatestProximitySampleNative)})
	{
		const UFunction* ReadFunction =
			UOpenMobileSensorsSubsystem::StaticClass()->FindFunctionByName(
				FunctionName
			);
		TestNotNull(TEXT("The raw latest read is reflected"), ReadFunction);
		if (ReadFunction)
		{
			TestEqual(TEXT("The raw Boolean is named Has Sample"),
				ReadFunction->GetMetaData(TEXT("ReturnDisplayName")),
				FString(TEXT("Has Sample")));
		}
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintDiscoveryHelperTest,
	"OpenMobile.Sensors.Blueprint.Helpers.DiscoveryAndDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintDiscoveryHelperTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	for (const EOpenMobileSensorType Type : {
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorType::Gyroscope,
		EOpenMobileSensorType::Shake})
	{
		TestEqual(
			TEXT("Vector sensors report their public family"),
			UOpenMobileSensorBlueprintLibrary::GetSensorSampleFamily(Type),
			EOpenMobileSensorSampleFamily::Vector
		);
	}
	TestEqual(
		TEXT("Attitude reports its public family"),
		UOpenMobileSensorBlueprintLibrary::GetSensorSampleFamily(
			EOpenMobileSensorType::Attitude
		),
		EOpenMobileSensorSampleFamily::Attitude
	);
	TestEqual(
		TEXT("Unknown sensors have no sample family"),
		UOpenMobileSensorBlueprintLibrary::GetSensorSampleFamily(
			EOpenMobileSensorType::Unknown
		),
		EOpenMobileSensorSampleFamily::Unknown
	);

	UOpenMobileSensorsSettings* Settings =
		GetMutableDefault<UOpenMobileSensorsSettings>();
	const FOpenMobileSensorStreamOptions SavedDefaults =
		Settings->DefaultStreamOptions;
	Settings->DefaultStreamOptions.CustomFrequencyHz = 73.0;
	const FOpenMobileSensorStreamOptions Defaults =
		UOpenMobileSensorBlueprintLibrary::GetDefaultSensorStreamOptions();
	TestEqual(
		TEXT("The Blueprint factory reads project stream defaults"),
		Defaults.CustomFrequencyHz,
		73.0
	);
	Settings->DefaultStreamOptions = SavedDefaults;

	const FOpenMobileSensorIdentifier Identifier =
		UOpenMobileSensorBlueprintLibrary::MakeSensorIdentifier(
			EOpenMobileSensorType::Gyroscope,
			NAME_None
		);
	TestEqual(
		TEXT("The identifier factory preserves the sensor type"),
		Identifier.Type,
		EOpenMobileSensorType::Gyroscope
	);
	TestTrue(
		TEXT("An empty instance selects the preferred sensor"),
		Identifier.InstanceId.IsNone()
	);
	EOpenMobileSensorType BrokenType = EOpenMobileSensorType::Unknown;
	FName BrokenInstance;
	UOpenMobileSensorBlueprintLibrary::BreakSensorIdentifier(
		Identifier, BrokenType, BrokenInstance);
	TestEqual(TEXT("The intentional break returns the sensor type"),
		BrokenType, EOpenMobileSensorType::Gyroscope);
	TestTrue(TEXT("The intentional break preserves an empty instance"),
		BrokenInstance.IsNone());
#if WITH_METADATA
	const UFunction* BreakFunction =
		UOpenMobileSensorBlueprintLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorBlueprintLibrary,
				BreakSensorIdentifier));
	TestNotNull(TEXT("The identifier break helper is reflected"),
		BreakFunction);
	if (BreakFunction)
	{
		TestTrue(TEXT("The identifier helper replaces automatic struct breaks"),
			BreakFunction->HasMetaData(TEXT("NativeBreakFunc")));
		TestEqual(TEXT("The raw instance remains advanced"),
			BreakFunction->GetMetaData(TEXT("AdvancedDisplay")),
			FString(TEXT("OutInstanceId")));
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintOptionalAndTimeHelpersTest,
	"OpenMobile.Sensors.Blueprint.Helpers.OptionalValuesAndDates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintOptionalAndTimeHelpersTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorOptionalNumber Number;
	TestEqual(TEXT("Unavailable numbers return the requested default"),
		UOpenMobileSensorOptionalValueLibrary::GetOptionalNumberOrDefault(
			Number, 42.0),
		42.0);
	double NumberValue = -1.0;
	TestFalse(TEXT("Unavailable numbers take the missing branch"),
		UOpenMobileSensorOptionalValueLibrary::TryGetOptionalNumber(
			Number, NumberValue));
	TestEqual(TEXT("Missing optional outputs are initialized safely"),
		NumberValue, 0.0);
	Number.bAvailable = true;
	Number.Value = 12.5;
	TestTrue(TEXT("Available numbers take the present branch"),
		UOpenMobileSensorOptionalValueLibrary::TryGetOptionalNumber(
			Number, NumberValue));
	TestEqual(TEXT("Available numbers return their value"),
		NumberValue, 12.5);

	FOpenMobilePedometerMetrics Metrics;
	double DistanceMetres = -1.0;
	TestFalse(TEXT("Missing pedometer distance branches explicitly"),
		UOpenMobileSensorOptionalValueLibrary::TryGetPedometerDistance(
			Metrics, DistanceMetres));
	Metrics.bHasDistanceMeters = true;
	Metrics.DistanceMeters = 123.0;
	TestTrue(TEXT("Present pedometer distance branches explicitly"),
		UOpenMobileSensorOptionalValueLibrary::TryGetPedometerDistance(
			Metrics, DistanceMetres));
	TestEqual(TEXT("Pedometer distance uses metres"),
		DistanceMetres, 123.0);
	FOpenMobileHeadingSensorSample Heading;
	double AccuracyDegrees = -1.0;
	TestFalse(TEXT("Missing heading accuracy branches explicitly"),
		UOpenMobileSensorOptionalValueLibrary::TryGetHeadingAccuracy(
			Heading, AccuracyDegrees));
	Heading.bHasAccuracyDegrees = true;
	Heading.AccuracyDegrees = 5.0;
	TestTrue(TEXT("Present heading accuracy branches explicitly"),
		UOpenMobileSensorOptionalValueLibrary::TryGetHeadingAccuracy(
			Heading, AccuracyDegrees));
	TestEqual(TEXT("Heading accuracy uses degrees"),
		AccuracyDegrees, 5.0);

	const FDateTime Start(2025, 1, 2, 3, 4, 5);
	const FDateTime End(2025, 1, 2, 4, 4, 5);
	FOpenMobileNativeStepCountQuery Query;
	TestTrue(TEXT("Date ranges make a native step query"),
		UOpenMobileNativeStepCountLibrary::MakeNativeStepQueryBetweenDates(
			Start, End, Query));
	TestEqual(TEXT("Date query start converts to Unix seconds"),
		Query.StartUnixTimeSeconds,
		static_cast<double>(Start.ToUnixTimestamp()));
	TestEqual(TEXT("Date query end converts to Unix seconds"),
		Query.EndUnixTimeSeconds,
		static_cast<double>(End.ToUnixTimestamp()));
	TestFalse(TEXT("Reversed date ranges are rejected"),
		UOpenMobileNativeStepCountLibrary::MakeNativeStepQueryBetweenDates(
			End, Start, Query));
	TestTrue(TEXT("Recent durations make a native step query"),
		UOpenMobileNativeStepCountLibrary::MakeNativeStepQueryForLastDuration(
			FTimespan::FromMinutes(10.0), Query));
	TestTrue(TEXT("Recent duration keeps its requested length"),
		FMath::IsNearlyEqual(
			Query.EndUnixTimeSeconds - Query.StartUnixTimeSeconds,
			600.0, 0.001));
	TestFalse(TEXT("Nonpositive durations are rejected"),
		UOpenMobileNativeStepCountLibrary::MakeNativeStepQueryForLastDuration(
			FTimespan::Zero(), Query));
#if WITH_METADATA
	const UFunction* RecentFunction =
		UOpenMobileNativeStepCountLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileNativeStepCountLibrary,
				MakeNativeStepQueryForLastDuration));
	TestNotNull(TEXT("The recent-query helper is reflected"), RecentFunction);
	if (RecentFunction)
	{
		TestFalse(TEXT("The current-time helper is not Blueprint pure"),
			RecentFunction->HasAnyFunctionFlags(FUNC_BlueprintPure));
		TestEqual(TEXT("The recent-query helper branches on validity"),
			RecentFunction->GetMetaData(TEXT("ExpandBoolAsExecs")),
			FString(TEXT("ReturnValue")));
	}
	const FProperty* OptionalNumberValue = FindFProperty<FProperty>(
		FOpenMobileSensorOptionalNumber::StaticStruct(),
		GET_MEMBER_NAME_CHECKED(FOpenMobileSensorOptionalNumber, Value));
	TestNotNull(TEXT("The optional number backing value is reflected"),
		OptionalNumberValue);
	if (OptionalNumberValue)
	{
		TestTrue(TEXT("Optional backing values stay out of normal breaks"),
			OptionalNumberValue->HasMetaData(TEXT("AdvancedDisplay")));
		TestFalse(TEXT("Optional backing values explain availability"),
			OptionalNumberValue->GetToolTipText().IsEmpty());
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintPermissionSurfaceTest,
	"OpenMobile.Sensors.Blueprint.Helpers.PermissionSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintPermissionSurfaceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
#if WITH_METADATA
	const UEnum* PermissionEnum = StaticEnum<EOpenMobileSensorPermission>();
	TestNotNull(TEXT("The sensor permission enum is reflected"), PermissionEnum);
	if (PermissionEnum)
	{
		const int32 LocationIndex = PermissionEnum->GetIndexByValue(
			static_cast<int64>(
				EOpenMobileSensorPermission::TrueHeadingLocation
			)
		);
		TestTrue(
			TEXT("The external location prerequisite is hidden from requests"),
			PermissionEnum->HasMetaData(TEXT("Hidden"), LocationIndex)
		);
	}
	const UEnum* ResultEnum = StaticEnum<EOpenMobileSensorResultCode>();
	TestNotNull(TEXT("The sensor result enum is reflected"), ResultEnum);
	if (ResultEnum)
	{
		const int32 AcceptedIndex = ResultEnum->GetIndexByValue(
			static_cast<int64>(EOpenMobileSensorResultCode::Accepted)
		);
		TestEqual(
			TEXT("Accepted explains its asynchronous state"),
			ResultEnum->GetDisplayNameTextByIndex(AcceptedIndex).ToString(),
			FString(TEXT("Accepted, Starting Asynchronously"))
		);
	}
	const UEnum* DeliveryEnum = StaticEnum<EOpenMobileSensorDeliveryMode>();
	TestNotNull(TEXT("The sensor delivery enum is reflected"), DeliveryEnum);
	if (DeliveryEnum)
	{
		const int32 EventIndex = DeliveryEnum->GetIndexByValue(
			static_cast<int64>(EOpenMobileSensorDeliveryMode::EventBatches)
		);
		TestTrue(
			TEXT("Event delivery explains its delegate requirement"),
			DeliveryEnum->GetToolTipTextByIndex(EventIndex).ToString().Contains(
				TEXT("sample events")
			)
		);
	}

	const UFunction* BranchFunction =
		UOpenMobileSensorBlueprintLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorBlueprintLibrary,
				BranchOnSensorOperationResult
			)
		);
	TestNotNull(TEXT("The operation branch helper is reflected"), BranchFunction);
	if (BranchFunction)
	{
		TestEqual(
			TEXT("The operation helper expands its Boolean into execution pins"),
			BranchFunction->GetMetaData(TEXT("ExpandBoolAsExecs")),
			FString(TEXT("ReturnValue"))
		);
	}
#endif
	return true;
}

#endif
