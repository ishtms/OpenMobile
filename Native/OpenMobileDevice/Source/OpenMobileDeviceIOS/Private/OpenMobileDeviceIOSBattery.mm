#include "OpenMobileDeviceIOSBattery.h"

#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceBatteryInfo.h"
#include "OpenMobileDeviceMonitoringService.h"

#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSBatteryPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;
	bool bRestoreBatteryMonitoringDisabled = false;

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

@interface OpenMobileDeviceBatteryObserver : NSObject
- (void)handleBatteryLevelChange:(NSNotification*)Notification;
@end

@implementation OpenMobileDeviceBatteryObserver
- (void)handleBatteryLevelChange:(NSNotification*)Notification
{
	static_cast<void>(Notification);
	OpenMobileDeviceIOSBatteryPrivate::NotifyChange();
}
@end

namespace OpenMobileDeviceIOSBatteryPrivate
{
	OpenMobileDeviceBatteryObserver* Observer = nil;
}

FOpenMobilePowerSnapshot GetOpenMobileDeviceIOSPowerSnapshot()
{
	FOpenMobilePowerSnapshot Snapshot;
#if !TARGET_OS_SIMULATOR
	@autoreleasepool
	{
		UIDevice* Device = [UIDevice currentDevice];
		const bool bWasMonitoring = Device.batteryMonitoringEnabled;
		if (!bWasMonitoring)
		{
			Device.batteryMonitoringEnabled = YES;
		}
		FOpenMobileDeviceBatteryInfo::ApplyFraction(
			Snapshot,
			Device.batteryLevel,
			true
		);
		if (!bWasMonitoring)
		{
			Device.batteryMonitoringEnabled = NO;
		}
	}
#endif
	return Snapshot;
}

bool StartOpenMobileDeviceIOSBatteryMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSBatteryPrivate;
#if TARGET_OS_SIMULATOR
	static_cast<void>(CallbackToken);
	return false;
#else
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
		UIDevice* Device = [UIDevice currentDevice];
		bRestoreBatteryMonitoringDisabled = !Device.batteryMonitoringEnabled;
		if (bRestoreBatteryMonitoringDisabled)
		{
			Device.batteryMonitoringEnabled = YES;
		}
		Observer = [[OpenMobileDeviceBatteryObserver alloc] init];
		[[NSNotificationCenter defaultCenter]
			addObserver:Observer
			selector:@selector(handleBatteryLevelChange:)
			name:UIDeviceBatteryLevelDidChangeNotification
			object:nil];
	}
	return true;
#endif
}

void StopOpenMobileDeviceIOSBatteryMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSBatteryPrivate;
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
		if (bRestoreBatteryMonitoringDisabled)
		{
			[UIDevice currentDevice].batteryMonitoringEnabled = NO;
		}
		bRestoreBatteryMonitoringDisabled = false;
	}
}
