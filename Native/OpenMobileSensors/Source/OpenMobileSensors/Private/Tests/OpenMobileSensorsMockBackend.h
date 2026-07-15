#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"
#include "OpenMobileSensorsErrorMapper.h"

enum class EOpenMobileSensorsMockEventType : uint8
{
	Capability,
	SampleBatch,
	Permission,
	Lifecycle,
	Error,
	Delay
};

struct FOpenMobileSensorsMockEvent
{
	EOpenMobileSensorsMockEventType Type =
		EOpenMobileSensorsMockEventType::Capability;
	FOpenMobileCapability Capability;
	TArray<FVector3d> SampleBatch;
	FName Permission;
	EOpenMobileCapabilityState PermissionState =
		EOpenMobileCapabilityState::Unavailable;
	bool bForeground = true;
	FOpenMobileError Error;
	double DelaySeconds = 0.0;
};

class FOpenMobileSensorsMockBackend final : public IOpenMobileSensorsBackend
{
public:
	explicit FOpenMobileSensorsMockBackend(
		FName InName = TEXT("Mock"),
		int32 InPriority = 100
	)
		: Name(InName)
		, Priority(InPriority)
	{
		BackendCapability.Name = GetModularFeatureName();
		BackendCapability.State = EOpenMobileCapabilityState::Available;
		StartSensorStreamResult.Code = EOpenMobileSensorResultCode::Success;
		ReconfigureSensorStreamResult.Code =
			EOpenMobileSensorResultCode::Success;
		FlushSensorStreamResult.Code =
			EOpenMobileSensorResultCode::NotSupported;
		CalibrationPromptResult = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}

	virtual FName GetBackendName() const override
	{
		return Name;
	}

	virtual int32 GetPriority() const override
	{
		return Priority;
	}

	virtual bool IsAvailable() const override
	{
		return bAvailable;
	}

	virtual bool RequiresHighSamplingRateDeclaration() const override
	{
		return bHighSamplingRateDeclarationRequired;
	}

	virtual bool HasHighSamplingRateDeclaration() const override
	{
		return bHighSamplingRateDeclarationPresent;
	}

	virtual FOpenMobileCapability GetBackendCapability() const override
	{
		++CapabilityQueryCount;
		return BackendCapability;
	}

	virtual TArray<FOpenMobileSensorCapability>
	GetSensorCapabilities() const override
	{
		++SensorCapabilityQueryCount;
		return SensorCapabilities;
	}

	virtual TArray<FOpenMobileSensorBackendMetadata>
	GetSensorMetadata() const override
	{
		++SensorMetadataQueryCount;
		return SensorMetadata;
	}

	virtual void RefreshMutableSensorMetadata(
		TArray<FOpenMobileSensorBackendMetadata>& InOutMetadata
	) const override
	{
		++MutableSensorMetadataRefreshCount;
		LastMutableSensorMetadataRefreshCount = InOutMetadata.Num();
		if (bHasRefreshedMutableSensorMetadata)
		{
			InOutMetadata = RefreshedMutableSensorMetadata;
		}
	}

