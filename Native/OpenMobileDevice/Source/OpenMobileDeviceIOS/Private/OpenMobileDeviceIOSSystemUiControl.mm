#include "OpenMobileDeviceIOSSystemUiControl.h"

#include "Async/Async.h"
#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"
#include "OpenMobileDeviceMonitoringService.h"

#import <TargetConditionals.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

namespace OpenMobileDeviceIOSSystemUiControlPrivate
{
	using FPreferenceFunction = BOOL(*)(id, SEL);

	FPreferenceFunction OriginalPrefersStatusBarHidden = nullptr;
	FPreferenceFunction OriginalPrefersHomeIndicatorAutoHidden = nullptr;
	IOSViewController* ControlledController = nil;
	UIRectEdge SavedExtendedEdges = UIRectEdgeNone;
	BOOL bSavedExtendedLayoutIncludesOpaqueBars = NO;
	EOpenMobileSystemUiMode EffectiveMode = EOpenMobileSystemUiMode::Normal;
	bool bHooksInstalled = false;
	bool bOverrideActive = false;

	BOOL PrefersStatusBarHidden(id Self, SEL Selector)
	{
		if (bOverrideActive)
		{
			return EffectiveMode == EOpenMobileSystemUiMode::Immersive;
		}
		return OriginalPrefersStatusBarHidden
			? OriginalPrefersStatusBarHidden(Self, Selector)
			: NO;
	}

	BOOL PrefersHomeIndicatorAutoHidden(id Self, SEL Selector)
	{
		if (bOverrideActive)
		{
			return EffectiveMode == EOpenMobileSystemUiMode::Immersive;
		}
		return OriginalPrefersHomeIndicatorAutoHidden
			? OriginalPrefersHomeIndicatorAutoHidden(Self, Selector)
			: NO;
	}

	bool InstallPreferenceHooks()
	{
		if (bHooksInstalled)
		{
			return true;
		}
		Class ControllerClass = [IOSViewController class];
		Method StatusMethod = class_getInstanceMethod(
			ControllerClass,
			@selector(prefersStatusBarHidden)
		);
		Method HomeMethod = class_getInstanceMethod(
			ControllerClass,
			@selector(prefersHomeIndicatorAutoHidden)
		);
		if (!StatusMethod || !HomeMethod)
		{
			return false;
		}
		OriginalPrefersStatusBarHidden =
			reinterpret_cast<FPreferenceFunction>(
				method_getImplementation(StatusMethod)
			);
		OriginalPrefersHomeIndicatorAutoHidden =
			reinterpret_cast<FPreferenceFunction>(
				method_getImplementation(HomeMethod)
			);
		method_setImplementation(
			StatusMethod,
			reinterpret_cast<IMP>(&PrefersStatusBarHidden)
		);
		method_setImplementation(
			HomeMethod,
			reinterpret_cast<IMP>(&PrefersHomeIndicatorAutoHidden)
		);
		bHooksInstalled = true;
		return true;
	}

	void RunOnMainThread(dispatch_block_t Block)
	{
		if ([NSThread isMainThread])
		{
			Block();
		}
		else
		{
			dispatch_sync(dispatch_get_main_queue(), Block);
		}
	}

	void NotifyWindowSettled()
	{
		dispatch_async(dispatch_get_main_queue(), ^{
			dispatch_async(dispatch_get_main_queue(), ^{
				AsyncTask(ENamedThreads::GameThread, []
				{
					FOpenMobileDeviceMonitoringService::NotifyWindowSettled();
				});
			});
		});
	}

	void RefreshControllerPreferences(IOSViewController* Controller)
	{
		[Controller setNeedsStatusBarAppearanceUpdate];
		[Controller setNeedsUpdateOfHomeIndicatorAutoHidden];
		[Controller.view setNeedsLayout];
		NotifyWindowSettled();
	}

	void RestoreController()
	{
		if (ControlledController == nil)
		{
			bOverrideActive = false;
			return;
		}
		bOverrideActive = false;
		ControlledController.edgesForExtendedLayout = SavedExtendedEdges;
		ControlledController.extendedLayoutIncludesOpaqueBars =
			bSavedExtendedLayoutIncludesOpaqueBars;
		RefreshControllerPreferences(ControlledController);
		[ControlledController release];
		ControlledController = nil;
	}
}

FOpenMobileSystemUiResult ApplyOpenMobileDeviceIOSSystemUiMode(
	const FOpenMobileSystemUiRequest& Request
)
{
	FOpenMobileSystemUiResult Result;
	Result.Request = Request;
#if TARGET_OS_SIMULATOR
	Result.State = EOpenMobileSystemUiApplyState::Unsupported;
	Result.Error = FOpenMobileError::Make(
		EOpenMobileErrorCode::NotSupported,
		TEXT("iOS Simulator does not represent device system-bar behavior."),
		FString(),
		TEXT("IOS")
	);
#else
	using namespace OpenMobileDeviceIOSSystemUiControlPrivate;
	FOpenMobileSystemUiResult* ResultPtr = &Result;
	RunOnMainThread(^{
		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		FIOSView* View = AppDelegate.IOSView;
		IOSViewController* Controller = AppDelegate.IOSController;
		UIApplication* Application = [UIApplication sharedApplication];
		if (View == nil || View.window == nil || Controller == nil
			|| Application.applicationState == UIApplicationStateBackground)
		{
			ResultPtr->State = EOpenMobileSystemUiApplyState::Rejected;
			ResultPtr->Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The active iOS app window is unavailable for the system UI request."),
				FString(),
				TEXT("IOS")
			);
			return;
		}
		if (!InstallPreferenceHooks())
		{
			ResultPtr->State = EOpenMobileSystemUiApplyState::Unsupported;
			ResultPtr->Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NotSupported,
				TEXT("The Unreal iOS view controller does not expose system UI preferences."),
				FString(),
				TEXT("IOS")
			);
			return;
		}
		if (ControlledController != Controller)
		{
			RestoreController();
			ControlledController = [Controller retain];
			SavedExtendedEdges = Controller.edgesForExtendedLayout;
			bSavedExtendedLayoutIncludesOpaqueBars =
				Controller.extendedLayoutIncludesOpaqueBars;
		}
		EffectiveMode = Request.Mode;
		bOverrideActive = true;
		Controller.edgesForExtendedLayout =
			Request.Mode == EOpenMobileSystemUiMode::Normal
				? UIRectEdgeNone
				: UIRectEdgeAll;
		Controller.extendedLayoutIncludesOpaqueBars =
			Request.Mode != EOpenMobileSystemUiMode::Normal;
		RefreshControllerPreferences(Controller);
		ResultPtr->bEffectiveModeAvailable = true;
		ResultPtr->EffectiveMode = EffectiveMode;
		ResultPtr->State = EOpenMobileSystemUiApplyState::Applied;
	});
#endif
	return Result;
}

void ClearOpenMobileDeviceIOSSystemUiMode()
{
#if !TARGET_OS_SIMULATOR
	using namespace OpenMobileDeviceIOSSystemUiControlPrivate;
	RunOnMainThread(^{
		RestoreController();
	});
#endif
}
