#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorBlueprintLibrary.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsSettings.h"
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
