#include "OpenMobileDeviceIOSBrightnessControl.h"

#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"
#include "OpenMobileDeviceBrightnessOverrideState.h"

#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSBrightnessControlPrivate
{
	FOpenMobileDeviceBrightnessOverrideState OverrideState;
	UIScreen* ControlledScreen = nil;

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

	uint64 GetScopeId(UIScreen* Screen)
	{
		return static_cast<uint64>(reinterpret_cast<UPTRINT>(Screen));
	}

	UIScreen* GetActiveScreen()
	{
		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		FIOSView* View = AppDelegate.IOSView;
		return View == nil ? nil : View.window.screen;
	}

	void RestoreControlledScreen()
	{
		if (ControlledScreen == nil)
		{
			OverrideState.Reset();
			return;
		}
		const TOptional<float> RestoreValue = OverrideState.ReleaseScope(
			GetScopeId(ControlledScreen),
			static_cast<float>(ControlledScreen.brightness)
		);
		if (RestoreValue.IsSet())
		{
			ControlledScreen.brightness = RestoreValue.GetValue();
		}
		[ControlledScreen release];
		ControlledScreen = nil;
	}
}

FOpenMobileBrightnessSnapshot GetOpenMobileDeviceIOSBrightnessSnapshot()
{
	FOpenMobileBrightnessSnapshot Snapshot;
#if !TARGET_OS_SIMULATOR
	using namespace OpenMobileDeviceIOSBrightnessControlPrivate;
	FOpenMobileBrightnessSnapshot* SnapshotPtr = &Snapshot;
	RunOnMainThread(^{
		UIScreen* Screen = GetActiveScreen();
		if (Screen != nil && Screen == [UIScreen mainScreen])
		{
			SnapshotPtr->CurrentBrightness =
				FOpenMobileDeviceOptionalFloat::MakeAvailable(
					static_cast<float>(Screen.brightness)
				);
		}
	});
#endif
	return Snapshot;
}

FOpenMobileBrightnessResult ApplyOpenMobileDeviceIOSBrightness(
	const FOpenMobileBrightnessRequest& Request
)
{
	FOpenMobileBrightnessResult Result;
	Result.Request = Request;
#if TARGET_OS_SIMULATOR
	Result.State = EOpenMobileBrightnessApplyState::Unsupported;
	Result.Error = FOpenMobileError::Make(
		EOpenMobileErrorCode::NotSupported,
		TEXT("iOS Simulator does not control device screen brightness."),
		FString(),
		TEXT("IOS")
	);
#else
	using namespace OpenMobileDeviceIOSBrightnessControlPrivate;
	FOpenMobileBrightnessResult* ResultPtr = &Result;
	RunOnMainThread(^{
		UIScreen* Screen = GetActiveScreen();
		if (Screen == nil || Screen != [UIScreen mainScreen])
		{
			RestoreControlledScreen();
			ResultPtr->State = EOpenMobileBrightnessApplyState::Unsupported;
			ResultPtr->Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NotSupported,
				TEXT("iOS brightness control is supported only on the main screen."),
				FString(),
				TEXT("IOS")
			);
			return;
		}
		if (ControlledScreen != Screen)
		{
			RestoreControlledScreen();
			ControlledScreen = [Screen retain];
		}
		const uint64 ScopeId = GetScopeId(Screen);
		OverrideState.BeginScope(
			ScopeId,
			static_cast<float>(Screen.brightness)
		);
		Screen.brightness = Request.Brightness;
		const float Effective = static_cast<float>(Screen.brightness);
		OverrideState.RecordApplied(ScopeId, Effective);
		ResultPtr->EffectiveBrightness =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(Effective);
		ResultPtr->State = EOpenMobileBrightnessApplyState::Applied;
	});
#endif
	return Result;
}

void ClearOpenMobileDeviceIOSBrightness()
{
#if !TARGET_OS_SIMULATOR
	using namespace OpenMobileDeviceIOSBrightnessControlPrivate;
	RunOnMainThread(^{
		RestoreControlledScreen();
	});
#endif
}
