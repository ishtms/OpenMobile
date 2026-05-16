#include "OpenMobileDeviceIOSLocaleMonitor.h"

#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMonitoringService.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSLocaleMonitorPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;

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
}

@interface OpenMobileDeviceLocaleObserver : NSObject
- (void)handleLocaleOrTimeChange:(NSNotification*)Notification;
@end

@implementation OpenMobileDeviceLocaleObserver
- (void)handleLocaleOrTimeChange:(NSNotification*)Notification
{
	static_cast<void>(Notification);
	OpenMobileDeviceIOSLocaleMonitorPrivate::NotifyChange();
}
@end

namespace OpenMobileDeviceIOSLocaleMonitorPrivate
{
	OpenMobileDeviceLocaleObserver* Observer = nil;
}

bool StartOpenMobileDeviceIOSLocaleMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSLocaleMonitorPrivate;
	if (!CallbackToken.IsValid() || Observer)
	{
		return false;
	}
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = CallbackToken;
		SourceSequence = 0;
	}

	@autoreleasepool
	{
		Observer = [[OpenMobileDeviceLocaleObserver alloc] init];
		NSNotificationCenter* Center = [NSNotificationCenter defaultCenter];
		for (NSNotificationName Name in @[
			NSCurrentLocaleDidChangeNotification,
			UIApplicationSignificantTimeChangeNotification,
			NSSystemClockDidChangeNotification,
			NSSystemTimeZoneDidChangeNotification
		])
		{
			[Center addObserver:Observer
				selector:@selector(handleLocaleOrTimeChange:)
				name:Name
				object:nil];
		}
	}
	return true;
}

void StopOpenMobileDeviceIOSLocaleMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSLocaleMonitorPrivate;
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = {};
		SourceSequence = 0;
	}
	if (!Observer)
	{
		return;
	}
	@autoreleasepool
	{
		[[NSNotificationCenter defaultCenter] removeObserver:Observer];
		[Observer release];
		Observer = nil;
	}
}
