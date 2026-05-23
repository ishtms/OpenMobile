#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceRefreshRateControlService.h"

#include "Modules/ModuleManager.h"

class FOpenMobileDeviceModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FOpenMobileDeviceBackendRegistry::Start();
		FOpenMobileDeviceMonitoringService::Start();
		FOpenMobileDeviceRefreshRateControlService::Start();
	}

	virtual void ShutdownModule() override
	{
		FOpenMobileDeviceRefreshRateControlService::Shutdown();
		FOpenMobileDeviceMonitoringService::Shutdown();
		FOpenMobileDeviceBackendRegistry::BeginShutdown();
	}
};

IMPLEMENT_MODULE(FOpenMobileDeviceModule, OpenMobileDevice)
