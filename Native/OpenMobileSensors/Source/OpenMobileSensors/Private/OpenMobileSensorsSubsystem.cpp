#include "OpenMobileSensorsSubsystem.h"

#include "OpenMobileAsync.h"
#include "OpenMobilePermissions.h"
#include "OpenMobileSensorAsyncActionBase.h"
#include "HAL/PlatformTime.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsMetadataService.h"
#include "OpenMobileSensorsModule.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsSubsystemPrivate
{
	template <typename BatchType>
	bool DrainUnavailable(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		BatchType& OutBatch
	)
	{
		OutBatch = {};
		OutResult = {};
		if (MaximumSamples <= 0)
		{
			OutResult.Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidRequest
			);
			return false;
		}
		OutResult.Operation =
			FOpenMobileSensorsSubscriptionService::GetHandleStatus(
				OwnerIdentifier,
				Handle
			);
		return OutResult.Operation.IsSuccess();
	}
}

void UOpenMobileSensorsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (SubscriptionOwnerIdentifier.IsValid())
	{
		FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(
			SubscriptionOwnerIdentifier
		);
	}
	bDeinitialized = false;
	SubscriptionOwnerIdentifier = FGuid::NewGuid();
	EnsureCapabilityListener();
	EnsureSubscriptionListener();
}

