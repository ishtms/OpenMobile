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
	NSMutableDictionary<NSNumber*, id<CHHapticAdvancedPatternPlayer>>* Players;
	TMap<
		uint64,
		TSharedPtr<
			FOpenMobileHapticsApplePlaybackEventCallback,
			ESPMode::ThreadSafe
		>
	> PlaybackCallbacks;
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
- (EOpenMobileHapticsAppleSubmissionResult)playTransientPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleTransientPattern&)Pattern
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback;
- (EOpenMobileHapticsAppleSubmissionResult)stopPattern:(uint64)RequestId;
- (void)completePattern:(uint64)RequestId
	event:(EOpenMobileHapticsApplePlaybackEvent)Event;
- (void)failAllPatterns;
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
			[Service failAllPatterns];
			[Service emitEvent:
				EOpenMobileHapticsAppleBridgeEvent::EngineStopped];
		};
		Engine.resetHandler = ^
		{
			[Service failAllPatterns];
			[Service emitEvent:EOpenMobileHapticsAppleBridgeEvent::EngineReset];
		};
		return EOpenMobileHapticsAppleEngineResult::Ready;
	}
	return EOpenMobileHapticsAppleEngineResult::UnsupportedHardware;
}

- (void)completePattern:(uint64)RequestId
	event:(EOpenMobileHapticsApplePlaybackEvent)Event
{
	if (![NSThread isMainThread])
	{
		[self retain];
		dispatch_async(dispatch_get_main_queue(), ^{
			[self completePattern:RequestId event:Event];
			[self release];
		});
		return;
	}
	if (bShuttingDown)
	{
		return;
	}

	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:RequestId];
	id<CHHapticAdvancedPatternPlayer> Player = [Players objectForKey:Key];
	if (!Player)
	{
		return;
	}
	Player.completionHandler = ^(NSError* Error)
	{
		static_cast<void>(Error);
	};
	[Players removeObjectForKey:Key];
	TSharedPtr<
		FOpenMobileHapticsApplePlaybackEventCallback,
		ESPMode::ThreadSafe
	> Callback = PlaybackCallbacks.FindRef(RequestId);
	PlaybackCallbacks.Remove(RequestId);
	if (Callback && *Callback)
	{
		(*Callback)(Event);
	}
}

- (void)failAllPatterns
{
	if (![NSThread isMainThread])
	{
		[self retain];
		dispatch_async(dispatch_get_main_queue(), ^{
			[self failAllPatterns];
			[self release];
		});
		return;
	}
	TArray<uint64> RequestIds;
	PlaybackCallbacks.GetKeys(RequestIds);
	for (const uint64 RequestId : RequestIds)
	{
		[self completePattern:RequestId
			event:EOpenMobileHapticsApplePlaybackEvent::Failed];
	}
}

