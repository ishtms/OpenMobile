#include "OpenMobileHapticsIOSBridge.h"

#include "Misc/ScopeLock.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreHaptics/CoreHaptics.h>
#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

namespace OpenMobileHapticsIOSBridgePrivate
{
	constexpr double MaximumNativePatternDurationSeconds = 300.0;
	constexpr double MaximumNativeEventDurationSeconds = 30.0;
	constexpr int32 MaximumNativeEventCount = 4096;
	constexpr int32 MaximumNativeCurveCount = 128;
	constexpr int32 MaximumNativeCurvePointCount = 4096;
	constexpr double TimingToleranceSeconds = 0.0000005;

	bool IsFiniteInRange(double Value, double Minimum, double Maximum)
	{
		return FMath::IsFinite(Value)
			&& Value >= Minimum
			&& Value <= Maximum;
	}

	bool ValidateContinuousPattern(
		const FOpenMobileHapticsAppleContinuousPattern& Pattern
	)
	{
		if (!Pattern.IsValid()
			|| Pattern.Events.Num() > MaximumNativeEventCount
			|| Pattern.ParameterCurves.Num() > MaximumNativeCurveCount
			|| !IsFiniteInRange(
				Pattern.DurationSeconds,
				TimingToleranceSeconds,
				MaximumNativePatternDurationSeconds
			)
			|| !IsFiniteInRange(
				Pattern.SafetyDurationSeconds,
				TimingToleranceSeconds,
				MaximumNativePatternDurationSeconds
			)
			|| (Pattern.bLoop
				&& (!IsFiniteInRange(
					Pattern.LoopEndSeconds,
					TimingToleranceSeconds,
					Pattern.DurationSeconds
				) || Pattern.SafetyDurationSeconds + TimingToleranceSeconds
					< Pattern.LoopEndSeconds)))
		{
			return false;
		}

		double PreviousStart = 0.0;
		double PreviousEnd = 0.0;
		bool bHasPreviousEvent = false;
		bool bHasContinuousEvent = false;
		for (const FOpenMobileHapticsAppleRichEvent& Event : Pattern.Events)
		{
			if ((Event.Type != EOpenMobileHapticPatternEventType::Transient
				&& Event.Type != EOpenMobileHapticPatternEventType::Continuous)
				|| !IsFiniteInRange(
					Event.StartTimeSeconds,
					0.0,
					Pattern.DurationSeconds
				)
				|| !IsFiniteInRange(Event.Intensity, 0.0, 1.0)
				|| !IsFiniteInRange(Event.Sharpness, 0.0, 1.0)
				|| (bHasPreviousEvent
					&& (Event.StartTimeSeconds + TimingToleranceSeconds
						< PreviousStart
						|| Event.StartTimeSeconds + TimingToleranceSeconds
							< PreviousEnd)))
			{
				return false;
			}

			if (Event.Type == EOpenMobileHapticPatternEventType::Transient)
			{
				if (Event.DurationSeconds != 0.0)
				{
					return false;
				}
			}
			else
			{
				if (!IsFiniteInRange(
						Event.DurationSeconds,
						TimingToleranceSeconds,
						MaximumNativeEventDurationSeconds
					)
					|| Event.StartTimeSeconds + Event.DurationSeconds
						> Pattern.DurationSeconds + TimingToleranceSeconds)
				{
					return false;
				}
				bHasContinuousEvent = true;
			}

			bHasPreviousEvent = true;
			PreviousStart = Event.StartTimeSeconds;
			PreviousEnd = Event.StartTimeSeconds + Event.DurationSeconds;
		}

		int32 TotalPointCount = 0;
		double PreviousCurveStart = 0.0;
		double PreviousCurveEnd[2] = {0.0, 0.0};
		bool bHasPreviousCurve = false;
		bool bHasParameterCurve[2] = {false, false};
		for (const FOpenMobileHapticsAppleParameterCurve& Curve
			: Pattern.ParameterCurves)
		{
			const int32 ParameterIndex = static_cast<int32>(Curve.Parameter);
			if (ParameterIndex >= UE_ARRAY_COUNT(bHasParameterCurve)
				|| !Curve.IsValid()
				|| !IsFiniteInRange(
					Curve.StartTimeSeconds,
					0.0,
					Pattern.DurationSeconds
				)
				|| (bHasPreviousCurve
					&& Curve.StartTimeSeconds + TimingToleranceSeconds
						< PreviousCurveStart)
				|| Curve.Values.Num()
					> MaximumNativeCurvePointCount - TotalPointCount)
			{
				return false;
			}

			TotalPointCount += Curve.Values.Num();
			double PreviousPointTime = 0.0;
			for (int32 PointIndex = 0;
				PointIndex < Curve.Values.Num();
				++PointIndex)
			{
				const double PointTime =
					Curve.RelativeTimesSeconds[PointIndex];
				const float Value = Curve.Values[PointIndex];
				const bool bValueValid = Curve.Parameter
					== EOpenMobileHapticCurveParameter::IntensityControl
						? IsFiniteInRange(Value, 0.0, 1.0)
						: IsFiniteInRange(Value, -1.0, 1.0);
				if (!FMath::IsFinite(PointTime)
					|| PointTime < 0.0
					|| (PointIndex == 0 && PointTime != 0.0)
					|| (PointIndex > 0 && PointTime <= PreviousPointTime)
					|| !bValueValid)
				{
					return false;
				}
				PreviousPointTime = PointTime;
			}

			const double CurveEnd = Curve.StartTimeSeconds
				+ Curve.RelativeTimesSeconds.Last();
			if (CurveEnd
					> Pattern.DurationSeconds + TimingToleranceSeconds
				|| (bHasParameterCurve[ParameterIndex]
					&& Curve.StartTimeSeconds + TimingToleranceSeconds
						< PreviousCurveEnd[ParameterIndex]))
			{
				return false;
			}
			bHasPreviousCurve = true;
			PreviousCurveStart = Curve.StartTimeSeconds;
			bHasParameterCurve[ParameterIndex] = true;
			PreviousCurveEnd[ParameterIndex] = CurveEnd;
		}
		return bHasContinuousEvent;
	}
}

