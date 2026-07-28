#include "OpenMobileSensorListener.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubsystem.h"

void UOpenMobileSensorListener::ConfigureListener(
	const UObject* WorldContextObject,
	UObject* ListenerOwner,
	EOpenMobileSensorType Sensor,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	bool bUseAdvancedOptions
)
{
	ActivationWorldContext = const_cast<UObject*>(WorldContextObject);
	LifetimeOwner = ListenerOwner ? ListenerOwner : ActivationWorldContext.Get();
	RequestedSensor.Type = Sensor;
	RequestedSensor.InstanceId = TEXT("Default");
	RequestedOptions = bUseAdvancedOptions
		? AdvancedOptions
		: GetDefault<UOpenMobileSensorsSettings>()->DefaultStreamOptions;
	RequestedOptions.RatePreset = RatePreset;
	RequestedOptions.CoordinateSpace = CoordinateSpace;
	RequestedOptions.DeliveryMode =
		EOpenMobileSensorDeliveryMode::EventBatches;
}

void UOpenMobileSensorListener::Activate()
{
	UWorld* World = GEngine && ActivationWorldContext
		? GEngine->GetWorldFromContextObject(
			ActivationWorldContext,
			EGetWorldErrorMode::ReturnNull
		)
		: nullptr;
	if (!InitializeAction(World ? World : ActivationWorldContext.Get()))
	{
		return;
	}
	ActivationWorldContext = nullptr;
	BoundSubsystem = GetSensorsSubsystem();
	BindEvents();
	OwnerTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UOpenMobileSensorListener::TickOwner
		)
	);

	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor = RequestedSensor;
	Request.Options = RequestedOptions;
	const FOpenMobileSensorSubscriptionResult Result =
		GetSensorsSubsystem()->StartSubscriptionNative(Request);
	LastOperation = Result.Operation;
	Handle = Result.Handle;
	AppliedOptions = Result.AppliedOptions;
	RateResolution = Result.RateResolution;
	if (!Result.Operation.IsSuccess() || !Handle.IsValid())
	{
		FinishFromOperation(Result.Operation);
		return;
	}
	CachedState = EOpenMobileSensorSubscriptionState::Accepted;
	FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
	if (GetSensorsSubsystem()->GetSubscriptionStateNative(Handle, Snapshot)
		&& Snapshot.State != EOpenMobileSensorSubscriptionState::Accepted)
	{
		HandleStateChanged(Snapshot);
	}
}

void UOpenMobileSensorListener::Stop()
{
	if (IsFinished())
	{
		return;
	}
	CancelNativeOperation();
	if (!IsFinished())
	{
		FinishSucceeded();
	}
}

bool UOpenMobileSensorListener::IsActive() const
{
	return !IsFinished()
		&& CachedState == EOpenMobileSensorSubscriptionState::Active;
}

EOpenMobileSensorSubscriptionState
UOpenMobileSensorListener::GetListenerState() const
{
	return CachedState;
}

FOpenMobileSensorIdentifier UOpenMobileSensorListener::GetSensor() const
{
	return RequestedSensor;
}

FOpenMobileSensorStreamOptions
UOpenMobileSensorListener::GetAppliedOptions() const
{
	return AppliedOptions;
}

bool UOpenMobileSensorListener::GetLastSampleDrop(
	FOpenMobileSensorDropInfo& OutDropInfo
) const
{
	OutDropInfo = LastDropInfo;
	return bHasDropInfo;
}

FOpenMobileSensorSampleInfo UOpenMobileSensorListener::MakeSampleInfo(
	const FOpenMobileSensorSampleHeader& Header
)
{
	FOpenMobileSensorSampleInfo Info;
	Info.TimestampMonotonicSeconds = Header.TimestampSeconds;
	const double DeliverySeconds = Header.bHasGameThreadReceiptTime
		? Header.GameThreadReceiptSeconds
		: FPlatformTime::Seconds();
	Info.AgeSeconds = FMath::Max(
		0.0,
		DeliverySeconds - Header.TimestampSeconds
	);
	Info.Sequence = Header.Sequence;
	Info.Accuracy = Header.Accuracy;
	Info.SourceFlags = Header.SourceFlags;
	Info.bValid = Header.bValid;
	return Info;
}

