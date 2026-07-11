#include "OpenMobileSensorsIOSBridge.h"

#import <CoreMotion/CoreMotion.h>
#import <UIKit/UIKit.h>

#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileMotionActivityClassifier.h"
#include "OpenMobileProximityMonitoringPolicy.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsIOSBackend.h"

namespace OpenMobileSensorsIOSBridgePrivate
{
	constexpr int32 MaximumCallbackBatchSamples = 1;

	enum class EService : uint8
	{
		Unknown,
		Accelerometer,
		Gyroscope,
		Magnetometer,
		DeviceMotion,
		RelativeAltitude,
		AbsoluteAltitude,
		Pedometer,
		MotionActivity,
		Proximity
	};

	struct FPedometerCallbackGate
	{
		FCriticalSection Mutex;
		void* Owner = nullptr;
	};

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

	FString FromNSString(NSString* Value)
	{
		return Value ? FString(UTF8_TO_TCHAR(Value.UTF8String)) : FString{};
	}

	FOpenMobileSensorSampleHeader MakeHeader(
		const FOpenMobileSensorIdentifier& Sensor,
		double TimestampSeconds,
		bool bReset,
		int32 SourceFlags = 0
	)
	{
		FOpenMobileSensorSampleHeader Header;
		Header.Sensor = Sensor;
		Header.TimestampSeconds = TimestampSeconds;
		Header.bStatefulProcessingReset = bReset;
		Header.bValid = true;
		Header.SourceFlags = SourceFlags != 0
			? SourceFlags
			: FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
				Sensor.Type
			);
		return Header;
	}

	bool TryGetPedometerDouble(NSNumber* Value, double& OutValue)
	{
		if (!Value)
		{
			return false;
		}
		const double NativeValue = Value.doubleValue;
		if (!FMath::IsFinite(NativeValue) || NativeValue < 0.0)
		{
			return false;
		}
		OutValue = NativeValue;
		return true;
	}

	bool TryGetPedometerCount(NSNumber* Value, int64& OutValue)
	{
		if (!Value)
		{
			return false;
		}
		const int64 NativeValue = Value.longLongValue;
		if (NativeValue < 0)
		{
			return false;
		}
		OutValue = NativeValue;
		return true;
	}

	void PopulatePedometerMetrics(
		CMPedometerData* Data,
		FOpenMobileStepsSensorSample& Sample
	)
	{
		Sample.Metrics = {};
		Sample.Metrics.bHasDistanceMeters = TryGetPedometerDouble(
			Data.distance,
			Sample.Metrics.DistanceMeters
		);
		Sample.Metrics.bHasFloorsAscended = TryGetPedometerCount(
			Data.floorsAscended,
			Sample.Metrics.FloorsAscended
		);
		Sample.Metrics.bHasFloorsDescended = TryGetPedometerCount(
			Data.floorsDescended,
			Sample.Metrics.FloorsDescended
		);
		if (@available(iOS 9.0, *))
		{
			Sample.Metrics.bHasPaceSecondsPerMeter =
				TryGetPedometerDouble(
					Data.currentPace,
					Sample.Metrics.PaceSecondsPerMeter
				);
			Sample.Metrics.bHasCadenceStepsPerSecond =
				TryGetPedometerDouble(
					Data.currentCadence,
					Sample.Metrics.CadenceStepsPerSecond
				);
		}
	}

	EService ServiceForType(EOpenMobileSensorType Type)
	{
		switch (Type)
		{
		case EOpenMobileSensorType::Accelerometer:
			return EService::Accelerometer;
		case EOpenMobileSensorType::Gyroscope:
			return EService::Gyroscope;
		case EOpenMobileSensorType::MagnetometerUncalibrated:
			return EService::Magnetometer;
		case EOpenMobileSensorType::Magnetometer:
		case EOpenMobileSensorType::Gravity:
		case EOpenMobileSensorType::LinearAcceleration:
		case EOpenMobileSensorType::Attitude:
		case EOpenMobileSensorType::MagneticHeading:
			return EService::DeviceMotion;
		case EOpenMobileSensorType::BarometricPressure:
		case EOpenMobileSensorType::RelativeAltitude:
			return EService::RelativeAltitude;
		case EOpenMobileSensorType::AbsoluteAltitude:
			return EService::AbsoluteAltitude;
		case EOpenMobileSensorType::StepCounter:
		case EOpenMobileSensorType::StepDetector:
			return EService::Pedometer;
		case EOpenMobileSensorType::MotionActivity:
			return EService::MotionActivity;
		case EOpenMobileSensorType::Proximity:
			return EService::Proximity;
		default:
			return EService::Unknown;
		}
	}

	bool IsPermissionFailure(NSInteger Code)
	{
		return Code == CMErrorNotAuthorized
			|| Code == CMErrorMotionActivityNotAuthorized
			|| Code == CMErrorNotEntitled
			|| Code == CMErrorMotionActivityNotEntitled;
	}

	EOpenMobileSensorsIOSBridgeFailure FailureFromError(NSError* Error)
	{
		if (!Error)
		{
			return EOpenMobileSensorsIOSBridgeFailure::ManagerError;
		}
		if ([Error.domain isEqualToString:CMErrorDomain])
		{
			if (IsPermissionFailure(Error.code))
			{
				return EOpenMobileSensorsIOSBridgeFailure::PermissionDenied;
			}
			if (Error.code == CMErrorNotAvailable
				|| Error.code == CMErrorNilData)
			{
				return EOpenMobileSensorsIOSBridgeFailure::
					ServiceTemporarilyUnavailable;
			}
			if (Error.code == CMErrorMotionActivityNotAvailable)
			{
				return EOpenMobileSensorsIOSBridgeFailure::SensorUnavailable;
			}
			if (Error.code == CMErrorTrueNorthNotAvailable)
			{
				return EOpenMobileSensorsIOSBridgeFailure::
					ReferenceFrameUnavailable;
			}
		}
		return EOpenMobileSensorsIOSBridgeFailure::ManagerError;
	}

	bool IsTransientMovementError(NSError* Error)
	{
		return Error
			&& [Error.domain isEqualToString:CMErrorDomain]
			&& Error.code == CMErrorDeviceRequiresMovement;
	}

	FOpenMobileSensorsIOSAvailability AvailabilityForManager(
		CMMotionManager* Manager
	)
	{
		FOpenMobileSensorsIOSAvailability Availability;
		Availability.bProximityApiSupported = true;
		if (@available(iOS 8.0, *))
		{
			Availability.bPedometerApiSupported = true;
			Availability.bStepCounting =
				[CMPedometer isStepCountingAvailable];
		}
		if (@available(iOS 7.0, *))
		{
			Availability.bMotionActivityApiSupported = true;
			Availability.bMotionActivity =
				[CMMotionActivityManager isActivityAvailable];
		}
		if (@available(iOS 11.0, *))
		{
			switch ([CMPedometer authorizationStatus])
			{
			case CMAuthorizationStatusAuthorized:
				Availability.PedometerAuthorizationStatus =
					EOpenMobilePermissionStatus::Granted;
				break;
			case CMAuthorizationStatusDenied:
				Availability.PedometerAuthorizationStatus =
					EOpenMobilePermissionStatus::Denied;
				break;
			case CMAuthorizationStatusRestricted:
				Availability.PedometerAuthorizationStatus =
					EOpenMobilePermissionStatus::Restricted;
				break;
			case CMAuthorizationStatusNotDetermined:
			default:
				Availability.PedometerAuthorizationStatus =
					EOpenMobilePermissionStatus::NotDetermined;
				break;
			}
			switch ([CMMotionActivityManager authorizationStatus])
			{
			case CMAuthorizationStatusAuthorized:
				Availability.MotionActivityAuthorizationStatus =
					EOpenMobilePermissionStatus::Granted;
				break;
			case CMAuthorizationStatusDenied:
				Availability.MotionActivityAuthorizationStatus =
					EOpenMobilePermissionStatus::Denied;
				break;
			case CMAuthorizationStatusRestricted:
				Availability.MotionActivityAuthorizationStatus =
					EOpenMobilePermissionStatus::Restricted;
				break;
			case CMAuthorizationStatusNotDetermined:
			default:
				Availability.MotionActivityAuthorizationStatus =
					EOpenMobilePermissionStatus::NotDetermined;
				break;
			}
		}
		if (@available(iOS 15.0, *))
		{
			Availability.bAbsoluteAltitudeApiSupported = true;
		}
		if (!Manager)
		{
			return Availability;
		}
		if (@available(iOS 4.0, *))
		{
			Availability.bAccelerometer = Manager.isAccelerometerAvailable;
			Availability.bGyroscope = Manager.isGyroAvailable;
			Availability.bDeviceMotion = Manager.isDeviceMotionAvailable;
		}
		if (@available(iOS 5.0, *))
		{
			Availability.bMagnetometer = Manager.isMagnetometerAvailable;
			const CMAttitudeReferenceFrame Frames =
				[CMMotionManager availableAttitudeReferenceFrames];
			Availability.bMagneticNorthReference =
				(Frames & CMAttitudeReferenceFrameXMagneticNorthZVertical) != 0;
		}
		if (@available(iOS 8.0, *))
		{
			Availability.bRelativeAltitude =
				[CMAltimeter isRelativeAltitudeAvailable];
		}
		if (@available(iOS 15.0, *))
		{
			Availability.bAbsoluteAltitude =
				[CMAltimeter isAbsoluteAltitudeAvailable];
		}
		return Availability;
	}

	bool SupportsType(
		const FOpenMobileSensorsIOSAvailability& Availability,
		EOpenMobileSensorType Type
	)
	{
		switch (Type)
		{
		case EOpenMobileSensorType::Accelerometer:
			return Availability.bAccelerometer;
		case EOpenMobileSensorType::Gyroscope:
			return Availability.bGyroscope;
		case EOpenMobileSensorType::MagnetometerUncalibrated:
			return Availability.bMagnetometer;
		case EOpenMobileSensorType::Magnetometer:
			return Availability.bDeviceMotion && Availability.bMagnetometer;
		case EOpenMobileSensorType::Gravity:
		case EOpenMobileSensorType::LinearAcceleration:
		case EOpenMobileSensorType::Attitude:
			return Availability.bDeviceMotion;
		case EOpenMobileSensorType::MagneticHeading:
			return Availability.bDeviceMotion
				&& Availability.bMagneticNorthReference;
		case EOpenMobileSensorType::BarometricPressure:
		case EOpenMobileSensorType::RelativeAltitude:
			return Availability.bRelativeAltitude;
		case EOpenMobileSensorType::AbsoluteAltitude:
			return Availability.bAbsoluteAltitude;
		case EOpenMobileSensorType::StepCounter:
		case EOpenMobileSensorType::StepDetector:
			return Availability.bStepCounting;
		case EOpenMobileSensorType::MotionActivity:
			return Availability.bMotionActivity;
		case EOpenMobileSensorType::Proximity:
			return Availability.bProximityApiSupported;
		default:
			return false;
		}
	}

	NSOperationQueue* MakeSerialQueue(NSString* Name)
	{
		NSOperationQueue* Queue = [[NSOperationQueue alloc] init];
		Queue.name = Name;
		Queue.maxConcurrentOperationCount = 1;
		Queue.qualityOfService = NSQualityOfServiceUserInitiated;
		return Queue;
	}

	double MonotonicTimestampForDate(NSDate* Date)
	{
		const double NowSeconds = FPlatformTime::Seconds();
		if (!Date)
		{
			return NowSeconds;
		}
		const double AgeSeconds = FMath::Max(
			0.0,
			-[Date timeIntervalSinceNow]
		);
		return FMath::Max(0.0, NowSeconds - AgeSeconds);
	}
}

