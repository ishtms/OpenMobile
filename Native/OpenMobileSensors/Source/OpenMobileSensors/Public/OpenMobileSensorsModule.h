#pragma once

#include "Modules/ModuleInterface.h"
#include "OpenMobileCoreTypes.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsModule final : public IModuleInterface
{
public:
	/** Unreal calls this when the plugin module loads. It registers the sensor services only, hardware still waits for a request. */
	virtual void StartupModule() override;

	/** Unreal calls this before unloading the module. Services are removed here so callers can't retain dead providers. */
	virtual void ShutdownModule() override;

	/** You'll get the module's portable support declaration without starting a sensor backend. */
	static FOpenMobileCapability GetBackendCapability();
};