	virtual FOpenMobileSensorOperationResult StartSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	) override
	{
		++StartSensorStreamCount;
		LastStartedPhysicalHandle = Handle;
		if (StartSensorStreamResult.IsSuccess()
			&& AppliedStartFrequencyForTests.IsSet())
		{
			InOutRequest.RequestedFrequencyHz =
				AppliedStartFrequencyForTests.GetValue();
		}
		if (StartSensorStreamResult.IsSuccess())
		{
			InOutRequest.bNativeBatchingApplied =
				InOutRequest.bNativeBatchingRequested
				&& NativeBatchingAppliedForTests.Get(false);
			InOutRequest.AppliedRateAdjustmentReason =
				AppliedRateAdjustmentReasonForTests;
			if (AppliedAttitudeReferenceForTests.IsSet())
			{
				InOutRequest.AttitudeReferenceState =
					AppliedAttitudeReferenceForTests.GetValue();
			}
		}
		LastStartedPhysicalRequest = InOutRequest;
		if (StartSensorStreamResult.IsSuccess())
		{
			ActiveSensorStreams.Add(Handle);
		}
		if (StartSensorStreamHookForTests)
		{
			StartSensorStreamHookForTests();
		}
		return StartSensorStreamResult;
	}

	virtual FOpenMobileSensorOperationResult ReconfigureSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	) override
	{
		++ReconfigureSensorStreamCount;
		if (!ActiveSensorStreams.Contains(Handle))
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidHandle
			);
		}
		if (ReconfigureSensorStreamResult.IsSuccess()
			&& AppliedReconfigureFrequencyForTests.IsSet())
		{
			InOutRequest.RequestedFrequencyHz =
				AppliedReconfigureFrequencyForTests.GetValue();
		}
		if (ReconfigureSensorStreamResult.IsSuccess())
		{
			InOutRequest.bNativeBatchingApplied =
				InOutRequest.bNativeBatchingRequested
				&& NativeBatchingAppliedForTests.Get(false);
			InOutRequest.AppliedRateAdjustmentReason =
				AppliedRateAdjustmentReasonForTests;
			if (AppliedAttitudeReferenceForTests.IsSet())
			{
				InOutRequest.AttitudeReferenceState =
					AppliedAttitudeReferenceForTests.GetValue();
			}
		}
		LastReconfiguredPhysicalRequest = InOutRequest;
		return ReconfigureSensorStreamResult;
	}

	virtual void StopSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle
	) override
	{
		++StopSensorStreamCount;
		ActiveSensorStreams.Remove(Handle);
	}

	virtual FOpenMobileSensorOperationResult FlushSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FGuid& RequestId,
		FOnOpenMobileSensorBackendFlushComplete&& Completion
	) override
	{
		++FlushSensorStreamCount;
		LastFlushRequestId = RequestId;
		if (!ActiveSensorStreams.Contains(Handle))
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidHandle
			);
		}
		if (FlushSensorStreamResult.IsSuccess())
		{
			PendingFlushes.Add(RequestId, MoveTemp(Completion));
		}
		return FlushSensorStreamResult;
	}

	virtual FOpenMobileSensorOperationResult RequestCalibrationPrompt(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorIdentifier& Sensor
	) override
	{
		++CalibrationPromptCount;
		LastCalibrationPromptSensor = Sensor;
		if (!ActiveSensorStreams.Contains(Handle))
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidHandle
			);
		}
		return CalibrationPromptResult;
	}

	virtual void BeginShutdown() override
	{
		bShutdown = true;
		ActiveSensorStreams.Reset();
		PendingFlushes.Reset();
		Script.Reset();
		NextEventIndex = 0;
		RemainingDelaySeconds = 0.0;
	}

	void SetAvailable(bool bInAvailable)
	{
		bAvailable = bInAvailable;
	}

	void SetSensorCapabilities(
		TArray<FOpenMobileSensorCapability> InCapabilities
	)
	{
		SensorCapabilities = MoveTemp(InCapabilities);
	}

	void SetSensorMetadata(
		TArray<FOpenMobileSensorBackendMetadata> InMetadata
	)
	{
		SensorMetadata = MoveTemp(InMetadata);
	}

	void SetRefreshedMutableSensorMetadata(
		TArray<FOpenMobileSensorBackendMetadata> InMetadata
	)
	{
		RefreshedMutableSensorMetadata = MoveTemp(InMetadata);
		bHasRefreshedMutableSensorMetadata = true;
	}

	void SetStartSensorStreamResult(
		FOpenMobileSensorOperationResult InResult
	)
	{
		StartSensorStreamResult = MoveTemp(InResult);
	}

	void SetReconfigureSensorStreamResult(
		FOpenMobileSensorOperationResult InResult
	)
	{
		ReconfigureSensorStreamResult = MoveTemp(InResult);
	}

	void SetFlushSensorStreamResult(
		FOpenMobileSensorOperationResult InResult
	)
	{
		FlushSensorStreamResult = MoveTemp(InResult);
	}

	void SetCalibrationPromptResult(
		FOpenMobileSensorOperationResult InResult
	)
	{
		CalibrationPromptResult = MoveTemp(InResult);
	}

	void CompleteFlushForTests(
		const FGuid& RequestId,
		const FOpenMobileSensorOperationResult& Result
	)
	{
		FOnOpenMobileSensorBackendFlushComplete Completion;
		if (FOnOpenMobileSensorBackendFlushComplete* Pending =
			PendingFlushes.Find(RequestId))
		{
			Completion = MoveTemp(*Pending);
			PendingFlushes.Remove(RequestId);
		}
		Completion.ExecuteIfBound(RequestId, Result);
	}

	void SetAppliedStartFrequencyForTests(double FrequencyHz)
	{
		AppliedStartFrequencyForTests = FrequencyHz;
	}

	void SetStartSensorStreamHookForTests(TFunction<void()> Hook)
	{
		StartSensorStreamHookForTests = MoveTemp(Hook);
	}

	void SetAppliedReconfigureFrequencyForTests(double FrequencyHz)
	{
		AppliedReconfigureFrequencyForTests = FrequencyHz;
	}

	void SetNativeBatchingAppliedForTests(bool bApplied)
	{
		NativeBatchingAppliedForTests = bApplied;
	}

	void SetHighSamplingRateDeclarationForTests(
		bool bRequired,
		bool bPresent
	)
	{
		bHighSamplingRateDeclarationRequired = bRequired;
		bHighSamplingRateDeclarationPresent = bPresent;
	}

	void SetAppliedRateAdjustmentReasonForTests(
		EOpenMobileSensorRateAdjustmentReason Reason
	)
	{
		AppliedRateAdjustmentReasonForTests = Reason;
	}

	void SetAppliedAttitudeReferenceForTests(
		const FOpenMobileAttitudeReferenceState& State
	)
	{
		AppliedAttitudeReferenceForTests = State;
	}

	void AddCapability(FOpenMobileCapability Capability)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Capability;
		Event.Capability = MoveTemp(Capability);
		Script.Add(MoveTemp(Event));
	}

	void AddSampleBatch(TArray<FVector3d> Samples)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::SampleBatch;
		Event.SampleBatch = MoveTemp(Samples);
		Script.Add(MoveTemp(Event));
	}

	void AddPermission(
		FName Permission,
		EOpenMobileCapabilityState PermissionState
	)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Permission;
		Event.Permission = Permission;
		Event.PermissionState = PermissionState;
		Script.Add(MoveTemp(Event));
	}

	void AddLifecycle(bool bForeground)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Lifecycle;
		Event.bForeground = bForeground;
		Script.Add(MoveTemp(Event));
	}

	void AddError(FOpenMobileError Error)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Error;
		Event.Error = MoveTemp(Error);
		Script.Add(MoveTemp(Event));
	}

	void AddDelay(double DelaySeconds)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Delay;
		Event.DelaySeconds = FMath::Max(0.0, DelaySeconds);
		Script.Add(MoveTemp(Event));
	}

	void AdvanceScript(
		double DeltaSeconds,
		TFunctionRef<void(const FOpenMobileSensorsMockEvent&)> OnEvent
	)
	{
		constexpr double DelayToleranceSeconds = 1.e-9;
		double RemainingDeltaSeconds = FMath::Max(0.0, DeltaSeconds);
		if (RemainingDelaySeconds
			> RemainingDeltaSeconds + DelayToleranceSeconds)
		{
			RemainingDelaySeconds -= RemainingDeltaSeconds;
			return;
		}
		RemainingDeltaSeconds = FMath::Max(
			0.0,
			RemainingDeltaSeconds - RemainingDelaySeconds
		);
		RemainingDelaySeconds = 0.0;

		while (NextEventIndex < Script.Num())
		{
			const FOpenMobileSensorsMockEvent& Event = Script[NextEventIndex++];
			if (Event.Type != EOpenMobileSensorsMockEventType::Delay)
			{
				OnEvent(Event);
				continue;
			}

			if (Event.DelaySeconds
				> RemainingDeltaSeconds + DelayToleranceSeconds)
			{
				RemainingDelaySeconds =
					Event.DelaySeconds - RemainingDeltaSeconds;
				return;
			}
			RemainingDeltaSeconds -= Event.DelaySeconds;
		}
	}

	bool WasShutdown() const
	{
		return bShutdown;
	}

	int32 GetCapabilityQueryCount() const
	{
		return CapabilityQueryCount;
	}

	int32 GetSensorCapabilityQueryCount() const
	{
		return SensorCapabilityQueryCount;
	}

	int32 GetSensorMetadataQueryCount() const
	{
		return SensorMetadataQueryCount;
	}

	int32 GetMutableSensorMetadataRefreshCount() const
	{
		return MutableSensorMetadataRefreshCount;
	}

	int32 GetLastMutableSensorMetadataRefreshCount() const
	{
		return LastMutableSensorMetadataRefreshCount;
	}

	int32 GetStartSensorStreamCount() const
	{
		return StartSensorStreamCount;
	}

	int32 GetReconfigureSensorStreamCount() const
	{
		return ReconfigureSensorStreamCount;
	}

	int32 GetStopSensorStreamCount() const
	{
		return StopSensorStreamCount;
	}

	int32 GetFlushSensorStreamCount() const
	{
		return FlushSensorStreamCount;
	}

	int32 GetCalibrationPromptCount() const
	{
		return CalibrationPromptCount;
	}

	const FGuid& GetLastFlushRequestId() const
	{
		return LastFlushRequestId;
	}

	const FOpenMobileSensorPhysicalStreamRequest&
	GetLastStartedPhysicalRequest() const
	{
		return LastStartedPhysicalRequest;
	}

	const FOpenMobileSensorBackendStreamHandle&
	GetLastStartedPhysicalHandle() const
	{
		return LastStartedPhysicalHandle;
	}

	const FOpenMobileSensorPhysicalStreamRequest&
	GetLastReconfiguredPhysicalRequest() const
	{
		return LastReconfiguredPhysicalRequest;
	}

