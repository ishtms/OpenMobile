#include "OpenMobileHapticsIOSBridge.h"

#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
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
	constexpr int32 MaximumAudioResourceCount = 16;
	constexpr int64 MaximumAudioResourceBytes = 4 * 1024 * 1024;
	constexpr int64 MaximumActiveAudioResourceBytes = 16 * 1024 * 1024;
	constexpr int32 MaximumActiveAudioPatternCount = 4;
	constexpr double TimingToleranceSeconds = 0.0000005;

	struct FPreparedAHAPFiles
	{
		FString Directory;
		FString PatternPath;
		bool bSucceeded = false;
	};

	bool IsSafeAudioResourcePath(const FString& RelativePath)
	{
		if (RelativePath.IsEmpty()
			|| RelativePath.Len() > 256
			|| RelativePath.StartsWith(TEXT("/"))
			|| RelativePath.Contains(TEXT("\\"))
			|| RelativePath.Contains(TEXT(":")))
		{
			return false;
		}
		TArray<FString> Segments;
		RelativePath.ParseIntoArray(Segments, TEXT("/"), false);
		if (Segments.IsEmpty())
		{
			return false;
		}
		for (const FString& Segment : Segments)
		{
			if (Segment.IsEmpty()
				|| Segment == TEXT(".")
				|| Segment == TEXT(".."))
			{
				return false;
			}
		}
		const FString Extension = FPaths::GetExtension(RelativePath).ToLower();
		return Extension == TEXT("caf")
			|| Extension == TEXT("wav")
			|| Extension == TEXT("aif")
			|| Extension == TEXT("aiff");
	}

	bool ValidateAudioResources(
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		int64& OutTotalBytes
	)
	{
		OutTotalBytes = 0;
		if (Pattern.AudioResources.IsEmpty()
			|| Pattern.AudioResources.Num() > MaximumAudioResourceCount)
		{
			return false;
		}
		TSet<FString> Paths;
		for (const FOpenMobileHapticsAppleAudioResource& Resource
			: Pattern.AudioResources)
		{
			if (!IsSafeAudioResourcePath(Resource.RelativePath)
				|| Resource.Data.IsEmpty()
				|| Resource.Data.Num() > MaximumAudioResourceBytes
				|| Paths.Contains(Resource.RelativePath))
			{
				return false;
			}
			Paths.Add(Resource.RelativePath);
			OutTotalBytes += Resource.Data.Num();
			if (OutTotalBytes > MaximumActiveAudioResourceBytes)
			{
				return false;
			}
		}
		return true;
	}

	void PrepareAHAPFiles(
		const FOpenMobileHapticsAppleAHAPPattern& Pattern,
		FPreparedAHAPFiles& Result
	)
	{
		@autoreleasepool
		{
			NSFileManager* FileManager = [NSFileManager defaultManager];
			NSString* Directory = [[NSTemporaryDirectory()
				stringByAppendingPathComponent:@"OpenMobileHaptics"]
				stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];
			NSError* Error = nil;
			if (![FileManager
				createDirectoryAtPath:Directory
				withIntermediateDirectories:YES
				attributes:nil
				error:&Error] || Error)
			{
				return;
			}

			const FTCHARToUTF8 JsonUTF8(*Pattern.NormalizedJson);
			NSData* JsonData = [NSData
				dataWithBytes:JsonUTF8.Get()
				length:static_cast<NSUInteger>(JsonUTF8.Length())];
			NSString* PatternPath = [Directory
				stringByAppendingPathComponent:@"Pattern.ahap"];
			if (![JsonData
				writeToFile:PatternPath
				options:NSDataWritingAtomic
				error:&Error] || Error)
			{
				[FileManager removeItemAtPath:Directory error:nil];
				return;
			}

			for (const FOpenMobileHapticsAppleAudioResource& Resource
				: Pattern.AudioResources)
			{
				NSString* RelativePath = [NSString
					stringWithUTF8String:TCHAR_TO_UTF8(*Resource.RelativePath)];
				NSString* ResourcePath = [Directory
					stringByAppendingPathComponent:RelativePath];
				NSString* ParentDirectory = [ResourcePath
					stringByDeletingLastPathComponent];
				Error = nil;
				if (![FileManager
					createDirectoryAtPath:ParentDirectory
					withIntermediateDirectories:YES
					attributes:nil
					error:&Error] || Error)
				{
					[FileManager removeItemAtPath:Directory error:nil];
					return;
				}
				NSData* ResourceData = [NSData
					dataWithBytes:Resource.Data.GetData()
					length:static_cast<NSUInteger>(Resource.Data.Num())];
				Error = nil;
				if (![ResourceData
					writeToFile:ResourcePath
					options:NSDataWritingAtomic
					error:&Error] || Error)
				{
					[FileManager removeItemAtPath:Directory error:nil];
					return;
				}
			}

			Result.Directory = UTF8_TO_TCHAR([Directory UTF8String]);
			Result.PatternPath = UTF8_TO_TCHAR([PatternPath UTF8String]);
			Result.bSucceeded = true;
		}
	}

	bool IsFiniteInRange(double Value, double Minimum, double Maximum)
	{
		return FMath::IsFinite(Value)
			&& Value >= Minimum
			&& Value <= Maximum;
	}

	bool ValidateDynamicParameters(
		const FOpenMobileHapticDynamicParameterUpdate& Update
	)
	{
		if (!Update.bUpdateIntensity && !Update.bUpdateSharpness)
		{
			return false;
		}
		if (Update.bUpdateIntensity
			&& !IsFiniteInRange(Update.Intensity, 0.0, 1.0))
		{
			return false;
		}
		return !Update.bUpdateSharpness
			|| IsFiniteInRange(Update.Sharpness, 0.0, 1.0);
	}

	bool SendDynamicParameters(
		id<CHHapticPatternPlayer> Player,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	)
	{
		if (!Player || !ValidateDynamicParameters(Update))
		{
			return false;
		}
		NSMutableArray<CHHapticDynamicParameter*>* Parameters =
			[[NSMutableArray alloc] initWithCapacity:2];
		if (Update.bUpdateIntensity)
		{
			CHHapticDynamicParameter* Parameter =
				[[CHHapticDynamicParameter alloc]
					initWithParameterID:
						CHHapticDynamicParameterIDHapticIntensityControl
					value:Update.Intensity
					relativeTime:0.0];
			[Parameters addObject:Parameter];
			[Parameter release];
		}
		if (Update.bUpdateSharpness)
		{
			CHHapticDynamicParameter* Parameter =
				[[CHHapticDynamicParameter alloc]
					initWithParameterID:
						CHHapticDynamicParameterIDHapticSharpnessControl
					value:Update.Sharpness * 2.0f - 1.0f
					relativeTime:0.0];
			[Parameters addObject:Parameter];
			[Parameter release];
		}
		NSError* Error = nil;
		const bool bSent = [Player
			sendParameters:Parameters
			atTime:CHHapticTimeImmediate
			error:&Error] && !Error;
		[Parameters release];
		return bSent;
	}

	id<CHHapticAdvancedPatternPlayer> AsAdvancedPlayer(
		id<CHHapticPatternPlayer> Player
	)
	{
		return Player
			&& [Player conformsToProtocol:@protocol(CHHapticAdvancedPatternPlayer)]
				? static_cast<id<CHHapticAdvancedPatternPlayer>>(Player)
				: nil;
	}

	void ClearCompletionHandler(id<CHHapticPatternPlayer> Player)
	{
		id<CHHapticAdvancedPatternPlayer> Advanced = AsAdvancedPlayer(Player);
		if (Advanced)
		{
			Advanced.completionHandler = ^(NSError* Error)
			{
				static_cast<void>(Error);
			};
		}
	}

	bool ValidateContinuousPattern(
		const FOpenMobileHapticsAppleContinuousPattern& Pattern
	)
	{
		if (!Pattern.IsValid()
			|| (Pattern.bHasInitialDynamicParameters
				&& !ValidateDynamicParameters(
					Pattern.InitialDynamicParameters
				))
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

	CHHapticPattern* CreateTransientPattern(
		const FOpenMobileHapticsAppleTransientPattern& Pattern
	)
	{
		if (!Pattern.IsValid())
		{
			return nil;
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
				return nil;
			}
			PreviousStart = Start;
		}

		NSMutableArray<CHHapticEvent*>* Events =
			[[NSMutableArray alloc]
				initWithCapacity:Pattern.StartTimesSeconds.Num()];
		for (int32 Index = 0; Index < Pattern.StartTimesSeconds.Num(); ++Index)
		{
			CHHapticEventParameter* Intensity =
				[[CHHapticEventParameter alloc]
					initWithParameterID:
						CHHapticEventParameterIDHapticIntensity
					value:Pattern.Intensities[Index]];
			CHHapticEventParameter* Sharpness =
				[[CHHapticEventParameter alloc]
					initWithParameterID:
						CHHapticEventParameterIDHapticSharpness
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
		NSError* Error = nil;
		CHHapticPattern* NativePattern = [[CHHapticPattern alloc]
			initWithEvents:Events
			parameters:@[]
			error:&Error];
		[Events release];
		if (!NativePattern || Error)
		{
			[NativePattern release];
			return nil;
		}
		return NativePattern;
	}

	CHHapticPattern* CreateContinuousPattern(
		const FOpenMobileHapticsAppleContinuousPattern& Pattern
	)
	{
		if (!ValidateContinuousPattern(Pattern))
		{
			return nil;
		}
		NSMutableArray<CHHapticEvent*>* Events =
			[[NSMutableArray alloc] initWithCapacity:Pattern.Events.Num()];
		for (const FOpenMobileHapticsAppleRichEvent& NativeEvent :
			Pattern.Events)
		{
			CHHapticEventParameter* Intensity =
				[[CHHapticEventParameter alloc]
					initWithParameterID:
						CHHapticEventParameterIDHapticIntensity
					value:NativeEvent.Intensity];
			CHHapticEventParameter* Sharpness =
				[[CHHapticEventParameter alloc]
					initWithParameterID:
						CHHapticEventParameterIDHapticSharpness
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
		for (const FOpenMobileHapticsAppleParameterCurve& NativeCurve :
			Pattern.ParameterCurves)
		{
			NSMutableArray<CHHapticParameterCurveControlPoint*>*
				ControlPoints = [[NSMutableArray alloc]
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
			const CHHapticDynamicParameterID ParameterId =
				NativeCurve.Parameter
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

		NSError* Error = nil;
		CHHapticPattern* NativePattern = [[CHHapticPattern alloc]
			initWithEvents:Events
			parameterCurves:Curves
			error:&Error];
		[Curves release];
		[Events release];
		if (!NativePattern || Error)
		{
			[NativePattern release];
			return nil;
		}
		return NativePattern;
	}
}

@interface OpenMobileHapticsAppleNativeService : NSObject
{
	UISelectionFeedbackGenerator* SelectionGenerator;
	UIImpactFeedbackGenerator* ImpactGenerators[5];
	UINotificationFeedbackGenerator* NotificationGenerator;
	CHHapticEngine* Engine;
	NSMutableDictionary<NSNumber*, CHHapticPattern*>* PreparedPatterns;
	TMap<uint64, int64> PreparedPatternBytes;
	TMap<uint64, double> PreparedPatternLastAccess;
	TMap<uint64, uint64> PreparedPatternAccessSequence;
	int64 ActivePreparedPatternBytes;
	int32 MaximumPreparedPatternCount;
	int64 MaximumPreparedPatternBytes;
	double PreparedPatternIdleLifetimeSeconds;
	uint64 NextPreparedPatternAccessSequence;
	NSMutableDictionary<NSNumber*, id<CHHapticPatternPlayer>>* Players;
	NSMutableDictionary<NSNumber*, NSTimer*>* SafetyTimers;
	NSMutableSet<NSNumber*>* PendingAHAPRequests;
	NSMutableDictionary<NSNumber*, NSString*>* ResourceDirectories;
	TMap<uint64, int64> AudioResourceBytesByRequest;
	int64 ActiveAudioResourceBytes;
	int32 ActiveAudioPatternCount;
	dispatch_queue_t AudioPreparationQueue;
	TMap<
		uint64,
		TSharedPtr<
			FOpenMobileHapticsApplePlaybackEventCallback,
			ESPMode::ThreadSafe
		>
	> PlaybackCallbacks;
	NSUInteger ActivityGeneration;
	NSUInteger PreparationGeneration;
	bool bShuttingDown;
	FCriticalSection EventCallbackMutex;
	TSharedPtr<
		FOpenMobileHapticsAppleBridgeEventCallback,
		ESPMode::ThreadSafe
	> EventCallback;
}

- (void)playBehavior:(EOpenMobileHapticsSemanticBehavior)Behavior
	intensity:(CGFloat)Intensity;
- (EOpenMobileHapticsAppleSubmissionResult)prepareSemanticGenerators:
	(double)IdleLifetimeSeconds;
- (EOpenMobileHapticsAppleSubmissionResult)prepareTransientPattern:
	(uint64)ResourceId
	pattern:(const FOpenMobileHapticsAppleTransientPattern&)Pattern
	estimatedBytes:(int64)EstimatedBytes
	limits:(const FOpenMobileHapticsPreparedResourceLimits&)Limits;
- (EOpenMobileHapticsAppleSubmissionResult)prepareContinuousPattern:
	(uint64)ResourceId
	pattern:(const FOpenMobileHapticsAppleContinuousPattern&)Pattern
	estimatedBytes:(int64)EstimatedBytes
	limits:(const FOpenMobileHapticsPreparedResourceLimits&)Limits;
- (void)releasePreparedResources;
- (void)prunePreparedPatterns:(double)CurrentTimeSeconds;
- (void)storePreparedPattern:(CHHapticPattern*)Pattern
	resourceId:(uint64)ResourceId
	estimatedBytes:(int64)EstimatedBytes
	limits:(const FOpenMobileHapticsPreparedResourceLimits&)Limits;
- (CHHapticPattern*)preparedPattern:(uint64)ResourceId;
- (void)playSystemVibration;
- (EOpenMobileHapticsAppleEngineResult)createEngine;
- (EOpenMobileHapticsAppleSubmissionResult)playTransientPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleTransientPattern&)Pattern
	initialParameters:
		(const FOpenMobileHapticDynamicParameterUpdate*)InitialParameters
	preparedResourceId:(uint64)PreparedResourceId
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback;
- (EOpenMobileHapticsAppleSubmissionResult)playContinuousPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleContinuousPattern&)Pattern
	initialParameters:
		(const FOpenMobileHapticDynamicParameterUpdate*)InitialParameters
	preparedResourceId:(uint64)PreparedResourceId
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback;
- (EOpenMobileHapticsAppleSubmissionResult)playAHAPPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleAHAPPattern&)Pattern
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback;
- (EOpenMobileHapticsAppleSubmissionResult)startAHAPPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleAHAPPattern&)Pattern
	nativePattern:(CHHapticPattern*)NativePattern;
- (void)finishAHAPPreparation:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleAHAPPattern&)Pattern
	files:(const OpenMobileHapticsIOSBridgePrivate::FPreparedAHAPFiles&)Files
	generation:(NSUInteger)Generation;
- (void)releaseAudioResourcesForRequest:(uint64)RequestId;
- (void)handleSafetyTimer:(NSTimer*)Timer;
- (void)cancelSafetyTimerForKey:(NSNumber*)Key;
- (EOpenMobileHapticsAppleSubmissionResult)stopPattern:(uint64)RequestId;
- (EOpenMobileHapticsAppleSubmissionResult)updatePattern:
	(uint64)RequestId
	parameters:(const FOpenMobileHapticDynamicParameterUpdate&)Update;
- (void)completePattern:(uint64)RequestId
	event:(EOpenMobileHapticsApplePlaybackEvent)Event;
- (void)failAllPatterns;
- (void)setEventCallback:
	(FOpenMobileHapticsAppleBridgeEventCallback)Callback;
- (void)releaseObjects;

@end

@implementation OpenMobileHapticsAppleNativeService

- (instancetype)init
{
	self = [super init];
	if (self)
	{
		MaximumPreparedPatternCount = 32;
		MaximumPreparedPatternBytes = 4 * 1024 * 1024;
		PreparedPatternIdleLifetimeSeconds = 30.0;
		AudioPreparationQueue = dispatch_queue_create(
			"com.openmobile.haptics.audio-preparation",
			DISPATCH_QUEUE_SERIAL
		);
	}
	return self;
}

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

- (void)prunePreparedPatterns:(double)CurrentTimeSeconds
{
	TArray<uint64> Expired;
	for (const TPair<uint64, double>& Access : PreparedPatternLastAccess)
	{
		if (CurrentTimeSeconds - Access.Value
			>= PreparedPatternIdleLifetimeSeconds)
		{
			Expired.Add(Access.Key);
		}
	}
	for (uint64 ResourceId : Expired)
	{
		if (const int64* Bytes = PreparedPatternBytes.Find(ResourceId))
		{
			ActivePreparedPatternBytes = FMath::Max<int64>(
				0,
				ActivePreparedPatternBytes - *Bytes
			);
		}
		NSNumber* Key = [NSNumber numberWithUnsignedLongLong:ResourceId];
		[PreparedPatterns removeObjectForKey:Key];
		PreparedPatternBytes.Remove(ResourceId);
		PreparedPatternLastAccess.Remove(ResourceId);
		PreparedPatternAccessSequence.Remove(ResourceId);
	}
}

- (void)storePreparedPattern:(CHHapticPattern*)Pattern
	resourceId:(uint64)ResourceId
	estimatedBytes:(int64)EstimatedBytes
	limits:(const FOpenMobileHapticsPreparedResourceLimits&)Limits
{
	MaximumPreparedPatternCount = FMath::Max(1, Limits.MaximumCount);
	MaximumPreparedPatternBytes = FMath::Max<int64>(1, Limits.MaximumBytes);
	PreparedPatternIdleLifetimeSeconds = FMath::Max(
		0.001,
		Limits.IdleLifetimeSeconds
	);
	const double CurrentTimeSeconds = FPlatformTime::Seconds();
	[self prunePreparedPatterns:CurrentTimeSeconds];
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:ResourceId];
	if (const int64* PreviousBytes = PreparedPatternBytes.Find(ResourceId))
	{
		ActivePreparedPatternBytes = FMath::Max<int64>(
			0,
			ActivePreparedPatternBytes - *PreviousBytes
		);
		[PreparedPatterns removeObjectForKey:Key];
		PreparedPatternBytes.Remove(ResourceId);
		PreparedPatternLastAccess.Remove(ResourceId);
		PreparedPatternAccessSequence.Remove(ResourceId);
	}
	if (EstimatedBytes <= 0 || EstimatedBytes > MaximumPreparedPatternBytes)
	{
		return;
	}
	if (!PreparedPatterns)
	{
		PreparedPatterns = [[NSMutableDictionary alloc] init];
	}
	[PreparedPatterns setObject:Pattern forKey:Key];
	PreparedPatternBytes.Add(ResourceId, EstimatedBytes);
	PreparedPatternLastAccess.Add(ResourceId, CurrentTimeSeconds);
	PreparedPatternAccessSequence.Add(
		ResourceId,
		++NextPreparedPatternAccessSequence
	);
	ActivePreparedPatternBytes += EstimatedBytes;

	while (PreparedPatternBytes.Num() > MaximumPreparedPatternCount
		|| ActivePreparedPatternBytes > MaximumPreparedPatternBytes)
	{
		uint64 EvictionId = 0;
		uint64 OldestSequence = MAX_uint64;
		for (const TPair<uint64, uint64>& Access :
			PreparedPatternAccessSequence)
		{
			if (Access.Value < OldestSequence
				|| (Access.Value == OldestSequence
					&& Access.Key < EvictionId))
			{
				EvictionId = Access.Key;
				OldestSequence = Access.Value;
			}
		}
		if (EvictionId == 0)
		{
			break;
		}
		ActivePreparedPatternBytes -= PreparedPatternBytes.FindRef(EvictionId);
		NSNumber* EvictionKey =
			[NSNumber numberWithUnsignedLongLong:EvictionId];
		[PreparedPatterns removeObjectForKey:EvictionKey];
		PreparedPatternBytes.Remove(EvictionId);
		PreparedPatternLastAccess.Remove(EvictionId);
		PreparedPatternAccessSequence.Remove(EvictionId);
	}
}

