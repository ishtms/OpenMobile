#include "OpenMobileSensorsSubsystem.h"

#include "OpenMobileAsync.h"
#include "OpenMobilePermissions.h"
#include "OpenMobileNativeStepQueryService.h"
#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "HAL/PlatformTime.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsDiagnosticsService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsMetadataService.h"
#include "OpenMobileSensorsPermissionPolicy.h"
#include "OpenMobileSensorsRecordingService.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsTrueHeadingService.h"

void UOpenMobileSensorsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (SubscriptionOwnerIdentifier.IsValid())
	{
		FOpenMobileSensorsTrueHeadingService::RemoveOwner(
			SubscriptionOwnerIdentifier
		);
		FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(
			FOpenMobileSensorsTrueHeadingService::HasAnyLocationInput(
				FPlatformTime::Seconds()
			)
		);
		FOpenMobileSensorsRecordingService::CancelOwner(
			SubscriptionOwnerIdentifier
		);
		FOpenMobileNativeStepQueryService::CancelOwner(
			SubscriptionOwnerIdentifier
		);
		FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(
			SubscriptionOwnerIdentifier
		);
		FOpenMobileSensorsScreenRotationService::RemoveOwner(
			SubscriptionOwnerIdentifier
		);
	}
	bDeinitialized = false;
	SubscriptionOwnerIdentifier = FGuid::NewGuid();
	EnsureCapabilityListener();
	EnsureSubscriptionListener();
	EnsureSampleListeners();
}

void UOpenMobileSensorsSubsystem::Deinitialize()
{
	if (bDeinitialized)
	{
		return;
	}
	if (SubscriptionOwnerIdentifier.IsValid())
	{
		FOpenMobileSensorsTrueHeadingService::RemoveOwner(
			SubscriptionOwnerIdentifier
		);
		FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(
			FOpenMobileSensorsTrueHeadingService::HasAnyLocationInput(
				FPlatformTime::Seconds()
			)
		);
		FOpenMobileSensorsRecordingService::CancelOwner(
			SubscriptionOwnerIdentifier
		);
		FOpenMobileNativeStepQueryService::CancelOwner(
			SubscriptionOwnerIdentifier
		);
		FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(
			SubscriptionOwnerIdentifier
		);
		FOpenMobileSensorsScreenRotationService::RemoveOwner(
			SubscriptionOwnerIdentifier
		);
		SubscriptionOwnerIdentifier.Invalidate();
	}
	bDeinitialized = true;
	if (CapabilityServiceChangedHandle.IsValid())
	{
		FOpenMobileSensorsCapabilityService::OnChanged().Remove(
			CapabilityServiceChangedHandle
		);
		CapabilityServiceChangedHandle.Reset();
	}
	if (SubscriptionServiceChangedHandle.IsValid())
	{
		FOpenMobileSensorsSubscriptionService::OnStateChanged().Remove(
			SubscriptionServiceChangedHandle
		);
		SubscriptionServiceChangedHandle.Reset();
	}
	if (AccuracyChangedReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnAccuracyChanged().Remove(
			AccuracyChangedReadyHandle
		);
		AccuracyChangedReadyHandle.Reset();
	}
	if (CalibrationChangedReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnCalibrationChanged().Remove(
			CalibrationChangedReadyHandle
		);
		CalibrationChangedReadyHandle.Reset();
	}
	if (VectorBatchReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnVectorBatch().Remove(
			VectorBatchReadyHandle
		);
		VectorBatchReadyHandle.Reset();
	}
	if (AttitudeBatchReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnAttitudeBatch().Remove(
			AttitudeBatchReadyHandle
		);
		AttitudeBatchReadyHandle.Reset();
	}
	if (ScalarBatchReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnScalarBatch().Remove(
			ScalarBatchReadyHandle
		);
		ScalarBatchReadyHandle.Reset();
	}
	if (HeadingBatchReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnHeadingBatch().Remove(
			HeadingBatchReadyHandle
		);
		HeadingBatchReadyHandle.Reset();
	}
	if (StepsBatchReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnStepsBatch().Remove(
			StepsBatchReadyHandle
		);
		StepsBatchReadyHandle.Reset();
	}
	if (ActivityBatchReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnActivityBatch().Remove(
			ActivityBatchReadyHandle
		);
		ActivityBatchReadyHandle.Reset();
	}
	if (OrientationBatchReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnOrientationBatch().Remove(
			OrientationBatchReadyHandle
		);
		OrientationBatchReadyHandle.Reset();
	}
	if (ProximityBatchReadyHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnProximityBatch().Remove(
			ProximityBatchReadyHandle
		);
		ProximityBatchReadyHandle.Reset();
	}
	TArray<TWeakObjectPtr<UOpenMobileSensorAsyncActionBase>> PendingActions;
	PendingActions.Reserve(AsyncActions.Num());
	for (const TWeakObjectPtr<UOpenMobileSensorAsyncActionBase>& Action
		: AsyncActions)
	{
		PendingActions.Add(Action);
	}
	AsyncActions.Reset();
	for (const TWeakObjectPtr<UOpenMobileSensorAsyncActionBase>& Action
		: PendingActions)
	{
		if (Action.IsValid())
		{
			Action->HandleGameInstanceTeardown();
		}
	}
	OnCapabilitiesChanged.Clear();
	OnSubscriptionStateChanged.Clear();
	OnAccuracyChanged.Clear();
	OnCalibrationChanged.Clear();
	OnVectorSamples.Clear();
	OnAttitudeSamples.Clear();
	OnScalarSamples.Clear();
	OnHeadingSamples.Clear();
	OnStepsSamples.Clear();
	OnActivitySamples.Clear();
	OnOrientationSamples.Clear();
	OnProximitySamples.Clear();
	CapabilitiesChangedEvent.Clear();
	SubscriptionStateChangedEvent.Clear();
	AccuracyChangedEvent.Clear();
	CalibrationChangedEvent.Clear();
	VectorSamplesEvent.Clear();
	AttitudeSamplesEvent.Clear();
	ScalarSamplesEvent.Clear();
	HeadingSamplesEvent.Clear();
	StepsSamplesEvent.Clear();
	ActivitySamplesEvent.Clear();
	OrientationSamplesEvent.Clear();
	ProximitySamplesEvent.Clear();
	Super::Deinitialize();
}

