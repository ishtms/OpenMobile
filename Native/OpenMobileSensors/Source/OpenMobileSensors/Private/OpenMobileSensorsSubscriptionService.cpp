#include "OpenMobileSensorsSubscriptionService.h"

#include "Containers/Ticker.h"
#include "IOpenMobileSensorsBackend.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"

namespace OpenMobileSensorsSubscriptionServicePrivate
{
	struct FPhysicalStreamKey
	{
		FOpenMobileSensorIdentifier Sensor;
		EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame =
			EOpenMobileAttitudeReferenceFrame::GameRelative;
		bool bAllowDerivedFallback = true;

		bool operator==(const FPhysicalStreamKey& Other) const
		{
			return Sensor == Other.Sensor
				&& AttitudeReferenceFrame == Other.AttitudeReferenceFrame
				&& bAllowDerivedFallback == Other.bAllowDerivedFallback;
		}

		friend uint32 GetTypeHash(const FPhysicalStreamKey& Key)
		{
			uint32 Hash = GetTypeHash(Key.Sensor);
			Hash = HashCombine(
				Hash,
				GetTypeHash(static_cast<uint8>(Key.AttitudeReferenceFrame))
			);
			return HashCombine(Hash, GetTypeHash(Key.bAllowDerivedFallback));
		}
	};

	struct FSubscriptionEntry
	{
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		FOpenMobileSensorSubscriptionRequest Request;
		FOpenMobileSensorStreamOptions AppliedOptions;
		FPhysicalStreamKey PhysicalKey;
		FOpenMobileSensorsBackendToken BackendToken;
		EOpenMobileSensorSubscriptionState State =
			EOpenMobileSensorSubscriptionState::Accepted;
		FOpenMobileError Error;
		double LastDeliveryTimestampSeconds = 0.0;
		bool bHasDeliveredSample = false;
	};

	struct FPhysicalStreamEntry
	{
		FOpenMobileSensorBackendStreamHandle Handle;
		FPhysicalStreamKey Key;
		FOpenMobileSensorPhysicalStreamRequest Request;
		FOpenMobileSensorsBackendToken BackendToken;
		IOpenMobileSensorsBackend* Backend = nullptr;
	};

	TMap<FGuid, FSubscriptionEntry> Subscriptions;
	TMap<FPhysicalStreamKey, FPhysicalStreamEntry> PhysicalStreams;
	FOnOpenMobileSensorSubscriptionServiceStateChanged StateChangedEvent;
	FTSTicker::FDelegateHandle PendingOperationsTickHandle;
	uint32 NextHandleGeneration = 1;
	bool bShuttingDown = false;

	FOpenMobileSensorOperationResult MakeSuccess(
		EOpenMobileSensorResultCode ResultCode =
			EOpenMobileSensorResultCode::Success
	)
	{
		FOpenMobileSensorOperationResult Result;
		Result.Code = ResultCode;
		return Result;
	}

