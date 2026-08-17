#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_CustomEvent.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileAdsAsyncAction.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsRewardedAsyncAction.h"
#include "OpenMobileAdsSetupAsyncAction.h"

namespace OpenMobileAdsBlueprintFixtureTests
{
	UBlueprint* CreateBlueprint(const TCHAR* BaseName)
	{
		return FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			GetTransientPackage(),
			MakeUniqueObjectName(
				GetTransientPackage(),
				UBlueprint::StaticClass(),
				BaseName
			),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass()
		);
	}

	UEdGraph* CreateGraph(UBlueprint* Blueprint, FName Name)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			Name,
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

	UK2Node_AsyncAction* AddAsync(UEdGraph* Graph, UFunction* Function)
	{
		UK2Node_AsyncAction* Node = NewObject<UK2Node_AsyncAction>(Graph);
		Node->InitializeProxyFromFunction(Function);
		Node->CreateNewGuid();
		Graph->AddNode(Node, false, false);
		Node->AllocateDefaultPins();
		return Node;
	}

	void SetPlacement(UK2Node_AsyncAction* Node, FName Placement)
	{
		if (UEdGraphPin* Pin = Node->FindPin(TEXT("Placement")))
		{
			Pin->DefaultValue = Placement.ToString();
		}
	}

	struct FScopedPlacements
	{
		FScopedPlacements()
			: Settings(GetMutableDefault<UOpenMobileAdsSettings>())
			, Saved(Settings->Placements)
		{
		}

		~FScopedPlacements()
		{
			Settings->Placements = MoveTemp(Saved);
		}

		UOpenMobileAdsSettings* Settings;
		TArray<FOpenMobileAdsPlacementSettings> Saved;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsBlueprintLiteralValidationTest,
	"OpenMobile.Ads.Editor.BlueprintLiteralValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsBlueprintLiteralValidationTest::RunTest(
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
			TEXT("OpenMobileAdsLiteralFixture")
		),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass()
	);
	TestNotNull(TEXT("The Ads fixture Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		TEXT("AdsLiteralValidation"),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
	UK2Node_CustomEvent* Entry = NewObject<UK2Node_CustomEvent>(Graph);
	Entry->CustomFunctionName = TEXT("ShowConfiguredReward");
	Entry->CreateNewGuid();
	Graph->AddNode(Entry, false, false);
	Entry->AllocateDefaultPins();
	UK2Node_AsyncAction* ShowRewarded = NewObject<UK2Node_AsyncAction>(Graph);
	ShowRewarded->InitializeProxyFromFunction(
		UOpenMobileAdsRewardedAsyncAction::StaticClass()->FindFunctionByName(
			TEXT("ShowRewardedAd")
		)
	);
	ShowRewarded->CreateNewGuid();
	Graph->AddNode(ShowRewarded, false, false);
	ShowRewarded->AllocateDefaultPins();
	UEdGraphPin* PlacementPin = ShowRewarded->FindPin(TEXT("Placement"));
	TestNotNull(TEXT("The rewarded node exposes Placement"), PlacementPin);
	if (PlacementPin)
	{
		PlacementPin->DefaultValue = TEXT("TypoReward");
	}
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	TestTrue(
		TEXT("The rewarded fixture has an execution path"),
		Schema->TryCreateConnection(
			Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output),
			ShowRewarded->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input)
		)
	);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(
		Blueprint,
		EBlueprintCompileOptions::None,
		&Results
	);
	TestEqual(TEXT("Placement guidance does not break compilation"),
		Results.NumErrors, 0);
	bool bFoundPlacementWarning = false;
	for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
	{
		bFoundPlacementWarning |= Message->ToText().ToString().Contains(
			TEXT("unknown literal Ads placement")
		);
	}
	TestTrue(TEXT("A typo receives an actionable compile warning"),
		bFoundPlacementWarning);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsBlueprintFirstRunFixtureTest,
	"OpenMobile.Ads.Editor.BlueprintFirstRunFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsBlueprintFirstRunFixtureTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileAdsBlueprintFixtureTests;
	UBlueprint* Blueprint = CreateBlueprint(TEXT("OpenMobileAdsFirstRunFixture"));
	TestNotNull(TEXT("The first-run Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	UEdGraph* Graph = CreateGraph(Blueprint, TEXT("FirstRewardedAd"));
	UK2Node_CustomEvent* Entry =
		OpenMobileAdsBlueprintFixtureTests::AddEvent(
			Graph,
			TEXT("StartAds")
		);
	UK2Node_AsyncAction* Consent = AddAsync(
		Graph,
		UOpenMobileAdsSetupAsyncAction::StaticClass()->FindFunctionByName(
			TEXT("RefreshAdsConsent")
		)
	);
	UK2Node_AsyncAction* Tracking = AddAsync(
		Graph,
		UOpenMobileAdsSetupAsyncAction::StaticClass()->FindFunctionByName(
			TEXT("RequestTrackingAuthorization")
		)
	);
	UK2Node_AsyncAction* Initialize = AddAsync(
		Graph,
		UOpenMobileAdsSetupAsyncAction::StaticClass()->FindFunctionByName(
			TEXT("InitializeAds")
		)
	);
	UK2Node_AsyncAction* Load = AddAsync(
		Graph,
		UOpenMobileAdsAsyncAction::StaticClass()->FindFunctionByName(
			TEXT("LoadAd")
		)
	);
	UK2Node_AsyncAction* Show = AddAsync(
		Graph,
		UOpenMobileAdsRewardedAsyncAction::StaticClass()->FindFunctionByName(
			TEXT("ShowRewardedAd")
		)
	);
	SetPlacement(Load, TEXT("ExampleRewarded"));
	SetPlacement(Show, TEXT("ExampleRewarded"));
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	auto Connect = [this, Schema](
		UEdGraphNode* From,
		FName FromPin,
		UEdGraphNode* To
	)
	{
		UEdGraphPin* Output = From->FindPin(FromPin, EGPD_Output);
		UEdGraphPin* Input = To->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
		TestNotNull(TEXT("The workflow output pin exists"), Output);
		TestNotNull(TEXT("The workflow input pin exists"), Input);
		return Output && Input && Schema->TryCreateConnection(Output, Input);
	};
	TestTrue(TEXT("The flow starts with consent"),
		Connect(Entry, UEdGraphSchema_K2::PN_Then, Consent));
	TestTrue(TEXT("Consent continues to tracking"),
		Connect(Consent, TEXT("OnCompleted"), Tracking));
	TestTrue(TEXT("Tracking continues to initialization"),
		Connect(Tracking, TEXT("OnCompleted"), Initialize));
	TestTrue(TEXT("Initialization continues to load"),
		Connect(Initialize, TEXT("OnCompleted"), Load));
	TestTrue(TEXT("Load continues to rewarded show"),
		Connect(Load, TEXT("OnCompleted"), Show));
	TestNotNull(TEXT("Reward is an explicit output"),
		Show->FindPin(TEXT("OnRewardEarned"), EGPD_Output));
	TestNotNull(TEXT("Dismissal is an explicit output"),
		Show->FindPin(TEXT("OnDismissed"), EGPD_Output));
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(
		Blueprint,
		EBlueprintCompileOptions::None,
		&Results
	);
	TestEqual(TEXT("The first-run Ads workflow compiles"), Results.NumErrors, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsPersistentBannerFixtureTest,
	"OpenMobile.Ads.Editor.PersistentBannerFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsPersistentBannerFixtureTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileAdsBlueprintFixtureTests;
	FScopedPlacements ScopedPlacements;
	FOpenMobileAdsPlacementSettings Banner;
	Banner.Placement = TEXT("ExampleBanner");
	Banner.Format = EOpenMobileAdFormat::Banner;
	Banner.bEnabled = false;
	ScopedPlacements.Settings->Placements.Add(Banner);
	UBlueprint* Blueprint = CreateBlueprint(TEXT("OpenMobileAdsBannerFixture"));
	TestNotNull(TEXT("The banner Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	UEdGraph* Graph = CreateGraph(Blueprint, TEXT("ShowBanner"));
	UK2Node_CustomEvent* Entry =
		OpenMobileAdsBlueprintFixtureTests::AddEvent(
			Graph,
			TEXT("StartBanner")
		);
	UK2Node_AsyncAction* Load = AddAsync(
		Graph,
		UOpenMobileAdsAsyncAction::StaticClass()->FindFunctionByName(TEXT("LoadAd"))
	);
	UK2Node_AsyncAction* Show = AddAsync(
		Graph,
		UOpenMobileAdsAsyncAction::StaticClass()->FindFunctionByName(TEXT("ShowAd"))
	);
	SetPlacement(Load, TEXT("ExampleBanner"));
	SetPlacement(Show, TEXT("ExampleBanner"));
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	TestTrue(TEXT("The banner load is executable"),
		Schema->TryCreateConnection(
			Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output),
			Load->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input)
		));
	TestTrue(TEXT("Banner show follows load completion"),
		Schema->TryCreateConnection(
			Load->FindPin(TEXT("OnCompleted"), EGPD_Output),
			Show->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input)
		));
	TestNotNull(TEXT("Persistent show exposes completion when shown"),
		Show->FindPin(TEXT("OnCompleted"), EGPD_Output));
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(
		Blueprint,
		EBlueprintCompileOptions::None,
		&Results
	);
	TestEqual(TEXT("The persistent banner workflow compiles"),
		Results.NumErrors, 0);
	return true;
}

#endif
