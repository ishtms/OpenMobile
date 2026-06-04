#include "OpenMobileDeviceIOSDisplay.h"

#include "DynamicRHI.h"
#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"
#include "OpenMobileDeviceHdrInfo.h"
#include "OpenMobileDeviceRefreshRateInfo.h"
#include "OpenMobileDeviceWindowInsets.h"
#include "OpenMobileDeviceWindowMetrics.h"
#include "OpenMobileDeviceWindowMode.h"
#include "OpenMobileDeviceWindowOrientation.h"
#include "RHIGlobals.h"

#include <TargetConditionals.h>
#import <UIKit/UIKit.h>

FOpenMobileWindowDisplaySnapshot GetOpenMobileDeviceIOSWindowDisplaySnapshot()
{
	@autoreleasepool
	{
		__block FOpenMobileDeviceWindowMetricsEvidence Evidence;
		__block FOpenMobileDeviceRefreshRateEvidence RefreshRateEvidence;
		__block FOpenMobileDeviceHdrEvidence HdrEvidence;
		__block FOpenMobileDeviceWindowInsetsEvidence InsetEvidence;
		__block int32 InterfaceOrientation = 0;
#if !TARGET_OS_SIMULATOR
		if (GDynamicRHI)
		{
			HdrEvidence.bHdrOutputActive = GRHIIsHDREnabled;
		}
#endif
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
			if (View.window != nil)
			{
				const UIEdgeInsets SafeArea = View.safeAreaInsets;
				InsetEvidence.SafeArea = FOpenMobileDeviceInsetValues{
					static_cast<float>(SafeArea.left),
					static_cast<float>(SafeArea.top),
					static_cast<float>(SafeArea.right),
					static_cast<float>(SafeArea.bottom)
				};
				InsetEvidence.HomeIndicator = FOpenMobileDeviceInsetValues{
					0.0f,
					0.0f,
					0.0f,
					static_cast<float>(SafeArea.bottom)
				};
			}

			UIScreen* Screen = View.window.screen;
			if (Screen != nil)
			{
				RefreshRateEvidence.MaximumRefreshRateHz =
					static_cast<float>(Screen.maximumFramesPerSecond);
#if !TARGET_OS_SIMULATOR
				if (@available(iOS 10.0, *))
				{
					switch (View.traitCollection.displayGamut)
					{
					case UIDisplayGamutP3:
						HdrEvidence.bWideColorAvailable = true;
						break;
					case UIDisplayGamutSRGB:
						HdrEvidence.bWideColorAvailable = false;
						break;
					case UIDisplayGamutUnspecified:
					default:
						break;
					}
				}
				if (@available(iOS 16.0, *))
				{
					const CGFloat PotentialHeadroom =
						Screen.potentialEDRHeadroom;
					if (FMath::IsFinite(PotentialHeadroom)
						&& PotentialHeadroom >= 1.0)
					{
						HdrEvidence.bHdrAvailable =
							PotentialHeadroom > 1.0;
					}
				}
#endif
				UIWindowScene* WindowScene = View.window.windowScene;
				InterfaceOrientation = static_cast<int32>(
					WindowScene.interfaceOrientation
				);
				UIStatusBarManager* StatusBarManager =
					WindowScene.statusBarManager;
				if (StatusBarManager != nil)
				{
					FOpenMobileDeviceInsetValues SystemBars;
					if (!StatusBarManager.statusBarHidden)
					{
						const CGRect StatusFrame = [View convertRect:
							StatusBarManager.statusBarFrame
							fromCoordinateSpace:WindowScene.screen.coordinateSpace];
						const CGRect Intersection = CGRectIntersection(
							Bounds,
							StatusFrame
						);
						if (!CGRectIsNull(Intersection)
							&& !CGRectIsEmpty(Intersection))
						{
							const CGFloat Tolerance = 0.5;
							if (CGRectGetMinY(Intersection)
								<= CGRectGetMinY(Bounds) + Tolerance)
							{
								SystemBars.Top = static_cast<float>(
									CGRectGetMaxY(Intersection)
									- CGRectGetMinY(Bounds)
								);
							}
						}
					}
					InsetEvidence.SystemBars = SystemBars;
				}
				const CGRect ScreenBounds = WindowScene != nil
					? WindowScene.screen.coordinateSpace.bounds
					: Screen.coordinateSpace.bounds;
				Evidence.ScreenIdentifier = FString::Printf(
					TEXT("IOS:%.0fx%.0f@%.3g"),
					ScreenBounds.size.width,
					ScreenBounds.size.height,
					Screen.scale
				);
				const bool bIsWindowed =
					FMath::Abs(Bounds.size.width - ScreenBounds.size.width) > 1.0
					|| FMath::Abs(Bounds.size.height - ScreenBounds.size.height) > 1.0;
				Evidence.bIsWindowed = bIsWindowed;
				Evidence.WindowMode = FOpenMobileDeviceWindowMode::FromIOS(
					!bIsWindowed
				);
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
		FOpenMobileDeviceWindowInsets::Apply(Snapshot, InsetEvidence);
		Snapshot.Orientation =
			FOpenMobileDeviceWindowOrientation::FromIOSInterfaceOrientation(
				InterfaceOrientation
			);
		FOpenMobileDeviceRefreshRateInfo::Apply(
			Snapshot,
			RefreshRateEvidence
		);
		FOpenMobileDeviceHdrInfo::Apply(Snapshot, HdrEvidence);
		return Snapshot;
	}
}
