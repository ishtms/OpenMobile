#include "OpenMobileHapticsBlueprintCompilerExtension.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_CallFunction.h"
#include "OpenMobileHapticNamedPlaybackAsyncAction.h"
#include "OpenMobileHapticPatternPlaybackAsyncAction.h"
#include "OpenMobileHapticPreparationAsyncAction.h"
#include "OpenMobileHapticsBlueprintLibrary.h"

namespace OpenMobileHapticsBlueprintCompilerExtensionPrivate
{
	/** Restricts static validation to unlinked pins, connected values aren't known during Blueprint compilation. */
	bool IsLiteral(const UEdGraphPin* Pin)
	{
		return Pin && Pin->LinkedTo.IsEmpty();
	}

	/** Routes validation through compiler results so warnings attach to the actual Blueprint node. */
	void Warn(
		const FKismetCompilerContext& Context,
		UEdGraphNode* Node,
		const TCHAR* Message
	)
	{
		Context.MessageLog.Warning(
			*FString::Printf(TEXT("@@ %s"), Message),
			Node
		);
	}

	/** Checks a literal identifier against configured names while leaving dynamic pins for runtime validation. */
	void ValidateLiteralName(
		const FKismetCompilerContext& Context,
		UK2Node_CallFunction* Node,
		const TArray<FName>& AllowedNames,
		const TCHAR* Kind
	)
	{
		const UEdGraphPin* NamePin = Node->FindPin(TEXT("Name"));
		if (!IsLiteral(NamePin))
		{
			return;
		}
		const FName Value(*NamePin->DefaultValue);
		if (Value.IsNone())
		{
			Warn(
				Context,
				Node,
				*FString::Printf(
					TEXT("needs a %s selected from its project-backed picker."),
					Kind
				)
			);
			return;
		}
		if (!AllowedNames.Contains(Value))
		{
			Warn(
				Context,
				Node,
				*FString::Printf(
					TEXT("uses unknown literal %s '%s'. Select a configured value or connect a dynamic identifier."),
					Kind,
					*Value.ToString()
				)
			);
		}
	}

	/** Applies function-specific checks only to Haptics library calls recognised by reflection identity. */
	void ValidateCall(
		const FKismetCompilerContext& Context,
		UK2Node_CallFunction* Node
	)
	{
		const UFunction* Function = Node->GetTargetFunction();
		if (!Function
			|| Function->GetOuterUClass()
				!= UOpenMobileHapticsBlueprintLibrary::StaticClass())
		{
			return;
		}
		const FName FunctionName = Function->GetFName();
		if (FunctionName == TEXT("MakeHapticPatternIdentifier"))
		{
			ValidateLiteralName(
				Context,
				Node,
				UOpenMobileHapticsBlueprintLibrary::
					GetConfiguredHapticPatternNames(),
				TEXT("pattern")
			);
		}
		else if (FunctionName == TEXT("MakeHapticLibraryIdentifier"))
		{
			ValidateLiteralName(
				Context,
				Node,
				UOpenMobileHapticsBlueprintLibrary::
					GetConfiguredHapticLibraryNames(),
				TEXT("library")
			);
		}
		else if (FunctionName == TEXT("MakeHapticChannelIdentifier"))
		{
			ValidateLiteralName(
				Context,
				Node,
				UOpenMobileHapticsBlueprintLibrary::
					GetConfiguredHapticChannelNames(),
				TEXT("channel")
			);
		}
		else if (FunctionName == TEXT("MakeHapticCategoryIdentifier"))
		{
			ValidateLiteralName(
				Context,
				Node,
				UOpenMobileHapticsBlueprintLibrary::
					GetConfiguredHapticCategoryNames(),
				TEXT("category")
			);
		}
		else if (FunctionName == TEXT("MakeHapticEffectIdentifier"))
		{
			ValidateLiteralName(
				Context,
				Node,
				UOpenMobileHapticsBlueprintLibrary::
					GetConfiguredHapticEffectNames(),
				TEXT("effect")
			);
		}
		else if (FunctionName == TEXT("MakeHapticGameTimeSchedule")
			|| FunctionName == TEXT("MakeHapticAudioTimeSchedule"))
		{
			Warn(
				Context,
				Node,
				TEXT("uses an absolute Haptic schedule. Calibrate the matching clock after lifecycle changes before this path executes.")
			);
		}
	}

	/** Applies the same literal checks to async action nodes whose factory functions bypass ordinary call-node handling. */
	void ValidateAsync(
		const FKismetCompilerContext& Context,
		UK2Node_AsyncAction* Node
	)
	{
		const UFunction* Function = Node->GetFactoryFunction();
		if (!Function)
		{
			return;
		}
		if (Function->GetOuterUClass()
			== UOpenMobileHapticPatternPlaybackAsyncAction::StaticClass())
		{
			const UEdGraphPin* PatternPin = Node->FindPin(TEXT("Pattern"));
			if (IsLiteral(PatternPin) && !PatternPin->DefaultObject)
			{
				Warn(
					Context,
					Node,
					TEXT("needs a Haptic Pattern asset or a connected asset reference.")
				);
			}
		}
		else if (Function->GetOuterUClass()
			== UOpenMobileHapticNamedPlaybackAsyncAction::StaticClass())
		{
			const UEdGraphPin* PatternPin = Node->FindPin(TEXT("Pattern"));
			if (IsLiteral(PatternPin)
				&& (PatternPin->DefaultValue.IsEmpty()
					|| PatternPin->DefaultValue.Contains(TEXT("Name=None"))))
			{
				Warn(
					Context,
					Node,
					TEXT("needs a configured Haptic pattern identifier. Use the project-backed identifier picker or connect a dynamic identifier.")
				);
			}
		}
		else if (Function->GetOuterUClass()
			== UOpenMobileHapticPreparationAsyncAction::StaticClass())
		{
			if (Function->GetFName() == TEXT("PrepareHapticPatternAsync"))
			{
				const UEdGraphPin* PatternPin = Node->FindPin(TEXT("Pattern"));
				if (IsLiteral(PatternPin) && !PatternPin->DefaultObject)
				{
					Warn(
						Context,
						Node,
						TEXT("needs a Haptic Pattern asset to prepare.")
					);
				}
			}
			else if (Function->GetFName()
				== TEXT("PrepareHapticLibraryAsync"))
			{
				const UEdGraphPin* LibraryPin = Node->FindPin(TEXT("Library"));
				if (IsLiteral(LibraryPin)
					&& (LibraryPin->DefaultValue.IsEmpty()
						|| LibraryPin->DefaultValue.Contains(TEXT("Name=None"))))
				{
					Warn(
						Context,
						Node,
						TEXT("needs a configured Haptic library identifier from the project-backed picker.")
					);
				}
			}
		}
	}
}

void UOpenMobileHapticsBlueprintCompilerExtension::ProcessBlueprintCompiled(
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
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				OpenMobileHapticsBlueprintCompilerExtensionPrivate::
					ValidateCall(CompilationContext, Call);
			}
			else if (UK2Node_AsyncAction* Async = Cast<UK2Node_AsyncAction>(Node))
			{
				OpenMobileHapticsBlueprintCompilerExtensionPrivate::
					ValidateAsync(CompilationContext, Async);
			}
		}
	}
}