void UOpenMobileSensorListener::HandleVectorSample(
	const FOpenMobileVectorSensorSample& Sample
)
{
	static_cast<void>(Sample);
}

void UOpenMobileSensorListener::CancelNativeOperation()
{
	if (!Handle.IsValid())
	{
		return;
	}
	UOpenMobileSensorsSubsystem* Subsystem = BoundSubsystem.Get();
	const FOpenMobileSensorSubscriptionHandle HandleToStop = Handle;
	Handle.Reset();
	if (Subsystem)
	{
		const FOpenMobileSensorOperationResult Result =
			Subsystem->StopSubscriptionNative(HandleToStop);
		if (!Result.IsSuccess() && !IsFinished())
		{
			LastOperation = Result;
			FinishFromOperation(Result);
		}
	}
}

void UOpenMobileSensorListener::OnActionSucceeded()
{
	CachedState = EOpenMobileSensorSubscriptionState::Stopped;
	UnbindEvents();
	Stopped.Broadcast(this);
}

void UOpenMobileSensorListener::OnActionFailed(
	const FOpenMobileError& Error
)
{
	if (!LastOperation.Error.IsSet())
	{
		LastOperation.Error = Error;
	}
	CachedState = EOpenMobileSensorSubscriptionState::Failed;
	UnbindEvents();
	BroadcastFailure();
}

void UOpenMobileSensorListener::OnActionCancelled(
	const FOpenMobileError& Error
)
{
	static_cast<void>(Error);
	CachedState = EOpenMobileSensorSubscriptionState::Stopped;
	UnbindEvents();
	Stopped.Broadcast(this);
}

void UOpenMobileSensorListener::BindEvents()
{
	UOpenMobileSensorsSubsystem* Subsystem = BoundSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}
	StateChangedHandle = Subsystem->OnSubscriptionStateChangedNative().AddUObject(
		this,
		&UOpenMobileSensorListener::HandleStateChanged
	);
	SamplesDroppedHandle = Subsystem->OnSamplesDroppedNative().AddUObject(
		this,
		&UOpenMobileSensorListener::HandleSamplesDropped
	);
	if (FOpenMobileSensorTypes::GetSampleFamily(RequestedSensor.Type) ==
		EOpenMobileSensorSampleFamily::Vector)
	{
		SampleHandle = Subsystem->OnVectorSamplesNative().AddUObject(
			this,
			&UOpenMobileSensorListener::HandleVectorBatch
		);
	}
}

void UOpenMobileSensorListener::UnbindEvents()
{
	if (OwnerTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(OwnerTickerHandle);
		OwnerTickerHandle.Reset();
	}
	UOpenMobileSensorsSubsystem* Subsystem = BoundSubsystem.Get();
	if (Subsystem && StateChangedHandle.IsValid())
	{
		Subsystem->OnSubscriptionStateChangedNative().Remove(
			StateChangedHandle
		);
	}
	if (Subsystem && SampleHandle.IsValid())
	{
		Subsystem->OnVectorSamplesNative().Remove(SampleHandle);
	}
	if (Subsystem && SamplesDroppedHandle.IsValid())
	{
		Subsystem->OnSamplesDroppedNative().Remove(SamplesDroppedHandle);
	}
	StateChangedHandle.Reset();
	SampleHandle.Reset();
	SamplesDroppedHandle.Reset();
	BoundSubsystem.Reset();
}

bool UOpenMobileSensorListener::TickOwner(float DeltaSeconds)
{
	static_cast<void>(DeltaSeconds);
	if (IsFinished())
	{
		return false;
	}
	if (!LifetimeOwner.IsValid())
	{
		Stop();
		return false;
	}
	return true;
}

