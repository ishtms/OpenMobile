#include "OpenMobileDeviceIOSDisplay.h"

#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"
#include "OpenMobileDeviceRefreshRateInfo.h"
#include "OpenMobileDeviceWindowMetrics.h"

#import <UIKit/UIKit.h>

FOpenMobileWindowDisplaySnapshot GetOpenMobileDeviceIOSWindowDisplaySnapshot()
{
	@autoreleasepool
	{
		__block FOpenMobileDeviceWindowMetricsEvidence Evidence;
		__block FOpenMobileDeviceRefreshRateEvidence RefreshRateEvidence;
		void (^CaptureMetrics)(void) = ^{
			FIOSView* View = [IOSAppDelegate GetDelegate].IOSView;
			if (View == nil)
			{
				return;
			}
			const CGRect Bounds = View.bounds;
			Evidence.LogicalWindowSize = FVector2D(
				Bounds.size.width,
				Bounds.size.height
			);
			const CGSize DrawableSize = View.ViewSize;
			Evidence.DrawablePixelSize = FIntPoint(
				FMath::RoundToInt(DrawableSize.width),
				FMath::RoundToInt(DrawableSize.height)
			);
			Evidence.ScaleFactor = static_cast<float>(View.contentScaleFactor);

			UIScreen* Screen = View.window.screen;
			if (Screen != nil)
			{
				RefreshRateEvidence.MaximumRefreshRateHz =
					static_cast<float>(Screen.maximumFramesPerSecond);
				UIWindowScene* WindowScene = View.window.windowScene;
				const CGRect ScreenBounds = WindowScene != nil
					? WindowScene.screen.coordinateSpace.bounds
					: Screen.coordinateSpace.bounds;
				Evidence.ScreenIdentifier = FString::Printf(
					TEXT("IOS:%.0fx%.0f@%.3g"),
					ScreenBounds.size.width,
					ScreenBounds.size.height,
					Screen.scale
				);
				Evidence.bIsWindowed =
					FMath::Abs(Bounds.size.width - ScreenBounds.size.width) > 1.0
					|| FMath::Abs(Bounds.size.height - ScreenBounds.size.height) > 1.0;
			}
		};
		if ([NSThread isMainThread])
		{
			CaptureMetrics();
		}
		else
		{
			dispatch_sync(dispatch_get_main_queue(), CaptureMetrics);
		}
		FOpenMobileWindowDisplaySnapshot Snapshot =
			FOpenMobileDeviceWindowMetrics::Build(Evidence);
		FOpenMobileDeviceRefreshRateInfo::Apply(
			Snapshot,
			RefreshRateEvidence
		);
		return Snapshot;
	}
}