	FOpenMobileSensorOperationResult MakeHandleFailure(
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		if (!Handle.IsValid())
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidHandle
			);
		}
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::StaleHandle
		);
	}

	uint32 AllocateGeneration()
	{
		const uint32 Generation = NextHandleGeneration++;
		if (NextHandleGeneration == 0)
		{
			NextHandleGeneration = 1;
		}
		return Generation == 0 ? NextHandleGeneration++ : Generation;
	}

	bool IsFiniteInRange(double Value, double Minimum, double Maximum)
	{
		return FMath::IsFinite(Value)
			&& Value >= Minimum
			&& Value <= Maximum;
	}

	template <typename EnumType>
	bool IsValidEnum(EnumType Value)
	{
		return StaticEnum<EnumType>()->IsValidEnumValue(
			static_cast<int64>(Value)
		);
	}

	bool ValidateFilterOptions(
		const FOpenMobileSensorFilterOptions& Filters
	)
	{
		return (!Filters.bEnableLowPass
				|| IsFiniteInRange(
					Filters.LowPassTimeConstantSeconds,
					0.0001,
					60.0
				))
			&& (!Filters.bEnableHighPass
				|| IsFiniteInRange(
					Filters.HighPassTimeConstantSeconds,
					0.0001,
					60.0
				))
			&& (!Filters.bEnableExponentialSmoothing
				|| IsFiniteInRange(
					Filters.SmoothingTimeConstantSeconds,
					0.0001,
					60.0
				))
			&& FMath::IsFinite(Filters.DeadZone)
			&& Filters.DeadZone >= 0.0;
	}

	bool ValidateAndResolveOptions(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Requested,
		FOpenMobileSensorStreamOptions& OutApplied
	)
	{
		const int32 AllowedAttitudeRepresentations =
			static_cast<int32>(
				EOpenMobileAttitudeRepresentation::Quaternion
			)
			| static_cast<int32>(
				EOpenMobileAttitudeRepresentation::EulerAngles
			)
			| static_cast<int32>(
				EOpenMobileAttitudeRepresentation::RotationMatrix
			);
		if (!Sensor.IsValid()
			|| !IsValidEnum(Requested.RatePreset)
			|| !IsValidEnum(Requested.DeliveryMode)
			|| !IsValidEnum(Requested.CoordinateSpace)
			|| !IsValidEnum(Requested.OverflowPolicy)
			|| !IsValidEnum(Requested.LifecyclePolicy)
			|| !IsValidEnum(Requested.AttitudeReferenceFrame)
			|| !IsFiniteInRange(Requested.CustomFrequencyHz, 1.0, 1000.0)
			|| !IsFiniteInRange(
				Requested.MaximumDeliveryLatencySeconds,
				0.0,
				10.0
			)
			|| !IsFiniteInRange(
				Requested.MaximumCallbackFrequencyHz,
				1.0,
				120.0
			)
			|| Requested.BufferCapacitySamples < 1
			|| Requested.BufferCapacitySamples > 65536
			|| !ValidateFilterOptions(Requested.Filters)
			|| Requested.AttitudeRepresentations == 0
			|| (Requested.AttitudeRepresentations
				& ~AllowedAttitudeRepresentations) != 0)
		{
			return false;
		}

		OutApplied = Requested;
		const UOpenMobileSensorsSettings* Settings =
			GetDefault<UOpenMobileSensorsSettings>();
		const FOpenMobileSensorRatePresetSettings* Preset = nullptr;
		switch (Requested.RatePreset)
		{
		case EOpenMobileSensorRatePreset::UI:
			Preset = &Settings->UIPreset;
			break;
		case EOpenMobileSensorRatePreset::Game:
			Preset = &Settings->GamePreset;
			break;
		case EOpenMobileSensorRatePreset::Fast:
			Preset = &Settings->FastPreset;
			break;
		case EOpenMobileSensorRatePreset::Custom:
		default:
			break;
		}
		if (Preset)
		{
			OutApplied.CustomFrequencyHz = Preset->RequestedFrequencyHz;
			OutApplied.MaximumDeliveryLatencySeconds =
				Preset->MaximumDeliveryLatencySeconds;
			OutApplied.MaximumCallbackFrequencyHz =
				Preset->MaximumCallbackFrequencyHz;
		}
		return IsFiniteInRange(OutApplied.CustomFrequencyHz, 1.0, 1000.0)
			&& IsFiniteInRange(
				OutApplied.MaximumDeliveryLatencySeconds,
				0.0,
				10.0
			)
			&& IsFiniteInRange(
				OutApplied.MaximumCallbackFrequencyHz,
				1.0,
				120.0
			);
	}

	FPhysicalStreamKey MakePhysicalKey(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Options
	)
	{
		FPhysicalStreamKey Key;
		Key.Sensor = Sensor;
		Key.bAllowDerivedFallback = Options.bAllowDerivedFallback;
		if (Sensor.Type == EOpenMobileSensorType::Attitude)
		{
			Key.AttitudeReferenceFrame = Options.AttitudeReferenceFrame;
		}
		return Key;
	}

	FSubscriptionEntry* FindOwnedEntry(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		if (!OwnerIdentifier.IsValid() || !Handle.IsValid())
		{
			return nullptr;
		}
		FSubscriptionEntry* Entry = Subscriptions.Find(
			Handle.GetIdentifier()
		);
		if (!Entry
			|| Entry->OwnerIdentifier != OwnerIdentifier
			|| Entry->Handle != Handle
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				Entry->BackendToken
			))
		{
			return nullptr;
		}
		return Entry;
	}

	FOpenMobileSensorSubscriptionStateSnapshot MakeSnapshot(
		const FSubscriptionEntry& Entry
	)
	{
		FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
		Snapshot.Handle = Entry.Handle;
		Snapshot.Sensor = Entry.Request.Sensor;
		Snapshot.State = Entry.State;
		Snapshot.RequestedOptions = Entry.Request.Options;
		Snapshot.AppliedOptions = Entry.AppliedOptions;
		Snapshot.Error = Entry.Error;
		return Snapshot;
	}

	void BroadcastState(const FSubscriptionEntry& Entry)
	{
		const FGuid OwnerIdentifier = Entry.OwnerIdentifier;
		const FOpenMobileSensorSubscriptionStateSnapshot Snapshot =
			MakeSnapshot(Entry);
		StateChangedEvent.Broadcast(OwnerIdentifier, Snapshot);
	}

	void SetState(
		const FGuid& Identifier,
		EOpenMobileSensorSubscriptionState State,
		const FOpenMobileError& Error = {}
	)
	{
		FSubscriptionEntry* Entry = Subscriptions.Find(Identifier);
		if (!Entry)
		{
			return;
		}
		Entry->State = State;
		Entry->Error = Error;
		FOpenMobileSensorsSampleService::SetSubscriptionState(
			Entry->Handle,
			State
		);
		BroadcastState(*Entry);
	}

	bool BuildPhysicalRequest(
		const FPhysicalStreamKey& Key,
		FOpenMobileSensorPhysicalStreamRequest& OutRequest
	)
	{
		bool bFound = false;
		OutRequest = {};
		OutRequest.Sensor = Key.Sensor;
		OutRequest.AttitudeReferenceFrame = Key.AttitudeReferenceFrame;
		OutRequest.bAllowDerivedFallback = Key.bAllowDerivedFallback;
		for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			const FSubscriptionEntry& Entry = Pair.Value;
			if (!(Entry.PhysicalKey == Key)
				|| (Entry.State != EOpenMobileSensorSubscriptionState::Accepted
					&& Entry.State !=
						EOpenMobileSensorSubscriptionState::Starting
					&& Entry.State !=
						EOpenMobileSensorSubscriptionState::Active))
			{
				continue;
			}
			if (!bFound)
			{
				OutRequest.MaximumDeliveryLatencySeconds =
					Entry.AppliedOptions.MaximumDeliveryLatencySeconds;
				bFound = true;
			}
			else
			{
				OutRequest.MaximumDeliveryLatencySeconds = FMath::Min(
					OutRequest.MaximumDeliveryLatencySeconds,
					Entry.AppliedOptions.MaximumDeliveryLatencySeconds
				);
			}
			OutRequest.RequestedFrequencyHz = FMath::Max(
				OutRequest.RequestedFrequencyHz,
				Entry.AppliedOptions.CustomFrequencyHz
			);
			OutRequest.bAllowHighSamplingRate |=
				Entry.AppliedOptions.bAllowHighSamplingRate;
			OutRequest.bLowLatency |= Entry.AppliedOptions.bLowLatency;
		}
		return bFound;
	}

	FOpenMobileError GetOperationError(
		const FOpenMobileSensorOperationResult& Operation
	)
	{
		return Operation.Error.IsSet()
			? Operation.Error
			: FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::OperationalFailure
			).Error;
	}

	void CancelPendingOperationsTick()
	{
		if (PendingOperationsTickHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(
				PendingOperationsTickHandle
			);
			PendingOperationsTickHandle.Reset();
		}
	}

	void ProcessPendingBackendOperations();

	bool TickPendingBackendOperations(float DeltaSeconds)
	{
		static_cast<void>(DeltaSeconds);
		PendingOperationsTickHandle.Reset();
		ProcessPendingBackendOperations();
		return false;
	}

	void SchedulePendingBackendOperations()
	{
		if (!PendingOperationsTickHandle.IsValid())
		{
			PendingOperationsTickHandle =
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateStatic(
						&TickPendingBackendOperations
					)
				);
		}
	}

	void ReconcilePhysicalStream(const FPhysicalStreamKey& Key)
	{
		FPhysicalStreamEntry* Physical = PhysicalStreams.Find(Key);
		if (!Physical)
		{
			return;
		}
		FOpenMobileSensorPhysicalStreamRequest DesiredRequest;
		if (!BuildPhysicalRequest(Key, DesiredRequest))
		{
			if (Physical->Backend
				&& FOpenMobileSensorsBackendRegistry::IsBackendRegistered(
					Physical->Backend
				))
			{
				Physical->Backend->StopSensorStream(Physical->Handle);
			}
			PhysicalStreams.Remove(Key);
			return;
		}
		if (DesiredRequest == Physical->Request
			|| !Physical->Backend
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				Physical->BackendToken
			))
		{
			return;
		}
		FOpenMobileSensorPhysicalStreamRequest AppliedRequest = DesiredRequest;
		const FOpenMobileSensorOperationResult Result =
			Physical->Backend->ReconfigureSensorStream(
				Physical->Handle,
				AppliedRequest
			);
		if (Result.IsSuccess())
		{
			Physical->Request = MoveTemp(AppliedRequest);
		}
	}

	void ProcessPendingKey(const FPhysicalStreamKey& Key)
	{
		TArray<FGuid> PendingIdentifiers;
		for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			if (Pair.Value.State ==
					EOpenMobileSensorSubscriptionState::Accepted
				&& Pair.Value.PhysicalKey == Key)
			{
				PendingIdentifiers.Add(Pair.Key);
			}
		}
		for (const FGuid& Identifier : PendingIdentifiers)
		{
			SetState(
				Identifier,
				EOpenMobileSensorSubscriptionState::Starting
			);
		}

		TArray<FGuid> StartingIdentifiers;
		for (const FGuid& Identifier : PendingIdentifiers)
		{
			const FSubscriptionEntry* Entry = Subscriptions.Find(Identifier);
			if (Entry
				&& Entry->State ==
					EOpenMobileSensorSubscriptionState::Starting
				&& Entry->PhysicalKey == Key)
			{
				StartingIdentifiers.Add(Identifier);
			}
		}
		if (StartingIdentifiers.IsEmpty())
		{
			ReconcilePhysicalStream(Key);
			return;
		}

		FOpenMobileSensorPhysicalStreamRequest DesiredRequest;
		if (!BuildPhysicalRequest(Key, DesiredRequest))
		{
			return;
		}
		const FSubscriptionEntry* FirstEntry =
			Subscriptions.Find(StartingIdentifiers[0]);
		IOpenMobileSensorsBackend* Backend =
			FOpenMobileSensorsBackendRegistry::FindBackend();
		FOpenMobileSensorOperationResult Operation;
		Operation.Code = EOpenMobileSensorResultCode::Failed;
		FPhysicalStreamEntry* ExistingPhysical = PhysicalStreams.Find(Key);
		FOpenMobileSensorBackendStreamHandle NewPhysicalHandle;
		if (!FirstEntry
			|| !Backend
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				FirstEntry->BackendToken
			))
		{
			Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::TemporarilyUnavailable
			);
		}
		else if (ExistingPhysical)
		{
			if (DesiredRequest == ExistingPhysical->Request)
			{
				Operation = MakeSuccess();
			}
			else
			{
				Operation = Backend->ReconfigureSensorStream(
					ExistingPhysical->Handle,
					DesiredRequest
				);
			}
		}
		else
		{
			NewPhysicalHandle.Identifier = FGuid::NewGuid();
			Operation = Backend->StartSensorStream(
				NewPhysicalHandle,
				DesiredRequest
			);
		}

		if (Operation.IsSuccess())
		{
			if (ExistingPhysical)
			{
				ExistingPhysical->Request = DesiredRequest;
			}
			else
			{
				FPhysicalStreamEntry Physical;
				Physical.Handle = NewPhysicalHandle;
				Physical.Key = Key;
				Physical.Request = DesiredRequest;
				Physical.BackendToken = FirstEntry->BackendToken;
				Physical.Backend = Backend;
				PhysicalStreams.Add(Key, MoveTemp(Physical));
			}
			for (const FGuid& Identifier : StartingIdentifiers)
			{
				SetState(
					Identifier,
					EOpenMobileSensorSubscriptionState::Active
				);
			}
			return;
		}

		const FOpenMobileError Error = GetOperationError(Operation);
		for (const FGuid& Identifier : StartingIdentifiers)
		{
			SetState(
				Identifier,
				EOpenMobileSensorSubscriptionState::Failed,
				Error
			);
		}
	}

	void ProcessPendingBackendOperations()
	{
		CancelPendingOperationsTick();
		if (bShuttingDown)
		{
			return;
		}
		TSet<FPhysicalStreamKey> PendingKeys;
		for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			if (Pair.Value.State ==
				EOpenMobileSensorSubscriptionState::Accepted)
			{
				PendingKeys.Add(Pair.Value.PhysicalKey);
			}
		}
		for (const FPhysicalStreamKey& Key : PendingKeys)
		{
			ProcessPendingKey(Key);
		}
	}

	void StopPhysicalStreams()
	{
		for (const TPair<FPhysicalStreamKey, FPhysicalStreamEntry>& Pair
			: PhysicalStreams)
		{
			const FPhysicalStreamEntry& Physical = Pair.Value;
			if (Physical.Backend
				&& FOpenMobileSensorsBackendRegistry::IsBackendRegistered(
					Physical.Backend
				))
			{
				Physical.Backend->StopSensorStream(Physical.Handle);
			}
		}
		PhysicalStreams.Reset();
	}
}

