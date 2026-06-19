#include "OpenMobileDeviceApplicationSettingsService.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBrightnessControlService.h"
#include "OpenMobileDeviceClipboardService.h"
#include "OpenMobileDeviceFlashlightControlService.h"
#include "OpenMobileDeviceKeepScreenAwakeControlService.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceOrientationControlService.h"
#include "OpenMobileDeviceRefreshRateControlService.h"
#include "OpenMobileDeviceSystemUiControlService.h"
#include "OpenMobileDeviceUserInitiatedPasteService.h"

#include "Modules/ModuleManager.h"

class FOpenMobileDeviceModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FOpenMobileDeviceBackendRegistry::Start();
		FOpenMobileDeviceMonitoringService::Start();
		FOpenMobileDeviceApplicationSettingsService::Start();
		FOpenMobileDeviceOrientationControlService::Start();
		FOpenMobileDeviceRefreshRateControlService::Start();
		FOpenMobileDeviceBrightnessControlService::Start();
		FOpenMobileDeviceFlashlightControlService::Start();
		FOpenMobileDeviceClipboardService::Start();
		FOpenMobileDeviceKeepScreenAwakeControlService::Start();
		FOpenMobileDeviceSystemUiControlService::Start();
		FOpenMobileDeviceUserInitiatedPasteService::Start();
	}

	virtual void ShutdownModule() override
	{
		FOpenMobileDeviceUserInitiatedPasteService::Shutdown();
		FOpenMobileDeviceApplicationSettingsService::Shutdown();
		FOpenMobileDeviceSystemUiControlService::Shutdown();
		FOpenMobileDeviceKeepScreenAwakeControlService::Shutdown();
		FOpenMobileDeviceClipboardService::Shutdown();
		FOpenMobileDeviceFlashlightControlService::Shutdown();
		FOpenMobileDeviceBrightnessControlService::Shutdown();
		FOpenMobileDeviceRefreshRateControlService::Shutdown();
		FOpenMobileDeviceOrientationControlService::Shutdown();
		FOpenMobileDeviceMonitoringService::Shutdown();
		FOpenMobileDeviceBackendRegistry::BeginShutdown();
	}
};

IMPLEMENT_MODULE(FOpenMobileDeviceModule, OpenMobileDevice)
