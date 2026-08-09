#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorFlushAsyncAction.h"
#include "OpenMobileNativeStepCountAsyncAction.h"
#include "OpenMobileSensorActivityListeners.h"
#include "OpenMobileSensorDiscoveryLibrary.h"
#include "OpenMobileSensorFlagLibrary.h"
#include "OpenMobileSensorListener.h"
#include "OpenMobileSensorOptionalValueLibrary.h"
#include "OpenMobileSensorPermissionAsyncAction.h"
#include "OpenMobileSensorQuality.h"
#include "OpenMobileSensorRecordingAsyncAction.h"
#include "OpenMobileSensorRecordingSession.h"
#include "OpenMobileSensorReplayAsyncAction.h"
#include "OpenMobileSensorReplaySession.h"
#include "OpenMobileSensorsDevelopmentInput.h"
#include "OpenMobileSensorsSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintEnumMetadataTest,
	"OpenMobile.Sensors.Blueprint.EnumMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintEnumMetadataTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	int32 BlueprintEnumCount = 0;
	for (TObjectIterator<UEnum> Enum; Enum; ++Enum)
	{
		if (Enum->GetOutermost()->GetName() != TEXT("/Script/OpenMobileSensors") ||
			!Enum->GetBoolMetaData(TEXT("BlueprintType")))
		{
			continue;
		}
		++BlueprintEnumCount;
		for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
		{
			if (Enum->HasMetaData(TEXT("Hidden"), Index) ||
				Enum->GetNameStringByIndex(Index).EndsWith(TEXT("_MAX")))
			{
				continue;
			}
			const FString EntryName = FString::Printf(
				TEXT("%s::%s"),
				*Enum->GetName(),
				*Enum->GetNameStringByIndex(Index)
			);
			TestTrue(
				*FString::Printf(TEXT("%s has an authored display name"), *EntryName),
				Enum->HasMetaData(TEXT("DisplayName"), Index)
			);
			TestTrue(
				*FString::Printf(TEXT("%s has an authored tooltip"), *EntryName),
				Enum->HasMetaData(TEXT("ToolTip"), Index)
			);
		}
	}
	TestTrue(TEXT("Blueprint Sensors enums are reflected"), BlueprintEnumCount >= 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsBlueprintPropertyMetadataTest,
	"OpenMobile.Sensors.Blueprint.PropertyMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsBlueprintPropertyMetadataTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	int32 BlueprintPropertyCount = 0;
	for (TObjectIterator<UStruct> Struct; Struct; ++Struct)
	{
		if (Struct->GetOutermost()->GetName() != TEXT("/Script/OpenMobileSensors") ||
			(!Struct->IsA<UClass>() && !Struct->IsA<UScriptStruct>()))
		{
			continue;
		}
		for (TFieldIterator<FProperty> Property(
			*Struct, EFieldIterationFlags::None); Property; ++Property)
		{
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
			{
				continue;
			}
			++BlueprintPropertyCount;
			const FString PropertyName = FString::Printf(
				TEXT("%s::%s"),
				*Struct->GetName(),
				*Property->GetName()
			);
			TestTrue(
				*FString::Printf(TEXT("%s uses the Sensors category"), *PropertyName),
				Property->GetMetaData(TEXT("Category")).StartsWith(
					TEXT("OpenMobile|Sensors"))
			);
			TestFalse(
				*FString::Printf(TEXT("%s has an authored tooltip"), *PropertyName),
				Property->GetMetaData(TEXT("ToolTip")).IsEmpty()
			);
		}
	}
	TestTrue(TEXT("Blueprint Sensors properties are reflected"),
		BlueprintPropertyCount >= 400);
	return true;
}

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
		UOpenMobileSensorDiscoveryLibrary::StaticClass(),
		UOpenMobileSensorListener::StaticClass(),
		UOpenMobileGyroscopeListener::StaticClass(),
		UOpenMobileStepCountListener::StaticClass(),
		UOpenMobileSensorRecordingSession::StaticClass(),
		UOpenMobileSensorReplaySession::StaticClass(),
		UOpenMobileSensorPermissionAsyncAction::StaticClass(),
		UOpenMobileSensorFlushAsyncAction::StaticClass(),
		UOpenMobileSensorRecordingAsyncAction::StaticClass(),
		UOpenMobileSensorReplayAsyncAction::StaticClass(),
		UOpenMobileNativeStepCountAsyncAction::StaticClass(),
		UOpenMobileSensorsDevelopmentLibrary::StaticClass(),
		UOpenMobileSensorQualityLibrary::StaticClass(),
		UOpenMobileSensorFlagLibrary::StaticClass(),
		UOpenMobileSensorOptionalValueLibrary::StaticClass(),
		UOpenMobileSensorRateLibrary::StaticClass()
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
			TestTrue(
				*FString::Printf(TEXT("%s uses the Sensors category"),
					*Function->GetName()),
				Function->GetMetaData(TEXT("Category")).StartsWith(
					TEXT("OpenMobile|Sensors"))
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
			TestTrue(
				*FString::Printf(TEXT("%s uses the Sensors category"),
					*Event->GetName()),
				Event->GetMetaData(TEXT("Category")).StartsWith(
					TEXT("OpenMobile|Sensors"))
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
	for (const FName EventName : {
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSubsystem, OnVectorSamples),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSubsystem, OnAttitudeSamples),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSubsystem, OnScalarSamples),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSubsystem, OnHeadingSamples),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSubsystem, OnStepsSamples),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSubsystem, OnActivitySamples),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSubsystem, OnOrientationSamples),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSubsystem, OnProximitySamples)})
	{
		const FMulticastDelegateProperty* Event =
			FindFProperty<FMulticastDelegateProperty>(
				UOpenMobileSensorsSubsystem::StaticClass(),
				EventName
			);
		TestNotNull(TEXT("The raw sample event is reflected"), Event);
		if (Event)
		{
			TestTrue(
				*FString::Printf(
					TEXT("%s states its delivery requirement"),
					*EventName.ToString()
				),
				Event->GetMetaData(TEXT("ToolTip")).Contains(
					TEXT("Requires Delivery Mode = Event Batches")
				)
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
		AActor::StaticClass(),
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
	UK2Node_CustomEvent* Entry = NewObject<UK2Node_CustomEvent>(Graph);
	Entry->CustomFunctionName = TEXT("ExerciseSensorListener");
	Graph->AddNode(Entry, false, false);
	Entry->AllocateDefaultPins();

	const TArray<UFunction*> WorkflowFunctions = {
		UOpenMobileGyroscopeListener::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileGyroscopeListener,
				ListenForGyroscope)),
		UOpenMobileGyroscopeListener::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileGyroscopeListener,
				GetLatestAngularVelocity)),
		UOpenMobileSensorListener::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorListener,
				GetLastSensorError)),
		UOpenMobileSensorListener::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorListener,
				Stop))
	};
	TArray<UK2Node_CallFunction*> WorkflowNodes;
	for (UFunction* Function : WorkflowFunctions)
	{
		TestNotNull(TEXT("Workflow function is reflected"), Function);
		if (!Function)
		{
			continue;
		}
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
		Node->SetFromFunction(Function);
		Graph->AddNode(Node, false, false);
		Node->AllocateDefaultPins();
		WorkflowNodes.Add(Node);
		TestTrue(
			*FString::Printf(TEXT("%s allocates graph pins"),
				*Function->GetName()),
			Node->Pins.Num() >= 2
		);
	}
	if (WorkflowNodes.Num() != WorkflowFunctions.Num())
	{
		return false;
	}
	UK2Node_CallFunction* Start = WorkflowNodes[0];
	UK2Node_CallFunction* ReadLatest = WorkflowNodes[1];
	UK2Node_CallFunction* ReadError = WorkflowNodes[2];
	UK2Node_CallFunction* Stop = WorkflowNodes[3];
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	auto Connect = [this, Schema](
		UEdGraphNode* FromNode,
		const FName FromPin,
		UEdGraphNode* ToNode,
		const FName ToPin)
	{
		UEdGraphPin* Output = FromNode->FindPin(FromPin, EGPD_Output);
		UEdGraphPin* Input = ToNode->FindPin(ToPin, EGPD_Input);
		TestNotNull(*FString::Printf(TEXT("%s has output pin %s"),
			*FromNode->GetName(), *FromPin.ToString()), Output);
		TestNotNull(*FString::Printf(TEXT("%s has input pin %s"),
			*ToNode->GetName(), *ToPin.ToString()), Input);
		return Output && Input && Schema->TryCreateConnection(Output, Input);
	};
	TestTrue(TEXT("The workflow starts from an execution event"),
		Connect(Entry, UEdGraphSchema_K2::PN_Then,
			Start, UEdGraphSchema_K2::PN_Execute));
	TestTrue(TEXT("The workflow always reaches explicit cleanup"),
		Connect(Start, UEdGraphSchema_K2::PN_Then,
			Stop, UEdGraphSchema_K2::PN_Execute));
	TestTrue(TEXT("The latest read uses the typed listener"),
		Connect(Start, UEdGraphSchema_K2::PN_ReturnValue,
			ReadLatest, UEdGraphSchema_K2::PN_Self));
	TestTrue(TEXT("The error read uses the typed listener"),
		Connect(Start, UEdGraphSchema_K2::PN_ReturnValue,
			ReadError, UEdGraphSchema_K2::PN_Self));
	TestTrue(TEXT("Cleanup uses the typed listener"),
		Connect(Start, UEdGraphSchema_K2::PN_ReturnValue,
			Stop, UEdGraphSchema_K2::PN_Self));
	UEdGraphPin* RatePreset = Start->FindPin(TEXT("RatePreset"));
	TestNotNull(TEXT("The listener exposes its rate preset"), RatePreset);
	if (RatePreset)
	{
		TestEqual(TEXT("The gyroscope listener defaults to the Game preset"),
			RatePreset->DefaultValue, FString(TEXT("Game")));
	}
	TestEqual(TEXT("Advanced listener inputs start collapsed"),
		Start->AdvancedPinDisplay, ENodeAdvancedPins::Hidden);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	int32 ConnectedPinCount = 0;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			ConnectedPinCount += Pin && !Pin->LinkedTo.IsEmpty() ? 1 : 0;
		}
	}
	TestTrue(TEXT("The representative workflow has connected data and execution pins"),
		ConnectedPinCount >= 8);
	FCompilerResultsLog CompileResults;
	FKismetEditorUtilities::CompileBlueprint(
		Blueprint,
		EBlueprintCompileOptions::None,
		&CompileResults
	);
	TestEqual(TEXT("The representative workflow compiles without errors"),
		CompileResults.NumErrors, 0);
	TestEqual(TEXT("The representative Blueprint is up to date"),
		Blueprint->Status, BS_UpToDate);
	return true;
}

#endif