void FOpenMobileSensorsSubscriptionService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	CancelPendingOperationsTick();
	bShuttingDown = false;
	Subscriptions.Reset();
	PhysicalStreams.Reset();
	FOpenMobileSensorsSampleService::UnregisterAll();
}

void FOpenMobileSensorsSubscriptionService::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (bShuttingDown)
	{
		return;
	}
	bShuttingDown = true;
	CancelPendingOperationsTick();
	StopPhysicalStreams();
	Subscriptions.Reset();
	FOpenMobileSensorsSampleService::UnregisterAll();
	StateChangedEvent.Clear();
}

void FOpenMobileSensorsSubscriptionService::HandleBackendGenerationChanged()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	CancelPendingOperationsTick();
	StopPhysicalStreams();
	Subscriptions.Reset();
	FOpenMobileSensorsSampleService::UnregisterAll();
}

FOpenMobileSensorSubscriptionResult
FOpenMobileSensorsSubscriptionService::StartSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionRequest& Request
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	FOpenMobileSensorSubscriptionResult Result;
	Result.RequestedOptions = Request.Options;
	if (!OwnerIdentifier.IsValid())
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
		return Result;
	}
	FOpenMobileSensorStreamOptions AppliedOptions;
	if (!ValidateAndResolveOptions(
		Request.Sensor,
		Request.Options,
		AppliedOptions
	))
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
		return Result;
	}
	Result.AppliedOptions = AppliedOptions;
	if (bShuttingDown)
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
		return Result;
	}
	const FOpenMobileSensorsBackendToken BackendToken =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	if (BackendToken.Generation == 0)
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedPlatform
		);
		return Result;
	}

	FOpenMobileSensorSubscriptionHandle Handle;
	do
	{
		Handle.Identifier = FGuid::NewGuid();
	}
	while (!Handle.Identifier.IsValid()
		|| Subscriptions.Contains(Handle.Identifier));
	Handle.Generation = AllocateGeneration();

	FSubscriptionEntry Entry;
	Entry.OwnerIdentifier = OwnerIdentifier;
	Entry.Handle = Handle;
	Entry.Request = Request;
	Entry.AppliedOptions = AppliedOptions;
	Entry.PhysicalKey = MakePhysicalKey(Request.Sensor, AppliedOptions);
	Entry.BackendToken = BackendToken;
	Subscriptions.Add(Handle.Identifier, MoveTemp(Entry));
	FOpenMobileSensorsSampleService::RegisterSubscription(
		OwnerIdentifier,
		Handle,
		Request.Sensor,
		AppliedOptions
	);
	SchedulePendingBackendOperations();

	Result.Handle = Handle;
	Result.Operation = MakeSuccess(EOpenMobileSensorResultCode::Accepted);
	return Result;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::UpdateSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorStreamOptions& Options
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeHandleFailure(Handle);
	}
	if (Entry->State == EOpenMobileSensorSubscriptionState::Starting
		|| Entry->State == EOpenMobileSensorSubscriptionState::Failed)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	FOpenMobileSensorStreamOptions AppliedOptions;
	if (!ValidateAndResolveOptions(
		Entry->Request.Sensor,
		Options,
		AppliedOptions
	))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
	}
	const FPhysicalStreamKey NewKey = MakePhysicalKey(
		Entry->Request.Sensor,
		AppliedOptions
	);
	if (Entry->State == EOpenMobileSensorSubscriptionState::Active
		&& !(NewKey == Entry->PhysicalKey))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}

	const FOpenMobileSensorStreamOptions PreviousRequested =
		Entry->Request.Options;
	const FOpenMobileSensorStreamOptions PreviousApplied =
		Entry->AppliedOptions;
	const FPhysicalStreamKey PreviousKey = Entry->PhysicalKey;
	Entry->Request.Options = Options;
	Entry->AppliedOptions = AppliedOptions;
	Entry->PhysicalKey = NewKey;
	if (Entry->State != EOpenMobileSensorSubscriptionState::Active)
	{
		FOpenMobileSensorsSampleService::UpdateSubscriptionOptions(
			Handle,
			AppliedOptions
		);
		return MakeSuccess();
	}

	FPhysicalStreamEntry* Physical = PhysicalStreams.Find(PreviousKey);
	FOpenMobileSensorPhysicalStreamRequest DesiredRequest;
	if (!Physical || !BuildPhysicalRequest(PreviousKey, DesiredRequest))
	{
		Entry->Request.Options = PreviousRequested;
		Entry->AppliedOptions = PreviousApplied;
		Entry->PhysicalKey = PreviousKey;
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	if (DesiredRequest == Physical->Request)
	{
		FOpenMobileSensorsSampleService::UpdateSubscriptionOptions(
			Handle,
			AppliedOptions
		);
		return MakeSuccess();
	}
	FOpenMobileSensorPhysicalStreamRequest BackendRequest = DesiredRequest;
	const FOpenMobileSensorOperationResult ReconfigureResult =
		Physical->Backend->ReconfigureSensorStream(
			Physical->Handle,
			BackendRequest
		);
	if (!ReconfigureResult.IsSuccess())
	{
		Entry->Request.Options = PreviousRequested;
		Entry->AppliedOptions = PreviousApplied;
		Entry->PhysicalKey = PreviousKey;
		return ReconfigureResult;
	}
	Physical->Request = MoveTemp(BackendRequest);
	FOpenMobileSensorsSampleService::UpdateSubscriptionOptions(
		Handle,
		AppliedOptions
	);
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::StopSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeHandleFailure(Handle);
	}
	if (Entry->State == EOpenMobileSensorSubscriptionState::Stopping)
	{
		return MakeSuccess();
	}
	const FSubscriptionEntry StoppedEntry = *Entry;
	SetState(
		Handle.GetIdentifier(),
		EOpenMobileSensorSubscriptionState::Stopping
	);
	Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeSuccess();
	}
	const FPhysicalStreamKey Key = Entry->PhysicalKey;
	Subscriptions.Remove(Handle.GetIdentifier());
	FOpenMobileSensorsSampleService::UnregisterSubscription(Handle);
	ReconcilePhysicalStream(Key);
	FSubscriptionEntry FinalEntry = StoppedEntry;
	FinalEntry.State = EOpenMobileSensorSubscriptionState::Stopped;
	FinalEntry.Error = {};
	BroadcastState(FinalEntry);
	return MakeSuccess();
}

