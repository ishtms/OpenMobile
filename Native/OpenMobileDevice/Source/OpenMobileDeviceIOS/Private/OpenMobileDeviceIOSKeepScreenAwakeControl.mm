#include "OpenMobileDeviceIOSKeepScreenAwakeControl.h"

#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"

#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSKeepScreenAwakeControlPrivate
{
	bool bHasSavedState = false;
	bool bSavedIdleTimerDisabled = false;

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
}

FOpenMobileKeepScreenAwakeResult ApplyOpenMobileDeviceIOSKeepScreenAwake()
{
	FOpenMobileKeepScreenAwakeResult Result;
#if TARGET_OS_SIMULATOR
	Result.State = EOpenMobileKeepScreenAwakeApplyState::Unsupported;
	Result.Error = FOpenMobileError::Make(
		EOpenMobileErrorCode::NotSupported,
		TEXT("iOS Simulator does not represent device idle-timer behavior."),
		FString(),
		TEXT("IOS")
	);
#else
	using namespace OpenMobileDeviceIOSKeepScreenAwakeControlPrivate;
	FOpenMobileKeepScreenAwakeResult* ResultPtr = &Result;
	RunOnMainThread(^{
		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		FIOSView* View = AppDelegate.IOSView;
		UIApplication* Application = [UIApplication sharedApplication];
		if (View == nil || View.window == nil
			|| Application.applicationState == UIApplicationStateBackground)
		{
			ResultPtr->State =
				EOpenMobileKeepScreenAwakeApplyState::Rejected;
			ResultPtr->Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The active iOS app window is unavailable for the keep-awake request."),
				FString(),
				TEXT("IOS")
			);
			return;
		}
		if (!bHasSavedState)
		{
			bSavedIdleTimerDisabled = Application.idleTimerDisabled;
			bHasSavedState = true;
		}
		Application.idleTimerDisabled = YES;
		const bool bEffective = Application.idleTimerDisabled;
		ResultPtr->bEffectiveKeepScreenAwake =
			FOpenMobileDeviceOptionalBool::MakeAvailable(bEffective);
		ResultPtr->State = bEffective
			? EOpenMobileKeepScreenAwakeApplyState::Applied
			: EOpenMobileKeepScreenAwakeApplyState::Rejected;
		if (!bEffective)
		{
			bHasSavedState = false;
			bSavedIdleTimerDisabled = false;
			ResultPtr->Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("iOS did not disable the app idle timer."),
				FString(),
				TEXT("IOS")
			);
		}
	});
#endif
	return Result;
}

void ClearOpenMobileDeviceIOSKeepScreenAwake()
{
#if !TARGET_OS_SIMULATOR
	using namespace OpenMobileDeviceIOSKeepScreenAwakeControlPrivate;
	RunOnMainThread(^{
		if (!bHasSavedState)
		{
			return;
		}
		UIApplication* Application = [UIApplication sharedApplication];
		if (Application.idleTimerDisabled)
		{
			Application.idleTimerDisabled = bSavedIdleTimerDisabled;
		}
		bHasSavedState = false;
		bSavedIdleTimerDisabled = false;
	});
#endif
}