FOpenMobileSensorCapabilitySnapshot
UOpenMobileSensorsSubsystem::GetCapabilitySnapshotNative() const
{
	EnsureCapabilityListener();
	FOpenMobileSensorsCapabilityService::RefreshPermissionStatus(
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::TrueHeadingLocation
		)
	);
	const double CurrentMonotonicSeconds = FPlatformTime::Seconds();
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(
		FOpenMobileSensorsTrueHeadingService::HasAnyLocationInput(
			CurrentMonotonicSeconds
		)
	);
	FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	EOpenMobileSensorFailureReason LocationState =
		FOpenMobileSensorsTrueHeadingService::GetLocationInputState(
			SubscriptionOwnerIdentifier,
			CurrentMonotonicSeconds
		);
	if (LocationState == EOpenMobileSensorFailureReason::InvalidRequest)
	{
		LocationState = EOpenMobileSensorFailureReason::MissingLocationInput;
	}
	FOpenMobileSensorsCapabilityService::ApplyTrueHeadingLocationInputState(
		Snapshot,
		LocationState
	);
	return Snapshot;
}

TArray<FOpenMobileSensorMetadata>
UOpenMobileSensorsSubsystem::GetMetadataNative() const
{
	return FOpenMobileSensorsMetadataService::GetMetadata();
}

FOpenMobileSensorSubscriptionResult
UOpenMobileSensorsSubsystem::StartSubscriptionNative(
	const FOpenMobileSensorSubscriptionRequest& Request
)
{
	EnsureSubscriptionListener();
	EnsureSampleListeners();
	if (bDeinitialized)
	{
		FOpenMobileSensorSubscriptionResult Result;
		Result.RequestedOptions = Request.Options;
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
		return Result;
	}
	return FOpenMobileSensorsSubscriptionService::StartSubscription(
		GetOrCreateSubscriptionOwnerIdentifier(),
		Request
	);
}

bool UOpenMobileSensorsSubsystem::UpdateApplicationWindowRotationNative(
	EOpenMobileSensorScreenRotation Rotation,
	double TimestampSeconds,
	bool bNaturalOrientationLandscape
)
{
	if (bDeinitialized)
	{
		return false;
	}
	return FOpenMobileSensorsScreenRotationService::
		CaptureApplicationWindowRotation(
			GetOrCreateSubscriptionOwnerIdentifier(),
			Rotation,
			TimestampSeconds,
			bNaturalOrientationLandscape
		);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::UpdateSubscriptionNative(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorStreamOptions& Options
)
{
	return FOpenMobileSensorsSubscriptionService::UpdateSubscription(
		SubscriptionOwnerIdentifier,
		Handle,
		Options
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::StopSubscriptionNative(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	return FOpenMobileSensorsSubscriptionService::StopSubscription(
		SubscriptionOwnerIdentifier,
		Handle
	);
}

int32 UOpenMobileSensorsSubsystem::StopAllSubscriptionsNative()
{
	return FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(
		SubscriptionOwnerIdentifier
	);
}

bool UOpenMobileSensorsSubsystem::GetSubscriptionStateNative(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	FOpenMobileSensorSubscriptionStateSnapshot& OutState
) const
{
	return FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		SubscriptionOwnerIdentifier,
		Handle,
		OutState
	);
}

#define OPENMOBILE_IMPLEMENT_LATEST_SAMPLE( \
	MethodName, ServiceMethod, SampleType \
) \
	bool UOpenMobileSensorsSubsystem::MethodName( \
		const FOpenMobileSensorSubscriptionHandle& Handle, \
		int64 LastSeenSequence, \
		FOpenMobileSensorReadResult& OutResult, \
		SampleType& OutSample \
	) const \
	{ \
		return FOpenMobileSensorsSampleService::ServiceMethod( \
			SubscriptionOwnerIdentifier, Handle, LastSeenSequence, \
			FPlatformTime::Seconds(), OutResult, OutSample \
		); \
	}

OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestVectorSampleNative,
	ReadLatestVector,
	FOpenMobileVectorSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestAttitudeSampleNative,
	ReadLatestAttitude,
	FOpenMobileAttitudeSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestScalarSampleNative,
	ReadLatestScalar,
	FOpenMobileScalarSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestHeadingSampleNative,
	ReadLatestHeading,
	FOpenMobileHeadingSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestStepsSampleNative,
	ReadLatestSteps,
	FOpenMobileStepsSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestActivitySampleNative,
	ReadLatestActivity,
	FOpenMobileActivitySensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestOrientationSampleNative,
	ReadLatestOrientation,
	FOpenMobileOrientationSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestProximitySampleNative,
	ReadLatestProximity,
	FOpenMobileProximitySensorSample
)

