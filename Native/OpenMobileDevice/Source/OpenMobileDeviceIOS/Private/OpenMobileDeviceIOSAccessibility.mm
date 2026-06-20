#include "OpenMobileDeviceIOSAccessibility.h"

#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDevicePreferredTextScale.h"
#include "OpenMobileDeviceReducedAnimation.h"

#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSAccessibilityPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;
	id ContentSizeObserver = nil;
	id ReduceMotionObserver = nil;

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

	void RemoveObserver()
	{
		if (ContentSizeObserver)
		{
			[[NSNotificationCenter defaultCenter]
				removeObserver:ContentSizeObserver];
			[ContentSizeObserver release];
			ContentSizeObserver = nil;
		}
		if (ReduceMotionObserver)
		{
			[[NSNotificationCenter defaultCenter]
				removeObserver:ReduceMotionObserver];
			[ReduceMotionObserver release];
			ReduceMotionObserver = nil;
		}
	}
}

FOpenMobileAccessibilitySnapshot GetOpenMobileDeviceIOSAccessibilitySnapshot()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSAccessibilityPrivate;
	@autoreleasepool
	{
		__block UIContentSizeCategory Category = nil;
		__block float RelativeScale = 0.0f;
		__block bool bReduceMotionEnabled = false;
		RunOnMainThread(^{
			UIApplication* Application = [UIApplication sharedApplication];
			Category = [Application.preferredContentSizeCategory copy];
			bReduceMotionEnabled = UIAccessibilityIsReduceMotionEnabled();
			if ([Category length] == 0)
			{
				return;
			}
			if (@available(iOS 11.0, *))
			{
				UITraitCollection* Traits = [UITraitCollection
					traitCollectionWithPreferredContentSizeCategory:Category];
				UIFontMetrics* Metrics = [UIFontMetrics
					metricsForTextStyle:UIFontTextStyleBody];
				RelativeScale = static_cast<float>(
					[Metrics scaledValueForValue:1.0
						compatibleWithTraitCollection:Traits]
				);
			}
		});
		FOpenMobileAccessibilitySnapshot Snapshot =
			FOpenMobileDevicePreferredTextScale::FromIOSContentSizeCategory(
				FString(Category),
				RelativeScale
			);
		const FOpenMobileAccessibilitySnapshot ReducedAnimation =
			FOpenMobileDeviceReducedAnimation::FromIOSReduceMotion(
				bReduceMotionEnabled
			);
		Snapshot.bReducedAnimationPreferred =
			ReducedAnimation.bReducedAnimationPreferred;
		Snapshot.ReducedAnimationPlatformDetail =
			ReducedAnimation.ReducedAnimationPlatformDetail;
		[Category release];
		return Snapshot;
	}
}

bool StartOpenMobileDeviceIOSAccessibilityMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSAccessibilityPrivate;
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
		RemoveObserver();
		ContentSizeObserver = [[NSNotificationCenter defaultCenter]
			addObserverForName:UIContentSizeCategoryDidChangeNotification
			object:nil
			queue:[NSOperationQueue mainQueue]
			usingBlock:^(NSNotification* Notification)
			{
				static_cast<void>(Notification);
				NotifyChange();
			}];
		[ContentSizeObserver retain];
		ReduceMotionObserver = [[NSNotificationCenter defaultCenter]
			addObserverForName:
				UIAccessibilityReduceMotionStatusDidChangeNotification
			object:nil
			queue:[NSOperationQueue mainQueue]
			usingBlock:^(NSNotification* Notification)
			{
				static_cast<void>(Notification);
				NotifyChange();
			}];
		[ReduceMotionObserver retain];
		bInstalled = ContentSizeObserver != nil
			&& ReduceMotionObserver != nil;
		if (!bInstalled)
		{
			RemoveObserver();
		}
	});
	if (!bInstalled)
	{
		ClearState();
	}
	return bInstalled;
}

void StopOpenMobileDeviceIOSAccessibilityMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSAccessibilityPrivate;
	ClearState();
	RunOnMainThread(^{
		RemoveObserver();
	});
}