class FOpenMobileSensorsIOSBridge::FImpl final
{
public:
	explicit FImpl(FOpenMobileSensorsIOSBackend& InBackend)
		: Backend(InBackend)
	{
		MotionManager = [[CMMotionManager alloc] init];
		if (@available(iOS 7.0, *))
		{
			ActivityManager = [[CMMotionActivityManager alloc] init];
		}
		if (@available(iOS 8.0, *))
		{
			Altimeter = [[CMAltimeter alloc] init];
			Pedometer = [[CMPedometer alloc] init];
		}
		PedometerCallbackGate =
			MakeShared<OpenMobileSensorsIOSBridgePrivate::
				FPedometerCallbackGate, ESPMode::ThreadSafe>();
		PedometerCallbackGate->Owner = this;
		Availability =
			OpenMobileSensorsIOSBridgePrivate::AvailabilityForManager(
				MotionManager
			);
		LifecycleQueue =
			OpenMobileSensorsIOSBridgePrivate::MakeSerialQueue(
				@"OpenMobileSensorsLifecycleQueue"
			);
		bApplicationActive = [UIApplication sharedApplication].applicationState
			== UIApplicationStateActive;
		FImpl* Self = this;
		NSOperationQueue* Queue = LifecycleQueue;
		WillResignObserver = [[NSNotificationCenter defaultCenter]
			addObserverForName:UIApplicationWillResignActiveNotification
			object:nil
			queue:nil
			usingBlock:^(NSNotification*)
			{
				[Queue addOperationWithBlock:^
				{
					Self->SetApplicationActive(false);
				}];
			}];
		DidBecomeActiveObserver = [[NSNotificationCenter defaultCenter]
			addObserverForName:UIApplicationDidBecomeActiveNotification
			object:nil
			queue:nil
			usingBlock:^(NSNotification*)
			{
				[Queue addOperationWithBlock:^
				{
					Self->SetApplicationActive(true);
				}];
			}];
	}

	~FImpl()
	{
		Shutdown();
	}

	FOpenMobileSensorsIOSAvailability QueryAvailability() const
	{
		return Availability;
	}