#undef OPENMOBILE_IMPLEMENT_LATEST_SAMPLE

#define OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES( \
	MethodName, ServiceMethod, BatchType \
) \
	bool UOpenMobileSensorsSubsystem::MethodName( \
		const FOpenMobileSensorSubscriptionHandle& Handle, \
		int32 MaximumSamples, \
		FOpenMobileSensorBufferReadResult& OutResult, \
		BatchType& OutBatch \
	) \
	{ \
		if (MaximumSamples < 1 || MaximumSamples > 4096) \
		{ \
			OutBatch = {}; \
			OutResult = {}; \
			OutResult.Operation = FOpenMobileSensorsErrorMapper::Map( \
				EOpenMobileSensorFailureReason::InvalidRequest \
			); \
			return false; \
		} \
		return FOpenMobileSensorsSampleService::ServiceMethod( \
			SubscriptionOwnerIdentifier, Handle, MaximumSamples, \
			OutResult, OutBatch \
		); \
	}

OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedVectorSamplesNative,
	DrainBufferedVector,
	FOpenMobileVectorSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedAttitudeSamplesNative,
	DrainBufferedAttitude,
	FOpenMobileAttitudeSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedScalarSamplesNative,
	DrainBufferedScalar,
	FOpenMobileScalarSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedHeadingSamplesNative,
	DrainBufferedHeading,
	FOpenMobileHeadingSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedStepsSamplesNative,
	DrainBufferedSteps,
	FOpenMobileStepsSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedActivitySamplesNative,
	DrainBufferedActivity,
	FOpenMobileActivitySensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedOrientationSamplesNative,
	DrainBufferedOrientation,
	FOpenMobileOrientationSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedProximitySamplesNative,
	DrainBufferedProximity,
	FOpenMobileProximitySensorBatch
)

#undef OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES

