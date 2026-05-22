#pragma once

#include "Modules/ModuleInterface.h"

class OPENMOBILEPERMISSIONS_API FOpenMobilePermissionsModule final
	: public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
