#include "OpenMobileHapticsBackendRegistry.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

class FOpenMobileHapticsModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FOpenMobileHapticsBackendRegistry::Start();
		BackgroundHandle =
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddStatic(
				&FOpenMobileHapticsBackendRegistry::NotifyLifecycleChange
			);
		ForegroundHandle =
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddStatic(
				&FOpenMobileHapticsBackendRegistry::NotifyLifecycleChange
			);
		ReactivatedHandle =
			FCoreDelegates::ApplicationHasReactivatedDelegate.AddStatic(
				&FOpenMobileHapticsBackendRegistry::NotifyLifecycleChange
			);
	}

	virtual void ShutdownModule() override
	{
		if (BackgroundHandle.IsValid())
		{
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(
				BackgroundHandle
			);
		}
		if (ForegroundHandle.IsValid())
		{
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(
				ForegroundHandle
			);
		}
		if (ReactivatedHandle.IsValid())
		{
			FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(
				ReactivatedHandle
			);
		}
		FOpenMobileHapticsBackendRegistry::BeginShutdown();
	}

private:
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;
};

IMPLEMENT_MODULE(FOpenMobileHapticsModule, OpenMobileHaptics)
