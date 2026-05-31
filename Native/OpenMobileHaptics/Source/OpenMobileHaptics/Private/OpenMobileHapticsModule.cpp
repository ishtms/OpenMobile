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
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddLambda(
				[]()
				{
					FOpenMobileHapticsBackendRegistry::SetApplicationActive(false);
				}
			);
		ForegroundHandle =
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddLambda(
				[]()
				{
					FOpenMobileHapticsBackendRegistry::SetApplicationActive(true);
				}
			);
		ReactivatedHandle =
			FCoreDelegates::ApplicationHasReactivatedDelegate.AddLambda(
				[]()
				{
					FOpenMobileHapticsBackendRegistry::SetApplicationActive(true);
				}
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