void UOpenMobileSensorsSubsystem::Deinitialize()
{
	if (bDeinitialized)
	{
		return;
	}
	if (SubscriptionOwnerIdentifier.IsValid())
	{
		FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(
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
	return FOpenMobileSensorsCapabilityService::GetSnapshot();
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

#define OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(MethodName, BatchType) \
	bool UOpenMobileSensorsSubsystem::MethodName( \
		const FOpenMobileSensorSubscriptionHandle& Handle, \
		int32 MaximumSamples, \
		FOpenMobileSensorBufferReadResult& OutResult, \
		BatchType& OutBatch \
	) \
	{ \
		return OpenMobileSensorsSubsystemPrivate::DrainUnavailable( \
			SubscriptionOwnerIdentifier, Handle, MaximumSamples, \
			OutResult, OutBatch \
		); \
	}

OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedVectorSamplesNative,
	FOpenMobileVectorSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedAttitudeSamplesNative,
	FOpenMobileAttitudeSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedScalarSamplesNative,
	FOpenMobileScalarSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedHeadingSamplesNative,
	FOpenMobileHeadingSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedStepsSamplesNative,
	FOpenMobileStepsSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedActivitySamplesNative,
	FOpenMobileActivitySensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedOrientationSamplesNative,
	FOpenMobileOrientationSensorBatch
)
OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES(
	GetBufferedProximitySamplesNative,
	FOpenMobileProximitySensorBatch
)

#undef OPENMOBILE_IMPLEMENT_BUFFERED_SAMPLES

FGuid UOpenMobileSensorsSubsystem::FlushNative(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	FOnOpenMobileSensorFlushComplete&& Completion
)
{
	const FGuid RequestId = FGuid::NewGuid();
	FOpenMobileSensorFlushResult Result;
	Result.RequestId = RequestId;
	Result.Handle = Handle;
	Result.Operation = FOpenMobileSensorsSubscriptionService::GetHandleStatus(
		SubscriptionOwnerIdentifier,
		Handle
	);
	if (Result.Operation.IsSuccess())
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
	OpenMobile::DispatchToGameThread(
		[Completion = MoveTemp(Completion), Result]() mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
	return RequestId;
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
	Result.Operation = FOpenMobileSensorsSubscriptionService::GetHandleStatus(
		SubscriptionOwnerIdentifier,
		Handle
	);
	if (Result.Operation.IsSuccess())
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
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
	Result.Operation = FOpenMobileSensorsSubscriptionService::GetHandleStatus(
		SubscriptionOwnerIdentifier,
		Handle
	);
	if (Result.Operation.IsSuccess())
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
	return Result;
}

FOpenMobilePermissionResult
UOpenMobileSensorsSubsystem::GetPermissionStatusNative(
	EOpenMobileSensorPermission Permission
) const
{
	const FName PermissionName =
		FOpenMobileSensorPermissions::GetPermissionName(Permission);
	const FOpenMobilePermissionResult Result =
		FOpenMobilePermissions::GetStatus(PermissionName);
	if (!Result.Error.IsSet())
	{
		FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
			PermissionName,
			Result.Status
		);
	}
	return Result;
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
	using namespace OpenMobileSensorsSubsystemPrivate;
	if (!FMath::IsFinite(LocationInput.LatitudeDegrees)
		|| !FMath::IsFinite(LocationInput.LongitudeDegrees)
		|| !FMath::IsFinite(LocationInput.AltitudeMeters)
		|| !FMath::IsFinite(LocationInput.HorizontalAccuracyMeters)
		|| !FMath::IsFinite(LocationInput.TimestampSeconds)
		|| LocationInput.LatitudeDegrees < -90.0
		|| LocationInput.LatitudeDegrees > 90.0
		|| LocationInput.LongitudeDegrees < -180.0
		|| LocationInput.LongitudeDegrees > 180.0
		|| LocationInput.HorizontalAccuracyMeters < 0.0
		|| LocationInput.TimestampSeconds < 0.0)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
	}
	FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(true);
	return FOpenMobileSensorsErrorMapper::Map(
		EOpenMobileSensorFailureReason::DerivedInputUnavailable
	);
}

FGuid UOpenMobileSensorsSubsystem::StartRecordingNative(
	const FOpenMobileSensorRecordingOptions& Options,
	FOnOpenMobileSensorRecordingComplete&& Completion
)
{
	static_cast<void>(Options);
	const FGuid RequestId = FGuid::NewGuid();
	FOpenMobileSensorRecordingResult Result;
	Result.Recording.RequestId = RequestId;
	Result.Operation = FOpenMobileSensorsErrorMapper::Map(
		EOpenMobileSensorFailureReason::UnsupportedOperation
	);
	OpenMobile::DispatchToGameThread(
		[Completion = MoveTemp(Completion), Result]() mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
	return RequestId;
}

FGuid UOpenMobileSensorsSubsystem::StopRecordingNative(
	FGuid RequestId,
	FOnOpenMobileSensorRecordingComplete&& Completion
)
{
	FOpenMobileSensorRecordingResult Result;
	Result.Recording.RequestId = RequestId;
	if (!RequestId.IsValid())
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
	}
	else
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	OpenMobile::DispatchToGameThread(
		[Completion = MoveTemp(Completion), Result]() mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
	return RequestId;
}

FGuid UOpenMobileSensorsSubsystem::ReplayRecordingNative(
	const FString& FilePath,
	const FOpenMobileSensorReplayOptions& Options,
	FOnOpenMobileSensorReplayComplete&& Completion
)
{
	static_cast<void>(Options);
	const FGuid RequestId = FGuid::NewGuid();
	FOpenMobileSensorReplayResult Result;
	Result.RequestId = RequestId;
	if (FilePath.IsEmpty())
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
	}
	else
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
	OpenMobile::DispatchToGameThread(
		[Completion = MoveTemp(Completion), Result]() mutable
		{
			Completion.ExecuteIfBound(Result);
		}
	);
	return RequestId;
}

FOpenMobileSensorDiagnosticsSnapshot
UOpenMobileSensorsSubsystem::GetDiagnosticsSnapshotNative() const
{
	FOpenMobileSensorDiagnosticsSnapshot Snapshot;
	const FOpenMobileCapability Backend =
		FOpenMobileSensorsModule::GetBackendCapability();
	if (Backend.IsAvailable())
	{
		Snapshot.BackendName = TEXT("Registered");
	}
	return Snapshot;
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

FOnOpenMobileVectorSensorBatch&
UOpenMobileSensorsSubsystem::OnVectorSamplesNative()
{
	return VectorSamplesEvent;
}

FOnOpenMobileAttitudeSensorBatch&
UOpenMobileSensorsSubsystem::OnAttitudeSamplesNative()
{
	return AttitudeSamplesEvent;
}

FOnOpenMobileScalarSensorBatch&
UOpenMobileSensorsSubsystem::OnScalarSamplesNative()
{
	return ScalarSamplesEvent;
}

FOnOpenMobileHeadingSensorBatch&
UOpenMobileSensorsSubsystem::OnHeadingSamplesNative()
{
	return HeadingSamplesEvent;
}

FOnOpenMobileStepsSensorBatch&
UOpenMobileSensorsSubsystem::OnStepsSamplesNative()
{
	return StepsSamplesEvent;
}

FOnOpenMobileActivitySensorBatch&
UOpenMobileSensorsSubsystem::OnActivitySamplesNative()
{
	return ActivitySamplesEvent;
}

FOnOpenMobileOrientationSensorBatch&
UOpenMobileSensorsSubsystem::OnOrientationSamplesNative()
{
	return OrientationSamplesEvent;
}

FOnOpenMobileProximitySensorBatch&
UOpenMobileSensorsSubsystem::OnProximitySamplesNative()
{
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

void UOpenMobileSensorsSubsystem::HandleCapabilitySnapshotChanged(
	const FOpenMobileSensorCapabilitySnapshot& Snapshot
)
{
	OnCapabilitiesChanged.Broadcast(Snapshot);
	CapabilitiesChangedEvent.Broadcast(Snapshot);
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
