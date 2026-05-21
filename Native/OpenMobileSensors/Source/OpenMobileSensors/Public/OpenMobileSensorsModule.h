#pragma once

#include "Modules/ModuleInterface.h"
#include "OpenMobileCoreTypes.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FOpenMobileCapability GetBackendCapability();
};