- (CHHapticPattern*)preparedPattern:(uint64)ResourceId
{
	if (ResourceId == 0)
	{
		return nil;
	}
	const double CurrentTimeSeconds = FPlatformTime::Seconds();
	[self prunePreparedPatterns:CurrentTimeSeconds];
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:ResourceId];
	CHHapticPattern* Pattern = [PreparedPatterns objectForKey:Key];
	if (Pattern)
	{
		PreparedPatternLastAccess.Add(ResourceId, CurrentTimeSeconds);
		PreparedPatternAccessSequence.Add(
			ResourceId,
			++NextPreparedPatternAccessSequence
		);
	}
	return Pattern;
}

- (void)releasePreparedResources
{
	++ActivityGeneration;
	[self releaseGenerators];
	[PreparedPatterns removeAllObjects];
	[PreparedPatterns release];
	PreparedPatterns = nil;
	PreparedPatternBytes.Reset();
	PreparedPatternLastAccess.Reset();
	PreparedPatternAccessSequence.Reset();
	ActivePreparedPatternBytes = 0;
	NextPreparedPatternAccessSequence = 0;
}

- (EOpenMobileHapticsAppleSubmissionResult)prepareSemanticGenerators:
	(double)IdleLifetimeSeconds
{
	if (bShuttingDown || !FMath::IsFinite(IdleLifetimeSeconds)
		|| IdleLifetimeSeconds <= 0.0)
	{
		return bShuttingDown
			? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
			: EOpenMobileHapticsAppleSubmissionResult::Unsupported;
	}
	if (!SelectionGenerator)
	{
		SelectionGenerator = [[UISelectionFeedbackGenerator alloc] init];
	}
	[SelectionGenerator prepare];
	const UIImpactFeedbackStyle Styles[] = {
		UIImpactFeedbackStyleLight,
		UIImpactFeedbackStyleMedium,
		UIImpactFeedbackStyleHeavy,
		UIImpactFeedbackStyleSoft,
		UIImpactFeedbackStyleRigid
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ImpactGenerators); ++Index)
	{
		if (!ImpactGenerators[Index])
		{
			ImpactGenerators[Index] = [[UIImpactFeedbackGenerator alloc]
				initWithStyle:Styles[Index]];
		}
		[ImpactGenerators[Index] prepare];
	}
	if (!NotificationGenerator)
	{
		NotificationGenerator =
			[[UINotificationFeedbackGenerator alloc] init];
	}
	[NotificationGenerator prepare];
	const NSUInteger ExpectedGeneration = ++ActivityGeneration;
	const int64 DelayNanoseconds = FMath::Max<int64>(
		1,
		FMath::RoundToInt64(IdleLifetimeSeconds * NSEC_PER_SEC)
	);
	dispatch_after(
		dispatch_time(DISPATCH_TIME_NOW, DelayNanoseconds),
		dispatch_get_main_queue(),
		^{
			if (ActivityGeneration == ExpectedGeneration)
			{
				[self releaseGenerators];
			}
		}
	);
	return EOpenMobileHapticsAppleSubmissionResult::Accepted;
}

