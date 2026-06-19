#include "OpenMobileHapticsIOSBridge.h"

#include "Misc/ScopeLock.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreHaptics/CoreHaptics.h>
#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

@interface OpenMobileHapticsAppleNativeService : NSObject
{
	UISelectionFeedbackGenerator* SelectionGenerator;
	UIImpactFeedbackGenerator* ImpactGenerators[5];
	UINotificationFeedbackGenerator* NotificationGenerator;
	CHHapticEngine* Engine;
	NSUInteger ActivityGeneration;
	bool bShuttingDown;
	FCriticalSection EventCallbackMutex;
	TSharedPtr<
		FOpenMobileHapticsAppleBridgeEventCallback,
		ESPMode::ThreadSafe
	> EventCallback;
}

- (void)playBehavior:(EOpenMobileHapticsSemanticBehavior)Behavior
	intensity:(CGFloat)Intensity;
- (void)playSystemVibration;
- (EOpenMobileHapticsAppleEngineResult)createEngine;
- (void)setEventCallback:
	(FOpenMobileHapticsAppleBridgeEventCallback)Callback;
- (void)releaseObjects;

@end

@implementation OpenMobileHapticsAppleNativeService

- (void)emitEvent:(EOpenMobileHapticsAppleBridgeEvent)Event
{
	TSharedPtr<
		FOpenMobileHapticsAppleBridgeEventCallback,
		ESPMode::ThreadSafe
	> Callback;
	{
		FScopeLock Lock(&EventCallbackMutex);
		Callback = EventCallback;
	}
	if (Callback && *Callback)
	{
		(*Callback)(Event);
	}
}

- (void)setEventCallback:
	(FOpenMobileHapticsAppleBridgeEventCallback)Callback
{
	FScopeLock Lock(&EventCallbackMutex);
	if (Callback)
	{
		EventCallback = MakeShared<
			FOpenMobileHapticsAppleBridgeEventCallback,
			ESPMode::ThreadSafe
		>(MoveTemp(Callback));
	}
	else
	{
		EventCallback.Reset();
	}
}

- (void)playBehavior:(EOpenMobileHapticsSemanticBehavior)Behavior
	intensity:(CGFloat)Intensity
{
	if (bShuttingDown)
	{
		return;
	}
	switch (Behavior)
	{
	case EOpenMobileHapticsSemanticBehavior::Selection:
		if (!SelectionGenerator)
		{
			SelectionGenerator = [[UISelectionFeedbackGenerator alloc] init];
		}
		[SelectionGenerator prepare];
		[SelectionGenerator selectionChanged];
		[SelectionGenerator prepare];
		break;
	case EOpenMobileHapticsSemanticBehavior::ImpactLight:
	case EOpenMobileHapticsSemanticBehavior::ImpactMedium:
	case EOpenMobileHapticsSemanticBehavior::ImpactHeavy:
	case EOpenMobileHapticsSemanticBehavior::ImpactSoft:
	case EOpenMobileHapticsSemanticBehavior::ImpactRigid:
	{
		const int32 Index = static_cast<int32>(Behavior)
			- static_cast<int32>(
				EOpenMobileHapticsSemanticBehavior::ImpactLight
			);
		if (!ImpactGenerators[Index])
		{
			const UIImpactFeedbackStyle Styles[] = {
				UIImpactFeedbackStyleLight,
				UIImpactFeedbackStyleMedium,
				UIImpactFeedbackStyleHeavy,
				UIImpactFeedbackStyleSoft,
				UIImpactFeedbackStyleRigid
			};
			ImpactGenerators[Index] =
				[[UIImpactFeedbackGenerator alloc]
					initWithStyle:Styles[Index]];
		}
		[ImpactGenerators[Index] prepare];
		[ImpactGenerators[Index] impactOccurredWithIntensity:Intensity];
		[ImpactGenerators[Index] prepare];
		break;
	}
	case EOpenMobileHapticsSemanticBehavior::NotificationSuccess:
	case EOpenMobileHapticsSemanticBehavior::NotificationWarning:
	case EOpenMobileHapticsSemanticBehavior::NotificationError:
	{
		if (!NotificationGenerator)
		{
			NotificationGenerator =
				[[UINotificationFeedbackGenerator alloc] init];
		}
		UINotificationFeedbackType Type =
			UINotificationFeedbackTypeSuccess;
		if (Behavior
			== EOpenMobileHapticsSemanticBehavior::NotificationWarning)
		{
			Type = UINotificationFeedbackTypeWarning;
		}
		else if (Behavior
			== EOpenMobileHapticsSemanticBehavior::NotificationError)
		{
			Type = UINotificationFeedbackTypeError;
		}
		[NotificationGenerator prepare];
		[NotificationGenerator notificationOccurred:Type];
		[NotificationGenerator prepare];
		break;
	}
	}

	const NSUInteger ExpectedGeneration = ++ActivityGeneration;
	dispatch_after(
		dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
		dispatch_get_main_queue(),
		^{
			if (ActivityGeneration == ExpectedGeneration)
			{
				[self releaseGenerators];
			}
		}
	);
}

