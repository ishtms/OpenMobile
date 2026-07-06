#include "OpenMobileSensorsIOSBridge.h"

#import <CoreMotion/CoreMotion.h>
#import <UIKit/UIKit.h>

#include "Misc/ScopeLock.h"
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
		AbsoluteAltitude
	};

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
				|| Error.code == CMErrorMotionActivityNotAvailable)
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
}

class FOpenMobileSensorsIOSBridge::FImpl final
{
public:
	explicit FImpl(FOpenMobileSensorsIOSBackend& InBackend)
		: Backend(InBackend)
	{
		MotionManager = [[CMMotionManager alloc] init];
		if (@available(iOS 8.0, *))
		{
			Altimeter = [[CMAltimeter alloc] init];
		}
		Availability =
			OpenMobileSensorsIOSBridgePrivate::AvailabilityForManager(
				MotionManager
			);
		bApplicationActive = [UIApplication sharedApplication].applicationState
			== UIApplicationStateActive;
		FImpl* Self = this;
		WillResignObserver = [[NSNotificationCenter defaultCenter]
			addObserverForName:UIApplicationWillResignActiveNotification
			object:nil
			queue:nil
			usingBlock:^(NSNotification*)
			{
				Self->SetApplicationActive(false);
			}];
		DidBecomeActiveObserver = [[NSNotificationCenter defaultCenter]
			addObserverForName:UIApplicationDidBecomeActiveNotification
			object:nil
			queue:nil
			usingBlock:^(NSNotification*)
			{
				Self->SetApplicationActive(true);
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
		if (!HasMotionUsageDescription())
		{
			return {
				EOpenMobileSensorsIOSBridgeFailure::MissingUsageDescription
			};
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

	void Shutdown()
	{
		TArray<NSOperationQueue*> Queues;
		id LocalWillResignObserver = nil;
		id LocalDidBecomeActiveObserver = nil;
		{
			FScopeLock Lock(&Mutex);
			if (bShuttingDown)
			{
				return;
			}
			bShuttingDown = true;
			bApplicationActive = false;
			StopAllServicesLocked();
			ActiveStreams.Reset();
			Queues = {
				AccelerometerQueue,
				GyroscopeQueue,
				MagnetometerQueue,
				DeviceMotionQueue,
				AltimeterQueue,
				AbsoluteAltitudeQueue
			};
			LocalWillResignObserver = WillResignObserver;
			LocalDidBecomeActiveObserver = DidBecomeActiveObserver;
			WillResignObserver = nil;
			DidBecomeActiveObserver = nil;
		}
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
		for (NSOperationQueue* Queue : Queues)
		{
			if (Queue)
			{
				[Queue cancelAllOperations];
				[Queue waitUntilAllOperationsAreFinished];
			}
		}
		MotionManager = nil;
		Altimeter = nil;
		AccelerometerQueue = nil;
		GyroscopeQueue = nil;
		MagnetometerQueue = nil;
		DeviceMotionQueue = nil;
		AltimeterQueue = nil;
		AbsoluteAltitudeQueue = nil;
	}

private:
	struct FActiveStream
	{
		FOpenMobileSensorsBackendToken Token;
		FOpenMobileSensorBackendStreamHandle Handle;
		FOpenMobileSensorPhysicalStreamRequest Request;
		uint64 RegistrationGeneration = 0;
		bool bResetNextSample = false;
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
			if (Pair.Value.Request.Sensor == Request.Sensor)
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

	FOpenMobileSensorsIOSBridgeResult RestartServiceLocked(
		OpenMobileSensorsIOSBridgePrivate::EService Service
	)
	{
		using namespace OpenMobileSensorsIOSBridgePrivate;
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
	}

	void SetApplicationActive(bool bActive)
	{
		TArray<FActiveStream> PermissionFailures;
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
				const CMAuthorizationStatus Status =
					[CMAltimeter authorizationStatus];
				if (Status == CMAuthorizationStatusDenied
					|| Status == CMAuthorizationStatusRestricted)
				{
					for (auto Iterator = ActiveStreams.CreateIterator();
						Iterator;
						++Iterator)
					{
						using namespace OpenMobileSensorsIOSBridgePrivate;
						const EService Service = ServiceForType(
							Iterator.Value().Request.Sensor.Type
						);
						if (Service == EService::RelativeAltitude
							|| Service == EService::AbsoluteAltitude)
						{
							PermissionFailures.Add(Iterator.Value());
							Iterator.RemoveCurrent();
						}
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

	FOpenMobileSensorsIOSBackend& Backend;
	FCriticalSection Mutex;
	TMap<FGuid, FActiveStream> ActiveStreams;
	FOpenMobileSensorsIOSAvailability Availability;
	__strong CMMotionManager* MotionManager = nil;
	__strong CMAltimeter* Altimeter = nil;
	__strong NSOperationQueue* AccelerometerQueue = nil;
	__strong NSOperationQueue* GyroscopeQueue = nil;
	__strong NSOperationQueue* MagnetometerQueue = nil;
	__strong NSOperationQueue* DeviceMotionQueue = nil;
	__strong NSOperationQueue* AltimeterQueue = nil;
	__strong NSOperationQueue* AbsoluteAltitudeQueue = nil;
	__strong id WillResignObserver = nil;
	__strong id DidBecomeActiveObserver = nil;
	uint64 AccelerometerGeneration = 0;
	uint64 GyroscopeGeneration = 0;
	uint64 MagnetometerGeneration = 0;
	uint64 DeviceMotionGeneration = 0;
	uint64 AltimeterGeneration = 0;
	uint64 AbsoluteAltitudeGeneration = 0;
	bool bApplicationActive = false;
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

void FOpenMobileSensorsIOSBridge::Shutdown()
{
	if (Impl)
	{
		Impl->Shutdown();
		Impl.Reset();
	}
}