	FOpenMobileSensorsIOSBridgeResult StartStream(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorPhysicalStreamRequest& Request
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (Token.Generation == 0
			|| !Handle.IsValid()
			|| !Request.Sensor.IsValid()
			|| !FMath::IsFinite(Request.RequestedFrequencyHz)
			|| Request.RequestedFrequencyHz <= 0.0)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::InvalidArgument};
		}
		if (Request.Sensor.Type != EOpenMobileSensorType::Proximity
			&& !HasMotionUsageDescription())
		{
			return {
				EOpenMobileSensorsIOSBridgeFailure::MissingUsageDescription
			};
		}
		if (Request.Sensor.Type == EOpenMobileSensorType::StepCounter
			|| Request.Sensor.Type == EOpenMobileSensorType::StepDetector)
		{
			if (@available(iOS 11.0, *))
			{
				const CMAuthorizationStatus Status =
					[CMPedometer authorizationStatus];
				if (Status == CMAuthorizationStatusRestricted)
				{
					return {
						EOpenMobileSensorsIOSBridgeFailure::PermissionRestricted
					};
				}
				if (Status == CMAuthorizationStatusDenied)
				{
					return {
						EOpenMobileSensorsIOSBridgeFailure::PermissionDenied
					};
				}
			}
		}
		if (Request.Sensor.Type == EOpenMobileSensorType::MotionActivity)
		{
			if (@available(iOS 11.0, *))
			{
				const CMAuthorizationStatus Status =
					[CMMotionActivityManager authorizationStatus];
				if (Status == CMAuthorizationStatusRestricted)
				{
					return {
						EOpenMobileSensorsIOSBridgeFailure::PermissionRestricted
					};
				}
				if (Status == CMAuthorizationStatusDenied)
				{
					return {
						EOpenMobileSensorsIOSBridgeFailure::PermissionDenied
					};
				}
			}
		}
		FScopeLock Lock(&Mutex);
		if (bShuttingDown)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::ShuttingDown};
		}
		if (!bApplicationActive)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::Paused};
		}
		if (ActiveStreams.Contains(Handle.Identifier)
			|| !SupportsType(Availability, Request.Sensor.Type))
		{
			return {
				ActiveStreams.Contains(Handle.Identifier)
					? EOpenMobileSensorsIOSBridgeFailure::InvalidArgument
					: EOpenMobileSensorsIOSBridgeFailure::SensorUnavailable
			};
		}
		const EService Service = ServiceForType(Request.Sensor.Type);
		if (Service == EService::Unknown || HasDuplicateTypeLocked(Request))
		{
			return {EOpenMobileSensorsIOSBridgeFailure::InvalidArgument};
		}
		const FOpenMobileSensorsIOSBridgeResult FrameValidation =
			ValidateDeviceMotionFrameLocked(Request, nullptr);
		if (!FrameValidation.IsSuccess())
		{
			return FrameValidation;
		}
		FActiveStream Active;
		Active.Token = Token;
		Active.Handle = Handle;
		Active.Request = Request;
		ActiveStreams.Add(Handle.Identifier, MoveTemp(Active));
		FOpenMobileSensorsIOSBridgeResult Result =
			RestartServiceLocked(Service);
		if (!Result.IsSuccess())
		{
			ActiveStreams.Remove(Handle.Identifier);
			RestartServiceLocked(Service);
			return Result;
		}
		return Result;
	}

	FOpenMobileSensorsIOSBridgeResult ReconfigureStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorPhysicalStreamRequest& Request
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (!Handle.IsValid()
			|| !FMath::IsFinite(Request.RequestedFrequencyHz)
			|| Request.RequestedFrequencyHz <= 0.0)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::InvalidArgument};
		}
		FScopeLock Lock(&Mutex);
		if (bShuttingDown)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::ShuttingDown};
		}
		if (!bApplicationActive)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::Paused};
		}
		FActiveStream* Active = ActiveStreams.Find(Handle.Identifier);
		if (!Active)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::StreamMissing};
		}
		if (Active->Request.Sensor != Request.Sensor)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::InvalidArgument};
		}
		const FOpenMobileSensorsIOSBridgeResult FrameValidation =
			ValidateDeviceMotionFrameLocked(Request, &Handle.Identifier);
		if (!FrameValidation.IsSuccess())
		{
			return FrameValidation;
		}
		const FOpenMobileSensorPhysicalStreamRequest Previous = Active->Request;
		Active->Request = Request;
		const EService Service = ServiceForType(Request.Sensor.Type);
		if (Service == EService::Pedometer)
		{
			return {};
		}
		if (Service == EService::Proximity)
		{
			Active->bResetNextSample = true;
			Active->RegistrationGeneration = ProximityGeneration;
			FOpenMobileSensorsIOSBridgeResult Result;
			Result.AppliedFrequencyHz = Request.RequestedFrequencyHz;
			return Result;
		}
		FOpenMobileSensorsIOSBridgeResult Result =
			RestartServiceLocked(Service);
		if (!Result.IsSuccess())
		{
			Active->Request = Previous;
			RestartServiceLocked(Service);
		}
		return Result;
	}

	FOpenMobileSensorsIOSBridgeResult StopStream(
		const FOpenMobileSensorBackendStreamHandle& Handle
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		FScopeLock Lock(&Mutex);
		FActiveStream* Active = ActiveStreams.Find(Handle.Identifier);
		if (!Active)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::StreamMissing};
		}
		const EService Service = ServiceForType(Active->Request.Sensor.Type);
		ActiveStreams.Remove(Handle.Identifier);
		return RestartServiceLocked(Service);
	}

	FOpenMobileSensorsIOSBridgeResult QueryNativeStepCount(
		const FGuid& RequestId,
		const FOpenMobileNativeStepCountQuery& Query,
		FOnOpenMobileNativeStepCountBackendQueryComplete&& Completion
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (!RequestId.IsValid()
			|| !Completion.IsBound()
			|| !FMath::IsFinite(Query.StartUnixTimeSeconds)
			|| !FMath::IsFinite(Query.EndUnixTimeSeconds)
			|| Query.StartUnixTimeSeconds < 0.0
			|| Query.EndUnixTimeSeconds <= Query.StartUnixTimeSeconds)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::InvalidArgument};
		}
		if (!HasMotionUsageDescription())
		{
			return {
				EOpenMobileSensorsIOSBridgeFailure::MissingUsageDescription
			};
		}
		if (@available(iOS 11.0, *))
		{
			const CMAuthorizationStatus Status =
				[CMPedometer authorizationStatus];
			if (Status == CMAuthorizationStatusRestricted)
			{
				return {
					EOpenMobileSensorsIOSBridgeFailure::PermissionRestricted
				};
			}
			if (Status == CMAuthorizationStatusDenied)
			{
				return {
					EOpenMobileSensorsIOSBridgeFailure::PermissionDenied
				};
			}
		}
		FScopeLock Lock(&Mutex);
		if (bShuttingDown)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::ShuttingDown};
		}
		if (!bApplicationActive)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::Paused};
		}
		if (!Availability.bPedometerApiSupported
			|| !Availability.bStepCounting
			|| !Pedometer)
		{
			return {EOpenMobileSensorsIOSBridgeFailure::SensorUnavailable};
		}
		if (PendingStepQueries.Contains(RequestId))
		{
			return {EOpenMobileSensorsIOSBridgeFailure::InvalidArgument};
		}
		FPendingStepQuery Pending;
		Pending.Query = Query;
		Pending.Completion = MoveTemp(Completion);
		PendingStepQueries.Add(RequestId, MoveTemp(Pending));
		const TSharedPtr<FPedometerCallbackGate, ESPMode::ThreadSafe> Gate =
			PedometerCallbackGate;
		NSDate* StartDate = [NSDate
			dateWithTimeIntervalSince1970:Query.StartUnixTimeSeconds];
		NSDate* EndDate = [NSDate
			dateWithTimeIntervalSince1970:Query.EndUnixTimeSeconds];
		[Pedometer
			queryPedometerDataFromDate:StartDate
			toDate:EndDate
			withHandler:^(CMPedometerData* Data, NSError* Error)
			{
				if (!Gate)
				{
					return;
				}
				FScopeLock GateLock(&Gate->Mutex);
				if (Gate->Owner)
				{
					static_cast<FImpl*>(Gate->Owner)->
						HandleHistoricalStepQuery(RequestId, Data, Error);
				}
			}];
		return {};
	}

	bool CancelNativeStepCountQuery(const FGuid& RequestId)
	{
		FScopeLock Lock(&Mutex);
		return PendingStepQueries.Remove(RequestId) > 0;
	}

	void Shutdown()
	{
		TArray<NSOperationQueue*> Queues;
		TArray<FOnOpenMobileNativeStepCountBackendQueryComplete>
			PendingStepCompletions;
		id LocalWillResignObserver = nil;
		id LocalDidBecomeActiveObserver = nil;
		{
			FScopeLock Lock(&Mutex);
			if (bShuttingDown)
			{
				return;
			}
			bShuttingDown = true;
			PendingStepCompletions.Reserve(PendingStepQueries.Num());
			for (TPair<FGuid, FPendingStepQuery>& Pair : PendingStepQueries)
			{
				PendingStepCompletions.Add(MoveTemp(Pair.Value.Completion));
			}
			PendingStepQueries.Reset();
			LocalWillResignObserver = WillResignObserver;
			LocalDidBecomeActiveObserver = DidBecomeActiveObserver;
			WillResignObserver = nil;
			DidBecomeActiveObserver = nil;
		}
		OpenMobileSensorsIOSBridgePrivate::RunOnMainQueue(
			[LocalWillResignObserver, LocalDidBecomeActiveObserver]()
			{
				if (LocalWillResignObserver)
				{
					[[NSNotificationCenter defaultCenter]
						removeObserver:LocalWillResignObserver];
				}
				if (LocalDidBecomeActiveObserver)
				{
					[[NSNotificationCenter defaultCenter]
						removeObserver:LocalDidBecomeActiveObserver];
				}
			}
		);
		{
			FScopeLock Lock(&Mutex);
			bApplicationActive = false;
			StopAllServicesLocked();
			ActiveStreams.Reset();
			ApplyProximityMonitoringActionLocked(
				ProximityMonitoringPolicy.Shutdown()
			);
			Queues = {
				AccelerometerQueue,
				GyroscopeQueue,
				MagnetometerQueue,
				DeviceMotionQueue,
				AltimeterQueue,
				AbsoluteAltitudeQueue,
				ActivityQueue,
				ProximityQueue,
				LifecycleQueue
			};
		}
		if (PedometerCallbackGate)
		{
			FScopeLock GateLock(&PedometerCallbackGate->Mutex);
			PedometerCallbackGate->Owner = nullptr;
		}
		const FOpenMobileSensorOperationResult ShutdownResult =
			Backend.MapBridgeFailure(
				EOpenMobileSensorsIOSBridgeFailure::ShuttingDown);
		const FOpenMobileStepsSensorSample EmptyStepSample;
		for (FOnOpenMobileNativeStepCountBackendQueryComplete& Completion
			: PendingStepCompletions)
		{
			Completion.ExecuteIfBound(ShutdownResult, EmptyStepSample);
		}
		for (NSOperationQueue* Queue : Queues)
		{
			if (Queue)
			{
				[Queue cancelAllOperations];
				[Queue waitUntilAllOperationsAreFinished];
			}
		}
		MotionManager = nil;
		ActivityManager = nil;
		Altimeter = nil;
		Pedometer = nil;
		AccelerometerQueue = nil;
		GyroscopeQueue = nil;
		MagnetometerQueue = nil;
		DeviceMotionQueue = nil;
		AltimeterQueue = nil;
		AbsoluteAltitudeQueue = nil;
		ActivityQueue = nil;
		ProximityQueue = nil;
		LifecycleQueue = nil;
	}