- (void)playSystemVibration
{
	if (!bShuttingDown)
	{
		AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
	}
}

- (EOpenMobileHapticsAppleEngineResult)createEngine
{
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleEngineResult::ShuttingDown;
	}
	if (Engine)
	{
		return EOpenMobileHapticsAppleEngineResult::Ready;
	}
	if (@available(iOS 13.0, *))
	{
		id<CHHapticDeviceCapability> Hardware =
			[CHHapticEngine capabilitiesForHardware];
		if (!Hardware)
		{
			return EOpenMobileHapticsAppleEngineResult::
				TemporarilyUnavailable;
		}
		if (!Hardware.supportsHaptics)
		{
			return EOpenMobileHapticsAppleEngineResult::UnsupportedHardware;
		}

		NSError* Error = nil;
		Engine = [[CHHapticEngine alloc] initAndReturnError:&Error];
		if (!Engine || Error)
		{
			[Engine release];
			Engine = nil;
			return EOpenMobileHapticsAppleEngineResult::NativeFailure;
		}

		OpenMobileHapticsAppleNativeService* Service = self;
		Engine.stoppedHandler = ^(CHHapticEngineStoppedReason Reason)
		{
			static_cast<void>(Reason);
			[Service emitEvent:
				EOpenMobileHapticsAppleBridgeEvent::EngineStopped];
		};
		Engine.resetHandler = ^
		{
			[Service emitEvent:EOpenMobileHapticsAppleBridgeEvent::EngineReset];
		};
		return EOpenMobileHapticsAppleEngineResult::Ready;
	}
	return EOpenMobileHapticsAppleEngineResult::UnsupportedHardware;
}

- (void)releaseGenerators
{
	[SelectionGenerator release];
	SelectionGenerator = nil;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ImpactGenerators); ++Index)
	{
		[ImpactGenerators[Index] release];
		ImpactGenerators[Index] = nil;
	}
	[NotificationGenerator release];
	NotificationGenerator = nil;
}

- (void)releaseObjects
{
	bShuttingDown = true;
	++ActivityGeneration;
	[self setEventCallback:{}];
	[self releaseGenerators];
	if (Engine)
	{
		Engine.stoppedHandler = ^(CHHapticEngineStoppedReason Reason)
		{
			static_cast<void>(Reason);
		};
		Engine.resetHandler = ^{};
		[Engine stopWithCompletionHandler:nil];
		[Engine release];
		Engine = nil;
	}
}

- (void)dealloc
{
	[self releaseObjects];
	[super dealloc];
}

@end

