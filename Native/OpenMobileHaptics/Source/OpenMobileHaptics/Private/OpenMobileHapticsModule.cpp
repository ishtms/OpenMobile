#include "OpenMobileHapticsBackendRegistry.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

class FOpenMobileHapticsModule final : public IModuleInterface
{
public:
	/** Registers the fallback backend and lifecycle hooks after runtime modules are ready. */
	virtual void StartupModule() override
	{
		FOpenMobileHapticsBackendRegistry::Start();
		DeactivatedHandle =
			FCoreDelegates::ApplicationWillDeactivateDelegate.AddLambda(
				[]()
				{
					FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
						EOpenMobileHapticsLifecycleEvent::WillDeactivate
					);
				}
			);
		BackgroundHandle =
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddLambda(
				[]()
				{
					FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
						EOpenMobileHapticsLifecycleEvent::WillEnterBackground
					);
				}
			);
		ForegroundHandle =
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddLambda(
				[]()
				{
					FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
						EOpenMobileHapticsLifecycleEvent::HasEnteredForeground
					);
				}
			);
		ReactivatedHandle =
			FCoreDelegates::ApplicationHasReactivatedDelegate.AddLambda(
				[]()
				{
					FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
						EOpenMobileHapticsLifecycleEvent::HasReactivated
					);
				}
			);
		TerminationHandle =
			FCoreDelegates::GetApplicationWillTerminateDelegate().AddLambda(
				[]()
				{
					FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
						EOpenMobileHapticsLifecycleEvent::WillTerminate
					);
				}
			);
	}

	/** Disconnects lifecycle hooks and seals backend registry before module-owned objects unload. */
	virtual void ShutdownModule() override
	{
		if (DeactivatedHandle.IsValid())
		{
			FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(
				DeactivatedHandle
			);
		}
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
		if (TerminationHandle.IsValid())
		{
			FCoreDelegates::GetApplicationWillTerminateDelegate().Remove(
				TerminationHandle
			);
		}
		FOpenMobileHapticsBackendRegistry::BeginShutdown();
	}

private:
	FDelegateHandle DeactivatedHandle;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;
	FDelegateHandle TerminationHandle;
};

IMPLEMENT_MODULE(FOpenMobileHapticsModule, OpenMobileHaptics)
