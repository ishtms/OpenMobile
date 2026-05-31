#include "OpenMobileSensorsModule.h"

#include "Modules/ModuleManager.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsSubscriptionService.h"

void FOpenMobileSensorsModule::StartupModule()
{
	FOpenMobileSensorsCapabilityService::Start();
	FOpenMobileSensorsSubscriptionService::Start();
	FOpenMobileSensorsBackendRegistry::Start();
}

void FOpenMobileSensorsModule::ShutdownModule()
{
	FOpenMobileSensorsCapabilityService::BeginShutdown();
	FOpenMobileSensorsSubscriptionService::BeginShutdown();
	FOpenMobileSensorsBackendRegistry::BeginShutdown();
}

FOpenMobileCapability FOpenMobileSensorsModule::GetBackendCapability()
{
	return FOpenMobileSensorsCapabilityService::GetSnapshot()
		.BackendAvailability;
}

IMPLEMENT_MODULE(FOpenMobileSensorsModule, OpenMobileSensors)