private:
	struct FActiveStream
	{
		FOpenMobileSensorsBackendToken Token;
		FOpenMobileSensorBackendStreamHandle Handle;
		FOpenMobileSensorPhysicalStreamRequest Request;
		uint64 RegistrationGeneration = 0;
		FGuid NativeStepOriginIdentifier;
		double QueryStartUnixTimeSeconds = 0.0;
		bool bResetNextSample = false;
	};

	struct FPendingStepQuery
	{
		FOpenMobileNativeStepCountQuery Query;
		FOnOpenMobileNativeStepCountBackendQueryComplete Completion;
	};

	bool HasMotionUsageDescription() const
	{
		id Value = [[NSBundle mainBundle]
			objectForInfoDictionaryKey:@"NSMotionUsageDescription"];
		return [Value isKindOfClass:[NSString class]]
			&& [(NSString*)Value stringByTrimmingCharactersInSet:
				[NSCharacterSet whitespaceAndNewlineCharacterSet]].length > 0;
	}

	bool HasDuplicateTypeLocked(
		const FOpenMobileSensorPhysicalStreamRequest& Request
	) const
	{
		for (const TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
		{
			if (Pair.Value.Request.Sensor == Request.Sensor
				&& Request.Sensor.Type != EOpenMobileSensorType::Proximity)
			{
				return true;
			}
		}
		return false;
	}

	FOpenMobileSensorsIOSBridgeResult ValidateDeviceMotionFrameLocked(
		const FOpenMobileSensorPhysicalStreamRequest& Request,
		const FGuid* IgnoredHandle
	) const
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (ServiceForType(Request.Sensor.Type) != EService::DeviceMotion)
		{
			return {};
		}
		const int32 RequestedFrame = RequiredReferenceFrame(Request);
		if (RequestedFrame < 0)
		{
			return {
				EOpenMobileSensorsIOSBridgeFailure::ReferenceFrameUnavailable
			};
		}
		for (const TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
		{
			if ((IgnoredHandle && Pair.Key == *IgnoredHandle)
				|| ServiceForType(Pair.Value.Request.Sensor.Type)
					!= EService::DeviceMotion)
			{
				continue;
			}
			const int32 ExistingFrame =
				RequiredReferenceFrame(Pair.Value.Request);
			if (RequestedFrame != 0
				&& ExistingFrame != 0
				&& RequestedFrame != ExistingFrame)
			{
				return {
					EOpenMobileSensorsIOSBridgeFailure::
						ReferenceFrameUnavailable
				};
			}
		}
		return {};
	}

	int32 RequiredReferenceFrame(
		const FOpenMobileSensorPhysicalStreamRequest& Request
	) const
	{
		if (Request.Sensor.Type == EOpenMobileSensorType::MagneticHeading)
		{
			return Availability.bMagneticNorthReference ? 2 : -1;
		}
		if (Request.Sensor.Type != EOpenMobileSensorType::Attitude)
		{
			return 0;
		}
		switch (Request.AttitudeReferenceFrame)
		{
		case EOpenMobileAttitudeReferenceFrame::GameRelative:
		case EOpenMobileAttitudeReferenceFrame::ArbitraryVertical:
			return 1;
		case EOpenMobileAttitudeReferenceFrame::MagneticNorth:
			return Availability.bMagneticNorthReference ? 2 : -1;
		case EOpenMobileAttitudeReferenceFrame::TrueNorth:
		default:
			return -1;
		}
	}

	CMAttitudeReferenceFrame ResolveDeviceMotionFrameLocked() const
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		for (const TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
		{
			if (ServiceForType(Pair.Value.Request.Sensor.Type)
					== EService::DeviceMotion
				&& RequiredReferenceFrame(Pair.Value.Request) == 2)
			{
				return CMAttitudeReferenceFrameXMagneticNorthZVertical;
			}
		}
		return CMAttitudeReferenceFrameXArbitraryZVertical;
	}

	bool HasServiceStreamsLocked(
		OpenMobileSensorsIOSBridgePrivate::EService Service
	) const
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		for (const TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
		{
			if (ServiceForType(Pair.Value.Request.Sensor.Type) == Service)
			{
				return true;
			}
		}
		return false;
	}

	int32 CountServiceStreamsLocked(
		OpenMobileSensorsIOSBridgePrivate::EService Service
	) const
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		int32 Count = 0;
		for (const TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
		{
			if (ServiceForType(Pair.Value.Request.Sensor.Type) == Service)
			{
				++Count;
			}
		}
		return Count;
	}

	double ResolveIntervalLocked(
		OpenMobileSensorsIOSBridgePrivate::EService Service
	) const
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		double HighestFrequency = 1.0;
		for (const TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
		{
			if (ServiceForType(Pair.Value.Request.Sensor.Type) == Service)
			{
				HighestFrequency = FMath::Max(
					HighestFrequency,
					Pair.Value.Request.RequestedFrequencyHz
				);
			}
		}
		return FMath::Clamp(1.0 / HighestFrequency, 0.001, 1.0);
	}

	void SetRegistrationGenerationLocked(
		OpenMobileSensorsIOSBridgePrivate::EService Service,
		uint64 Generation
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		for (TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
		{
			if (ServiceForType(Pair.Value.Request.Sensor.Type) == Service)
			{
				Pair.Value.RegistrationGeneration = Generation;
				Pair.Value.bResetNextSample = true;
			}
		}
	}

	bool ReadProximityMonitoringEnabledLocked() const
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		bool bEnabled = false;
		RunOnMainQueue([&bEnabled]()
		{
			bEnabled = [UIDevice currentDevice].proximityMonitoringEnabled;
		});
		return bEnabled;
	}

	bool ApplyProximityMonitoringActionLocked(
		const FOpenMobileProximityMonitoringAction& Action
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		bool bEnabled = false;
		RunOnMainQueue([&Action, &bEnabled]()
		{
			UIDevice* Device = [UIDevice currentDevice];
			if (Action.bShouldSetMonitoringEnabled)
			{
				Device.proximityMonitoringEnabled =
					Action.bMonitoringEnabled;
			}
			bEnabled = Device.proximityMonitoringEnabled;
		});
		ProximityMonitoringPolicy.ObserveMonitoringEnabled(bEnabled);
		return bEnabled;
	}

	void SetProximityObserverLocked(bool bEnabled)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (bEnabled && !ProximityObserver)
		{
			ProximityQueue = ProximityQueue
				? ProximityQueue
				: MakeSerialQueue(@"OpenMobileSensorsProximityQueue");
			const uint64 Generation = ProximityGeneration;
			FImpl* Self = this;
			NSOperationQueue* Queue = ProximityQueue;
			RunOnMainQueue([this, Self, Queue, Generation]()
			{
				ProximityObserver = [[NSNotificationCenter defaultCenter]
					addObserverForName:
						UIDeviceProximityStateDidChangeNotification
					object:[UIDevice currentDevice]
					queue:[NSOperationQueue mainQueue]
					usingBlock:^(NSNotification*)
					{
						const bool bNear =
							[UIDevice currentDevice].proximityState;
						const double TimestampSeconds =
							FPlatformTime::Seconds();
						[Queue addOperationWithBlock:^
						{
							Self->HandleProximityStateChange(
								Generation,
								bNear,
								TimestampSeconds
							);
						}];
					}];
			});
			return;
		}
		if (!bEnabled && ProximityObserver)
		{
			id Observer = ProximityObserver;
			ProximityObserver = nil;
			RunOnMainQueue([Observer]()
			{
				[[NSNotificationCenter defaultCenter]
					removeObserver:Observer];
			});
			[ProximityQueue cancelAllOperations];
		}
	}

	void PublishProximityState(
		const FActiveStream& Active,
		bool bNear,
		double TimestampSeconds
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		FOpenMobileProximitySensorSample Sample;
		Sample.Header = MakeHeader(
			Active.Request.Sensor,
			TimestampSeconds,
			Active.bResetNextSample
		);
		Sample.bNear = bNear;
		FOpenMobileProximitySensorBatch Batch;
		Batch.Samples.Reserve(MaximumCallbackBatchSamples);
		Batch.Samples.Add(MoveTemp(Sample));
		Backend.PublishProximityBatchFromProximityQueue(
			Active.Token,
			Active.Handle,
			Batch
		);
	}

	void HandleProximityStateChange(
		uint64 Generation,
		bool bNear,
		double TimestampSeconds
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		const TArray<FActiveStream> Streams =
			TakeStreamsForCallback(EService::Proximity, Generation);
		for (const FActiveStream& Active : Streams)
		{
			PublishProximityState(Active, bNear, TimestampSeconds);
		}
	}

	void ScheduleInitialProximitySamplesLocked()
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		TArray<FActiveStream> Pending;
		for (TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
		{
			FActiveStream& Active = Pair.Value;
			if (ServiceForType(Active.Request.Sensor.Type) != EService::Proximity
				|| Active.RegistrationGeneration == ProximityGeneration)
			{
				continue;
			}
			Active.RegistrationGeneration = ProximityGeneration;
			Active.bResetNextSample = true;
			Pending.Add(Active);
			Active.bResetNextSample = false;
		}
		if (Pending.IsEmpty())
		{
			return;
		}
		bool bNear = false;
		RunOnMainQueue([&bNear]()
		{
			bNear = [UIDevice currentDevice].proximityState;
		});
		const double TimestampSeconds = FPlatformTime::Seconds();
		FImpl* Self = this;
		[ProximityQueue addOperationWithBlock:^
		{
			for (const FActiveStream& Active : Pending)
			{
				Self->PublishProximityState(
					Active,
					bNear,
					TimestampSeconds
				);
			}
		}];
	}

	FOpenMobileSensorsIOSBridgeResult ReconcileProximityServiceLocked()
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		const int32 DesiredLeaseCount =
			CountServiceStreamsLocked(EService::Proximity);
		const bool bShouldRun = DesiredLeaseCount > 0
			&& bApplicationActive
			&& !bShuttingDown;
		if (!bShouldRun && bProximityServiceActive)
		{
			++ProximityGeneration;
			SetProximityObserverLocked(false);
			bProximityServiceActive = false;
		}
		while (ProximityMonitoringPolicy.GetLeaseCount() < DesiredLeaseCount)
		{
			const bool bCurrentEnabled =
				ReadProximityMonitoringEnabledLocked();
			ApplyProximityMonitoringActionLocked(
				ProximityMonitoringPolicy.Acquire(
					bCurrentEnabled,
					bApplicationActive
				)
			);
		}
		while (ProximityMonitoringPolicy.GetLeaseCount() > DesiredLeaseCount)
		{
			ApplyProximityMonitoringActionLocked(
				ProximityMonitoringPolicy.Release()
			);
		}
		const bool bMonitoringEnabled =
			ApplyProximityMonitoringActionLocked(
				ProximityMonitoringPolicy.SetApplicationActive(
					bApplicationActive
				)
			);
		if (!bShouldRun)
		{
			return {};
		}
		if (!bMonitoringEnabled)
		{
			return {
				EOpenMobileSensorsIOSBridgeFailure::SensorUnavailable
			};
		}
		if (!bProximityServiceActive)
		{
			++ProximityGeneration;
			for (TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
			{
				if (ServiceForType(Pair.Value.Request.Sensor.Type) ==
					EService::Proximity)
				{
					Pair.Value.RegistrationGeneration = 0;
				}
			}
			SetProximityObserverLocked(true);
			bProximityServiceActive = true;
		}
		ScheduleInitialProximitySamplesLocked();
		return SuccessWithAppliedInterval(
			ResolveIntervalLocked(EService::Proximity)
		);
	}

	FOpenMobileSensorsIOSBridgeResult RestartServiceLocked(
		OpenMobileSensorsIOSBridgePrivate::EService Service
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (Service == EService::Proximity)
		{
			return ReconcileProximityServiceLocked();
		}
		StopServiceLocked(Service);
		if (!HasServiceStreamsLocked(Service)
			|| !bApplicationActive
			|| bShuttingDown)
		{
			return {};
		}
		const double Interval = ResolveIntervalLocked(Service);
		switch (Service)
		{
		case EService::Accelerometer:
		{
			AccelerometerQueue = AccelerometerQueue
				? AccelerometerQueue
				: MakeSerialQueue(@"OpenMobileSensorsAccelerometerQueue");
			const uint64 Generation = ++AccelerometerGeneration;
			SetRegistrationGenerationLocked(Service, Generation);
			MotionManager.accelerometerUpdateInterval = Interval;
			FImpl* Self = this;
			[MotionManager
				startAccelerometerUpdatesToQueue:AccelerometerQueue
				withHandler:^(CMAccelerometerData* Data, NSError* Error)
				{
					Self->HandleAccelerometer(Generation, Data, Error);
				}];
			return SuccessWithAppliedInterval(
				MotionManager.accelerometerUpdateInterval
			);
		}
		case EService::Gyroscope:
		{
			GyroscopeQueue = GyroscopeQueue
				? GyroscopeQueue
				: MakeSerialQueue(@"OpenMobileSensorsGyroscopeQueue");
			const uint64 Generation = ++GyroscopeGeneration;
			SetRegistrationGenerationLocked(Service, Generation);
			MotionManager.gyroUpdateInterval = Interval;
			FImpl* Self = this;
			[MotionManager
				startGyroUpdatesToQueue:GyroscopeQueue
				withHandler:^(CMGyroData* Data, NSError* Error)
				{
					Self->HandleGyroscope(Generation, Data, Error);
				}];
			return SuccessWithAppliedInterval(MotionManager.gyroUpdateInterval);
		}
		case EService::Magnetometer:
		{
			MagnetometerQueue = MagnetometerQueue
				? MagnetometerQueue
				: MakeSerialQueue(@"OpenMobileSensorsMagnetometerQueue");
			const uint64 Generation = ++MagnetometerGeneration;
			SetRegistrationGenerationLocked(Service, Generation);
			MotionManager.magnetometerUpdateInterval = Interval;
			FImpl* Self = this;
			[MotionManager
				startMagnetometerUpdatesToQueue:MagnetometerQueue
				withHandler:^(CMMagnetometerData* Data, NSError* Error)
				{
					Self->HandleMagnetometer(Generation, Data, Error);
				}];
			return SuccessWithAppliedInterval(
				MotionManager.magnetometerUpdateInterval
			);
		}
		case EService::DeviceMotion:
		{
			DeviceMotionQueue = DeviceMotionQueue
				? DeviceMotionQueue
				: MakeSerialQueue(@"OpenMobileSensorsDeviceMotionQueue");
			const uint64 Generation = ++DeviceMotionGeneration;
			SetRegistrationGenerationLocked(Service, Generation);
			MotionManager.deviceMotionUpdateInterval = Interval;
			const CMAttitudeReferenceFrame Frame =
				ResolveDeviceMotionFrameLocked();
			FImpl* Self = this;
			[MotionManager
				startDeviceMotionUpdatesUsingReferenceFrame:Frame
				toQueue:DeviceMotionQueue
				withHandler:^(CMDeviceMotion* Motion, NSError* Error)
				{
					Self->HandleDeviceMotion(Generation, Motion, Error);
				}];
			return SuccessWithAppliedInterval(
				MotionManager.deviceMotionUpdateInterval
			);
		}
		case EService::RelativeAltitude:
		{
			AltimeterQueue = AltimeterQueue
				? AltimeterQueue
				: MakeSerialQueue(@"OpenMobileSensorsAltimeterQueue");
			const uint64 Generation = ++AltimeterGeneration;
			SetRegistrationGenerationLocked(Service, Generation);
			FImpl* Self = this;
			[Altimeter
				startRelativeAltitudeUpdatesToQueue:AltimeterQueue
				withHandler:^(CMAltitudeData* Data, NSError* Error)
				{
					Self->HandleRelativeAltitude(Generation, Data, Error);
				}];
			return SuccessWithAppliedInterval(1.0);
		}
		case EService::AbsoluteAltitude:
		{
			if (@available(iOS 15.0, *))
			{
				AbsoluteAltitudeQueue = AbsoluteAltitudeQueue
					? AbsoluteAltitudeQueue
					: MakeSerialQueue(@"OpenMobileSensorsAbsoluteAltitudeQueue");
				const uint64 Generation = ++AbsoluteAltitudeGeneration;
				SetRegistrationGenerationLocked(Service, Generation);
				FImpl* Self = this;
				[Altimeter
					startAbsoluteAltitudeUpdatesToQueue:AbsoluteAltitudeQueue
					withHandler:^(CMAbsoluteAltitudeData* Data, NSError* Error)
					{
						Self->HandleAbsoluteAltitude(Generation, Data, Error);
					}];
				return SuccessWithAppliedInterval(Interval);
			}
			return {
				EOpenMobileSensorsIOSBridgeFailure::SensorUnavailable
			};
		}
		case EService::Pedometer:
		{
			if (@available(iOS 8.0, *))
			{
				const uint64 Generation = ++PedometerGeneration;
				SetRegistrationGenerationLocked(Service, Generation);
				NSDate* QueryStartDate = [NSDate date];
				const double QueryStartUnixTimeSeconds =
					QueryStartDate.timeIntervalSince1970;
				for (TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
				{
					FActiveStream& Active = Pair.Value;
					if (ServiceForType(Active.Request.Sensor.Type) == Service)
					{
						Active.NativeStepOriginIdentifier = FGuid::NewGuid();
						Active.QueryStartUnixTimeSeconds =
							QueryStartUnixTimeSeconds;
					}
				}
				FImpl* Self = this;
				const TSharedPtr<FPedometerCallbackGate, ESPMode::ThreadSafe>
					Gate = PedometerCallbackGate;
				[Pedometer
					startPedometerUpdatesFromDate:QueryStartDate
					withHandler:^(CMPedometerData* Data, NSError* Error)
					{
						FScopeLock GateLock(&Gate->Mutex);
						if (Gate->Owner == Self)
						{
							Self->HandlePedometer(
								Generation,
								Data,
								Error
							);
						}
					}];
				return {};
			}
			return {
				EOpenMobileSensorsIOSBridgeFailure::SensorUnavailable
			};
		}
		case EService::MotionActivity:
		{
			if (@available(iOS 7.0, *))
			{
				ActivityQueue = ActivityQueue
					? ActivityQueue
					: MakeSerialQueue(
						@"OpenMobileSensorsMotionActivityQueue"
					);
				const uint64 Generation = ++MotionActivityGeneration;
				SetRegistrationGenerationLocked(Service, Generation);
				FImpl* Self = this;
				[ActivityManager
					startActivityUpdatesToQueue:ActivityQueue
					withHandler:^(CMMotionActivity* Activity)
					{
						Self->HandleMotionActivity(Generation, Activity);
					}];
				return {};
			}
			return {
				EOpenMobileSensorsIOSBridgeFailure::SensorUnavailable
			};
		}
		case EService::Unknown:
		default:
			return {EOpenMobileSensorsIOSBridgeFailure::InvalidArgument};
		}
	}

	FOpenMobileSensorsIOSBridgeResult SuccessWithAppliedInterval(
		double Interval
	) const
	{
		FOpenMobileSensorsIOSBridgeResult Result;
		Result.AppliedFrequencyHz = FMath::IsFinite(Interval) && Interval > 0.0
			? 1.0 / Interval
			: 0.0;
		return Result;
	}

	void StopServiceLocked(
		OpenMobileSensorsIOSBridgePrivate::EService Service
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		switch (Service)
		{
		case EService::Accelerometer:
			++AccelerometerGeneration;
			[MotionManager stopAccelerometerUpdates];
			[AccelerometerQueue cancelAllOperations];
			break;
		case EService::Gyroscope:
			++GyroscopeGeneration;
			[MotionManager stopGyroUpdates];
			[GyroscopeQueue cancelAllOperations];
			break;
		case EService::Magnetometer:
			++MagnetometerGeneration;
			[MotionManager stopMagnetometerUpdates];
			[MagnetometerQueue cancelAllOperations];
			break;
		case EService::DeviceMotion:
			++DeviceMotionGeneration;
			[MotionManager stopDeviceMotionUpdates];
			[DeviceMotionQueue cancelAllOperations];
			break;
		case EService::RelativeAltitude:
			++AltimeterGeneration;
			[Altimeter stopRelativeAltitudeUpdates];
			[AltimeterQueue cancelAllOperations];
			break;
		case EService::AbsoluteAltitude:
			++AbsoluteAltitudeGeneration;
			if (@available(iOS 15.0, *))
			{
				[Altimeter stopAbsoluteAltitudeUpdates];
			}
			[AbsoluteAltitudeQueue cancelAllOperations];
			break;
		case EService::Pedometer:
			++PedometerGeneration;
			if (@available(iOS 8.0, *))
			{
				[Pedometer stopPedometerUpdates];
			}
			break;
		case EService::MotionActivity:
			++MotionActivityGeneration;
			if (@available(iOS 7.0, *))
			{
				[ActivityManager stopActivityUpdates];
			}
			[ActivityQueue cancelAllOperations];
			break;
		case EService::Proximity:
			++ProximityGeneration;
			SetProximityObserverLocked(false);
			bProximityServiceActive = false;
			ApplyProximityMonitoringActionLocked(
				ProximityMonitoringPolicy.SetApplicationActive(false)
			);
			break;
		case EService::Unknown:
		default:
			break;
		}
	}

	void StopAllServicesLocked()
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		StopServiceLocked(EService::Accelerometer);
		StopServiceLocked(EService::Gyroscope);
		StopServiceLocked(EService::Magnetometer);
		StopServiceLocked(EService::DeviceMotion);
		StopServiceLocked(EService::RelativeAltitude);
		StopServiceLocked(EService::AbsoluteAltitude);
		StopServiceLocked(EService::Pedometer);
		StopServiceLocked(EService::MotionActivity);
		StopServiceLocked(EService::Proximity);
	}

	void RestartAllServicesLocked()
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		RestartServiceLocked(EService::Accelerometer);
		RestartServiceLocked(EService::Gyroscope);
		RestartServiceLocked(EService::Magnetometer);
		RestartServiceLocked(EService::DeviceMotion);
		RestartServiceLocked(EService::RelativeAltitude);
		RestartServiceLocked(EService::AbsoluteAltitude);
		RestartServiceLocked(EService::Pedometer);
		RestartServiceLocked(EService::MotionActivity);
		RestartServiceLocked(EService::Proximity);
	}

	void SetApplicationActive(bool bActive)
	{
		TArray<FActiveStream> PermissionFailures;
		TArray<FActiveStream> RestrictedFailures;
		{
			FScopeLock Lock(&Mutex);
			if (bShuttingDown || bApplicationActive == bActive)
			{
				return;
			}
			bApplicationActive = bActive;
			if (!bActive)
			{
				StopAllServicesLocked();
				return;
			}
			if (@available(iOS 11.0, *))
			{
				const CMAuthorizationStatus AltimeterStatus =
					[CMAltimeter authorizationStatus];
				const CMAuthorizationStatus PedometerStatus =
					[CMPedometer authorizationStatus];
				const CMAuthorizationStatus MotionActivityStatus =
					[CMMotionActivityManager authorizationStatus];
				for (auto Iterator = ActiveStreams.CreateIterator();
					Iterator;
					++Iterator)
				{
					using namespace OpenMobileSensorsIOSBridgePrivate;
					const EService Service = ServiceForType(
						Iterator.Value().Request.Sensor.Type
					);
					const bool bAltimeter =
						Service == EService::RelativeAltitude
						|| Service == EService::AbsoluteAltitude;
					const bool bPedometer = Service == EService::Pedometer;
					const bool bMotionActivity =
						Service == EService::MotionActivity;
					const CMAuthorizationStatus Status = bPedometer
						? PedometerStatus
						: bMotionActivity
							? MotionActivityStatus
							: AltimeterStatus;
					if ((bAltimeter || bPedometer || bMotionActivity)
						&& (Status == CMAuthorizationStatusDenied
							|| Status == CMAuthorizationStatusRestricted))
					{
						if (Status == CMAuthorizationStatusRestricted)
						{
							RestrictedFailures.Add(Iterator.Value());
						}
						else
						{
							PermissionFailures.Add(Iterator.Value());
						}
						Iterator.RemoveCurrent();
					}
				}
			}
			RestartAllServicesLocked();
		}
		for (const FActiveStream& Active : PermissionFailures)
		{
			Backend.FailPhysicalStreamFromBackend(
				Active.Token,
				Active.Handle,
				EOpenMobileSensorsIOSBridgeFailure::PermissionDenied,
				TEXT("CMErrorDomain"),
				TEXT("CMErrorNotAuthorized")
			);
		}
		for (const FActiveStream& Active : RestrictedFailures)
		{
			Backend.FailPhysicalStreamFromBackend(
				Active.Token,
				Active.Handle,
				EOpenMobileSensorsIOSBridgeFailure::PermissionRestricted,
				TEXT("CMErrorDomain"),
				TEXT("CMErrorNotAuthorized")
			);
		}
	}

	uint64 CurrentGenerationLocked(
		OpenMobileSensorsIOSBridgePrivate::EService Service
	) const
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		switch (Service)
		{
		case EService::Accelerometer:
			return AccelerometerGeneration;
		case EService::Gyroscope:
			return GyroscopeGeneration;
		case EService::Magnetometer:
			return MagnetometerGeneration;
		case EService::DeviceMotion:
			return DeviceMotionGeneration;
		case EService::RelativeAltitude:
			return AltimeterGeneration;
		case EService::AbsoluteAltitude:
			return AbsoluteAltitudeGeneration;
		case EService::Pedometer:
			return PedometerGeneration;
		case EService::MotionActivity:
			return MotionActivityGeneration;
		case EService::Proximity:
			return ProximityGeneration;
		case EService::Unknown:
		default:
			return 0;
		}
	}

	TArray<FActiveStream> TakeStreamsForCallback(
		OpenMobileSensorsIOSBridgePrivate::EService Service,
		uint64 RegistrationGeneration
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		TArray<FActiveStream> Result;
		FScopeLock Lock(&Mutex);
		if (bShuttingDown
			|| !bApplicationActive
			|| CurrentGenerationLocked(Service) != RegistrationGeneration)
		{
			return Result;
		}
		for (TPair<FGuid, FActiveStream>& Pair : ActiveStreams)
		{
			FActiveStream& Active = Pair.Value;
			if (ServiceForType(Active.Request.Sensor.Type) == Service
				&& Active.RegistrationGeneration == RegistrationGeneration
				&& FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
					Active.Token
				))
			{
				Result.Add(Active);
				Active.bResetNextSample = false;
			}
		}
		return Result;
	}

	void FailServiceFromCallback(
		OpenMobileSensorsIOSBridgePrivate::EService Service,
		uint64 RegistrationGeneration,
		NSError* Error
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (IsTransientMovementError(Error))
		{
			return;
		}
		TArray<FActiveStream> Failed;
		{
			FScopeLock Lock(&Mutex);
			if (bShuttingDown
				|| CurrentGenerationLocked(Service) != RegistrationGeneration)
			{
				return;
			}
			for (auto Iterator = ActiveStreams.CreateIterator(); Iterator; ++Iterator)
			{
				if (ServiceForType(Iterator.Value().Request.Sensor.Type)
					== Service)
				{
					Failed.Add(Iterator.Value());
					Iterator.RemoveCurrent();
				}
			}
			StopServiceLocked(Service);
		}
		const EOpenMobileSensorsIOSBridgeFailure Failure =
			FailureFromError(Error);
		const FString Domain = FromNSString(Error.domain);
		const FString Code = Error
			? FString::Printf(TEXT("%lld"), static_cast<int64>(Error.code))
			: FString{};
		for (const FActiveStream& Active : Failed)
		{
			Backend.FailPhysicalStreamFromBackend(
				Active.Token,
				Active.Handle,
				Failure,
				Domain,
				Code
			);
		}
	}

	void HandleAccelerometer(
		uint64 Generation,
		CMAccelerometerData* Data,
		NSError* Error
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (Error)
		{
			FailServiceFromCallback(EService::Accelerometer, Generation, Error);
			return;
		}
		if (!Data)
		{
			return;
		}
		const TArray<FActiveStream> Streams =
			TakeStreamsForCallback(EService::Accelerometer, Generation);
		for (const FActiveStream& Active : Streams)
		{
			FOpenMobileVectorSensorSample Sample;
			Sample.Header = MakeHeader(
				Active.Request.Sensor,
				Data.timestamp,
				Active.bResetNextSample
			);
			Sample.Value = FVector(
				Data.acceleration.x,
				Data.acceleration.y,
				Data.acceleration.z
			);
			FOpenMobileVectorSensorBatch Batch;
			Batch.Samples.Reserve(MaximumCallbackBatchSamples);
			Batch.Samples.Add(MoveTemp(Sample));
			Backend.PublishVectorBatchFromMotionQueue(
				Active.Token,
				Active.Handle,
				Batch
			);
		}
	}

	void HandleGyroscope(uint64 Generation, CMGyroData* Data, NSError* Error)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (Error)
		{
			FailServiceFromCallback(EService::Gyroscope, Generation, Error);
			return;
		}
		if (!Data)
		{
			return;
		}
		const TArray<FActiveStream> Streams =
			TakeStreamsForCallback(EService::Gyroscope, Generation);
		for (const FActiveStream& Active : Streams)
		{
			FOpenMobileVectorSensorSample Sample;
			Sample.Header = MakeHeader(
				Active.Request.Sensor,
				Data.timestamp,
				Active.bResetNextSample
			);
			Sample.Value = FVector(
				Data.rotationRate.x,
				Data.rotationRate.y,
				Data.rotationRate.z
			);
			FOpenMobileVectorSensorBatch Batch;
			Batch.Samples.Reserve(MaximumCallbackBatchSamples);
			Batch.Samples.Add(MoveTemp(Sample));
			Backend.PublishVectorBatchFromMotionQueue(
				Active.Token,
				Active.Handle,
				Batch
			);
		}
	}

	void HandleMagnetometer(
		uint64 Generation,
		CMMagnetometerData* Data,
		NSError* Error
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (Error)
		{
			FailServiceFromCallback(EService::Magnetometer, Generation, Error);
			return;
		}
		if (!Data)
		{
			return;
		}
		const TArray<FActiveStream> Streams =
			TakeStreamsForCallback(EService::Magnetometer, Generation);
		for (const FActiveStream& Active : Streams)
		{
			FOpenMobileVectorSensorSample Sample;
			Sample.Header = MakeHeader(
				Active.Request.Sensor,
				Data.timestamp,
				Active.bResetNextSample
			);
			Sample.Value = FVector(
				Data.magneticField.x,
				Data.magneticField.y,
				Data.magneticField.z
			);
			FOpenMobileVectorSensorBatch Batch;
			Batch.Samples.Reserve(MaximumCallbackBatchSamples);
			Batch.Samples.Add(MoveTemp(Sample));
			Backend.PublishVectorBatchFromMotionQueue(
				Active.Token,
				Active.Handle,
				Batch
			);
		}
	}

	void HandleDeviceMotion(
		uint64 Generation,
		CMDeviceMotion* Motion,
		NSError* Error
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (Error)
		{
			FailServiceFromCallback(EService::DeviceMotion, Generation, Error);
			return;
		}
		if (!Motion)
		{
			return;
		}
		const TArray<FActiveStream> Streams =
			TakeStreamsForCallback(EService::DeviceMotion, Generation);
		for (const FActiveStream& Active : Streams)
		{
			const EOpenMobileSensorType Type = Active.Request.Sensor.Type;
			if (Type == EOpenMobileSensorType::Gravity
				|| Type == EOpenMobileSensorType::LinearAcceleration
				|| Type == EOpenMobileSensorType::Magnetometer)
			{
				FOpenMobileVectorSensorSample Sample;
				int32 SourceFlags = 0;
				if (Type == EOpenMobileSensorType::Gravity)
				{
					Sample.Value = FVector(
						Motion.gravity.x,
						Motion.gravity.y,
						Motion.gravity.z
					);
				}
				else if (Type == EOpenMobileSensorType::LinearAcceleration)
				{
					Sample.Value = FVector(
						Motion.userAcceleration.x,
						Motion.userAcceleration.y,
						Motion.userAcceleration.z
					);
				}
				else
				{
					Sample.Value = FVector(
						Motion.magneticField.field.x,
						Motion.magneticField.field.y,
						Motion.magneticField.field.z
					);
					SourceFlags = static_cast<int32>(
						EOpenMobileSensorSourceFlags::CalibratedNative
					);
				}
				Sample.Header = MakeHeader(
					Active.Request.Sensor,
					Motion.timestamp,
					Active.bResetNextSample,
					SourceFlags
				);
				FOpenMobileVectorSensorBatch Batch;
				Batch.Samples.Reserve(MaximumCallbackBatchSamples);
				Batch.Samples.Add(MoveTemp(Sample));
				if (Type == EOpenMobileSensorType::Magnetometer)
				{
					Backend.PublishMagneticFieldAccuracyFromMotionQueue(
						Active.Token,
						Active.Handle,
						Active.Request.Sensor,
						static_cast<int32>(Motion.magneticField.accuracy),
						Motion.timestamp
					);
				}
				Backend.PublishVectorBatchFromMotionQueue(
					Active.Token,
					Active.Handle,
					Batch
				);
				continue;
			}
			if (Type == EOpenMobileSensorType::Attitude)
			{
				const CMQuaternion Quaternion = Motion.attitude.quaternion;
				FOpenMobileAttitudeSensorSample Sample;
				Sample.Header = MakeHeader(
					Active.Request.Sensor,
					Motion.timestamp,
					Active.bResetNextSample
				);
				Sample.Quaternion = FQuat(
					Quaternion.x,
					Quaternion.y,
					Quaternion.z,
					Quaternion.w
				);
				Sample.ReferenceFrame =
					Active.Request.AttitudeReferenceFrame;
				FOpenMobileAttitudeSensorBatch Batch;
				Batch.Samples.Reserve(MaximumCallbackBatchSamples);
				Batch.Samples.Add(MoveTemp(Sample));
				Backend.PublishAttitudeBatchFromMotionQueue(
					Active.Token,
					Active.Handle,
					Batch
				);
				continue;
			}
			if (Type == EOpenMobileSensorType::MagneticHeading)
			{
				FOpenMobileHeadingSensorSample Sample;
				Sample.Header = MakeHeader(
					Active.Request.Sensor,
					Motion.timestamp,
					Active.bResetNextSample
				);
				if (@available(iOS 11.0, *))
				{
					Sample.HeadingDegrees = Motion.heading;
				}
				else
				{
					Sample.HeadingDegrees = -1.0;
					Sample.Header.bValid = false;
				}
				Sample.Reference =
					EOpenMobileHeadingReference::MagneticNorth;
				Sample.bTiltCompensated = true;
				Backend.PublishMagneticFieldAccuracyFromMotionQueue(
					Active.Token,
					Active.Handle,
					Active.Request.Sensor,
					static_cast<int32>(Motion.magneticField.accuracy),
					Motion.timestamp
				);
				FOpenMobileHeadingSensorBatch Batch;
				Batch.Samples.Reserve(MaximumCallbackBatchSamples);
				Batch.Samples.Add(MoveTemp(Sample));
				Backend.PublishHeadingBatchFromMotionQueue(
					Active.Token,
					Active.Handle,
					Batch
				);
			}
		}
	}

	void HandleRelativeAltitude(
		uint64 Generation,
		CMAltitudeData* Data,
		NSError* Error
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (Error)
		{
			FailServiceFromCallback(EService::RelativeAltitude, Generation, Error);
			return;
		}
		if (!Data)
		{
			return;
		}
		const TArray<FActiveStream> Streams =
			TakeStreamsForCallback(EService::RelativeAltitude, Generation);
		for (const FActiveStream& Active : Streams)
		{
			FOpenMobileScalarSensorSample Sample;
			Sample.Header = MakeHeader(
				Active.Request.Sensor,
				Data.timestamp,
				Active.bResetNextSample
			);
			Sample.Value = Active.Request.Sensor.Type ==
				EOpenMobileSensorType::BarometricPressure
				? Data.pressure.doubleValue
				: Data.relativeAltitude.doubleValue;
			FOpenMobileScalarSensorBatch Batch;
			Batch.Samples.Reserve(MaximumCallbackBatchSamples);
			Batch.Samples.Add(MoveTemp(Sample));
			Backend.PublishScalarBatchFromMotionQueue(
				Active.Token,
				Active.Handle,
				Batch
			);
		}
	}

	void HandleAbsoluteAltitude(
		uint64 Generation,
		CMAbsoluteAltitudeData* Data,
		NSError* Error
	) API_AVAILABLE(ios(15.0))
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (Error)
		{
			FailServiceFromCallback(EService::AbsoluteAltitude, Generation, Error);
			return;
		}
		if (!Data)
		{
			return;
		}
		const TArray<FActiveStream> Streams =
			TakeStreamsForCallback(EService::AbsoluteAltitude, Generation);
		for (const FActiveStream& Active : Streams)
		{
			FOpenMobileScalarSensorSample Sample;
			Sample.Header = MakeHeader(
				Active.Request.Sensor,
				Data.timestamp,
				Active.bResetNextSample
			);
			Sample.Value = Data.altitude;
			Sample.AbsoluteAltitude.Source =
				EOpenMobileAbsoluteAltitudeSource::NativePlatform;
			Sample.AbsoluteAltitude.bHasVerticalAccuracy =
				FMath::IsFinite(Data.accuracy)
				&& Data.accuracy >= 0.0;
			Sample.AbsoluteAltitude.VerticalAccuracyMeters =
				Sample.AbsoluteAltitude.bHasVerticalAccuracy
					? Data.accuracy
					: 0.0;
			FOpenMobileScalarSensorBatch Batch;
			Batch.Samples.Reserve(MaximumCallbackBatchSamples);
			Batch.Samples.Add(MoveTemp(Sample));
			Backend.PublishScalarBatchFromMotionQueue(
				Active.Token,
				Active.Handle,
				Batch
			);
		}
	}

	void HandlePedometer(
		uint64 Generation,
		CMPedometerData* Data,
		NSError* Error
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (Error)
		{
			FailServiceFromCallback(EService::Pedometer, Generation, Error);
			return;
		}
		if (!Data)
		{
			return;
		}
		const TArray<FActiveStream> Streams =
			TakeStreamsForCallback(EService::Pedometer, Generation);
		const double QueryStartUnixTimeSeconds = Data.startDate
			? Data.startDate.timeIntervalSince1970
			: 0.0;
		const double QueryEndUnixTimeSeconds = Data.endDate
			? Data.endDate.timeIntervalSince1970
			: 0.0;
		for (const FActiveStream& Active : Streams)
		{
			FOpenMobileStepsSensorSample Sample;
			Sample.Header = MakeHeader(
				Active.Request.Sensor,
				MonotonicTimestampForDate(Data.endDate),
				Active.bResetNextSample
			);
			Sample.Header.bValid &= Data.numberOfSteps != nil;
			Sample.Count = Data.numberOfSteps
				? Data.numberOfSteps.longLongValue
				: -1;
			Sample.Origin = EOpenMobileStepCountOrigin::QueryInterval;
			Sample.OriginIdentifier = Active.NativeStepOriginIdentifier;
			Sample.bHasQueryInterval = true;
			Sample.QueryStartUnixTimeSeconds = Data.startDate
				? QueryStartUnixTimeSeconds
				: Active.QueryStartUnixTimeSeconds;
			Sample.QueryEndUnixTimeSeconds = QueryEndUnixTimeSeconds;
			PopulatePedometerMetrics(Data, Sample);
			FOpenMobileStepsSensorBatch Batch;
			Batch.Samples.Reserve(MaximumCallbackBatchSamples);
			Batch.Samples.Add(MoveTemp(Sample));
			Backend.PublishStepsBatchFromPedometerQueue(
				Active.Token,
				Active.Handle,
				Batch
			);
		}
	}

	void HandleMotionActivity(
		uint64 Generation,
		CMMotionActivity* Activity
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		if (!Activity)
		{
			return;
		}
		const TArray<FActiveStream> Streams =
			TakeStreamsForCallback(EService::MotionActivity, Generation);
		FOpenMobileNativeMotionActivityState Native;
		Native.bUnknown = Activity.unknown;
		Native.bStationary = Activity.stationary;
		Native.bWalking = Activity.walking;
		Native.bRunning = Activity.running;
		if (@available(iOS 8.0, *))
		{
			Native.bCycling = Activity.cycling;
		}
		Native.bAutomotive = Activity.automotive;
		Native.Confidence = static_cast<int32>(Activity.confidence);
		for (const FActiveStream& Active : Streams)
		{
			FOpenMobileActivitySensorSample Sample;
			Sample.Header = MakeHeader(
				Active.Request.Sensor,
				MonotonicTimestampForDate(Activity.startDate),
				Active.bResetNextSample
			);
			FOpenMobileMotionActivityClassifier::Classify(Native, Sample);
			Sample.ActivityProvider = TEXT("CoreMotion");
			FOpenMobileActivitySensorBatch Batch;
			Batch.Samples.Reserve(MaximumCallbackBatchSamples);
			Batch.Samples.Add(MoveTemp(Sample));
			Backend.PublishActivityBatchFromMotionQueue(
				Active.Token,
				Active.Handle,
				Batch
			);
		}
	}

	void HandleHistoricalStepQuery(
		const FGuid& RequestId,
		CMPedometerData* Data,
		NSError* Error
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
		FPendingStepQuery Pending;
		{
			FScopeLock Lock(&Mutex);
			if (!PendingStepQueries.RemoveAndCopyValue(RequestId, Pending))
			{
				return;
			}
		}
		FOpenMobileSensorOperationResult Operation;
		FOpenMobileStepsSensorSample Sample;
		if (Error)
		{
			Operation = Backend.MapBridgeFailure(
				FailureFromError(Error),
				FromNSString(Error.domain),
				FString::Printf(
					TEXT("%lld"),
					static_cast<long long>(Error.code)));
		}
		else if (!Data
			|| !Data.startDate
			|| !Data.endDate
			|| !Data.numberOfSteps)
		{
			Operation = Backend.MapBridgeFailure(
				EOpenMobileSensorsIOSBridgeFailure::ManagerError,
				TEXT("CoreMotion"),
				TEXT("NilPedometerData"));
		}
		else
		{
			Operation.Code = EOpenMobileSensorResultCode::Success;
			FOpenMobileSensorIdentifier Sensor;
			Sensor.Type = EOpenMobileSensorType::StepCounter;
			Sensor.InstanceId = TEXT("Default");
			Sample.Header = MakeHeader(
				Sensor,
				MonotonicTimestampForDate(Data.endDate),
				false);
			Sample.Count = Data.numberOfSteps.longLongValue;
			Sample.Origin = EOpenMobileStepCountOrigin::QueryInterval;
			Sample.OriginIdentifier = RequestId;
			Sample.bHasQueryInterval = true;
			Sample.QueryStartUnixTimeSeconds =
				Data.startDate.timeIntervalSince1970;
			Sample.QueryEndUnixTimeSeconds =
				Data.endDate.timeIntervalSince1970;
			PopulatePedometerMetrics(Data, Sample);
		}
		Pending.Completion.ExecuteIfBound(Operation, Sample);
	}

	FOpenMobileSensorsIOSBackend& Backend;
	FCriticalSection Mutex;
	TMap<FGuid, FActiveStream> ActiveStreams;
	TMap<FGuid, FPendingStepQuery> PendingStepQueries;
	FOpenMobileSensorsIOSAvailability Availability;
	__strong CMMotionManager* MotionManager = nil;
	__strong CMMotionActivityManager* ActivityManager = nil;
	__strong CMAltimeter* Altimeter = nil;
	__strong CMPedometer* Pedometer = nil;
	__strong NSOperationQueue* AccelerometerQueue = nil;
	__strong NSOperationQueue* GyroscopeQueue = nil;
	__strong NSOperationQueue* MagnetometerQueue = nil;
	__strong NSOperationQueue* DeviceMotionQueue = nil;
	__strong NSOperationQueue* AltimeterQueue = nil;
	__strong NSOperationQueue* AbsoluteAltitudeQueue = nil;
	__strong NSOperationQueue* ActivityQueue = nil;
	__strong NSOperationQueue* ProximityQueue = nil;
	__strong NSOperationQueue* LifecycleQueue = nil;
	__strong id WillResignObserver = nil;
	__strong id DidBecomeActiveObserver = nil;
	__strong id ProximityObserver = nil;
	FOpenMobileProximityMonitoringPolicy ProximityMonitoringPolicy;
	TSharedPtr<
		OpenMobileSensorsIOSBridgePrivate::FPedometerCallbackGate,
		ESPMode::ThreadSafe
	> PedometerCallbackGate;
	uint64 AccelerometerGeneration = 0;
	uint64 GyroscopeGeneration = 0;
	uint64 MagnetometerGeneration = 0;
	uint64 DeviceMotionGeneration = 0;
	uint64 AltimeterGeneration = 0;
	uint64 AbsoluteAltitudeGeneration = 0;
	uint64 PedometerGeneration = 0;
	uint64 MotionActivityGeneration = 0;
	uint64 ProximityGeneration = 0;
	bool bApplicationActive = false;
	bool bProximityServiceActive = false;
	bool bShuttingDown = false;
};

