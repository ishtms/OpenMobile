#pragma once

#include "Modules/ModuleInterface.h"

class OPENMOBILECORE_API FOpenMobileCoreModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