@interface OpenMobileHapticsAppleNativeService : NSObject
{
	UISelectionFeedbackGenerator* SelectionGenerator;
	UIImpactFeedbackGenerator* ImpactGenerators[5];
	UINotificationFeedbackGenerator* NotificationGenerator;
	CHHapticEngine* Engine;
	NSMutableDictionary<NSNumber*, id<CHHapticAdvancedPatternPlayer>>* Players;
	NSMutableDictionary<NSNumber*, NSTimer*>* SafetyTimers;
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
- (EOpenMobileHapticsAppleSubmissionResult)playContinuousPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleContinuousPattern&)Pattern
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback;
- (void)handleSafetyTimer:(NSTimer*)Timer;
- (void)cancelSafetyTimerForKey:(NSNumber*)Key;
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
	[self cancelSafetyTimerForKey:Key];
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

- (EOpenMobileHapticsAppleSubmissionResult)playContinuousPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleContinuousPattern&)Pattern
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback
{
	using namespace OpenMobileHapticsIOSBridgePrivate;
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	if (!Engine || !ValidateContinuousPattern(Pattern))
	{
		return EOpenMobileHapticsAppleSubmissionResult::Unsupported;
	}
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:RequestId];
	if (RequestId == 0 || [Players objectForKey:Key])
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}

	NSError* Error = nil;
	if (![Engine startAndReturnError:&Error] || Error)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}

	NSMutableArray<CHHapticEvent*>* Events =
		[[NSMutableArray alloc] initWithCapacity:Pattern.Events.Num()];
	for (const FOpenMobileHapticsAppleRichEvent& NativeEvent : Pattern.Events)
	{
		CHHapticEventParameter* Intensity =
			[[CHHapticEventParameter alloc]
				initWithParameterID:CHHapticEventParameterIDHapticIntensity
				value:NativeEvent.Intensity];
		CHHapticEventParameter* Sharpness =
			[[CHHapticEventParameter alloc]
				initWithParameterID:CHHapticEventParameterIDHapticSharpness
				value:NativeEvent.Sharpness];
		NSArray<CHHapticEventParameter*>* Parameters =
			[[NSArray alloc] initWithObjects:Intensity, Sharpness, nil];
		CHHapticEvent* Event = nil;
		if (NativeEvent.Type
			== EOpenMobileHapticPatternEventType::Continuous)
		{
			Event = [[CHHapticEvent alloc]
				initWithEventType:CHHapticEventTypeHapticContinuous
				parameters:Parameters
				relativeTime:NativeEvent.StartTimeSeconds
				duration:NativeEvent.DurationSeconds];
		}
		else
		{
			Event = [[CHHapticEvent alloc]
				initWithEventType:CHHapticEventTypeHapticTransient
				parameters:Parameters
				relativeTime:NativeEvent.StartTimeSeconds];
		}
		[Events addObject:Event];
		[Event release];
		[Parameters release];
		[Sharpness release];
		[Intensity release];
	}

	NSMutableArray<CHHapticParameterCurve*>* Curves =
		[[NSMutableArray alloc]
			initWithCapacity:Pattern.ParameterCurves.Num()];
	for (const FOpenMobileHapticsAppleParameterCurve& NativeCurve
		: Pattern.ParameterCurves)
	{
		NSMutableArray<CHHapticParameterCurveControlPoint*>* ControlPoints =
			[[NSMutableArray alloc]
				initWithCapacity:NativeCurve.Values.Num()];
		for (int32 PointIndex = 0;
			PointIndex < NativeCurve.Values.Num();
			++PointIndex)
		{
			CHHapticParameterCurveControlPoint* ControlPoint =
				[[CHHapticParameterCurveControlPoint alloc]
					initWithRelativeTime:
						NativeCurve.RelativeTimesSeconds[PointIndex]
					value:NativeCurve.Values[PointIndex]];
			[ControlPoints addObject:ControlPoint];
			[ControlPoint release];
		}
		const CHHapticDynamicParameterID ParameterId = NativeCurve.Parameter
			== EOpenMobileHapticCurveParameter::IntensityControl
				? CHHapticDynamicParameterIDHapticIntensityControl
				: CHHapticDynamicParameterIDHapticSharpnessControl;
		CHHapticParameterCurve* Curve = [[CHHapticParameterCurve alloc]
			initWithParameterID:ParameterId
			controlPoints:ControlPoints
			relativeTime:NativeCurve.StartTimeSeconds];
		[Curves addObject:Curve];
		[Curve release];
		[ControlPoints release];
	}

	Error = nil;
	CHHapticPattern* NativePattern = [[CHHapticPattern alloc]
		initWithEvents:Events
		parameterCurves:Curves
		error:&Error];
	[Curves release];
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
	Player.loopEnabled = Pattern.bLoop;
	Player.loopEnd = Pattern.LoopEndSeconds;
	NSTimer* SafetyTimer = nil;
	if (Pattern.bLoop)
	{
		SafetyTimer = [NSTimer
			timerWithTimeInterval:Pattern.SafetyDurationSeconds
			target:self
			selector:@selector(handleSafetyTimer:)
			userInfo:Key
			repeats:NO];
		if (!SafetyTimer)
		{
			return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
		}
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
		[SafetyTimer invalidate];
		Player.completionHandler = ^(NSError* CompletionError)
		{
			static_cast<void>(CompletionError);
		};
		[Players removeObjectForKey:Key];
		PlaybackCallbacks.Remove(RequestId);
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}

	if (Pattern.bLoop)
	{
		if (!SafetyTimers)
		{
			SafetyTimers = [[NSMutableDictionary alloc] init];
		}
		[SafetyTimers setObject:SafetyTimer forKey:Key];
		[[NSRunLoop mainRunLoop]
			addTimer:SafetyTimer
			forMode:NSRunLoopCommonModes];
	}
	return EOpenMobileHapticsAppleSubmissionResult::Accepted;
}

