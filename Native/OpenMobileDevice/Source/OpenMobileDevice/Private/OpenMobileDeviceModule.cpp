#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBrightnessControlService.h"
#include "OpenMobileDeviceKeepScreenAwakeControlService.h"
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
		FOpenMobileDeviceKeepScreenAwakeControlService::Start();
	}

	virtual void ShutdownModule() override
	{
		FOpenMobileDeviceKeepScreenAwakeControlService::Shutdown();
		FOpenMobileDeviceBrightnessControlService::Shutdown();
		FOpenMobileDeviceRefreshRateControlService::Shutdown();
		FOpenMobileDeviceOrientationControlService::Shutdown();
		FOpenMobileDeviceMonitoringService::Shutdown();
		FOpenMobileDeviceBackendRegistry::BeginShutdown();
	}
};

IMPLEMENT_MODULE(FOpenMobileDeviceModule, OpenMobileDevice)
