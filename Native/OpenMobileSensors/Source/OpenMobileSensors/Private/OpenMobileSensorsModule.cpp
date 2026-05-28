#include "OpenMobileSensorsModule.h"

#include "IOpenMobileSensorsBackend.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsSubscriptionService.h"

void FOpenMobileSensorsModule::StartupModule()
{
	FOpenMobileSensorsSubscriptionService::Start();
	FOpenMobileSensorsBackendRegistry::Start();
}

void FOpenMobileSensorsModule::ShutdownModule()
{
	FOpenMobileSensorsSubscriptionService::BeginShutdown();
	FOpenMobileSensorsBackendRegistry::BeginShutdown();
}

FOpenMobileCapability FOpenMobileSensorsModule::GetBackendCapability()
{
	if (IOpenMobileSensorsBackend* Backend =
		FOpenMobileSensorsBackendRegistry::FindBackend())
	{
		return Backend->GetBackendCapability();
	}

	FOpenMobileCapability Capability;
	Capability.Name = IOpenMobileSensorsBackend::GetModularFeatureName();
	Capability.State = EOpenMobileCapabilityState::NotSupported;
	Capability.Detail = TEXT("No OpenMobile Sensors backend is registered.");
	return Capability;
}

IMPLEMENT_MODULE(FOpenMobileSensorsModule, OpenMobileSensors)