- (EOpenMobileHapticsAppleSubmissionResult)playTransientPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleTransientPattern&)Pattern
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback
{
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	if (!Engine || !Pattern.IsValid())
	{
		return EOpenMobileHapticsAppleSubmissionResult::Unsupported;
	}
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:RequestId];
	if (RequestId == 0 || [Players objectForKey:Key])
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}

	double PreviousStart = 0.0;
	for (int32 Index = 0; Index < Pattern.StartTimesSeconds.Num(); ++Index)
	{
		const double Start = Pattern.StartTimesSeconds[Index];
		const float Intensity = Pattern.Intensities[Index];
		const float Sharpness = Pattern.Sharpnesses[Index];
		if (!FMath::IsFinite(Start)
			|| !FMath::IsFinite(Intensity)
			|| !FMath::IsFinite(Sharpness)
			|| Start < PreviousStart
			|| Intensity < 0.0f
			|| Intensity > 1.0f
			|| Sharpness < 0.0f
			|| Sharpness > 1.0f)
		{
			return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
		}
		PreviousStart = Start;
	}

	NSError* Error = nil;
	if (![Engine startAndReturnError:&Error] || Error)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	NSMutableArray<CHHapticEvent*>* Events =
		[[NSMutableArray alloc] initWithCapacity:Pattern.StartTimesSeconds.Num()];
	for (int32 Index = 0; Index < Pattern.StartTimesSeconds.Num(); ++Index)
	{
		CHHapticEventParameter* Intensity =
			[[CHHapticEventParameter alloc]
				initWithParameterID:CHHapticEventParameterIDHapticIntensity
				value:Pattern.Intensities[Index]];
		CHHapticEventParameter* Sharpness =
			[[CHHapticEventParameter alloc]
				initWithParameterID:CHHapticEventParameterIDHapticSharpness
				value:Pattern.Sharpnesses[Index]];
		NSArray<CHHapticEventParameter*>* Parameters =
			[[NSArray alloc] initWithObjects:Intensity, Sharpness, nil];
		CHHapticEvent* Event = [[CHHapticEvent alloc]
			initWithEventType:CHHapticEventTypeHapticTransient
			parameters:Parameters
			relativeTime:Pattern.StartTimesSeconds[Index]];
		[Events addObject:Event];
		[Event release];
		[Parameters release];
		[Sharpness release];
		[Intensity release];
	}

	Error = nil;
	CHHapticPattern* NativePattern = [[CHHapticPattern alloc]
		initWithEvents:Events
		parameters:@[]
		error:&Error];
	[Events release];
	if (!NativePattern || Error)
	{
		[NativePattern release];
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}

	Error = nil;
	id<CHHapticAdvancedPatternPlayer> Player =
		[Engine createAdvancedPlayerWithPattern:NativePattern error:&Error];
	[NativePattern release];
	if (!Player || Error)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	if (!Players)
	{
		Players = [[NSMutableDictionary alloc] init];
	}
	OpenMobileHapticsAppleNativeService* Service = self;
	Player.completionHandler = ^(NSError* CompletionError)
	{
		[Service completePattern:RequestId
			event:CompletionError
				? EOpenMobileHapticsApplePlaybackEvent::Failed
				: EOpenMobileHapticsApplePlaybackEvent::Completed];
	};
	[Players setObject:Player forKey:Key];
	PlaybackCallbacks.Add(
		RequestId,
		MakeShared<
			FOpenMobileHapticsApplePlaybackEventCallback,
			ESPMode::ThreadSafe
		>(MoveTemp(Callback))
	);
	Error = nil;
	if (![Player startAtTime:CHHapticTimeImmediate error:&Error] || Error)
	{
		Player.completionHandler = ^(NSError* CompletionError)
		{
			static_cast<void>(CompletionError);
		};
		[Players removeObjectForKey:Key];
		PlaybackCallbacks.Remove(RequestId);
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	return EOpenMobileHapticsAppleSubmissionResult::Accepted;
}

- (EOpenMobileHapticsAppleSubmissionResult)stopPattern:(uint64)RequestId
{
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:RequestId];
	id<CHHapticAdvancedPatternPlayer> Player = [Players objectForKey:Key];
	if (!Player)
	{
		return EOpenMobileHapticsAppleSubmissionResult::Accepted;
	}
	NSError* Error = nil;
	const bool bStopped = [Player stopAtTime:CHHapticTimeImmediate error:&Error];
	if (!bStopped || Error)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	Player.completionHandler = ^(NSError* CompletionError)
	{
		static_cast<void>(CompletionError);
	};
	[Players removeObjectForKey:Key];
	PlaybackCallbacks.Remove(RequestId);
	return EOpenMobileHapticsAppleSubmissionResult::Accepted;
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
	for (id<CHHapticAdvancedPatternPlayer> Player in [Players allValues])
	{
		Player.completionHandler = ^(NSError* CompletionError)
		{
			static_cast<void>(CompletionError);
		};
		NSError* Error = nil;
		[Player stopAtTime:CHHapticTimeImmediate error:&Error];
	}
	[Players removeAllObjects];
	[Players release];
	Players = nil;
	PlaybackCallbacks.Reset();
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
			@autoreleasepool
			{
				Probe.OSMajorVersion = static_cast<int32>(
					[NSProcessInfo processInfo].operatingSystemVersion.majorVersion
				);
			}
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

		virtual EOpenMobileHapticsAppleSubmissionResult PlayTransientPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleTransientPattern& Pattern,
			FOpenMobileHapticsApplePlaybackEventCallback Callback
		) override
		{
			if (!NativeService)
			{
				return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
			}
			EOpenMobileHapticsAppleSubmissionResult Result =
				EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
			OpenMobileHapticsAppleNativeService* Service = NativeService;
			RunOnMainQueue(
				[Service, RequestId, &Pattern, &Callback, &Result]()
				{
					Result = [Service
						playTransientPattern:RequestId
						pattern:Pattern
						callback:MoveTemp(Callback)];
				}
			);
			return Result;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult StopPattern(
			uint64 RequestId
		) override
		{
			if (!NativeService)
			{
				return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
			}
			EOpenMobileHapticsAppleSubmissionResult Result =
				EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
			OpenMobileHapticsAppleNativeService* Service = NativeService;
			RunOnMainQueue([Service, RequestId, &Result]()
			{
				Result = [Service stopPattern:RequestId];
			});
			return Result;
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
