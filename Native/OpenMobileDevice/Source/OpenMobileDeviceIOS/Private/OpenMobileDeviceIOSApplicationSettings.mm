#include "OpenMobileDeviceIOSApplicationSettings.h"

#include "IOS/IOSAppDelegate.h"

#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSApplicationSettingsPrivate
{
	FOpenMobileApplicationSettingsOpenResult MakeResult(
		EOpenMobileApplicationSettingsOpenState State,
		EOpenMobileErrorCode ErrorCode,
		const TCHAR* Message
	)
	{
		FOpenMobileApplicationSettingsOpenResult Result;
		Result.State = State;
		Result.Error = FOpenMobileError::Make(
			ErrorCode,
			Message,
			FString(),
			TEXT("IOS")
		);
		return Result;
	}
}

FOpenMobileApplicationSettingsOpenResult
OpenOpenMobileDeviceIOSApplicationSettings()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSApplicationSettingsPrivate;
	@autoreleasepool
	{
		UIApplication* Application = [UIApplication sharedApplication];
		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		if (!Application || Application.applicationState != UIApplicationStateActive
			|| !AppDelegate || AppDelegate.IOSController == nil)
		{
			return MakeResult(
				EOpenMobileApplicationSettingsOpenState::NoPresenter,
				EOpenMobileErrorCode::Unavailable,
				TEXT("An active iOS application presenter is unavailable.")
			);
		}
		NSURL* SettingsUrl = [NSURL
			URLWithString:UIApplicationOpenSettingsURLString];
		if (!SettingsUrl)
		{
			return MakeResult(
				EOpenMobileApplicationSettingsOpenState::NativeFailure,
				EOpenMobileErrorCode::NativeFailure,
				TEXT("iOS did not provide a valid application-settings URL.")
			);
		}
		if (![Application canOpenURL:SettingsUrl])
		{
			return MakeResult(
				EOpenMobileApplicationSettingsOpenState::Unsupported,
				EOpenMobileErrorCode::NotSupported,
				TEXT("iOS cannot open this application's settings page.")
			);
		}
		[Application openURL:SettingsUrl
			options:@{}
			completionHandler:nil];
		FOpenMobileApplicationSettingsOpenResult Result;
		Result.State = EOpenMobileApplicationSettingsOpenState::Accepted;
		return Result;
	}
}