FGuid UOpenMobileSensorsSubsystem::FlushNative(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	FOnOpenMobileSensorFlushComplete&& Completion
)
{
	const FGuid RequestId = FGuid::NewGuid();
	FOpenMobileSensorsSubscriptionService::FlushSubscription(
		SubscriptionOwnerIdentifier,
		Handle,
		RequestId,
		[Completion = MoveTemp(Completion)](
			const FOpenMobileSensorFlushResult& Result
		) mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
	return RequestId;
}

bool UOpenMobileSensorsSubsystem::CancelFlushNative(FGuid RequestId)
{
	return FOpenMobileSensorsSubscriptionService::CancelFlush(
		SubscriptionOwnerIdentifier,
		RequestId
	);
}

FGuid UOpenMobileSensorsSubsystem::RecenterNative(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	EOpenMobileSensorRecenterMode Mode,
	FOnOpenMobileSensorRecenterComplete&& Completion
)
{
	const FGuid RequestId = FGuid::NewGuid();
	FOpenMobileSensorRecenterResult Result;
	Result.RequestId = RequestId;
	Result.Handle = Handle;
	Result.Mode = Mode;
	Result.Operation = FOpenMobileSensorsSubscriptionService::RecenterAttitude(
		SubscriptionOwnerIdentifier,
		Handle,
		Mode
	);
	OpenMobile::DispatchToGameThread(
		[Completion = MoveTemp(Completion), Result]() mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
	return RequestId;
}

FOpenMobileSensorRecenterResult
UOpenMobileSensorsSubsystem::RecenterSubscription(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	EOpenMobileSensorRecenterMode Mode
)
{
	FOpenMobileSensorRecenterResult Result;
	Result.RequestId = FGuid::NewGuid();
	Result.Handle = Handle;
	Result.Mode = Mode;
	Result.Operation = FOpenMobileSensorsSubscriptionService::RecenterAttitude(
		SubscriptionOwnerIdentifier,
		Handle,
		Mode
	);
	return Result;
}

FOpenMobileSensorSubscriptionResult
UOpenMobileSensorsSubsystem::BeginRelativeAltitudeSessionNative(
	const FOpenMobileSensorStreamOptions& Options
)
{
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::RelativeAltitude;
	Request.Sensor.InstanceId = TEXT("Default");
	Request.Options = Options;
	return StartSubscriptionNative(Request);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::RecenterRelativeAltitudeBaselineNative(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	return FOpenMobileSensorsSubscriptionService::RecenterRelativeAltitude(
		SubscriptionOwnerIdentifier,
		Handle
	);
}

bool UOpenMobileSensorsSubsystem::ReadRelativeAltitudeSessionNative(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	int64 LastSeenSequence,
	FOpenMobileSensorReadResult& OutResult,
	FOpenMobileScalarSensorSample& OutSample
) const
{
	FOpenMobileSensorSubscriptionStateSnapshot State;
	if (!FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			SubscriptionOwnerIdentifier,
			Handle,
			State
		)
		|| State.Sensor.Type != EOpenMobileSensorType::RelativeAltitude)
	{
		OutResult = {};
		OutResult.Status = EOpenMobileSensorReadStatus::InvalidHandle;
		OutResult.Error = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidHandle
		).Error;
		OutSample = {};
		return false;
	}
	return GetLatestScalarSampleNative(
		Handle,
		LastSeenSequence,
		OutResult,
		OutSample
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::StopRelativeAltitudeSessionNative(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	FOpenMobileSensorSubscriptionStateSnapshot State;
	if (!FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			SubscriptionOwnerIdentifier,
			Handle,
			State
		))
	{
		return FOpenMobileSensorsSubscriptionService::GetHandleStatus(
			SubscriptionOwnerIdentifier,
			Handle
		);
	}
	if (State.Sensor.Type != EOpenMobileSensorType::RelativeAltitude)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
	return StopSubscriptionNative(Handle);
}

FOpenMobileSensorSubscriptionResult
UOpenMobileSensorsSubsystem::BeginStepCountSessionNative(
	const FOpenMobileSensorStreamOptions& Options
)
{
	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::StepCounter;
	Request.Sensor.InstanceId = TEXT("Default");
	Request.Options = Options;
	Request.bResettableStepCountSession = true;
	return StartSubscriptionNative(Request);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::ResetStepCountSessionNative(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	return FOpenMobileSensorsSubscriptionService::ResetStepCountSession(
		SubscriptionOwnerIdentifier,
		Handle
	);
}

bool UOpenMobileSensorsSubsystem::ReadStepCountSessionNative(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	int64 LastSeenSequence,
	FOpenMobileSensorReadResult& OutResult,
	FOpenMobileStepsSensorSample& OutSample
) const
{
	FOpenMobileSensorSubscriptionStateSnapshot State;
	if (!FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			SubscriptionOwnerIdentifier,
			Handle,
			State
		)
		|| State.Sensor.Type != EOpenMobileSensorType::StepCounter
		|| !State.bResettableStepCountSession)
	{
		OutResult = {};
		OutResult.Status = EOpenMobileSensorReadStatus::InvalidHandle;
		OutResult.Error = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidHandle
		).Error;
		OutSample = {};
		return false;
	}
	return GetLatestStepsSampleNative(
		Handle,
		LastSeenSequence,
		OutResult,
		OutSample
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::StopStepCountSessionNative(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	FOpenMobileSensorSubscriptionStateSnapshot State;
	if (!FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			SubscriptionOwnerIdentifier,
			Handle,
			State
		))
	{
		return FOpenMobileSensorsSubscriptionService::GetHandleStatus(
			SubscriptionOwnerIdentifier,
			Handle
		);
	}
	if (State.Sensor.Type != EOpenMobileSensorType::StepCounter
		|| !State.bResettableStepCountSession)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
	return StopSubscriptionNative(Handle);
}

FOpenMobileStepCountSessionPolicy
UOpenMobileSensorsSubsystem::GetStepCountSessionPolicyNative() const
{
	return {};
}

FGuid UOpenMobileSensorsSubsystem::QueryNativeStepCountNative(
	const FOpenMobileNativeStepCountQuery& Query,
	FOnOpenMobileNativeStepCountQueryComplete&& Completion
)
{
	if (!Completion.IsBound())
	{
		return {};
	}
	return FOpenMobileNativeStepQueryService::Query(
		SubscriptionOwnerIdentifier,
		Query,
		[Completion = MoveTemp(Completion)](
			const FOpenMobileNativeStepCountQueryResult& Result) mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
}

bool UOpenMobileSensorsSubsystem::CancelNativeStepCountQueryNative(
	const FGuid& RequestId
)
{
	return FOpenMobileNativeStepQueryService::Cancel(
		SubscriptionOwnerIdentifier,
		RequestId
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::RequestNativeCalibrationPrompt(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	return FOpenMobileSensorsSubscriptionService::
		RequestNativeCalibrationPrompt(
			SubscriptionOwnerIdentifier,
			Handle
		);
}

FOpenMobilePermissionResult
UOpenMobileSensorsSubsystem::GetPermissionStatusNative(
	EOpenMobileSensorPermission Permission
) const
{
	const FName PermissionName =
		FOpenMobileSensorPermissions::GetPermissionName(Permission);
	return FOpenMobileSensorsCapabilityService::RefreshPermissionStatus(
		PermissionName
	);
}

FOpenMobilePermissionRequestHandle
UOpenMobileSensorsSubsystem::RequestPermissionNative(
	EOpenMobileSensorPermission Permission,
	FOnOpenMobilePermissionRequestComplete&& Completion
)
{
	if (!Completion.IsBound())
	{
		return {};
	}
	const FName PermissionName =
		FOpenMobileSensorPermissions::GetPermissionName(Permission);
	if (Permission == EOpenMobileSensorPermission::TrueHeadingLocation)
	{
		FOpenMobilePermissionResult Result;
		Result.Permission = PermissionName;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("Request location permission through its owning provider.")
		);
		OpenMobile::DispatchToGameThread(
			[Completion = MoveTemp(Completion), Result]() mutable
			{
				Completion.ExecuteIfBound(Result);
			}
		);
		return {};
	}
	return FOpenMobilePermissions::RequestPermission(
		PermissionName,
		FOnOpenMobilePermissionRequestComplete::CreateLambda(
			[PermissionName, Completion = MoveTemp(Completion)](
				const FOpenMobilePermissionResult& Result
			) mutable
			{
				if (!Result.Error.IsSet())
				{
					FOpenMobileSensorsCapabilityService::
						NotifyPermissionStatusChanged(
							PermissionName,
							Result.Status
						);
				}
				Completion.ExecuteIfBound(Result);
			}
		)
	);
}

bool UOpenMobileSensorsSubsystem::CancelPermissionRequestNative(
	const FOpenMobilePermissionRequestHandle& Handle
)
{
	return FOpenMobilePermissions::CancelRequest(Handle);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::SetTrueHeadingLocationInputNative(
	const FOpenMobileSensorLocationInput& LocationInput
)
{
	if (bDeinitialized)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	const FOpenMobilePermissionResult Permission =
		FOpenMobileSensorsCapabilityService::RefreshPermissionStatus(
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::TrueHeadingLocation
			)
		);
	if (!Permission.Error.IsSet())
	{
		switch (Permission.Status)
		{
		case EOpenMobilePermissionStatus::Granted:
			break;
		case EOpenMobilePermissionStatus::NotDetermined:
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::PermissionRequired
			);
		case EOpenMobilePermissionStatus::Denied:
		case EOpenMobilePermissionStatus::PermanentlyDenied:
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::PermissionDenied
			);
		case EOpenMobilePermissionStatus::Restricted:
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::PermissionRestricted
			);
		}
	}
	else if (Permission.Error.Code != EOpenMobileErrorCode::NotSupported)
	{
		return FOpenMobileSensorsErrorMapper::FromCommon(Permission.Error);
	}
	const EOpenMobileSensorFailureReason Result =
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			GetOrCreateSubscriptionOwnerIdentifier(),
			LocationInput,
			static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp()),
			FPlatformTime::Seconds()
		);
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(
		FOpenMobileSensorsTrueHeadingService::HasAnyLocationInput(
			FPlatformTime::Seconds()
		)
	);
	if (Result != EOpenMobileSensorFailureReason::None)
	{
		return FOpenMobileSensorsErrorMapper::Map(Result);
	}
	FOpenMobileSensorOperationResult Success;
	Success.Code = EOpenMobileSensorResultCode::Success;
	return Success;
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::ClearTrueHeadingLocationInputNative()
{
	if (bDeinitialized)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	FOpenMobileSensorsTrueHeadingService::ClearLocationInput(
		GetOrCreateSubscriptionOwnerIdentifier()
	);
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(
		FOpenMobileSensorsTrueHeadingService::HasAnyLocationInput(
			FPlatformTime::Seconds()
		)
	);
	FOpenMobileSensorOperationResult Success;
	Success.Code = EOpenMobileSensorResultCode::Success;
	return Success;
}