void UOpenMobileSensorListener::HandleStateChanged(
	const FOpenMobileSensorSubscriptionStateSnapshot& Snapshot
)
{
	if (IsFinished() || Snapshot.Handle != Handle)
	{
		return;
	}
	const EOpenMobileSensorSubscriptionState PreviousState = CachedState;
	CachedState = Snapshot.State;
	AppliedOptions = Snapshot.AppliedOptions;
	RateResolution = Snapshot.RateResolution;
	switch (Snapshot.State)
	{
	case EOpenMobileSensorSubscriptionState::Active:
		if (!bStartedBroadcast)
		{
			bStartedBroadcast = true;
			Started.Broadcast(
				this,
				RateResolution.AppliedNativeFrequencyHz,
				ResolveSource(),
				RateResolution.AdjustmentReason !=
					EOpenMobileSensorRateAdjustmentReason::None
			);
		}
		else if (PreviousState == EOpenMobileSensorSubscriptionState::Paused)
		{
			Resumed.Broadcast(this);
		}
		break;
	case EOpenMobileSensorSubscriptionState::Paused:
		Paused.Broadcast(this);
		break;
	case EOpenMobileSensorSubscriptionState::Failed:
		LastOperation.Code = EOpenMobileSensorResultCode::Failed;
		LastOperation.Error = Snapshot.Error;
		LastOperation.Failure = Snapshot.Failure;
		FinishFailed(Snapshot.Error);
		break;
	case EOpenMobileSensorSubscriptionState::Stopped:
		Handle.Reset();
		FinishSucceeded();
		break;
	default:
		break;
	}
}

void UOpenMobileSensorListener::HandleVectorBatch(
	FOpenMobileSensorSubscriptionHandle InHandle,
	const FOpenMobileVectorSensorBatch& Batch
)
{
	if (!IsFinished() && InHandle == Handle && !Batch.Samples.IsEmpty())
	{
		HandleVectorSample(Batch.Samples.Last());
	}
}

void UOpenMobileSensorListener::HandleSamplesDropped(
	FOpenMobileSensorSubscriptionHandle InHandle,
	const FOpenMobileSensorDropInfo& DropInfo
)
{
	if (IsFinished() || InHandle != Handle)
	{
		return;
	}
	LastDropInfo = DropInfo;
	bHasDropInfo = true;
	SamplesDropped.Broadcast(this, LastDropInfo);
}

void UOpenMobileSensorListener::FinishFromOperation(
	const FOpenMobileSensorOperationResult& Operation
)
{
	LastOperation = Operation;
	const FOpenMobileError Error = Operation.Error.IsSet()
		? Operation.Error
		: FOpenMobileSensorsErrorMapper::Map(
			Operation.Failure.Reason
		).Error;
	FinishFailed(Error);
}

void UOpenMobileSensorListener::BroadcastFailure()
{
	const FText Message = FText::FromString(LastOperation.Error.Message);
	const FText Correction =
		FText::FromString(LastOperation.Failure.Correction);
	switch (LastOperation.Failure.Reason)
	{
	case EOpenMobileSensorFailureReason::PermissionRequired:
	case EOpenMobileSensorFailureReason::PermissionDenied:
	case EOpenMobileSensorFailureReason::PermissionRestricted:
		PermissionRequired.Broadcast(
			this,
			Message,
			Correction,
			LastOperation
		);
		break;
	case EOpenMobileSensorFailureReason::UnsupportedPlatform:
	case EOpenMobileSensorFailureReason::UnsupportedOperation:
	case EOpenMobileSensorFailureReason::MissingHardware:
	case EOpenMobileSensorFailureReason::DerivedInputUnavailable:
	case EOpenMobileSensorFailureReason::MissingLocationInput:
	case EOpenMobileSensorFailureReason::StaleLocationInput:
	case EOpenMobileSensorFailureReason::PoorLocationAccuracy:
	case EOpenMobileSensorFailureReason::TemporarilyUnavailable:
	case EOpenMobileSensorFailureReason::ConfigurationBlocked:
		Unavailable.Broadcast(this, Message, Correction, LastOperation);
		break;
	default:
		Failed.Broadcast(this, Message, Correction, LastOperation);
		break;
	}
}