int32 FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(
	const FGuid& OwnerIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (!OwnerIdentifier.IsValid())
	{
		return 0;
	}
	TArray<FOpenMobileSensorSubscriptionHandle> Handles;
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		if (Pair.Value.OwnerIdentifier == OwnerIdentifier)
		{
			Handles.Add(Pair.Value.Handle);
		}
	}
	int32 Removed = 0;
	for (const FOpenMobileSensorSubscriptionHandle& Handle : Handles)
	{
		Removed += StopSubscription(OwnerIdentifier, Handle).IsSuccess() ? 1 : 0;
	}
	return Removed;
}

bool FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	FOpenMobileSensorSubscriptionStateSnapshot& OutState
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	OutState = {};
	OutState.Handle = Handle;
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		OutState.Error = MakeHandleFailure(Handle).Error;
		return false;
	}
	OutState = MakeSnapshot(*Entry);
	return true;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::GetHandleStatus(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	return FindOwnedEntry(OwnerIdentifier, Handle)
		? MakeSuccess()
		: MakeHandleFailure(Handle);
}

bool FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	return OpenMobileSensorsSubscriptionServicePrivate::FindOwnedEntry(
		OwnerIdentifier,
		Handle
	) != nullptr;
}

bool FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (!Handle.IsValid())
	{
		return false;
	}
	FSubscriptionEntry* Entry = Subscriptions.Find(Handle.GetIdentifier());
	return Entry
		&& Entry->Handle == Handle
		&& FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
			Entry->BackendToken
		);
}