- (void)cancelSafetyTimerForKey:(NSNumber*)Key
{
	NSTimer* Timer = [SafetyTimers objectForKey:Key];
	if (Timer)
	{
		[Timer invalidate];
		[SafetyTimers removeObjectForKey:Key];
	}
}

- (void)handleSafetyTimer:(NSTimer*)Timer
{
	id UserInfo = Timer.userInfo;
	if (![UserInfo isKindOfClass:[NSNumber class]])
	{
		[Timer invalidate];
		return;
	}
	NSNumber* Key = (NSNumber*)UserInfo;
	if ([SafetyTimers objectForKey:Key] != Timer)
	{
		[Timer invalidate];
		return;
	}
	[Timer invalidate];
	[SafetyTimers removeObjectForKey:Key];
	if (bShuttingDown)
	{
		return;
	}

	const uint64 RequestId = Key.unsignedLongLongValue;
	id<CHHapticAdvancedPatternPlayer> Player = [Players objectForKey:Key];
	if (!Player)
	{
		return;
	}
	NSError* Error = nil;
	bool bStopped = [Player
		stopAtTime:CHHapticTimeImmediate
		error:&Error] && !Error;
	if (!bStopped)
	{
		Error = nil;
		bStopped = [Player cancelAndReturnError:&Error] && !Error;
	}
	if (!bStopped)
	{
		[Engine stopWithCompletionHandler:nil];
		[self failAllPatterns];
		return;
	}
	[self completePattern:RequestId
		event:EOpenMobileHapticsApplePlaybackEvent::Completed];
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
	[self cancelSafetyTimerForKey:Key];
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
	for (NSTimer* Timer in [SafetyTimers allValues])
	{
		[Timer invalidate];
	}
	[SafetyTimers removeAllObjects];
	[SafetyTimers release];
	SafetyTimers = nil;
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

		virtual EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleContinuousPattern& Pattern,
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
						playContinuousPattern:RequestId
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
