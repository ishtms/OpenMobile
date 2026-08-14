#pragma once

#include "BlueprintCompilerExtension.h"
#include "OpenMobileHapticsBlueprintCompilerExtension.generated.h"

UCLASS()
class UOpenMobileHapticsBlueprintCompilerExtension final :
	public UBlueprintCompilerExtension
{
	GENERATED_BODY()

protected:
	/** Scans compiled Blueprint defaults for invalid Haptics references so asset problems appear during compilation, not first playback. */
	virtual void ProcessBlueprintCompiled(
		const FKismetCompilerContext& CompilationContext,
		const FBlueprintCompiledData& Data
	) override;
};