EOpenMobileSensorAvailabilitySource
UOpenMobileSensorListener::ResolveSource() const
{
	const UOpenMobileSensorsSubsystem* Subsystem = BoundSubsystem.Get();
	if (!Subsystem)
	{
		return EOpenMobileSensorAvailabilitySource::Unknown;
	}
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		Subsystem->GetCapabilitySnapshotNative();
	const FOpenMobileSensorCapability* Capability =
		Snapshot.Sensors.FindByPredicate(
			[this](const FOpenMobileSensorCapability& Candidate)
			{
				return Candidate.Sensor.Type == RequestedSensor.Type
					&& (RequestedSensor.InstanceId.IsNone()
						|| Candidate.Sensor.InstanceId ==
							RequestedSensor.InstanceId);
			}
		);
	return Capability
		? Capability->Source
		: EOpenMobileSensorAvailabilitySource::Unknown;
}

#if WITH_DEV_AUTOMATION_TESTS
bool UOpenMobileSensorListener::TickOwnerForTests()
{
	return TickOwner(0.0f);
}
#endif

UOpenMobileGyroscopeListener*
UOpenMobileGyroscopeListener::ListenForGyroscope(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner
)
{
	UOpenMobileGyroscopeListener* Listener =
		NewObject<UOpenMobileGyroscopeListener>();
	Listener->ConfigureListener(
		WorldContextObject,
		ListenerOwner,
		EOpenMobileSensorType::Gyroscope,
		AdvancedOptions,
		RatePreset,
		CoordinateSpace,
		bUseAdvancedOptions
	);
	return Listener;
}

bool UOpenMobileGyroscopeListener::GetLatestAngularVelocity(
	FVector& OutAngularVelocityRadiansPerSecond,
	FOpenMobileSensorSampleInfo& OutSampleInfo
) const
{
	OutAngularVelocityRadiansPerSecond = FVector::ZeroVector;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutAngularVelocityRadiansPerSecond = LatestSample.Value;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobileGyroscopeListener::HandleVectorSample(
	const FOpenMobileVectorSensorSample& InSample
)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Value, MakeSampleInfo(LatestSample.Header));
}

UOpenMobileAccelerometerListener*
UOpenMobileAccelerometerListener::ListenForAccelerometer(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner
)
{
	UOpenMobileAccelerometerListener* Listener =
		NewObject<UOpenMobileAccelerometerListener>();
	Listener->ConfigureListener(
		WorldContextObject,
		ListenerOwner,
		EOpenMobileSensorType::Accelerometer,
		AdvancedOptions,
		RatePreset,
		CoordinateSpace,
		bUseAdvancedOptions
	);
	return Listener;
}

bool UOpenMobileAccelerometerListener::GetLatestAcceleration(
	FVector& OutAccelerationMetresPerSecondSquared,
	FOpenMobileSensorSampleInfo& OutSampleInfo
) const
{
	OutAccelerationMetresPerSecondSquared = FVector::ZeroVector;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutAccelerationMetresPerSecondSquared = LatestSample.Value;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobileAccelerometerListener::HandleVectorSample(
	const FOpenMobileVectorSensorSample& InSample
)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Value, MakeSampleInfo(LatestSample.Header));
}

UOpenMobileMagnetometerListener*
UOpenMobileMagnetometerListener::ListenForMagnetometer(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner
)
{
	UOpenMobileMagnetometerListener* Listener =
		NewObject<UOpenMobileMagnetometerListener>();
	Listener->ConfigureListener(
		WorldContextObject,
		ListenerOwner,
		EOpenMobileSensorType::Magnetometer,
		AdvancedOptions,
		RatePreset,
		CoordinateSpace,
		bUseAdvancedOptions
	);
	return Listener;
}

