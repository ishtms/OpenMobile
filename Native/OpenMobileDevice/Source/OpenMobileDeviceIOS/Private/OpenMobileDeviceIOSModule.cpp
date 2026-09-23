#include "OpenMobileDeviceIOSBackend.h"

#include "Modules/ModuleManager.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceEndpointTransport.h"

FOpenMobileDeviceEndpointResponse OpenMobileQueryIOSEndpoint(const FString&,
	const FOpenMobileEndpointReachabilityOptions&, const TAtomic<bool>&);

class FOpenMobileDeviceIOSModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FOpenMobileDeviceEndpointTransport::SetPlatformQuery(OpenMobileQueryIOSEndpoint);
		Backend = MakeUnique<FOpenMobileDeviceIOSBackend>();
		if (!FOpenMobileDeviceBackendRegistry::RegisterBackend(*Backend))
		{
			Backend.Reset();
		}
	}

	virtual void ShutdownModule() override
	{
		FOpenMobileDeviceEndpointTransport::Shutdown();
		FOpenMobileDeviceEndpointTransport::SetPlatformQuery(nullptr);
		if (Backend)
		{
			FOpenMobileDeviceBackendRegistry::UnregisterBackend(*Backend);
			Backend.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileDeviceIOSBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileDeviceIOSModule, OpenMobileDeviceIOS)
