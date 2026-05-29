#include "OpenMobileDeviceIOSOrientationControl.h"

#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"

#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSOrientationControlPrivate
{
	bool bHasSavedPolicy = false;
	UIInterfaceOrientationMask SavedViewMask = UIInterfaceOrientationMaskAll;
	UIInterfaceOrientationMask SavedEffectiveMask = UIInterfaceOrientationMaskAll;

	bool SupportsRuntimeOrientationRequests()
	{
		if (@available(iOS 16.0, *))
		{
			return true;
		}
		return false;
	}

	UIInterfaceOrientationMask GetRequestedMask(
		EOpenMobileOrientationPolicy Policy,
		UIInterfaceOrientationMask AutomaticMask
	)
	{
		switch (Policy)
		{
		case EOpenMobileOrientationPolicy::Automatic:
			return AutomaticMask;
		case EOpenMobileOrientationPolicy::Portrait:
			return UIInterfaceOrientationMaskPortrait
				| UIInterfaceOrientationMaskPortraitUpsideDown;
		case EOpenMobileOrientationPolicy::Landscape:
			return UIInterfaceOrientationMaskLandscape;
		case EOpenMobileOrientationPolicy::PortraitOnly:
			return UIInterfaceOrientationMaskPortrait;
		case EOpenMobileOrientationPolicy::PortraitUpsideDownOnly:
			return UIInterfaceOrientationMaskPortraitUpsideDown;
		case EOpenMobileOrientationPolicy::LandscapeLeftOnly:
			return UIInterfaceOrientationMaskLandscapeLeft;
		case EOpenMobileOrientationPolicy::LandscapeRightOnly:
			return UIInterfaceOrientationMaskLandscapeRight;
		}
		return 0;
	}

	UIViewController* GetPresentedViewController(
		UIViewController* Controller
	)
	{
		UIViewController* Presented = Controller;
		while (Presented.presentedViewController != nil)
		{
			Presented = Presented.presentedViewController;
		}
		return Presented;
	}

	void RequestGeometry(
		IOSViewController* Controller,
		UIWindowScene* WindowScene,
		UIInterfaceOrientationMask Mask
	)
	{
		[Controller setNeedsUpdateOfSupportedInterfaceOrientations];
		UIWindowSceneGeometryPreferencesIOS* Preferences =
			[[[UIWindowSceneGeometryPreferencesIOS alloc]
				initWithInterfaceOrientations:Mask] autorelease];
		[WindowScene requestGeometryUpdateWithPreferences:Preferences
			errorHandler:^(NSError* Error)
			{
				static_cast<void>(Error);
			}];
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
}

FOpenMobileOrientationPolicyResult ApplyOpenMobileDeviceIOSOrientationPolicy(
	const FOpenMobileOrientationPolicyRequest& Request
)
{
	using namespace OpenMobileDeviceIOSOrientationControlPrivate;
	FOpenMobileOrientationPolicyResult Result;
	Result.Request = Request;
	FOpenMobileOrientationPolicyResult* ResultPtr = &Result;
	RunOnMainThread(^{
		if (!SupportsRuntimeOrientationRequests())
		{
			ResultPtr->State =
				EOpenMobileOrientationPolicyApplyState::Unsupported;
			ResultPtr->Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NotSupported,
				TEXT("Runtime orientation requests require iOS 16 or newer."),
				FString(),
				TEXT("IOS")
			);
			return;
		}

		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		FIOSView* View = AppDelegate.IOSView;
		IOSViewController* Controller = AppDelegate.IOSController;
		UIWindowScene* WindowScene = View.window.windowScene;
		if (View == nil || Controller == nil || WindowScene == nil
			|| WindowScene.activationState == UISceneActivationStateBackground
			|| WindowScene.activationState == UISceneActivationStateUnattached)
		{
			ResultPtr->State = EOpenMobileOrientationPolicyApplyState::Rejected;
			ResultPtr->Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The active iOS scene is unavailable for the orientation request."),
				FString(),
				TEXT("IOS")
			);
			return;
		}

		const UIInterfaceOrientationMask BaseMask = bHasSavedPolicy
			? SavedEffectiveMask
			: [Controller supportedInterfaceOrientations_Internal];
		UIInterfaceOrientationMask EffectiveMask =
			GetRequestedMask(Request.Policy, BaseMask) & BaseMask;
		UIViewController* Presented = GetPresentedViewController(Controller);
		if (Presented != Controller)
		{
			EffectiveMask &= Presented.supportedInterfaceOrientations;
		}
		if (EffectiveMask == 0)
		{
			ResultPtr->State = EOpenMobileOrientationPolicyApplyState::Restricted;
			ResultPtr->Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NotSupported,
				TEXT("The project or active iOS presentation does not support the requested orientation."),
				FString(),
				TEXT("IOS")
			);
			return;
		}

		if (!bHasSavedPolicy)
		{
			SavedViewMask = View->SupportedInterfaceOrientations;
			SavedEffectiveMask = BaseMask;
			bHasSavedPolicy = true;
		}
		View->SupportedInterfaceOrientations = EffectiveMask;
		RequestGeometry(Controller, WindowScene, EffectiveMask);
		ResultPtr->State = EOpenMobileOrientationPolicyApplyState::Accepted;
	});
	return Result;
}

void ClearOpenMobileDeviceIOSOrientationPolicy()
{
	using namespace OpenMobileDeviceIOSOrientationControlPrivate;
	RunOnMainThread(^{
		if (!bHasSavedPolicy)
		{
			return;
		}
		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		FIOSView* View = AppDelegate.IOSView;
		IOSViewController* Controller = AppDelegate.IOSController;
		UIWindowScene* WindowScene = View.window.windowScene;
		if (View != nil)
		{
			View->SupportedInterfaceOrientations = SavedViewMask;
		}
		if (SupportsRuntimeOrientationRequests())
		{
			if (Controller != nil && WindowScene != nil)
			{
				RequestGeometry(Controller, WindowScene, SavedEffectiveMask);
			}
		}
		bHasSavedPolicy = false;
	});
}
