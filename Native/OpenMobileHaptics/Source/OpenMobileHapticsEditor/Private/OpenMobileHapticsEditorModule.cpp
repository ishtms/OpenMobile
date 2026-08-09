#include "BlueprintCompilationManager.h"
#include "Engine/Blueprint.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileHapticsBlueprintCompilerExtension.h"

class FOpenMobileHapticsEditorModule final : public IModuleInterface
{
public:
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
