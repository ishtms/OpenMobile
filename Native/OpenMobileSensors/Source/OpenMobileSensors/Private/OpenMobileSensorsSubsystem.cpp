#include "OpenMobileSensorsSubsystem.h"

#include "OpenMobileAsync.h"
#include "OpenMobilePermissions.h"
#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorsModule.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsSubsystemPrivate
{
	FOpenMobileSensorOperationResult MakeOperationFailure(
		EOpenMobileSensorResultCode ResultCode,
		EOpenMobileErrorCode ErrorCode,
		FString Message
	)
	{
		FOpenMobileSensorOperationResult Result;
		Result.Code = ResultCode;
		Result.Error = FOpenMobileError::Make(ErrorCode, MoveTemp(Message));
		return Result;
	}

	template <typename SampleType>
	bool ReadUnavailable(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		FOpenMobileSensorReadResult& OutResult,
		SampleType& OutSample
	)
	{
		OutSample = {};
		OutResult = {};
		const FOpenMobileSensorOperationResult HandleStatus =
			FOpenMobileSensorsSubscriptionService::GetHandleStatus(
				OwnerIdentifier,
				Handle
			);
		if (!HandleStatus.IsSuccess())
		{
			OutResult.Status = EOpenMobileSensorReadStatus::InvalidHandle;
			OutResult.Error = HandleStatus.Error;
		}
		return false;
	}

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
			OutResult.Operation = MakeOperationFailure(
				EOpenMobileSensorResultCode::InvalidArgument,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("MaximumSamples must be greater than zero.")
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
	FOpenMobileSensorCapabilitySnapshot Snapshot;
	Snapshot.BackendAvailability =
		FOpenMobileSensorsModule::GetBackendCapability();
	return Snapshot;
}

TArray<FOpenMobileSensorMetadata>
UOpenMobileSensorsSubsystem::GetMetadataNative() const
{
	return {};
}

FOpenMobileSensorSubscriptionResult
UOpenMobileSensorsSubsystem::StartSubscriptionNative(
	const FOpenMobileSensorSubscriptionRequest& Request
)
{
	if (bDeinitialized)
	{
		FOpenMobileSensorSubscriptionResult Result;
		Result.RequestedOptions = Request.Options;
		Result.AppliedOptions = Request.Options;
		Result.Operation = OpenMobileSensorsSubsystemPrivate::MakeOperationFailure(
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Sensors subsystem has been deinitialized.")
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

#define OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(MethodName, SampleType) \
	bool UOpenMobileSensorsSubsystem::MethodName( \
		const FOpenMobileSensorSubscriptionHandle& Handle, \
		int64 LastSeenSequence, \
		FOpenMobileSensorReadResult& OutResult, \
		SampleType& OutSample \
	) const \
	{ \
		static_cast<void>(LastSeenSequence); \
		return OpenMobileSensorsSubsystemPrivate::ReadUnavailable( \
			SubscriptionOwnerIdentifier, Handle, OutResult, OutSample \
		); \
	}

OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestVectorSampleNative,
	FOpenMobileVectorSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestAttitudeSampleNative,
	FOpenMobileAttitudeSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestScalarSampleNative,
	FOpenMobileScalarSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestHeadingSampleNative,
	FOpenMobileHeadingSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestStepsSampleNative,
	FOpenMobileStepsSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestActivitySampleNative,
	FOpenMobileActivitySensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestOrientationSampleNative,
	FOpenMobileOrientationSensorSample
)
OPENMOBILE_IMPLEMENT_LATEST_SAMPLE(
	GetLatestProximitySampleNative,
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
		Result.Operation =
			OpenMobileSensorsSubsystemPrivate::MakeOperationFailure(
				EOpenMobileSensorResultCode::NotSupported,
				EOpenMobileErrorCode::NotSupported,
				TEXT("The active sensor backend does not support flushing yet.")
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
		Result.Operation =
			OpenMobileSensorsSubsystemPrivate::MakeOperationFailure(
				EOpenMobileSensorResultCode::NotSupported,
				EOpenMobileErrorCode::NotSupported,
				TEXT("The active sensor backend does not support recentering yet.")
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
		Result.Operation =
			OpenMobileSensorsSubsystemPrivate::MakeOperationFailure(
				EOpenMobileSensorResultCode::NotSupported,
				EOpenMobileErrorCode::NotSupported,
				TEXT("The active sensor backend does not support recentering yet.")
			);
	}
	return Result;
}

FOpenMobilePermissionResult
UOpenMobileSensorsSubsystem::GetPermissionStatusNative(
	EOpenMobileSensorPermission Permission
) const
{
	return FOpenMobilePermissions::GetStatus(
		FOpenMobileSensorPermissions::GetPermissionName(Permission)
	);
}

FOpenMobilePermissionRequestHandle
UOpenMobileSensorsSubsystem::RequestPermissionNative(
	EOpenMobileSensorPermission Permission,
	FOnOpenMobilePermissionRequestComplete&& Completion
)
{
	return FOpenMobilePermissions::RequestPermission(
		FOpenMobileSensorPermissions::GetPermissionName(Permission),
		MoveTemp(Completion)
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
		return MakeOperationFailure(
			EOpenMobileSensorResultCode::InvalidArgument,
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The true-heading location input is invalid.")
		);
	}
	return MakeOperationFailure(
		EOpenMobileSensorResultCode::NotSupported,
		EOpenMobileErrorCode::NotSupported,
		TEXT("True-heading location input is not supported by the active backend.")
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
	Result.Operation = OpenMobileSensorsSubsystemPrivate::MakeOperationFailure(
		EOpenMobileSensorResultCode::NotSupported,
		EOpenMobileErrorCode::NotSupported,
		TEXT("Sensor recording is not available without a recording service.")
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
		Result.Operation = OpenMobileSensorsSubsystemPrivate::MakeOperationFailure(
			EOpenMobileSensorResultCode::InvalidArgument,
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("A valid recording request identifier is required.")
		);
	}
	else
	{
		Result.Operation = OpenMobileSensorsSubsystemPrivate::MakeOperationFailure(
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable,
			TEXT("The sensor recording is no longer active.")
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
		Result.Operation = OpenMobileSensorsSubsystemPrivate::MakeOperationFailure(
			EOpenMobileSensorResultCode::InvalidArgument,
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("A sensor recording file path is required.")
		);
	}
	else
	{
		Result.Operation = OpenMobileSensorsSubsystemPrivate::MakeOperationFailure(
			EOpenMobileSensorResultCode::NotSupported,
			EOpenMobileErrorCode::NotSupported,
			TEXT("Sensor replay is not available without a replay service.")
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