namespace OpenMobileHapticsIOSBridgePrivate
{
	template <typename CallableType>
	void RunOnMainQueue(CallableType&& Callable)
	{
		if ([NSThread isMainThread])
		{
			Callable();
			return;
		}
		dispatch_sync(dispatch_get_main_queue(), ^{
			Callable();
		});
	}

	class FOpenMobileHapticsIOSBridge final
		: public IOpenMobileHapticsAppleBridge
	{
	public:
		FOpenMobileHapticsIOSBridge()
			: NativeService(
				[[OpenMobileHapticsAppleNativeService alloc] init]
			)
		{
		}

		virtual ~FOpenMobileHapticsIOSBridge() override
		{
			Shutdown();
		}

		virtual FOpenMobileHapticsAppleHardwareProbe
		QueryHardware() override
		{
			FOpenMobileHapticsAppleHardwareProbe Probe;
#if TARGET_OS_SIMULATOR
			Probe.RichHaptics =
				EOpenMobileHapticsAppleHardwareState::Unsupported;
#else
			@autoreleasepool
			{
				if (@available(iOS 13.0, *))
				{
					id<CHHapticDeviceCapability> Hardware =
						[CHHapticEngine capabilitiesForHardware];
					if (!Hardware)
					{
						return Probe;
					}
					Probe.RichHaptics = Hardware.supportsHaptics
						? EOpenMobileHapticsAppleHardwareState::Supported
						: EOpenMobileHapticsAppleHardwareState::Unsupported;
					Probe.bSupportsAudio = Hardware.supportsAudio;
				}
				else
				{
					Probe.RichHaptics =
						EOpenMobileHapticsAppleHardwareState::Unsupported;
				}
			}
#endif
			return Probe;
		}

		virtual EOpenMobileHapticsAppleEngineResult CreateEngine() override
		{
			if (!NativeService)
			{
				return EOpenMobileHapticsAppleEngineResult::ShuttingDown;
			}
			EOpenMobileHapticsAppleEngineResult Result =
				EOpenMobileHapticsAppleEngineResult::NativeFailure;
			OpenMobileHapticsAppleNativeService* Service = NativeService;
			RunOnMainQueue([Service, &Result]()
			{
				Result = [Service createEngine];
			});
			return Result;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult PlaySemantic(
			EOpenMobileHapticsSemanticBehavior Behavior,
			float Intensity
		) override
		{
			if (!NativeService)
			{
				return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
			}
			OpenMobileHapticsAppleNativeService* Service = NativeService;
			[Service retain];
			dispatch_async(dispatch_get_main_queue(), ^{
				[Service playBehavior:Behavior intensity:Intensity];
				[Service release];
			});
			return EOpenMobileHapticsAppleSubmissionResult::Accepted;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult
		PlaySystemVibration() override
		{
			if (!NativeService)
			{
				return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
			}
			OpenMobileHapticsAppleNativeService* Service = NativeService;
			[Service retain];
			dispatch_async(dispatch_get_main_queue(), ^{
				[Service playSystemVibration];
				[Service release];
			});
			return EOpenMobileHapticsAppleSubmissionResult::Accepted;
		}

		virtual void SetEventCallback(
			FOpenMobileHapticsAppleBridgeEventCallback Callback
		) override
		{
			if (NativeService)
			{
				[NativeService setEventCallback:MoveTemp(Callback)];
			}
		}

		virtual void Shutdown() override
		{
			OpenMobileHapticsAppleNativeService* Service = NativeService;
			NativeService = nil;
			if (!Service)
			{
				return;
			}
			[Service setEventCallback:{}];
			RunOnMainQueue([Service]()
			{
				[Service releaseObjects];
				[Service release];
			});
		}

	private:
		OpenMobileHapticsAppleNativeService* NativeService;
	};
}

TUniquePtr<IOpenMobileHapticsAppleBridge> CreateOpenMobileHapticsIOSBridge()
{
	return MakeUnique<
		OpenMobileHapticsIOSBridgePrivate::FOpenMobileHapticsIOSBridge
	>();
}
