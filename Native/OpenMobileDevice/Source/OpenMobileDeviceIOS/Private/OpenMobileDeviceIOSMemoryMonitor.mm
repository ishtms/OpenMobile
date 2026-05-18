#include "OpenMobileDeviceIOSMemoryMonitor.h"

#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMemoryPressureInfo.h"
#include "OpenMobileDeviceMonitoringService.h"

#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSMemoryMonitorPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;
	int64 EventSequence = 0;
	FDateTime EventTimeUtc;

	void RecordWarning()
	{
		FOpenMobileDeviceMonitoringCallbackToken CallbackToken;
		uint64 CallbackSequence = 0;
		{
			FScopeLock Lock(&StateMutex);
			if (!ActiveToken.IsValid())
			{
				return;
			}
			SourceSequence = SourceSequence == MAX_uint64
				? 1
				: SourceSequence + 1;
			EventSequence = EventSequence == MAX_int64
				? 1
				: EventSequence + 1;
			EventTimeUtc = FDateTime::UtcNow();
			CallbackToken = ActiveToken;
			CallbackSequence = SourceSequence;
		}
		FOpenMobileDeviceMonitoringService::NotifyNativeChange(
			CallbackToken,
			CallbackSequence
		);
	}

	void ClearState()
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = {};
		SourceSequence = 0;
		EventSequence = 0;
		EventTimeUtc = {};
	}
}

@interface OpenMobileDeviceMemoryObserver : NSObject
- (void)handleMemoryWarning:(NSNotification*)Notification;
@end

@implementation OpenMobileDeviceMemoryObserver
- (void)handleMemoryWarning:(NSNotification*)Notification
{
	static_cast<void>(Notification);
	OpenMobileDeviceIOSMemoryMonitorPrivate::RecordWarning();
}
@end

namespace OpenMobileDeviceIOSMemoryMonitorPrivate
{
	OpenMobileDeviceMemoryObserver* Observer = nil;
}

void ApplyOpenMobileDeviceIOSMemoryPressureEvent(
	FOpenMobileMemorySnapshot& Snapshot
)
{
	using namespace OpenMobileDeviceIOSMemoryMonitorPrivate;
	FScopeLock Lock(&StateMutex);
	if (EventSequence > 0)
	{
		Snapshot.LatestPressureEventState =
			FOpenMobileDeviceMemoryPressureInfo::NormalizeIOSWarning();
		Snapshot.PressureEventTimeUtc = EventTimeUtc;
		Snapshot.PressureEventSequence = EventSequence;
	}
}

bool StartOpenMobileDeviceIOSMemoryMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSMemoryMonitorPrivate;
	if (!CallbackToken.IsValid() || Observer)
	{
		return false;
	}
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = CallbackToken;
		SourceSequence = 0;
		EventSequence = 0;
		EventTimeUtc = {};
	}
	@autoreleasepool
	{
		Observer = [[OpenMobileDeviceMemoryObserver alloc] init];
		[[NSNotificationCenter defaultCenter]
			addObserver:Observer
			selector:@selector(handleMemoryWarning:)
			name:UIApplicationDidReceiveMemoryWarningNotification
			object:nil];
	}
	return true;
}

void StopOpenMobileDeviceIOSMemoryMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSMemoryMonitorPrivate;
	ClearState();
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