TArray<FOpenMobileSensorSubscriptionHandle>
FOpenMobileSensorsSubscriptionService::SelectSubscribersForSample(
	const FOpenMobileSensorIdentifier& Sensor,
	double TimestampSeconds
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<FOpenMobileSensorSubscriptionHandle> DueSubscribers;
	if (!FMath::IsFinite(TimestampSeconds) || TimestampSeconds < 0.0)
	{
		return DueSubscribers;
	}
	constexpr double DeliveryToleranceSeconds = 1.e-9;
	for (TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		FSubscriptionEntry& Entry = Pair.Value;
		if (Entry.State != EOpenMobileSensorSubscriptionState::Active
			|| Entry.Request.Sensor != Sensor)
		{
			continue;
		}
		const double IntervalSeconds =
			1.0 / Entry.AppliedOptions.MaximumCallbackFrequencyHz;
		if (Entry.bHasDeliveredSample
			&& TimestampSeconds + DeliveryToleranceSeconds
				< Entry.LastDeliveryTimestampSeconds + IntervalSeconds)
		{
			continue;
		}
		Entry.bHasDeliveredSample = true;
		Entry.LastDeliveryTimestampSeconds = TimestampSeconds;
		DueSubscribers.Add(Entry.Handle);
	}
	return DueSubscribers;
}

