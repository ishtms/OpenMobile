#include "OpenMobileSensorsModule.h"

#include "Modules/ModuleManager.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMetadataService.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

void FOpenMobileSensorsModule::StartupModule()
{
	FOpenMobileSensorsCapabilityService::Start();
	FOpenMobileSensorsMetadataService::Start();
	FOpenMobileSensorsSampleService::Start();
	FOpenMobileSensorsSubscriptionService::Start();
	FOpenMobileSensorsBackendRegistry::Start();
}

void FOpenMobileSensorsModule::ShutdownModule()
{
	FOpenMobileSensorsCapabilityService::BeginShutdown();
	FOpenMobileSensorsMetadataService::BeginShutdown();
	FOpenMobileSensorsSubscriptionService::BeginShutdown();
	FOpenMobileSensorsSampleService::BeginShutdown();
	FOpenMobileSensorsBackendRegistry::BeginShutdown();
}

FOpenMobileCapability FOpenMobileSensorsModule::GetBackendCapability()
{
	return FOpenMobileSensorsCapabilityService::GetSnapshot()
		.BackendAvailability;
}

IMPLEMENT_MODULE(FOpenMobileSensorsModule, OpenMobileSensors)
