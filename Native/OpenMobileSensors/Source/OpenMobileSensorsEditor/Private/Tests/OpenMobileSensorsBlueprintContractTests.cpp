#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorFlushAsyncAction.h"
#include "OpenMobileSensorPermissionAsyncAction.h"
#include "OpenMobileSensorRecordingAsyncAction.h"
#include "OpenMobileSensorReplayAsyncAction.h"
#include "OpenMobileSensorsSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintReflectionTest,
	"OpenMobile.Sensors.Blueprint.Reflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintReflectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const TArray<UClass*> BlueprintClasses = {
		UOpenMobileSensorsSubsystem::StaticClass(),
		UOpenMobileSensorPermissionAsyncAction::StaticClass(),
		UOpenMobileSensorFlushAsyncAction::StaticClass(),
		UOpenMobileSensorRecordingAsyncAction::StaticClass(),
		UOpenMobileSensorReplayAsyncAction::StaticClass()
	};
	int32 BlueprintCallableCount = 0;
	for (UClass* Class : BlueprintClasses)
	{
		for (TFieldIterator<UFunction> Function(
			Class,
			EFieldIterationFlags::None
		); Function; ++Function)
		{
			if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
			{
				continue;
			}
			++BlueprintCallableCount;
			TestEqual(
				*FString::Printf(TEXT("%s uses the Sensors category"),
					*Function->GetName()),
				Function->GetMetaData(TEXT("Category")),
				FString(TEXT("Open Mobile|Sensors"))
			);
			TestFalse(
				*FString::Printf(TEXT("%s has a display name"),
					*Function->GetName()),
				Function->GetMetaData(TEXT("DisplayName")).IsEmpty()
			);
			TestFalse(
				*FString::Printf(TEXT("%s has a tooltip"),
					*Function->GetName()),
				Function->GetMetaData(TEXT("ToolTip")).IsEmpty()
			);
		}
	}
	TestTrue(TEXT("BlueprintCallable Sensors nodes are reflected"), BlueprintCallableCount >= 30);

	for (UClass* Class : BlueprintClasses)
	{
		for (TFieldIterator<FMulticastDelegateProperty> Event(
			Class,
			EFieldIterationFlags::None
		); Event; ++Event)
		{
			if (!Event->HasAnyPropertyFlags(CPF_BlueprintAssignable))
			{
				continue;
			}
			TestEqual(
				*FString::Printf(TEXT("%s uses the Sensors category"),
					*Event->GetName()),
				Event->GetMetaData(TEXT("Category")),
				FString(TEXT("Open Mobile|Sensors"))
			);
			TestFalse(
				*FString::Printf(TEXT("%s has a display name"),
					*Event->GetName()),
				Event->GetMetaData(TEXT("DisplayName")).IsEmpty()
			);
			TestFalse(
				*FString::Printf(TEXT("%s has a tooltip"),
					*Event->GetName()),
				Event->GetMetaData(TEXT("ToolTip")).IsEmpty()
			);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintGraphNodesTest,
	"OpenMobile.Sensors.Blueprint.RepresentativeGraphNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintGraphNodesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		UObject::StaticClass(),
		GetTransientPackage(),
		MakeUniqueObjectName(
			GetTransientPackage(),
			UBlueprint::StaticClass(),
			TEXT("OpenMobileSensorsGraphTest")
		),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);
	TestNotNull(TEXT("Representative Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		TEXT("RepresentativeSensorsGraph"),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
	const TArray<UFunction*> RepresentativeFunctions = {
		UOpenMobileSensorsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorsSubsystem,
				StartSubscriptionNative
			)
		),
		UOpenMobileSensorsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorsSubsystem,
				GetLatestVectorSampleNative
			)
		),
		UOpenMobileSensorPermissionAsyncAction::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorPermissionAsyncAction,
				RequestSensorPermission
			)
		),
		UOpenMobileSensorFlushAsyncAction::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorFlushAsyncAction,
				FlushSensorSamples
			)
		)
	};
	for (UFunction* Function : RepresentativeFunctions)
	{
		TestNotNull(TEXT("Representative function is reflected"), Function);
		if (!Function)
		{
			continue;
		}
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
		Node->SetFromFunction(Function);
		Graph->AddNode(Node, false, false);
		Node->AllocateDefaultPins();
		TestTrue(
			*FString::Printf(TEXT("%s allocates graph pins"),
				*Function->GetName()),
			Node->Pins.Num() >= 2
		);
	}
	return true;
}

#endif
