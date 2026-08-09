#pragma once

#include "BlueprintCompilerExtension.h"
#include "OpenMobileHapticsBlueprintCompilerExtension.generated.h"

UCLASS()
class UOpenMobileHapticsBlueprintCompilerExtension final :
	public UBlueprintCompilerExtension
{
	GENERATED_BODY()

protected:
	virtual void ProcessBlueprintCompiled(
		const FKismetCompilerContext& CompilationContext,
		const FBlueprintCompiledData& Data
	) override;
};
