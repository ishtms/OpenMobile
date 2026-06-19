#include "OpenMobileDeviceIOSAppearance.h"

#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceSystemAppearance.h"

#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSAppearancePrivate
{
	void NotifyChange();
}

@interface OpenMobileDeviceAppearanceObserverView : UIView
- (void)openMobileDeviceAppearanceTraitChanged:
	(id<UITraitEnvironment>)environment
	previousTraitCollection:(UITraitCollection*)previousTraitCollection;
@end

@implementation OpenMobileDeviceAppearanceObserverView

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection
{
	[super traitCollectionDidChange:previousTraitCollection];
	if (@available(iOS 17.0, *))
	{
		return;
	}
	if (previousTraitCollection == nil
		|| previousTraitCollection.userInterfaceStyle
			!= self.traitCollection.userInterfaceStyle)
	{
		OpenMobileDeviceIOSAppearancePrivate::NotifyChange();
	}
}
#pragma clang diagnostic pop

- (void)openMobileDeviceAppearanceTraitChanged:
	(id<UITraitEnvironment>)environment
	previousTraitCollection:(UITraitCollection*)previousTraitCollection
{
	static_cast<void>(environment);
	static_cast<void>(previousTraitCollection);
	OpenMobileDeviceIOSAppearancePrivate::NotifyChange();
}

@end

namespace OpenMobileDeviceIOSAppearancePrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;
	OpenMobileDeviceAppearanceObserverView* ObserverView = nil;
	id TraitRegistration = nil;
	id WindowObserver = nil;

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

	void ClearState()
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = {};
		SourceSequence = 0;
	}

	void NotifyChange()
	{
		FOpenMobileDeviceMonitoringCallbackToken CallbackToken;
		uint64 Sequence = 0;
		{
			FScopeLock Lock(&StateMutex);
			if (!ActiveToken.IsValid())
			{
				return;
			}
			SourceSequence = SourceSequence == MAX_uint64
				? 1
				: SourceSequence + 1;
			CallbackToken = ActiveToken;
			Sequence = SourceSequence;
		}
		FOpenMobileDeviceMonitoringService::NotifyNativeChange(
			CallbackToken,
			Sequence
		);
	}

	void RemoveObserverView()
	{
		if (TraitRegistration && ObserverView)
		{
			if (@available(iOS 17.0, *))
			{
				[ObserverView unregisterForTraitChanges:TraitRegistration];
			}
		}
		[TraitRegistration release];
		TraitRegistration = nil;
		[ObserverView removeFromSuperview];
		[ObserverView release];
		ObserverView = nil;
	}

	bool InstallObserverView()
	{
		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		UIView* HostView = AppDelegate.IOSController.view;
		UIWindowScene* WindowScene = HostView.window.windowScene;
		if (!AppDelegate || !AppDelegate.IOSController || !HostView
			|| !WindowScene
			|| WindowScene.activationState == UISceneActivationStateBackground
			|| WindowScene.activationState == UISceneActivationStateUnattached)
		{
			return false;
		}
		if (ObserverView.superview == HostView)
		{
			return true;
		}
		RemoveObserverView();
		ObserverView = [[OpenMobileDeviceAppearanceObserverView alloc]
			initWithFrame:CGRectZero];
		ObserverView.userInteractionEnabled = NO;
		[HostView addSubview:ObserverView];
		if (@available(iOS 17.0, *))
		{
			TraitRegistration = [[ObserverView registerForTraitChanges:
				@[[UITraitUserInterfaceStyle class]]
				withTarget:ObserverView
				action:@selector(
					openMobileDeviceAppearanceTraitChanged:
					previousTraitCollection:
				)] retain];
		}
		return true;
	}
}

FOpenMobileAppearanceSnapshot GetOpenMobileDeviceIOSAppearanceSnapshot()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSAppearancePrivate;
	FOpenMobileAppearanceSnapshot Snapshot;
	@autoreleasepool
	{
		__block int32 UserInterfaceStyle = 0;
		RunOnMainThread(^{
			IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
			UIView* View = AppDelegate.IOSController.view;
			UIWindowScene* WindowScene = View.window.windowScene;
			if (!AppDelegate || !AppDelegate.IOSController || !View
				|| !WindowScene
				|| WindowScene.activationState
					== UISceneActivationStateBackground
				|| WindowScene.activationState
					== UISceneActivationStateUnattached)
			{
				return;
			}
			UserInterfaceStyle = static_cast<int32>(
				View.traitCollection.userInterfaceStyle
			);
		});
		Snapshot.Appearance =
			FOpenMobileDeviceSystemAppearance::FromIOSUserInterfaceStyle(
				UserInterfaceStyle
			);
	}
	return Snapshot;
}

bool StartOpenMobileDeviceIOSAppearanceMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSAppearancePrivate;
	if (!CallbackToken.IsValid())
	{
		return false;
	}
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = CallbackToken;
		SourceSequence = 0;
	}
	__block bool bInstalled = false;
	RunOnMainThread(^{
		bInstalled = InstallObserverView();
		if (bInstalled && !WindowObserver)
		{
			WindowObserver = [[NSNotificationCenter defaultCenter]
				addObserverForName:UIWindowDidBecomeKeyNotification
				object:nil
				queue:[NSOperationQueue mainQueue]
				usingBlock:^(NSNotification* Notification)
				{
					static_cast<void>(Notification);
					if (InstallObserverView())
					{
						NotifyChange();
					}
				}];
			[WindowObserver retain];
		}
	});
	if (!bInstalled)
	{
		ClearState();
	}
	return bInstalled;
}

void StopOpenMobileDeviceIOSAppearanceMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSAppearancePrivate;
	ClearState();
	RunOnMainThread(^{
		if (WindowObserver)
		{
			[[NSNotificationCenter defaultCenter]
				removeObserver:WindowObserver];
			[WindowObserver release];
			WindowObserver = nil;
		}
		RemoveObserverView();
	});
}