bool UOpenMobileMagnetometerListener::GetLatestMagneticField(
	FVector& OutMagneticFieldMicroteslas,
	FOpenMobileSensorSampleInfo& OutSampleInfo
) const
{
	OutMagneticFieldMicroteslas = FVector::ZeroVector;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutMagneticFieldMicroteslas = LatestSample.Value;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobileMagnetometerListener::HandleVectorSample(
	const FOpenMobileVectorSensorSample& InSample
)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Value, MakeSampleInfo(LatestSample.Header));
}

UOpenMobileGravityListener* UOpenMobileGravityListener::ListenForGravity(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner
)
{
	UOpenMobileGravityListener* Listener =
		NewObject<UOpenMobileGravityListener>();
	Listener->ConfigureListener(
		WorldContextObject,
		ListenerOwner,
		EOpenMobileSensorType::Gravity,
		AdvancedOptions,
		RatePreset,
		CoordinateSpace,
		bUseAdvancedOptions
	);
	return Listener;
}

bool UOpenMobileGravityListener::GetLatestGravity(
	FVector& OutGravityMetresPerSecondSquared,
	FOpenMobileSensorSampleInfo& OutSampleInfo
) const
{
	OutGravityMetresPerSecondSquared = FVector::ZeroVector;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutGravityMetresPerSecondSquared = LatestSample.Value;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobileGravityListener::HandleVectorSample(
	const FOpenMobileVectorSensorSample& InSample
)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Value, MakeSampleInfo(LatestSample.Header));
}

UOpenMobileLinearAccelerationListener*
UOpenMobileLinearAccelerationListener::ListenForLinearAcceleration(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner
)
{
	UOpenMobileLinearAccelerationListener* Listener =
		NewObject<UOpenMobileLinearAccelerationListener>();
	Listener->ConfigureListener(
		WorldContextObject,
		ListenerOwner,
		EOpenMobileSensorType::LinearAcceleration,
		AdvancedOptions,
		RatePreset,
		CoordinateSpace,
		bUseAdvancedOptions
	);
	return Listener;
}

bool UOpenMobileLinearAccelerationListener::GetLatestLinearAcceleration(
	FVector& OutLinearAccelerationMetresPerSecondSquared,
	FOpenMobileSensorSampleInfo& OutSampleInfo
) const
{
	OutLinearAccelerationMetresPerSecondSquared = FVector::ZeroVector;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutLinearAccelerationMetresPerSecondSquared = LatestSample.Value;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobileLinearAccelerationListener::HandleVectorSample(
	const FOpenMobileVectorSensorSample& InSample
)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Value, MakeSampleInfo(LatestSample.Header));
}

UOpenMobileShakeListener* UOpenMobileShakeListener::ListenForShake(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner
)
{
	UOpenMobileShakeListener* Listener = NewObject<UOpenMobileShakeListener>();
	Listener->ConfigureListener(
		WorldContextObject,
		ListenerOwner,
		EOpenMobileSensorType::Shake,
		AdvancedOptions,
		RatePreset,
		EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bUseAdvancedOptions
	);
	return Listener;
}

bool UOpenMobileShakeListener::GetLatestShake(
	FOpenMobileShakeEventData& OutShake,
	FOpenMobileSensorSampleInfo& OutSampleInfo
) const
{
	OutShake = {};
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutShake = LatestSample.ShakeEvent;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobileShakeListener::HandleVectorSample(
	const FOpenMobileVectorSensorSample& InSample
)
{
	if (!InSample.bHasShakeEvent)
	{
		return;
	}
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(
		this,
		LatestSample.ShakeEvent.StrengthMetresPerSecondSquared,
		LatestSample.ShakeEvent.DurationSeconds,
		LatestSample.ShakeEvent.ImpulseCount,
		MakeSampleInfo(LatestSample.Header)
	);
}
