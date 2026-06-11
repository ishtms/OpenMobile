#include "OpenMobileSensorsSubscriptionService.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobileSensorsBackend.h"
#include "OpenMobileAsync.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
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
		FOpenMobileSensorRateResolution RateResolution;
		EOpenMobileSensorRateAdjustmentReason CommonRateAdjustmentReason =
			EOpenMobileSensorRateAdjustmentReason::None;
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

	struct FPendingFlush
	{
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		FOpenMobileSensorBackendStreamHandle PhysicalStreamHandle;
		FOpenMobileSensorsBackendToken BackendToken;
		double DeadlineSeconds = 0.0;
		TFunction<void(const FOpenMobileSensorFlushResult&)> Completion;
	};

	TMap<FGuid, FSubscriptionEntry> Subscriptions;
	TMap<FPhysicalStreamKey, FPhysicalStreamEntry> PhysicalStreams;
	FOnOpenMobileSensorSubscriptionServiceStateChanged StateChangedEvent;
	FTSTicker::FDelegateHandle PendingOperationsTickHandle;
	FTSTicker::FDelegateHandle FlushTimeoutTickHandle;
	TMap<FGuid, FPendingFlush> PendingFlushes;
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

	bool ResolvePreset(
		const FOpenMobileSensorStreamOptions& Requested,
		const UOpenMobileSensorsSettings& Settings,
		FOpenMobileSensorStreamOptions& OutApplied
	)
	{
		OutApplied = Requested;
		const FOpenMobileSensorRatePresetSettings* Preset = nullptr;
		switch (Requested.RatePreset)
		{
		case EOpenMobileSensorRatePreset::UI:
			Preset = &Settings.UIPreset;
			break;
		case EOpenMobileSensorRatePreset::Game:
			Preset = &Settings.GamePreset;
			break;
		case EOpenMobileSensorRatePreset::Fast:
			Preset = &Settings.FastPreset;
			break;
		case EOpenMobileSensorRatePreset::Custom:
			return true;
		default:
			return false;
		}
		OutApplied.CustomFrequencyHz = Preset->RequestedFrequencyHz;
		OutApplied.MaximumDeliveryLatencySeconds =
			Preset->MaximumDeliveryLatencySeconds;
		OutApplied.MaximumCallbackFrequencyHz =
			Preset->MaximumCallbackFrequencyHz;
		return true;
	}

	void ApplyRateLimits(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Requested,
		const UOpenMobileSensorsSettings& Settings,
		FOpenMobileSensorStreamOptions& OutApplied,
		FOpenMobileSensorRateResolution& OutResolution
	)
	{
		constexpr double NormalMaximumFrequencyHz = 200.0;
		OutResolution = {};
		OutResolution.RequestedFrequencyHz = OutApplied.CustomFrequencyHz;
		const FOpenMobileSensorCapabilitySnapshot Snapshot =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		const FOpenMobileSensorCapability* Capability =
			Snapshot.Sensors.FindByPredicate(
				[&Sensor](const FOpenMobileSensorCapability& Candidate)
				{
					return Candidate.Sensor.Type == Sensor.Type
						&& (Sensor.InstanceId.IsNone()
							|| Candidate.Sensor.InstanceId ==
								Sensor.InstanceId);
				}
			);
		if (Capability)
		{
			const bool bHasMinimum =
				FMath::IsFinite(Capability->MinimumFrequencyHz)
				&& Capability->MinimumFrequencyHz > 0.0;
			const bool bHasMaximum =
				FMath::IsFinite(Capability->MaximumFrequencyHz)
				&& Capability->MaximumFrequencyHz > 0.0;
			if (bHasMinimum && bHasMaximum
				&& Capability->MinimumFrequencyHz <=
					Capability->MaximumFrequencyHz)
			{
				OutApplied.CustomFrequencyHz = FMath::Clamp(
					OutApplied.CustomFrequencyHz,
					Capability->MinimumFrequencyHz,
					Capability->MaximumFrequencyHz
				);
			}
			else if (bHasMaximum)
			{
				OutApplied.CustomFrequencyHz = FMath::Min(
					OutApplied.CustomFrequencyHz,
					Capability->MaximumFrequencyHz
				);
			}
			else if (bHasMinimum)
			{
				OutApplied.CustomFrequencyHz = FMath::Max(
					OutApplied.CustomFrequencyHz,
					Capability->MinimumFrequencyHz
				);
			}
			if (OutApplied.CustomFrequencyHz !=
				OutResolution.RequestedFrequencyHz)
			{
				OutResolution.AdjustmentReason =
					EOpenMobileSensorRateAdjustmentReason::HardwareLimit;
			}
		}
		if (!Requested.bAllowHighSamplingRate
			|| !Settings.bAllowHighSamplingRate)
		{
			const double BeforeProjectLimit =
				OutApplied.CustomFrequencyHz;
			OutApplied.CustomFrequencyHz = FMath::Min(
				OutApplied.CustomFrequencyHz,
				NormalMaximumFrequencyHz
			);
			if (OutApplied.CustomFrequencyHz < BeforeProjectLimit)
			{
				OutResolution.AdjustmentReason =
					EOpenMobileSensorRateAdjustmentReason::ProjectPolicy;
			}
		}
		OutApplied.MaximumCallbackFrequencyHz = FMath::Min(
			OutApplied.MaximumCallbackFrequencyHz,
			OutApplied.CustomFrequencyHz
		);
		OutResolution.ClampedFrequencyHz = OutApplied.CustomFrequencyHz;
		OutResolution.AppliedNativeFrequencyHz =
			OutApplied.CustomFrequencyHz;
	}

	bool ValidateAndResolveOptions(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Requested,
		FOpenMobileSensorStreamOptions& OutApplied,
		FOpenMobileSensorRateResolution& OutRateResolution
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
			|| Requested.BufferCapacitySamples > 4096
			|| !ValidateFilterOptions(Requested.Filters)
			|| Requested.AttitudeRepresentations == 0
			|| (Requested.AttitudeRepresentations
				& ~AllowedAttitudeRepresentations) != 0)
		{
			return false;
		}

		const UOpenMobileSensorsSettings* Settings =
			GetDefault<UOpenMobileSensorsSettings>();
		if (!ResolvePreset(Requested, *Settings, OutApplied))
		{
			return false;
		}
		ApplyRateLimits(
			Sensor,
			Requested,
			*Settings,
			OutApplied,
			OutRateResolution
		);
		if (OutApplied.bLowLatency)
		{
			OutApplied.MaximumDeliveryLatencySeconds = 0.0;
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
		Snapshot.RateResolution = Entry.RateResolution;
		Snapshot.Error = Entry.Error;
		return Snapshot;
	}

	void CancelFlushTimeoutTick()
	{
		if (FlushTimeoutTickHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(FlushTimeoutTickHandle);
			FlushTimeoutTickHandle.Reset();
		}
	}

	void CompleteFlush(
		const FGuid& RequestId,
		FOpenMobileSensorOperationResult Operation
	)
	{
		FPendingFlush Pending;
		if (!PendingFlushes.RemoveAndCopyValue(RequestId, Pending))
		{
			return;
		}
		FOpenMobileSensorFlushResult Result;
		Result.RequestId = RequestId;
		Result.Handle = Pending.Handle;
		Result.Operation = MoveTemp(Operation);
		if (Result.Operation.IsSuccess()
			&& !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				Pending.BackendToken
			))
		{
			Result.Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::Cancelled
			);
		}
		if (Result.Operation.IsSuccess()
			&& !FOpenMobileSensorsSampleService::FlushPluginSamples(
				Pending.OwnerIdentifier,
				Pending.Handle,
				Result.FlushedSamples
			))
		{
			Result.Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::StaleHandle
			);
			Result.FlushedSamples = 0;
		}
		if (PendingFlushes.IsEmpty())
		{
			CancelFlushTimeoutTick();
		}
		if (Pending.Completion)
		{
			Pending.Completion(Result);
		}
	}

	void ProcessFlushTimeouts(double NowSeconds)
	{
		TArray<FGuid> TimedOutRequests;
		for (const TPair<FGuid, FPendingFlush>& Pair : PendingFlushes)
		{
			if (NowSeconds >= Pair.Value.DeadlineSeconds)
			{
				TimedOutRequests.Add(Pair.Key);
			}
		}
		for (const FGuid& RequestId : TimedOutRequests)
		{
			CompleteFlush(
				RequestId,
				FOpenMobileSensorsErrorMapper::Map(
					EOpenMobileSensorFailureReason::OperationalFailure,
					TEXT("OpenMobileSensors"),
					TEXT("FlushTimeout")
				)
			);
		}
	}

	bool TickFlushTimeouts(float DeltaSeconds)
	{
		static_cast<void>(DeltaSeconds);
		FlushTimeoutTickHandle.Reset();
		ProcessFlushTimeouts(FPlatformTime::Seconds());
		if (!PendingFlushes.IsEmpty())
		{
			FlushTimeoutTickHandle =
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateStatic(&TickFlushTimeouts),
					0.1f
				);
		}
		return false;
	}

	void EnsureFlushTimeoutTick()
	{
		if (!FlushTimeoutTickHandle.IsValid())
		{
			FlushTimeoutTickHandle =
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateStatic(&TickFlushTimeouts),
					0.1f
				);
		}
	}

	void CancelFlushesForPhysicalStream(
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle
	)
	{
		TArray<FGuid> RequestIds;
		for (const TPair<FGuid, FPendingFlush>& Pair : PendingFlushes)
		{
			if (Pair.Value.PhysicalStreamHandle == PhysicalStreamHandle)
			{
				RequestIds.Add(Pair.Key);
			}
		}
		for (const FGuid& RequestId : RequestIds)
		{
			CompleteFlush(
				RequestId,
				FOpenMobileSensorsErrorMapper::Map(
					EOpenMobileSensorFailureReason::Cancelled
				)
			);
		}
	}

	void CancelFlushesForHandle(
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		TArray<FGuid> RequestIds;
		for (const TPair<FGuid, FPendingFlush>& Pair : PendingFlushes)
		{
			if (Pair.Value.Handle == Handle)
			{
				RequestIds.Add(Pair.Key);
			}
		}
		for (const FGuid& RequestId : RequestIds)
		{
			CompleteFlush(
				RequestId,
				FOpenMobileSensorsErrorMapper::Map(
					EOpenMobileSensorFailureReason::Cancelled
				)
			);
		}
	}

	void CancelAllFlushes()
	{
		TArray<FGuid> RequestIds;
		PendingFlushes.GetKeys(RequestIds);
		for (const FGuid& RequestId : RequestIds)
		{
			CompleteFlush(
				RequestId,
				FOpenMobileSensorsErrorMapper::Map(
					EOpenMobileSensorFailureReason::Cancelled
				)
			);
		}
		CancelFlushTimeoutTick();
	}

	bool SupportsNativeBatching(
		const FOpenMobileSensorIdentifier& Sensor
	)
	{
		const FOpenMobileSensorCapabilitySnapshot Snapshot =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		const FOpenMobileSensorCapability* Capability =
			Snapshot.Sensors.FindByPredicate(
				[&Sensor](const FOpenMobileSensorCapability& Candidate)
				{
					return Candidate.Sensor.Type == Sensor.Type
						&& (Sensor.InstanceId.IsNone()
							|| Candidate.Sensor.InstanceId ==
								Sensor.InstanceId);
				}
			);
		return Capability && Capability->bSupportsNativeBatching;
	}

	EOpenMobileSensorBatchingMode GetBatchingMode(
		const FSubscriptionEntry& Entry
	)
	{
		if (Entry.AppliedOptions.bLowLatency
			|| Entry.AppliedOptions.MaximumDeliveryLatencySeconds <= 0.0)
		{
			return EOpenMobileSensorBatchingMode::Disabled;
		}
		const FPhysicalStreamEntry* Physical =
			PhysicalStreams.Find(Entry.PhysicalKey);
		if (Physical && Physical->Request.bNativeBatchingApplied)
		{
			return EOpenMobileSensorBatchingMode::Native;
		}
		if (Entry.AppliedOptions.DeliveryMode ==
				EOpenMobileSensorDeliveryMode::Buffered
			|| Entry.AppliedOptions.DeliveryMode ==
				EOpenMobileSensorDeliveryMode::EventBatches)
		{
			return EOpenMobileSensorBatchingMode::Plugin;
		}
		return EOpenMobileSensorBatchingMode::Unavailable;
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
		const FOpenMobileSensorSubscriptionHandle EntryHandle = Entry->Handle;
		Entry->State = State;
		Entry->Error = Error;
		FOpenMobileSensorsSampleService::SetSubscriptionState(
			Entry->Handle,
			State
		);
		BroadcastState(*Entry);
		if (State == EOpenMobileSensorSubscriptionState::Paused
			|| State == EOpenMobileSensorSubscriptionState::Stopping
			|| State == EOpenMobileSensorSubscriptionState::Stopped
			|| State == EOpenMobileSensorSubscriptionState::Failed)
		{
			CancelFlushesForHandle(EntryHandle);
		}
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
		if (bFound && (OutRequest.bLowLatency
			|| OutRequest.MaximumDeliveryLatencySeconds <= 0.0))
		{
			OutRequest.MaximumDeliveryLatencySeconds = 0.0;
		}
		OutRequest.bNativeBatchingRequested = bFound
			&& OutRequest.MaximumDeliveryLatencySeconds > 0.0
			&& SupportsNativeBatching(Key.Sensor);
		return bFound;
	}

	void UpdateAppliedNativeRate(
		const FPhysicalStreamKey& Key,
		double AppliedNativeFrequencyHz
	)
	{
		if (!FMath::IsFinite(AppliedNativeFrequencyHz)
			|| AppliedNativeFrequencyHz <= 0.0)
		{
			return;
		}
		constexpr double RateToleranceHz = 1.e-9;
		for (TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			FSubscriptionEntry& Entry = Pair.Value;
			if (!(Entry.PhysicalKey == Key))
			{
				continue;
			}
			Entry.RateResolution.AppliedNativeFrequencyHz =
				AppliedNativeFrequencyHz;
			Entry.RateResolution.AdjustmentReason =
				Entry.CommonRateAdjustmentReason;
			if (AppliedNativeFrequencyHz + RateToleranceHz <
				Entry.RateResolution.ClampedFrequencyHz)
			{
				Entry.RateResolution.AdjustmentReason =
					EOpenMobileSensorRateAdjustmentReason::BackendLimit;
			}
		}
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
			UpdateAppliedNativeRate(
				Key,
				Physical->Request.RequestedFrequencyHz
			);
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
				const FOpenMobileSensorBackendStreamHandle ExistingHandle =
					ExistingPhysical->Handle;
				CancelFlushesForPhysicalStream(ExistingHandle);
				ExistingPhysical = PhysicalStreams.Find(Key);
				if (ExistingPhysical)
				{
					Operation = Backend->ReconfigureSensorStream(
						ExistingPhysical->Handle,
						DesiredRequest
					);
				}
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
			const FOpenMobileSensorBackendStreamHandle ActivePhysicalHandle =
				ExistingPhysical
				? ExistingPhysical->Handle
				: NewPhysicalHandle;
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
			UpdateAppliedNativeRate(
				Key,
				DesiredRequest.RequestedFrequencyHz
			);
			for (const FGuid& Identifier : StartingIdentifiers)
			{
				const FSubscriptionEntry* Entry =
					Subscriptions.Find(Identifier);
				if (Entry)
				{
					FOpenMobileSensorsSampleService::SetPhysicalStreamHandle(
						Entry->Handle,
						ActivePhysicalHandle
					);
				}
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
	CancelAllFlushes();
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
	CancelAllFlushes();
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
	CancelAllFlushes();
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
	Result.RateResolution.RequestedFrequencyHz =
		Request.Options.CustomFrequencyHz;
	if (!OwnerIdentifier.IsValid())
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
		return Result;
	}
	if (!IsFiniteInRange(Request.Options.CustomFrequencyHz, 1.0, 1000.0))
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidFrequency
		);
		return Result;
	}
	FOpenMobileSensorStreamOptions AppliedOptions;
	FOpenMobileSensorRateResolution RateResolution;
	if (!ValidateAndResolveOptions(
		Request.Sensor,
		Request.Options,
		AppliedOptions,
		RateResolution
	))
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
		return Result;
	}
	Result.AppliedOptions = AppliedOptions;
	Result.RateResolution = RateResolution;
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
	Entry.RateResolution = RateResolution;
	Entry.CommonRateAdjustmentReason = RateResolution.AdjustmentReason;
	Entry.PhysicalKey = MakePhysicalKey(Request.Sensor, AppliedOptions);
	Entry.BackendToken = BackendToken;
	Subscriptions.Add(Handle.Identifier, MoveTemp(Entry));
	FOpenMobileSensorsSampleService::RegisterSubscription(
		OwnerIdentifier,
		Handle,
		Request.Sensor,
		AppliedOptions,
		BackendToken.Generation
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
	if (!IsFiniteInRange(Options.CustomFrequencyHz, 1.0, 1000.0))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidFrequency
		);
	}
	FOpenMobileSensorStreamOptions AppliedOptions;
	FOpenMobileSensorRateResolution RateResolution;
	if (!ValidateAndResolveOptions(
		Entry->Request.Sensor,
		Options,
		AppliedOptions,
		RateResolution
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
	if (Entry->State == EOpenMobileSensorSubscriptionState::Active)
	{
		if (const FPhysicalStreamEntry* Physical =
			PhysicalStreams.Find(Entry->PhysicalKey))
		{
			CancelFlushesForPhysicalStream(Physical->Handle);
			Entry = FindOwnedEntry(OwnerIdentifier, Handle);
			if (!Entry)
			{
				return MakeHandleFailure(Handle);
			}
		}
	}

	const FOpenMobileSensorStreamOptions PreviousRequested =
		Entry->Request.Options;
	const FOpenMobileSensorStreamOptions PreviousApplied =
		Entry->AppliedOptions;
	const FOpenMobileSensorRateResolution PreviousRateResolution =
		Entry->RateResolution;
	const EOpenMobileSensorRateAdjustmentReason PreviousCommonRateReason =
		Entry->CommonRateAdjustmentReason;
	const FPhysicalStreamKey PreviousKey = Entry->PhysicalKey;
	Entry->Request.Options = Options;
	Entry->AppliedOptions = AppliedOptions;
	Entry->RateResolution = RateResolution;
	Entry->CommonRateAdjustmentReason = RateResolution.AdjustmentReason;
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
		Entry->RateResolution = PreviousRateResolution;
		Entry->CommonRateAdjustmentReason = PreviousCommonRateReason;
		Entry->PhysicalKey = PreviousKey;
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	if (DesiredRequest == Physical->Request)
	{
		UpdateAppliedNativeRate(
			PreviousKey,
			Physical->Request.RequestedFrequencyHz
		);
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
		Entry->RateResolution = PreviousRateResolution;
		Entry->CommonRateAdjustmentReason = PreviousCommonRateReason;
		Entry->PhysicalKey = PreviousKey;
		return ReconfigureResult;
	}
	Physical->Request = MoveTemp(BackendRequest);
	UpdateAppliedNativeRate(
		PreviousKey,
		Physical->Request.RequestedFrequencyHz
	);
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
	if (const FPhysicalStreamEntry* Physical =
		PhysicalStreams.Find(Entry->PhysicalKey))
	{
		CancelFlushesForPhysicalStream(Physical->Handle);
		Entry = FindOwnedEntry(OwnerIdentifier, Handle);
		if (!Entry)
		{
			return MakeSuccess();
		}
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

void FOpenMobileSensorsSubscriptionService::FlushSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FGuid& RequestId,
	TFunction<void(const FOpenMobileSensorFlushResult&)>&& Completion
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	auto FinishImmediately = [&Completion, &Handle, &RequestId](
		const FOpenMobileSensorOperationResult& Operation
	)
	{
		FOpenMobileSensorFlushResult Result;
		Result.RequestId = RequestId;
		Result.Handle = Handle;
		Result.Operation = Operation;
		if (Completion)
		{
			Completion(Result);
		}
	};
	if (!RequestId.IsValid() || PendingFlushes.Contains(RequestId))
	{
		FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		));
		return;
	}
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		FinishImmediately(MakeHandleFailure(Handle));
		return;
	}
	if (Entry->State != EOpenMobileSensorSubscriptionState::Active)
	{
		FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		));
		return;
	}
	FPhysicalStreamEntry* Physical = PhysicalStreams.Find(Entry->PhysicalKey);
	if (!Physical
		|| !Physical->Backend
		|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
			Physical->BackendToken
		))
	{
		FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		));
		return;
	}
	const bool bHasNativeFlush =
		Physical->Request.bNativeBatchingApplied;
	const bool bHasPluginFlush =
		Entry->AppliedOptions.DeliveryMode ==
			EOpenMobileSensorDeliveryMode::Buffered
		|| Entry->AppliedOptions.DeliveryMode ==
			EOpenMobileSensorDeliveryMode::EventBatches;
	if (!bHasNativeFlush && !bHasPluginFlush)
	{
		FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		));
		return;
	}
	for (const TPair<FGuid, FPendingFlush>& Pair : PendingFlushes)
	{
		if (Pair.Value.PhysicalStreamHandle == Physical->Handle)
		{
			FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::OperationalFailure,
				TEXT("OpenMobileSensors"),
				TEXT("ConcurrentFlush")
			));
			return;
		}
	}
	FPendingFlush Pending;
	Pending.OwnerIdentifier = OwnerIdentifier;
	Pending.Handle = Handle;
	Pending.PhysicalStreamHandle = Physical->Handle;
	Pending.BackendToken = Physical->BackendToken;
	Pending.DeadlineSeconds = FPlatformTime::Seconds() + 5.0;
	Pending.Completion = MoveTemp(Completion);
	PendingFlushes.Add(RequestId, MoveTemp(Pending));
	if (!bHasNativeFlush)
	{
		CompleteFlush(RequestId, MakeSuccess());
		return;
	}
	IOpenMobileSensorsBackend* Backend = Physical->Backend;
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle = Physical->Handle;
	const FOpenMobileSensorOperationResult Operation =
		Backend->FlushSensorStream(
			PhysicalHandle,
			RequestId,
			FOnOpenMobileSensorBackendFlushComplete::CreateLambda(
				[RequestId](
					const FGuid& CallbackRequestId,
					const FOpenMobileSensorOperationResult& CallbackOperation
				)
				{
					if (CallbackRequestId != RequestId)
					{
						return;
					}
					OpenMobile::DispatchToGameThread(
						[RequestId, CallbackOperation]()
						{
							CompleteFlush(RequestId, CallbackOperation);
						}
					);
				}
			)
		);
	if (!Operation.IsSuccess())
	{
		CompleteFlush(RequestId, Operation);
		return;
	}
	if (PendingFlushes.Contains(RequestId))
	{
		EnsureFlushTimeoutTick();
	}
}

