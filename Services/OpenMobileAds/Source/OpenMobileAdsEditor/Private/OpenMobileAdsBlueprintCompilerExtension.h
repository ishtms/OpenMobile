#pragma once

#include "BlueprintCompilerExtension.h"
#include "OpenMobileAdsBlueprintCompilerExtension.generated.h"

UCLASS()
class UOpenMobileAdsBlueprintCompilerExtension final :
	public UBlueprintCompilerExtension
{
	GENERATED_BODY()

protected:
	virtual void ProcessBlueprintCompiled(
		const FKismetCompilerContext& CompilationContext,
		const FBlueprintCompiledData& Data
	) override;
};
