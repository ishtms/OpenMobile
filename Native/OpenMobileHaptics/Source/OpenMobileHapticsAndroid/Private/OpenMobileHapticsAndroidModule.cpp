#include "OpenMobileHapticsAndroidBackend.h"

#include "Modules/ModuleManager.h"
#include "OpenMobileHapticsBackendRegistry.h"

class FOpenMobileHapticsAndroidModule final : public IModuleInterface
{
public:
	/** Registers the Android backend and its active JNI bridge after the Java activity is available. */
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileHapticsAndroidBackend>();
		if (!FOpenMobileHapticsBackendRegistry::RegisterBackend(*Backend))
		{
			Backend.Reset();
		}
	}

	/** Seals JNI callbacks before unregistering and releasing the Android backend. */
	virtual void ShutdownModule() override
	{
		if (Backend)
		{
			FOpenMobileHapticsBackendRegistry::UnregisterBackend(*Backend);
			Backend.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileHapticsAndroidBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileHapticsAndroidModule, OpenMobileHapticsAndroid)