- (EOpenMobileHapticsAppleSubmissionResult)prepareTransientPattern:
	(uint64)ResourceId
	pattern:(const FOpenMobileHapticsAppleTransientPattern&)Pattern
	estimatedBytes:(int64)EstimatedBytes
	limits:(const FOpenMobileHapticsPreparedResourceLimits&)Limits
{
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	CHHapticPattern* NativePattern =
		OpenMobileHapticsIOSBridgePrivate::CreateTransientPattern(Pattern);
	if (!NativePattern || ResourceId == 0)
	{
		[NativePattern release];
		return EOpenMobileHapticsAppleSubmissionResult::Unsupported;
	}
	[self storePreparedPattern:NativePattern
		resourceId:ResourceId
		estimatedBytes:EstimatedBytes
		limits:Limits];
	[NativePattern release];
	return EOpenMobileHapticsAppleSubmissionResult::Accepted;
}

- (EOpenMobileHapticsAppleSubmissionResult)prepareContinuousPattern:
	(uint64)ResourceId
	pattern:(const FOpenMobileHapticsAppleContinuousPattern&)Pattern
	estimatedBytes:(int64)EstimatedBytes
	limits:(const FOpenMobileHapticsPreparedResourceLimits&)Limits
{
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	CHHapticPattern* NativePattern =
		OpenMobileHapticsIOSBridgePrivate::CreateContinuousPattern(Pattern);
	if (!NativePattern || ResourceId == 0)
	{
		[NativePattern release];
		return EOpenMobileHapticsAppleSubmissionResult::Unsupported;
	}
	[self storePreparedPattern:NativePattern
		resourceId:ResourceId
		estimatedBytes:EstimatedBytes
		limits:Limits];
	[NativePattern release];
	return EOpenMobileHapticsAppleSubmissionResult::Accepted;
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
		NSError* Error = nil;
		if ([Engine startAndReturnError:&Error] && !Error)
		{
			return EOpenMobileHapticsAppleEngineResult::Ready;
		}
		Engine.stoppedHandler = ^(CHHapticEngineStoppedReason Reason)
		{
			static_cast<void>(Reason);
		};
		Engine.resetHandler = ^{};
		[Engine release];
		Engine = nil;
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
		Engine.autoShutdownEnabled = YES;

		OpenMobileHapticsAppleNativeService* Service = self;
		Engine.stoppedHandler = ^(CHHapticEngineStoppedReason Reason)
		{
			static_cast<void>(Reason);
			dispatch_async(dispatch_get_main_queue(), ^{
				[Service failAllPatterns];
				[Service emitEvent:
					EOpenMobileHapticsAppleBridgeEvent::EngineStopped];
			});
		};
		Engine.resetHandler = ^
		{
			dispatch_async(dispatch_get_main_queue(), ^{
				[Service failAllPatterns];
				[Service emitEvent:
					EOpenMobileHapticsAppleBridgeEvent::EngineReset];
			});
		};
		Error = nil;
		if (![Engine startAndReturnError:&Error] || Error)
		{
			Engine.stoppedHandler = ^(CHHapticEngineStoppedReason Reason)
			{
				static_cast<void>(Reason);
			};
			Engine.resetHandler = ^{};
			[Engine release];
			Engine = nil;
			return EOpenMobileHapticsAppleEngineResult::NativeFailure;
		}
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
	id<CHHapticPatternPlayer> Player = [Players objectForKey:Key];
	if (Player)
	{
		OpenMobileHapticsIOSBridgePrivate::ClearCompletionHandler(Player);
		[Players removeObjectForKey:Key];
	}
	[PendingAHAPRequests removeObject:Key];
	[self releaseAudioResourcesForRequest:RequestId];
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
	++PreparationGeneration;
	TArray<uint64> RequestIds;
	PlaybackCallbacks.GetKeys(RequestIds);
	for (const uint64 RequestId : RequestIds)
	{
		[self completePattern:RequestId
			event:EOpenMobileHapticsApplePlaybackEvent::Failed];
	}
}

