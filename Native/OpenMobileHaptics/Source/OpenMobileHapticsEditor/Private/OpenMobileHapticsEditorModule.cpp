#include "BlueprintCompilationManager.h"
#include "Engine/Blueprint.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileHapticsBlueprintCompilerExtension.h"

class FOpenMobileHapticsEditorModule final : public IModuleInterface
{
public:
	/** Registers asset tools, factories, compiler checks, and editor commands only after their owning modules are loaded. */
	virtual void StartupModule() override
	{
		CompilerExtension.Reset(
			NewObject<UOpenMobileHapticsBlueprintCompilerExtension>()
		);
		FBlueprintCompilationManager::RegisterCompilerExtension(
			UBlueprint::StaticClass(),
			CompilerExtension.Get()
		);
	}

private:
	TStrongObjectPtr<UOpenMobileHapticsBlueprintCompilerExtension>
		CompilerExtension;
};

IMPLEMENT_MODULE(FOpenMobileHapticsEditorModule, OpenMobileHapticsEditor)