FOpenMobileSensorsIOSBridge::FOpenMobileSensorsIOSBridge(
	FOpenMobileSensorsIOSBackend& InBackend
)
	: Impl(MakeUnique<FImpl>(InBackend))
{
}

FOpenMobileSensorsIOSBridge::~FOpenMobileSensorsIOSBridge()
{
	Shutdown();
}

FOpenMobileSensorsIOSAvailability
FOpenMobileSensorsIOSBridge::QuerySystemAvailability()
{
	CMMotionManager* Manager = [[CMMotionManager alloc] init];
	return OpenMobileSensorsIOSBridgePrivate::AvailabilityForManager(Manager);
}

FOpenMobileSensorsIOSAvailability
FOpenMobileSensorsIOSBridge::QueryAvailability() const
{
	return Impl ? Impl->QueryAvailability() : FOpenMobileSensorsIOSAvailability{};
}

FOpenMobileSensorsIOSBridgeResult FOpenMobileSensorsIOSBridge::StartStream(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorPhysicalStreamRequest& Request
)
{
	return Impl
		? Impl->StartStream(Token, Handle, Request)
		: FOpenMobileSensorsIOSBridgeResult{
			EOpenMobileSensorsIOSBridgeFailure::ShuttingDown
		};
}

FOpenMobileSensorsIOSBridgeResult
FOpenMobileSensorsIOSBridge::ReconfigureStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorPhysicalStreamRequest& Request
)
{
	return Impl
		? Impl->ReconfigureStream(Handle, Request)
		: FOpenMobileSensorsIOSBridgeResult{
			EOpenMobileSensorsIOSBridgeFailure::ShuttingDown
		};
}