- (void)releaseAudioResourcesForRequest:(uint64)RequestId
{
	if (int64* ResourceBytes = AudioResourceBytesByRequest.Find(RequestId))
	{
		ActiveAudioResourceBytes = FMath::Max<int64>(
			0,
			ActiveAudioResourceBytes - *ResourceBytes
		);
		ActiveAudioPatternCount = FMath::Max(0, ActiveAudioPatternCount - 1);
		AudioResourceBytesByRequest.Remove(RequestId);
	}
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:RequestId];
	NSString* Directory = [ResourceDirectories objectForKey:Key];
	if (Directory)
	{
		[[NSFileManager defaultManager]
			removeItemAtPath:Directory
			error:nil];
		[ResourceDirectories removeObjectForKey:Key];
	}
}

- (EOpenMobileHapticsAppleSubmissionResult)playTransientPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleTransientPattern&)Pattern
	initialParameters:
		(const FOpenMobileHapticDynamicParameterUpdate*)InitialParameters
	preparedResourceId:(uint64)PreparedResourceId
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback
{
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	if (!Engine
		|| !Pattern.IsValid()
		|| (InitialParameters
			&& !OpenMobileHapticsIOSBridgePrivate::ValidateDynamicParameters(
				*InitialParameters
			))
		|| (Pattern.bHasInitialDynamicParameters
			&& !OpenMobileHapticsIOSBridgePrivate::ValidateDynamicParameters(
				Pattern.InitialDynamicParameters
			)))
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
	CHHapticPattern* NativePattern =
		[[self preparedPattern:PreparedResourceId] retain];
	if (!NativePattern)
	{
		NativePattern =
			OpenMobileHapticsIOSBridgePrivate::CreateTransientPattern(Pattern);
	}
	if (!NativePattern)
	{
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
	const FOpenMobileHapticDynamicParameterUpdate* EffectiveInitialParameters =
		InitialParameters
			? InitialParameters
			: Pattern.bHasInitialDynamicParameters
				? &Pattern.InitialDynamicParameters
				: nullptr;
	if (EffectiveInitialParameters
		&& !OpenMobileHapticsIOSBridgePrivate::SendDynamicParameters(
			Player,
			*EffectiveInitialParameters
		))
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
	initialParameters:
		(const FOpenMobileHapticDynamicParameterUpdate*)InitialParameters
	preparedResourceId:(uint64)PreparedResourceId
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback
{
	using namespace OpenMobileHapticsIOSBridgePrivate;
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	if (!Engine
		|| !ValidateContinuousPattern(Pattern)
		|| (InitialParameters && !ValidateDynamicParameters(*InitialParameters)))
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

	CHHapticPattern* NativePattern =
		[[self preparedPattern:PreparedResourceId] retain];
	if (!NativePattern)
	{
		NativePattern =
			OpenMobileHapticsIOSBridgePrivate::CreateContinuousPattern(Pattern);
	}
	if (!NativePattern)
	{
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
	const FOpenMobileHapticDynamicParameterUpdate* EffectiveInitialParameters =
		InitialParameters
			? InitialParameters
			: Pattern.bHasInitialDynamicParameters
				? &Pattern.InitialDynamicParameters
				: nullptr;
	if (EffectiveInitialParameters
		&& !OpenMobileHapticsIOSBridgePrivate::SendDynamicParameters(
			Player,
			*EffectiveInitialParameters
		))
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

- (EOpenMobileHapticsAppleSubmissionResult)playAHAPPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleAHAPPattern&)Pattern
	callback:(FOpenMobileHapticsApplePlaybackEventCallback)Callback
{
	using namespace OpenMobileHapticsIOSBridgePrivate;
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	if (!Engine
		|| RequestId == 0
		|| Pattern.NormalizedJson.IsEmpty()
		|| !IsFiniteInRange(
			Pattern.DurationSeconds,
			0.0,
			MaximumNativePatternDurationSeconds
		)
		|| !IsFiniteInRange(
			Pattern.SafetyDurationSeconds,
			TimingToleranceSeconds,
			MaximumNativePatternDurationSeconds
		)
		|| Pattern.SafetyDurationSeconds + TimingToleranceSeconds
			< Pattern.DurationSeconds
		|| (Pattern.bScheduled
			&& (!FMath::IsFinite(Pattern.ScheduledPlatformTimeSeconds)
				|| Pattern.ScheduledPlatformTimeSeconds <= 0.0
				|| !IsFiniteInRange(
					Pattern.MaximumLatenessSeconds,
					0.0,
					1.0
				)))
		|| (Pattern.bLoop && !Pattern.bRequiresAdvancedPlayer)
		|| (Pattern.bHasInitialDynamicParameters
			&& !ValidateDynamicParameters(
				Pattern.InitialDynamicParameters
			)))
	{
		return EOpenMobileHapticsAppleSubmissionResult::Unsupported;
	}
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:RequestId];
	if ([Players objectForKey:Key] || PlaybackCallbacks.Contains(RequestId))
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}

	const FTCHARToUTF8 UTF8(*Pattern.NormalizedJson);
	if (UTF8.Length() <= 0 || UTF8.Length() > 256 * 1024)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}

	if (!Pattern.AudioResources.IsEmpty())
	{
		int64 ResourceBytes = 0;
		if (!AudioPreparationQueue
			|| !ValidateAudioResources(Pattern, ResourceBytes)
			|| ActiveAudioPatternCount >= MaximumActiveAudioPatternCount
			|| ActiveAudioResourceBytes + ResourceBytes
				> MaximumActiveAudioResourceBytes)
		{
			return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
		}
		if (!PendingAHAPRequests)
		{
			PendingAHAPRequests = [[NSMutableSet alloc] init];
		}
		[PendingAHAPRequests addObject:Key];
		PlaybackCallbacks.Add(
			RequestId,
			MakeShared<
				FOpenMobileHapticsApplePlaybackEventCallback,
				ESPMode::ThreadSafe
			>(MoveTemp(Callback))
		);
		AudioResourceBytesByRequest.Add(RequestId, ResourceBytes);
		ActiveAudioResourceBytes += ResourceBytes;
		++ActiveAudioPatternCount;
		const NSUInteger Generation = PreparationGeneration;
		TSharedRef<
			FOpenMobileHapticsAppleAHAPPattern,
			ESPMode::ThreadSafe
		> PatternCopy = MakeShared<
			FOpenMobileHapticsAppleAHAPPattern,
			ESPMode::ThreadSafe
		>(Pattern);
		TSharedRef<
			FPreparedAHAPFiles,
			ESPMode::ThreadSafe
		> Files = MakeShared<FPreparedAHAPFiles, ESPMode::ThreadSafe>();
		OpenMobileHapticsAppleNativeService* Service = [self retain];
		dispatch_async(AudioPreparationQueue, ^{
			PrepareAHAPFiles(*PatternCopy, *Files);
			dispatch_async(dispatch_get_main_queue(), ^{
				[Service
					finishAHAPPreparation:RequestId
					pattern:*PatternCopy
					files:*Files
					generation:Generation];
				[Service release];
			});
		});
		return EOpenMobileHapticsAppleSubmissionResult::Accepted;
	}

	NSData* Data = [NSData
		dataWithBytes:UTF8.Get()
		length:static_cast<NSUInteger>(UTF8.Length())];
	NSError* Error = nil;
	id JsonObject = [NSJSONSerialization
		JSONObjectWithData:Data
		options:0
		error:&Error];
	if (!JsonObject || Error || ![JsonObject isKindOfClass:[NSDictionary class]])
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	CHHapticPattern* NativePattern = [[CHHapticPattern alloc]
		initWithDictionary:static_cast<NSDictionary*>(JsonObject)
		error:&Error];
	if (!NativePattern || Error)
	{
		[NativePattern release];
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	PlaybackCallbacks.Add(
		RequestId,
		MakeShared<
			FOpenMobileHapticsApplePlaybackEventCallback,
			ESPMode::ThreadSafe
		>(MoveTemp(Callback))
	);
	const EOpenMobileHapticsAppleSubmissionResult Result = [self
		startAHAPPattern:RequestId
		pattern:Pattern
		nativePattern:NativePattern];
	[NativePattern release];
	if (Result != EOpenMobileHapticsAppleSubmissionResult::Accepted)
	{
		PlaybackCallbacks.Remove(RequestId);
	}
	return Result;
}

- (EOpenMobileHapticsAppleSubmissionResult)startAHAPPattern:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleAHAPPattern&)Pattern
	nativePattern:(CHHapticPattern*)NativePattern
{
	using namespace OpenMobileHapticsIOSBridgePrivate;
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:RequestId];
	if (bShuttingDown
		|| !Engine
		|| !NativePattern
		|| [Players objectForKey:Key]
		|| !PlaybackCallbacks.Contains(RequestId))
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}

	NSError* Error = nil;
	if (![Engine startAndReturnError:&Error] || Error)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	const double PlatformNow = FPlatformTime::Seconds();
	const double LatenessSeconds = Pattern.bScheduled
		? PlatformNow - Pattern.ScheduledPlatformTimeSeconds
		: 0.0;
	if (Pattern.bScheduled
		&& LatenessSeconds > Pattern.MaximumLatenessSeconds)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	const double StartDelaySeconds = Pattern.bScheduled
		? FMath::Max(
			0.0,
			Pattern.ScheduledPlatformTimeSeconds - PlatformNow
		)
		: 0.0;
	const NSTimeInterval StartTime = Pattern.bScheduled
		? Engine.currentTime + StartDelaySeconds
		: CHHapticTimeImmediate;
	id<CHHapticPatternPlayer> Player = nil;
	id<CHHapticAdvancedPatternPlayer> AdvancedPlayer = nil;
	if (Pattern.bRequiresAdvancedPlayer)
	{
		AdvancedPlayer = [Engine
			createAdvancedPlayerWithPattern:NativePattern
			error:&Error];
		Player = AdvancedPlayer;
	}
	else
	{
		Player = [Engine createPlayerWithPattern:NativePattern error:&Error];
	}
	if (!Player || Error)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	if (Pattern.bHasInitialDynamicParameters
		&& !SendDynamicParameters(
			Player,
			Pattern.InitialDynamicParameters
		))
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	if (AdvancedPlayer)
	{
		AdvancedPlayer.loopEnabled = Pattern.bLoop;
		if (Pattern.bLoop)
		{
			AdvancedPlayer.loopEnd = Pattern.DurationSeconds;
		}
	}

	const bool bUsesTimer = !AdvancedPlayer || Pattern.bLoop;
	NSTimer* SafetyTimer = bUsesTimer
		? [NSTimer
			timerWithTimeInterval:
				Pattern.SafetyDurationSeconds + StartDelaySeconds
			target:self
			selector:@selector(handleSafetyTimer:)
			userInfo:Key
			repeats:NO]
		: nil;
	if (bUsesTimer && !SafetyTimer)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	if (!Players)
	{
		Players = [[NSMutableDictionary alloc] init];
	}
	OpenMobileHapticsAppleNativeService* Service = self;
	if (AdvancedPlayer)
	{
		AdvancedPlayer.completionHandler = ^(NSError* CompletionError)
		{
			[Service completePattern:RequestId
				event:CompletionError
					? EOpenMobileHapticsApplePlaybackEvent::Failed
					: EOpenMobileHapticsApplePlaybackEvent::Completed];
		};
	}
	[Players setObject:Player forKey:Key];
	Error = nil;
	if (![Player startAtTime:StartTime error:&Error] || Error)
	{
		[SafetyTimer invalidate];
		ClearCompletionHandler(Player);
		[Players removeObjectForKey:Key];
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	if (bUsesTimer)
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

- (void)finishAHAPPreparation:
	(uint64)RequestId
	pattern:(const FOpenMobileHapticsAppleAHAPPattern&)Pattern
	files:(const OpenMobileHapticsIOSBridgePrivate::FPreparedAHAPFiles&)Files
	generation:(NSUInteger)Generation
{
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:RequestId];
	if (bShuttingDown
		|| Generation != PreparationGeneration
		|| ![PendingAHAPRequests containsObject:Key])
	{
		if (!Files.Directory.IsEmpty())
		{
			NSString* Directory = [NSString
				stringWithUTF8String:TCHAR_TO_UTF8(*Files.Directory)];
			[[NSFileManager defaultManager]
				removeItemAtPath:Directory
				error:nil];
		}
		return;
	}
	[PendingAHAPRequests removeObject:Key];
	if (!Files.bSucceeded)
	{
		[self completePattern:RequestId
			event:EOpenMobileHapticsApplePlaybackEvent::Failed];
		return;
	}
	if (!ResourceDirectories)
	{
		ResourceDirectories = [[NSMutableDictionary alloc] init];
	}
	NSString* Directory = [NSString
		stringWithUTF8String:TCHAR_TO_UTF8(*Files.Directory)];
	[ResourceDirectories setObject:Directory forKey:Key];
	NSString* PatternPath = [NSString
		stringWithUTF8String:TCHAR_TO_UTF8(*Files.PatternPath)];
	NSURL* PatternURL = [NSURL fileURLWithPath:PatternPath];
	NSError* Error = nil;
	CHHapticPattern* NativePattern = [[CHHapticPattern alloc]
		initWithContentsOfURL:PatternURL
		error:&Error];
	if (!NativePattern || Error)
	{
		[NativePattern release];
		[self completePattern:RequestId
			event:EOpenMobileHapticsApplePlaybackEvent::Failed];
		return;
	}
	const EOpenMobileHapticsAppleSubmissionResult Result = [self
		startAHAPPattern:RequestId
		pattern:Pattern
		nativePattern:NativePattern];
	[NativePattern release];
	if (Result != EOpenMobileHapticsAppleSubmissionResult::Accepted)
	{
		[self completePattern:RequestId
			event:EOpenMobileHapticsApplePlaybackEvent::Failed];
	}
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
	id<CHHapticPatternPlayer> Player = [Players objectForKey:Key];
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
	id<CHHapticPatternPlayer> Player = [Players objectForKey:Key];
	if (!Player)
	{
		if ([PendingAHAPRequests containsObject:Key])
		{
			[PendingAHAPRequests removeObject:Key];
			PlaybackCallbacks.Remove(RequestId);
			[self releaseAudioResourcesForRequest:RequestId];
		}
		return EOpenMobileHapticsAppleSubmissionResult::Accepted;
	}
	NSError* Error = nil;
	const bool bStopped = [Player stopAtTime:CHHapticTimeImmediate error:&Error];
	if (!bStopped || Error)
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	[self cancelSafetyTimerForKey:Key];
	OpenMobileHapticsIOSBridgePrivate::ClearCompletionHandler(Player);
	[Players removeObjectForKey:Key];
	PlaybackCallbacks.Remove(RequestId);
	[self releaseAudioResourcesForRequest:RequestId];
	return EOpenMobileHapticsAppleSubmissionResult::Accepted;
}

