#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBrightnessControlService.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceOrientationControlService.h"
#include "OpenMobileDeviceRefreshRateControlService.h"

#include "Modules/ModuleManager.h"

class FOpenMobileDeviceModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FOpenMobileDeviceBackendRegistry::Start();
		FOpenMobileDeviceMonitoringService::Start();
		FOpenMobileDeviceOrientationControlService::Start();
		FOpenMobileDeviceRefreshRateControlService::Start();
		FOpenMobileDeviceBrightnessControlService::Start();
	}

	virtual void ShutdownModule() override
	{
		FOpenMobileDeviceRefreshRateControlService::Shutdown();
		FOpenMobileDeviceBrightnessControlService::Shutdown();
		FOpenMobileDeviceOrientationControlService::Shutdown();
		FOpenMobileDeviceMonitoringService::Shutdown();
		FOpenMobileDeviceBackendRegistry::BeginShutdown();
	}
};

IMPLEMENT_MODULE(FOpenMobileDeviceModule, OpenMobileDevice)
