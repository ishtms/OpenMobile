#include "OpenMobileAdsBlueprintCompilerExtension.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_CallFunction.h"
#include "OpenMobileAdsAsyncAction.h"
#include "OpenMobileAdsRewardedAsyncAction.h"
#include "OpenMobileAdsSubsystem.h"

namespace OpenMobileAdsBlueprintCompilerExtensionPrivate
{
	bool IsAdsPlacementFunction(const UFunction* Function)
	{
		if (!Function || !Function->FindPropertyByName(TEXT("Placement")))
		{
			return false;
		}
		const UClass* Owner = Function->GetOuterUClass();
		return Owner == UOpenMobileAdsSubsystem::StaticClass()
			|| Owner == UOpenMobileAdsAsyncAction::StaticClass()
			|| Owner == UOpenMobileAdsRewardedAsyncAction::StaticClass();
	}

	void ValidateLiteralPlacement(
		const FKismetCompilerContext& Context,
		UEdGraphNode* Node,
		const UFunction* Function
	)
	{
		if (!IsAdsPlacementFunction(Function))
		{
			return;
		}
		const UEdGraphPin* Pin = Node->FindPin(TEXT("Placement"));
		if (!Pin || !Pin->LinkedTo.IsEmpty())
		{
			return;
		}
		const FName Placement(*Pin->DefaultValue);
		if (Placement.IsNone())
		{
			Context.MessageLog.Warning(
				TEXT("@@ needs an Ads placement selected from its project-backed picker."),
				Node
			);
			return;
		}
		if (!UOpenMobileAdsSubsystem::GetConfiguredAdsPlacementNames().Contains(
			Placement
		))
		{
			Context.MessageLog.Warning(
				*FString::Printf(
					TEXT("@@ uses unknown literal Ads placement '%s'. Select a configured placement or connect a dynamic name."),
					*Placement.ToString()
				),
				Node
			);
		}
	}
}

void UOpenMobileAdsBlueprintCompilerExtension::ProcessBlueprintCompiled(
	const FKismetCompilerContext& CompilationContext,
	const FBlueprintCompiledData& Data
)
{
	static_cast<void>(Data);
	if (!CompilationContext.Blueprint)
	{
		return;
	}
	TArray<UEdGraph*> Graphs;
	CompilationContext.Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_AsyncAction* Async = Cast<UK2Node_AsyncAction>(Node))
			{
				OpenMobileAdsBlueprintCompilerExtensionPrivate::
					ValidateLiteralPlacement(
						CompilationContext,
						Async,
						Async->GetFactoryFunction()
					);
			}
			else if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				OpenMobileAdsBlueprintCompilerExtensionPrivate::
					ValidateLiteralPlacement(
						CompilationContext,
						Call,
						Call->GetTargetFunction()
					);
			}
		}
	}
}