private:
	FName Name;
	int32 Priority = 100;
	bool bAvailable = true;
	bool bShutdown = false;
	mutable int32 CapabilityQueryCount = 0;
	FOpenMobileCapability BackendCapability;
	TArray<FOpenMobileSensorCapability> SensorCapabilities;
	mutable int32 SensorCapabilityQueryCount = 0;
	TArray<FOpenMobileSensorBackendMetadata> SensorMetadata;
	TArray<FOpenMobileSensorBackendMetadata> RefreshedMutableSensorMetadata;
	bool bHasRefreshedMutableSensorMetadata = false;
	mutable int32 SensorMetadataQueryCount = 0;
	mutable int32 MutableSensorMetadataRefreshCount = 0;
	mutable int32 LastMutableSensorMetadataRefreshCount = 0;
	FOpenMobileSensorOperationResult StartSensorStreamResult;
	FOpenMobileSensorOperationResult ReconfigureSensorStreamResult;
	FOpenMobileSensorOperationResult FlushSensorStreamResult;
	FOpenMobileSensorOperationResult CalibrationPromptResult;
	TFunction<void()> StartSensorStreamHookForTests;
	TOptional<double> AppliedStartFrequencyForTests;
	TOptional<double> AppliedReconfigureFrequencyForTests;
	TOptional<bool> NativeBatchingAppliedForTests;
	TOptional<FOpenMobileAttitudeReferenceState>
		AppliedAttitudeReferenceForTests;
	EOpenMobileSensorRateAdjustmentReason AppliedRateAdjustmentReasonForTests =
		EOpenMobileSensorRateAdjustmentReason::None;
	bool bHighSamplingRateDeclarationRequired = false;
	bool bHighSamplingRateDeclarationPresent = true;
	FOpenMobileSensorBackendStreamHandle LastStartedPhysicalHandle;
	FOpenMobileSensorPhysicalStreamRequest LastStartedPhysicalRequest;
	FOpenMobileSensorPhysicalStreamRequest LastReconfiguredPhysicalRequest;
	TSet<FOpenMobileSensorBackendStreamHandle> ActiveSensorStreams;
	TMap<FGuid, FOnOpenMobileSensorBackendFlushComplete> PendingFlushes;
	int32 StartSensorStreamCount = 0;
	int32 ReconfigureSensorStreamCount = 0;
	int32 StopSensorStreamCount = 0;
	int32 FlushSensorStreamCount = 0;
	int32 CalibrationPromptCount = 0;
	FOpenMobileSensorIdentifier LastCalibrationPromptSensor;
	FGuid LastFlushRequestId;
	TArray<FOpenMobileSensorsMockEvent> Script;
	int32 NextEventIndex = 0;
	double RemainingDelaySeconds = 0.0;
};

#endif