- (EOpenMobileHapticsAppleSubmissionResult)updatePattern:
	(uint64)RequestId
	parameters:(const FOpenMobileHapticDynamicParameterUpdate&)Update
{
	using namespace OpenMobileHapticsIOSBridgePrivate;
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	if (!Engine || RequestId == 0 || !ValidateDynamicParameters(Update))
	{
		return EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
	}
	NSNumber* Key = [NSNumber numberWithUnsignedLongLong:RequestId];
	id<CHHapticPatternPlayer> Player = [Players objectForKey:Key];
	if (!Player)
	{
		return EOpenMobileHapticsAppleSubmissionResult::StaleRequest;
	}
	return SendDynamicParameters(Player, Update)
		? EOpenMobileHapticsAppleSubmissionResult::Accepted
		: EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
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
	++PreparationGeneration;
	[self setEventCallback:{}];
	[self releasePreparedResources];
	for (NSTimer* Timer in [SafetyTimers allValues])
	{
		[Timer invalidate];
	}
	[SafetyTimers removeAllObjects];
	[SafetyTimers release];
	SafetyTimers = nil;
	for (id<CHHapticPatternPlayer> Player in [Players allValues])
	{
		OpenMobileHapticsIOSBridgePrivate::ClearCompletionHandler(Player);
		NSError* Error = nil;
		[Player stopAtTime:CHHapticTimeImmediate error:&Error];
	}
	[Players removeAllObjects];
	[Players release];
	Players = nil;
	for (NSString* Directory in [ResourceDirectories allValues])
	{
		[[NSFileManager defaultManager]
			removeItemAtPath:Directory
			error:nil];
	}
	[ResourceDirectories removeAllObjects];
	[ResourceDirectories release];
	ResourceDirectories = nil;
	[PendingAHAPRequests removeAllObjects];
	[PendingAHAPRequests release];
	PendingAHAPRequests = nil;
	AudioResourceBytesByRequest.Reset();
	ActiveAudioResourceBytes = 0;
	ActiveAudioPatternCount = 0;
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
	if (AudioPreparationQueue)
	{
#if !OS_OBJECT_USE_OBJC
		dispatch_release(AudioPreparationQueue);
#endif
		AudioPreparationQueue = nullptr;
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

		virtual EOpenMobileHapticsAppleSubmissionResult
		PrepareSemanticGenerators(double IdleLifetimeSeconds) override
		{
			if (!NativeService)
			{
				return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
			}
			EOpenMobileHapticsAppleSubmissionResult Result =
				EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
			OpenMobileHapticsAppleNativeService* Service = NativeService;
			RunOnMainQueue([Service, IdleLifetimeSeconds, &Result]()
			{
				Result = [Service
					prepareSemanticGenerators:IdleLifetimeSeconds];
			});
			return Result;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult
		PrepareTransientPattern(
			uint64 ResourceId,
			const FOpenMobileHapticsAppleTransientPattern& Pattern,
			int64 EstimatedBytes,
			const FOpenMobileHapticsPreparedResourceLimits& Limits
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
				[Service, ResourceId, &Pattern, EstimatedBytes, &Limits,
				 &Result]()
				{
					Result = [Service
						prepareTransientPattern:ResourceId
						pattern:Pattern
						estimatedBytes:EstimatedBytes
						limits:Limits];
				}
			);
			return Result;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult
		PrepareContinuousPattern(
			uint64 ResourceId,
			const FOpenMobileHapticsAppleContinuousPattern& Pattern,
			int64 EstimatedBytes,
			const FOpenMobileHapticsPreparedResourceLimits& Limits
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
				[Service, ResourceId, &Pattern, EstimatedBytes, &Limits,
				 &Result]()
				{
					Result = [Service
						prepareContinuousPattern:ResourceId
						pattern:Pattern
						estimatedBytes:EstimatedBytes
						limits:Limits];
				}
			);
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
			FOpenMobileHapticsApplePlaybackEventCallback Callback,
			const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
			uint64 PreparedResourceId
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
				[Service, RequestId, &Pattern, &Callback, &Result,
				 InitialParameters, PreparedResourceId]()
				{
					Result = [Service
						playTransientPattern:RequestId
						pattern:Pattern
						initialParameters:InitialParameters
						preparedResourceId:PreparedResourceId
						callback:MoveTemp(Callback)];
				}
			);
			return Result;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleContinuousPattern& Pattern,
			FOpenMobileHapticsApplePlaybackEventCallback Callback,
			const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
			uint64 PreparedResourceId
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
				[Service, RequestId, &Pattern, &Callback, &Result,
				 InitialParameters, PreparedResourceId]()
				{
					Result = [Service
						playContinuousPattern:RequestId
						pattern:Pattern
						initialParameters:InitialParameters
						preparedResourceId:PreparedResourceId
						callback:MoveTemp(Callback)];
				}
			);
			return Result;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult PlayAHAPPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleAHAPPattern& Pattern,
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
						playAHAPPattern:RequestId
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

		virtual EOpenMobileHapticsAppleSubmissionResult UpdatePattern(
			uint64 RequestId,
			const FOpenMobileHapticDynamicParameterUpdate& Update
		) override
		{
			if (!NativeService)
			{
				return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
			}
			EOpenMobileHapticsAppleSubmissionResult Result =
				EOpenMobileHapticsAppleSubmissionResult::NativeFailure;
			OpenMobileHapticsAppleNativeService* Service = NativeService;
			RunOnMainQueue([Service, RequestId, &Update, &Result]()
			{
				Result = [Service
					updatePattern:RequestId
					parameters:Update];
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

		virtual void ReleasePreparedResources() override
		{
			if (!NativeService)
			{
				return;
			}
			OpenMobileHapticsAppleNativeService* Service = NativeService;
			RunOnMainQueue([Service]()
			{
				[Service releasePreparedResources];
			});
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