FOpenMobileSensorsIOSBridgeResult FOpenMobileSensorsIOSBridge::StopStream(
	const FOpenMobileSensorBackendStreamHandle& Handle
)
{
	return Impl
		? Impl->StopStream(Handle)
		: FOpenMobileSensorsIOSBridgeResult{
			EOpenMobileSensorsIOSBridgeFailure::ShuttingDown
		};
}

FOpenMobileSensorsIOSBridgeResult
FOpenMobileSensorsIOSBridge::QueryNativeStepCount(
	const FGuid& RequestId,
	const FOpenMobileNativeStepCountQuery& Query,
	FOnOpenMobileNativeStepCountBackendQueryComplete&& Completion
)
{
	return Impl
		? Impl->QueryNativeStepCount(
			RequestId,
			Query,
			MoveTemp(Completion))
		: FOpenMobileSensorsIOSBridgeResult{
			EOpenMobileSensorsIOSBridgeFailure::ShuttingDown
		};
}

bool FOpenMobileSensorsIOSBridge::CancelNativeStepCountQuery(
	const FGuid& RequestId
)
{
	return Impl && Impl->CancelNativeStepCountQuery(RequestId);
}

void FOpenMobileSensorsIOSBridge::Shutdown()
{
	if (Impl)
	{
		Impl->Shutdown();
		Impl.Reset();
	}
}
