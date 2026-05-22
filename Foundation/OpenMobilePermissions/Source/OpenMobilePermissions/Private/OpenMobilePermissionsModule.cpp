#include "OpenMobilePermissionsModule.h"

#include "IOpenMobilePermissionProvider.h"
#include "Modules/ModuleManager.h"
#include "OpenMobilePermissions.h"

void FOpenMobilePermissionsModule::StartupModule()
{
	FOpenMobilePermissionProviderRegistry::Start();
	FOpenMobilePermissions::Start();
}

void FOpenMobilePermissionsModule::ShutdownModule()
{
	FOpenMobilePermissions::BeginShutdown();
	FOpenMobilePermissionProviderRegistry::BeginShutdown();
}

IMPLEMENT_MODULE(FOpenMobilePermissionsModule, OpenMobilePermissions)