void FOpenMobileSensorsSubscriptionService::
InvalidateForUnrecoverablePermissionLoss(EOpenMobileSensorType SensorType)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<TPair<FGuid, FOpenMobileSensorSubscriptionHandle>> Handles;
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		if (Pair.Value.Request.Sensor.Type == SensorType)
		{
			Handles.Emplace(Pair.Value.OwnerIdentifier, Pair.Value.Handle);
		}
	}
	for (const TPair<FGuid, FOpenMobileSensorSubscriptionHandle>& Pair : Handles)
	{
		StopSubscription(Pair.Key, Pair.Value);
	}
}

FOnOpenMobileSensorSubscriptionServiceStateChanged&
FOpenMobileSensorsSubscriptionService::OnStateChanged()
{
	return OpenMobileSensorsSubscriptionServicePrivate::StateChangedEvent;
}

#if WITH_DEV_AUTOMATION_TESTS
int32 FOpenMobileSensorsSubscriptionService::
GetActiveSubscriptionCountForTests()
{
	check(IsInGameThread());
	return OpenMobileSensorsSubscriptionServicePrivate::Subscriptions.Num();
}

int32 FOpenMobileSensorsSubscriptionService::
GetActiveSubscriptionCountForTests(
	const FOpenMobileSensorIdentifier& Sensor
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	int32 Count = 0;
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		if (Pair.Value.Request.Sensor == Sensor)
		{
			++Count;
		}
	}
	return Count;
}

int32 FOpenMobileSensorsSubscriptionService::
GetPhysicalStreamCountForTests()
{
	check(IsInGameThread());
	return OpenMobileSensorsSubscriptionServicePrivate::PhysicalStreams.Num();
}

void FOpenMobileSensorsSubscriptionService::
ProcessPendingBackendOperationsForTests()
{
	check(IsInGameThread());
	OpenMobileSensorsSubscriptionServicePrivate::
		ProcessPendingBackendOperations();
}

void FOpenMobileSensorsSubscriptionService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	CancelPendingOperationsTick();
	Subscriptions.Reset();
	PhysicalStreams.Reset();
	FOpenMobileSensorsSampleService::UnregisterAll();
	StateChangedEvent.Clear();
	NextHandleGeneration = 1;
	bShuttingDown = false;
}
#endif
