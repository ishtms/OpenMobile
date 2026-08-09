#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileHapticNamedPlaybackAsyncAction.h"
#include "OpenMobileHapticPatternPlaybackAsyncAction.h"
#include "OpenMobileHapticPreparationAsyncAction.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "OpenMobileHapticsBlueprintLibrary.h"
#include "OpenMobileHapticsSubsystem.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace OpenMobileHapticsBlueprintFixtureTests
{
	UBlueprint* CreateBlueprint()
	{
		return FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			GetTransientPackage(),
			MakeUniqueObjectName(
				GetTransientPackage(),
				UBlueprint::StaticClass(),
				TEXT("OpenMobileHapticsGraphFixture")
			),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass()
		);
	}

	UEdGraph* CreateGraph(UBlueprint* Blueprint)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			TEXT("HapticsWorkflows"),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass()
		);
		FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
		return Graph;
	}

	UK2Node_CustomEvent* AddEvent(UEdGraph* Graph, FName Name)
	{
		UK2Node_CustomEvent* Event = NewObject<UK2Node_CustomEvent>(Graph);
		Event->CustomFunctionName = Name;
		Event->CreateNewGuid();
		Graph->AddNode(Event, false, false);
		Event->AllocateDefaultPins();
		return Event;
	}

	UK2Node_CallFunction* AddCall(UEdGraph* Graph, UFunction* Function)
	{
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
		Node->SetFromFunction(Function);
		Node->CreateNewGuid();
		Graph->AddNode(Node, false, false);
		Node->AllocateDefaultPins();
		return Node;
	}

	UK2Node_AsyncAction* AddAsync(UEdGraph* Graph, UFunction* Function)
	{
		UK2Node_AsyncAction* Node = NewObject<UK2Node_AsyncAction>(Graph);
		Node->InitializeProxyFromFunction(Function);
		Node->CreateNewGuid();
		Graph->AddNode(Node, false, false);
		Node->AllocateDefaultPins();
		return Node;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBlueprintPropertyMetadataTest,
	"OpenMobile.Haptics.API.BlueprintPropertyMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBlueprintPropertyMetadataTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	int32 ReflectedPropertyCount = 0;
	for (TObjectIterator<UStruct> Struct; Struct; ++Struct)
	{
		const FString PackageName = Struct->GetOutermost()->GetName();
		if ((PackageName != TEXT("/Script/OpenMobileHaptics")
				&& PackageName != TEXT("/Script/OpenMobileHapticsPreview"))
			|| (!Struct->IsA<UClass>() && !Struct->IsA<UScriptStruct>()))
		{
			continue;
		}
		for (TFieldIterator<FProperty> Property(
			*Struct,
			EFieldIterationFlags::None
		); Property; ++Property)
		{
			if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)
				|| Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}
			++ReflectedPropertyCount;
			const FString PropertyName = FString::Printf(
				TEXT("%s::%s"),
				*Struct->GetName(),
				*Property->GetName()
			);
			TestTrue(
				*FString::Printf(
					TEXT("%s uses a stable Haptics category"),
					*PropertyName
				),
				Property->GetMetaData(TEXT("Category")).StartsWith(
					TEXT("OpenMobile|Haptics")
				)
			);
			TestFalse(
				*FString::Printf(
					TEXT("%s has an authored tooltip"),
					*PropertyName
				),
				Property->GetMetaData(TEXT("ToolTip")).IsEmpty()
			);
		}
	}
	TestTrue(TEXT("Haptics details and graph properties are reflected"),
		ReflectedPropertyCount >= 300);
	for (const TCHAR* InternalStructName : {
		TEXT("OpenMobileHapticPatternEvent"),
		TEXT("OpenMobileHapticCurvePoint"),
		TEXT("OpenMobileHapticParameterCurve"),
		TEXT("OpenMobileHapticPattern"),
		TEXT("OpenMobileHapticSemanticRequest"),
		TEXT("OpenMobileHapticOneShotRequest"),
		TEXT("OpenMobileHapticNamedPatternRequest")
	})
	{
		const UScriptStruct* InternalStruct = FindObject<UScriptStruct>(
			nullptr,
			*FString::Printf(
				TEXT("/Script/OpenMobileHaptics.%s"),
				InternalStructName
			)
		);
		TestTrue(TEXT("Internal Haptics structs remain reflected for C++ and assets"),
			InternalStruct != nullptr);
		TestFalse(TEXT("Internal Haptics structs stay out of Blueprint type search"),
			InternalStruct && InternalStruct->HasMetaData(TEXT("BlueprintType")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBlueprintFixtureCompilationTest,
	"OpenMobile.Haptics.API.BlueprintFixtureCompilation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBlueprintFixtureCompilationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsBlueprintFixtureTests;
	UBlueprint* Blueprint = CreateBlueprint();
	TestNotNull(TEXT("The Haptics fixture Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	UEdGraph* Graph = CreateGraph(Blueprint);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	auto Connect = [this, Schema](
		UEdGraphNode* FromNode,
		FName FromPin,
		UEdGraphNode* ToNode,
		FName ToPin
	)
	{
		UEdGraphPin* Output = FromNode->FindPin(FromPin, EGPD_Output);
		UEdGraphPin* Input = ToNode->FindPin(ToPin, EGPD_Input);
		TestNotNull(TEXT("The fixture output pin exists"), Output);
		TestNotNull(TEXT("The fixture input pin exists"), Input);
		return Output && Input && Schema->TryCreateConnection(Output, Input);
	};

	UK2Node_CustomEvent* SimpleEntry =
		OpenMobileHapticsBlueprintFixtureTests::AddEvent(
			Graph,
			TEXT("SimpleFeedback")
		);
	UK2Node_CallFunction* SimplePlay = AddCall(
		Graph,
		UOpenMobileHapticsBlueprintLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileHapticsBlueprintLibrary,
				PlaySelectionHaptic
			)
		)
	);
	TestTrue(TEXT("Simple feedback has one direct execution path"),
		Connect(SimpleEntry, UEdGraphSchema_K2::PN_Then,
			SimplePlay, UEdGraphSchema_K2::PN_Execute));
	for (const FName Branch : {
		FName(TEXT("Accepted")),
		FName(TEXT("Suppressed")),
		FName(TEXT("Rejected"))
	})
	{
		TestNotNull(TEXT("Simple feedback exposes every request outcome"),
			SimplePlay->FindPin(Branch, EGPD_Output));
	}
	const UEdGraphPin* Intensity = SimplePlay->FindPin(TEXT("Intensity"));
	TestTrue(TEXT("Simple feedback defaults to full normalized intensity"),
		Intensity && FMath::IsNearlyEqual(FCString::Atof(*Intensity->DefaultValue), 1.0f));

	struct FAsyncFixture
	{
		FName EventName;
		UFunction* Factory;
		TArray<FName> RequiredBranches;
	};
	const TArray<FAsyncFixture> AsyncFixtures = {
		{
			TEXT("PrepareContent"),
			UOpenMobileHapticPreparationAsyncAction::StaticClass()
				->FindFunctionByName(TEXT("PrepareHapticsAsync")),
			{TEXT("Ready"), TEXT("Cancelled"), TEXT("Failed")}
		},
		{
			TEXT("PlayAsset"),
			UOpenMobileHapticPatternPlaybackAsyncAction::StaticClass()
				->FindFunctionByName(TEXT("PlayHapticPatternAsset")),
			{
				TEXT("WaitingForPreparation"),
				TEXT("Accepted"),
				TEXT("Suppressed"),
				TEXT("Rejected"),
				TEXT("Cancelled")
			}
		},
		{
			TEXT("PlayConfigured"),
			UOpenMobileHapticNamedPlaybackAsyncAction::StaticClass()
				->FindFunctionByName(TEXT("PlayNamedHaptic")),
			{
				TEXT("WaitingForPreparation"),
				TEXT("Accepted"),
				TEXT("Suppressed"),
				TEXT("Rejected"),
				TEXT("Cancelled")
			}
		}
	};
	for (const FAsyncFixture& Fixture : AsyncFixtures)
	{
		TestNotNull(TEXT("The async factory is reflected"), Fixture.Factory);
		if (!Fixture.Factory)
		{
			continue;
		}
		UK2Node_CustomEvent* Entry =
			OpenMobileHapticsBlueprintFixtureTests::AddEvent(
				Graph,
				Fixture.EventName
			);
		UK2Node_AsyncAction* Async = AddAsync(Graph, Fixture.Factory);
		TestTrue(TEXT("The async fixture is executable"),
			Connect(Entry, UEdGraphSchema_K2::PN_Then,
				Async, UEdGraphSchema_K2::PN_Execute));
		for (const FName Branch : Fixture.RequiredBranches)
		{
			TestNotNull(TEXT("The async fixture exposes its honest branch"),
				Async->FindPin(Branch, EGPD_Output));
		}
	}

	UK2Node_CustomEvent* PolicyEntry =
		OpenMobileHapticsBlueprintFixtureTests::AddEvent(
			Graph,
			TEXT("PlayerPolicy")
		);
	UK2Node_CallFunction* Policy = AddCall(
		Graph,
		UOpenMobileHapticsBlueprintLibrary::StaticClass()->FindFunctionByName(
			TEXT("SetHapticsMasterIntensity")
		)
	);
	TestTrue(TEXT("The policy fixture is executable"),
		Connect(PolicyEntry, UEdGraphSchema_K2::PN_Then,
			Policy, UEdGraphSchema_K2::PN_Execute));
	TestNotNull(TEXT("Policy validation has a succeeded branch"),
		Policy->FindPin(TEXT("Succeeded"), EGPD_Output));
	TestNotNull(TEXT("Policy validation has a failed branch"),
		Policy->FindPin(TEXT("Failed"), EGPD_Output));

	UK2Node_CustomEvent* LegacyEntry =
		OpenMobileHapticsBlueprintFixtureTests::AddEvent(
			Graph,
			TEXT("LegacyMigration")
		);
	UK2Node_AsyncAction* LegacyPlay = AddAsync(
		Graph,
		UOpenMobileHapticPlaybackAsyncAction::StaticClass()->FindFunctionByName(
			TEXT("PlayNamedHapticAsync")
		)
	);
	UEdGraphPin* LegacyPattern = LegacyPlay->FindPin(TEXT("PatternName"));
	if (LegacyPattern)
	{
		LegacyPattern->DefaultValue = TEXT("Legacy.Pattern");
	}
	TestTrue(TEXT("The deprecated legacy node remains executable"),
		Connect(LegacyEntry, UEdGraphSchema_K2::PN_Then,
			LegacyPlay, UEdGraphSchema_K2::PN_Execute));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(
		Blueprint,
		EBlueprintCompileOptions::None,
		&Results
	);
	TestEqual(TEXT("Every common and migration fixture compiles"),
		Results.NumErrors, 0);
	TestTrue(TEXT("The fixture Blueprint is up to date"),
		Blueprint->Status == BS_UpToDate
			|| Blueprint->Status == BS_UpToDateWithWarnings);
	TestTrue(TEXT("The legacy fixture receives migration guidance"),
		Results.NumWarnings >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBlueprintLiteralValidationTest,
	"OpenMobile.Haptics.API.BlueprintLiteralValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBlueprintLiteralValidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsBlueprintFixtureTests;
	UBlueprint* Blueprint = CreateBlueprint();
	UEdGraph* Graph = Blueprint ? CreateGraph(Blueprint) : nullptr;
	TestNotNull(TEXT("The validation fixture Blueprint is created"), Blueprint);
	if (!Graph)
	{
		return false;
	}
	UK2Node_CustomEvent* Entry =
		OpenMobileHapticsBlueprintFixtureTests::AddEvent(
			Graph,
			TEXT("InvalidLiteralAsset")
		);
	UK2Node_AsyncAction* PlayAsset = AddAsync(
		Graph,
		UOpenMobileHapticPatternPlaybackAsyncAction::StaticClass()
			->FindFunctionByName(TEXT("PlayHapticPatternAsset"))
	);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	TestTrue(TEXT("The invalid literal fixture is executable"),
		Schema->TryCreateConnection(
			Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output),
			PlayAsset->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input)
		));
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(
		Blueprint,
		EBlueprintCompileOptions::None,
		&Results
	);
	TestEqual(TEXT("Actionable Haptics validation does not break compilation"),
		Results.NumErrors, 0);
	TestTrue(TEXT("An empty literal asset produces a compile warning"),
		Results.NumWarnings >= 1);
	bool bFoundActionableWarning = false;
	for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
	{
		bFoundActionableWarning |= Message->ToText().ToString().Contains(
			TEXT("needs a Haptic Pattern asset")
		);
	}
	TestTrue(TEXT("The compile warning explains the missing asset"),
		bFoundActionableWarning);
	return true;
}

#endif