FGuid UOpenMobileSensorsSubsystem::StartRecordingNative(
	const FOpenMobileSensorRecordingOptions& Options,
	FOnOpenMobileSensorRecordingComplete&& Completion
)
{
	return FOpenMobileSensorsRecordingService::StartRecording(
		GetOrCreateSubscriptionOwnerIdentifier(),
		Options,
		[Completion = MoveTemp(Completion)](
			const FOpenMobileSensorRecordingResult& Result) mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
}

FGuid UOpenMobileSensorsSubsystem::StopRecordingNative(
	FGuid RequestId,
	FOnOpenMobileSensorRecordingComplete&& Completion
)
{
	return FOpenMobileSensorsRecordingService::StopRecording(
		SubscriptionOwnerIdentifier,
		RequestId,
		[Completion = MoveTemp(Completion)](
			const FOpenMobileSensorRecordingResult& Result) mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::CancelRecordingNative(FGuid RequestId)
{
	return FOpenMobileSensorsRecordingService::CancelRecording(
		SubscriptionOwnerIdentifier,
		RequestId
	);
}

FGuid UOpenMobileSensorsSubsystem::ReplayRecordingNative(
	const FString& FilePath,
	const FOpenMobileSensorReplayOptions& Options,
	FOnOpenMobileSensorReplayComplete&& Completion
)
{
	return FOpenMobileSensorsRecordingService::ReplayRecording(
		GetOrCreateSubscriptionOwnerIdentifier(),
		FilePath,
		Options,
		[Completion = MoveTemp(Completion)](
			const FOpenMobileSensorReplayResult& Result) mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::CancelReplayNative(FGuid RequestId)
{
	return FOpenMobileSensorsRecordingService::CancelReplay(
		SubscriptionOwnerIdentifier,
		RequestId
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::PauseReplayNative(FGuid RequestId)
{
	return FOpenMobileSensorsRecordingService::PauseReplay(
		SubscriptionOwnerIdentifier,
		RequestId
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::ResumeReplayNative(FGuid RequestId)
{
	return FOpenMobileSensorsRecordingService::ResumeReplay(
		SubscriptionOwnerIdentifier,
		RequestId
	);
}

FOpenMobileSensorOperationResult UOpenMobileSensorsSubsystem::SeekReplayNative(
	FGuid RequestId,
	double PlaybackTimeSeconds
)
{
	return FOpenMobileSensorsRecordingService::SeekReplay(
		SubscriptionOwnerIdentifier,
		RequestId,
		PlaybackTimeSeconds
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::SetReplaySpeedNative(
	FGuid RequestId,
	double PlaybackSpeed
)
{
	return FOpenMobileSensorsRecordingService::SetReplaySpeed(
		SubscriptionOwnerIdentifier,
		RequestId,
		PlaybackSpeed
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::SetReplayLoopingNative(
	FGuid RequestId,
	bool bLoop
)
{
	return FOpenMobileSensorsRecordingService::SetReplayLooping(
		SubscriptionOwnerIdentifier,
		RequestId,
		bLoop
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsSubsystem::AdvanceReplayNative(
	FGuid RequestId,
	double DeltaSeconds
)
{
	return FOpenMobileSensorsRecordingService::AdvanceReplay(
		SubscriptionOwnerIdentifier,
		RequestId,
		DeltaSeconds
	);
}

bool UOpenMobileSensorsSubsystem::GetReplayStateNative(
	FGuid RequestId,
	FOpenMobileSensorReplaySnapshot& OutSnapshot
) const
{
	return FOpenMobileSensorsRecordingService::GetReplaySnapshot(
		SubscriptionOwnerIdentifier,
		RequestId,
		OutSnapshot
	);
}

FOpenMobileSensorDiagnosticsSnapshot
UOpenMobileSensorsSubsystem::GetDiagnosticsSnapshotNative() const
{
	return FOpenMobileSensorsDiagnosticsService::Capture(
		&SubscriptionOwnerIdentifier
	);
}

FOnOpenMobileSensorCapabilitiesChanged&
UOpenMobileSensorsSubsystem::OnCapabilitiesChangedNative()
{
	EnsureCapabilityListener();
	return CapabilitiesChangedEvent;
}

FOnOpenMobileSensorSubscriptionStateChanged&
UOpenMobileSensorsSubsystem::OnSubscriptionStateChangedNative()
{
	return SubscriptionStateChangedEvent;
}

FOnOpenMobileSensorAccuracyChanged&
UOpenMobileSensorsSubsystem::OnAccuracyChangedNative()
{
	EnsureSampleListeners();
	return AccuracyChangedEvent;
}

FOnOpenMobileSensorCalibrationChanged&
UOpenMobileSensorsSubsystem::OnCalibrationChangedNative()
{
	EnsureSampleListeners();
	return CalibrationChangedEvent;
}

FOnOpenMobileVectorSensorBatch&
UOpenMobileSensorsSubsystem::OnVectorSamplesNative()
{
	EnsureSampleListeners();
	return VectorSamplesEvent;
}

FOnOpenMobileAttitudeSensorBatch&
UOpenMobileSensorsSubsystem::OnAttitudeSamplesNative()
{
	EnsureSampleListeners();
	return AttitudeSamplesEvent;
}

FOnOpenMobileScalarSensorBatch&
UOpenMobileSensorsSubsystem::OnScalarSamplesNative()
{
	EnsureSampleListeners();
	return ScalarSamplesEvent;
}

FOnOpenMobileHeadingSensorBatch&
UOpenMobileSensorsSubsystem::OnHeadingSamplesNative()
{
	EnsureSampleListeners();
	return HeadingSamplesEvent;
}

FOnOpenMobileStepsSensorBatch&
UOpenMobileSensorsSubsystem::OnStepsSamplesNative()
{
	EnsureSampleListeners();
	return StepsSamplesEvent;
}

FOnOpenMobileActivitySensorBatch&
UOpenMobileSensorsSubsystem::OnActivitySamplesNative()
{
	EnsureSampleListeners();
	return ActivitySamplesEvent;
}

FOnOpenMobileOrientationSensorBatch&
UOpenMobileSensorsSubsystem::OnOrientationSamplesNative()
{
	EnsureSampleListeners();
	return OrientationSamplesEvent;
}

FOnOpenMobileProximitySensorBatch&
UOpenMobileSensorsSubsystem::OnProximitySamplesNative()
{
	EnsureSampleListeners();
	return ProximitySamplesEvent;
}

FGuid UOpenMobileSensorsSubsystem::GetOrCreateSubscriptionOwnerIdentifier()
{
	if (!SubscriptionOwnerIdentifier.IsValid())
	{
		SubscriptionOwnerIdentifier = FGuid::NewGuid();
	}
	return SubscriptionOwnerIdentifier;
}

void UOpenMobileSensorsSubsystem::EnsureCapabilityListener() const
{
	if (bDeinitialized || CapabilityServiceChangedHandle.IsValid())
	{
		return;
	}
	UOpenMobileSensorsSubsystem* MutableThis =
		const_cast<UOpenMobileSensorsSubsystem*>(this);
	CapabilityServiceChangedHandle =
		FOpenMobileSensorsCapabilityService::OnChanged().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleCapabilitySnapshotChanged
		);
}

void UOpenMobileSensorsSubsystem::EnsureSubscriptionListener() const
{
	if (bDeinitialized || SubscriptionServiceChangedHandle.IsValid())
	{
		return;
	}
	UOpenMobileSensorsSubsystem* MutableThis =
		const_cast<UOpenMobileSensorsSubsystem*>(this);
	SubscriptionServiceChangedHandle =
		FOpenMobileSensorsSubscriptionService::OnStateChanged().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleSubscriptionStateChanged
		);
}

void UOpenMobileSensorsSubsystem::EnsureSampleListeners() const
{
	if (bDeinitialized || VectorBatchReadyHandle.IsValid())
	{
		return;
	}
	UOpenMobileSensorsSubsystem* MutableThis =
		const_cast<UOpenMobileSensorsSubsystem*>(this);
	AccuracyChangedReadyHandle =
		FOpenMobileSensorsSampleService::OnAccuracyChanged().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleAccuracyChanged
		);
	CalibrationChangedReadyHandle =
		FOpenMobileSensorsSampleService::OnCalibrationChanged().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleCalibrationChanged
		);
	VectorBatchReadyHandle =
		FOpenMobileSensorsSampleService::OnVectorBatch().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleVectorBatch
		);
	AttitudeBatchReadyHandle =
		FOpenMobileSensorsSampleService::OnAttitudeBatch().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleAttitudeBatch
		);
	ScalarBatchReadyHandle =
		FOpenMobileSensorsSampleService::OnScalarBatch().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleScalarBatch
		);
	HeadingBatchReadyHandle =
		FOpenMobileSensorsSampleService::OnHeadingBatch().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleHeadingBatch
		);
	StepsBatchReadyHandle =
		FOpenMobileSensorsSampleService::OnStepsBatch().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleStepsBatch
		);
	ActivityBatchReadyHandle =
		FOpenMobileSensorsSampleService::OnActivityBatch().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleActivityBatch
		);
	OrientationBatchReadyHandle =
		FOpenMobileSensorsSampleService::OnOrientationBatch().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleOrientationBatch
		);
	ProximityBatchReadyHandle =
		FOpenMobileSensorsSampleService::OnProximityBatch().AddUObject(
			MutableThis,
			&UOpenMobileSensorsSubsystem::HandleProximityBatch
		);
}

void UOpenMobileSensorsSubsystem::HandleCapabilitySnapshotChanged(
	const FOpenMobileSensorCapabilitySnapshot& Snapshot
)
{
	FOpenMobileSensorCapabilitySnapshot OwnerSnapshot = Snapshot;
	const FOpenMobileSensorCapability* TrueHeading =
		OwnerSnapshot.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::TrueHeading;
			}
		);
	const bool bAuthorizationBlocked = TrueHeading
		&& TrueHeading->Prerequisites.ContainsByPredicate(
			[](const FOpenMobileSensorPrerequisiteCapability& Prerequisite)
			{
				return Prerequisite.RequiredPermission ==
					FOpenMobileSensorsPermissionPolicy::TrueHeadingLocation()
					&& Prerequisite.bPermissionStatusKnown
					&& Prerequisite.PermissionFailureReason !=
						EOpenMobileSensorFailureReason::None;
			}
		);
	if (bAuthorizationBlocked)
	{
		FOpenMobileSensorsTrueHeadingService::ClearLocationInput(
			SubscriptionOwnerIdentifier
		);
	}
	EOpenMobileSensorFailureReason LocationState =
		FOpenMobileSensorsTrueHeadingService::GetLocationInputState(
			SubscriptionOwnerIdentifier,
			FPlatformTime::Seconds()
		);
	if (LocationState == EOpenMobileSensorFailureReason::InvalidRequest)
	{
		LocationState = EOpenMobileSensorFailureReason::MissingLocationInput;
	}
	FOpenMobileSensorsCapabilityService::ApplyTrueHeadingLocationInputState(
		OwnerSnapshot,
		LocationState
	);
	OnCapabilitiesChanged.Broadcast(OwnerSnapshot);
	CapabilitiesChangedEvent.Broadcast(OwnerSnapshot);
}

void UOpenMobileSensorsSubsystem::HandleSubscriptionStateChanged(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionStateSnapshot& Snapshot
)
{
	if (bDeinitialized || OwnerIdentifier != SubscriptionOwnerIdentifier)
	{
		return;
	}
	OnSubscriptionStateChanged.Broadcast(Snapshot);
	SubscriptionStateChangedEvent.Broadcast(Snapshot);
}

void UOpenMobileSensorsSubsystem::HandleAccuracyChanged(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorAccuracySnapshot& Snapshot
)
{
	if (bDeinitialized || OwnerIdentifier != SubscriptionOwnerIdentifier)
	{
		return;
	}
	OnAccuracyChanged.Broadcast(Handle, Snapshot);
	AccuracyChangedEvent.Broadcast(Handle, Snapshot);
}

void UOpenMobileSensorsSubsystem::HandleCalibrationChanged(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorCalibrationEvent& Event
)
{
	if (bDeinitialized || OwnerIdentifier != SubscriptionOwnerIdentifier)
	{
		return;
	}
	OnCalibrationChanged.Broadcast(Handle, Event);
	CalibrationChangedEvent.Broadcast(Handle, Event);
}

#define OPENMOBILE_IMPLEMENT_BATCH_HANDLER(FamilyName, BatchType) \
	void UOpenMobileSensorsSubsystem::Handle##FamilyName##Batch( \
		const FGuid& OwnerIdentifier, \
		const FOpenMobileSensorSubscriptionHandle& Handle, \
		const BatchType& Batch \
	) \
	{ \
		if (bDeinitialized || OwnerIdentifier != SubscriptionOwnerIdentifier) \
		{ \
			return; \
		} \
		On##FamilyName##Samples.Broadcast(Handle, Batch); \
		FamilyName##SamplesEvent.Broadcast(Handle, Batch); \
	}

OPENMOBILE_IMPLEMENT_BATCH_HANDLER(Vector, FOpenMobileVectorSensorBatch)
OPENMOBILE_IMPLEMENT_BATCH_HANDLER(Attitude, FOpenMobileAttitudeSensorBatch)
OPENMOBILE_IMPLEMENT_BATCH_HANDLER(Scalar, FOpenMobileScalarSensorBatch)
OPENMOBILE_IMPLEMENT_BATCH_HANDLER(Heading, FOpenMobileHeadingSensorBatch)
OPENMOBILE_IMPLEMENT_BATCH_HANDLER(Steps, FOpenMobileStepsSensorBatch)
OPENMOBILE_IMPLEMENT_BATCH_HANDLER(Activity, FOpenMobileActivitySensorBatch)
OPENMOBILE_IMPLEMENT_BATCH_HANDLER(
	Orientation,
	FOpenMobileOrientationSensorBatch
)
OPENMOBILE_IMPLEMENT_BATCH_HANDLER(Proximity, FOpenMobileProximitySensorBatch)

#undef OPENMOBILE_IMPLEMENT_BATCH_HANDLER

void UOpenMobileSensorsSubsystem::RegisterAsyncAction(
	UOpenMobileSensorAsyncActionBase* Action
)
{
	if (Action)
	{
		AsyncActions.Add(Action);
	}
}

void UOpenMobileSensorsSubsystem::UnregisterAsyncAction(
	UOpenMobileSensorAsyncActionBase* Action
)
{
	AsyncActions.Remove(Action);
}