bool FOpenMobileSensorsSubscriptionService::CancelFlush(
	const FGuid& OwnerIdentifier,
	const FGuid& RequestId
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	const FPendingFlush* Pending = PendingFlushes.Find(RequestId);
	if (!Pending || Pending->OwnerIdentifier != OwnerIdentifier)
	{
		return false;
	}
	CompleteFlush(
		RequestId,
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::Cancelled
		)
	);
	return true;
}

TArray<FOpenMobileSensorStreamDiagnostics>
FOpenMobileSensorsSubscriptionService::GetStreamDiagnostics(
	const FGuid& OwnerIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<FOpenMobileSensorStreamDiagnostics> Streams;
	if (!OwnerIdentifier.IsValid())
	{
		return Streams;
	}
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		const FSubscriptionEntry& Entry = Pair.Value;
		if (Entry.OwnerIdentifier != OwnerIdentifier)
		{
			continue;
		}
		FOpenMobileSensorStreamDiagnostics& Diagnostics =
			Streams.AddDefaulted_GetRef();
		Diagnostics.Subscription = MakeSnapshot(Entry);
		FOpenMobileSensorsSampleService::GetRateDiagnostics(
			OwnerIdentifier,
			Entry.Handle,
			Diagnostics.Rate
		);
		FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
			OwnerIdentifier,
			Entry.Handle,
			FPlatformTime::Seconds(),
			Diagnostics
		);
		Diagnostics.Rate.RequestedFrequencyHz =
			Entry.RateResolution.RequestedFrequencyHz;
		Diagnostics.Rate.AppliedFrequencyHz =
			Entry.RateResolution.AppliedNativeFrequencyHz;
		Diagnostics.BatchingMode = GetBatchingMode(Entry);
	}
	return Streams;
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

void FOpenMobileSensorsSubscriptionService::ProcessFlushTimeoutsForTests(
	double NowSeconds
)
{
	check(IsInGameThread());
	OpenMobileSensorsSubscriptionServicePrivate::ProcessFlushTimeouts(
		NowSeconds
	);
}

void FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	EOpenMobileSensorSubscriptionState State
)
{
	check(IsInGameThread());
	OpenMobileSensorsSubscriptionServicePrivate::SetState(
		Handle.GetIdentifier(),
		State
	);
}

void FOpenMobileSensorsSubscriptionService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	CancelPendingOperationsTick();
	CancelAllFlushes();
	Subscriptions.Reset();
	PhysicalStreams.Reset();
	FOpenMobileSensorsSampleService::UnregisterAll();
	StateChangedEvent.Clear();
	NextHandleGeneration = 1;
	bShuttingDown = false;
}
#endif
