#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileNativeStepCount.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorsBackendTypes.h"

DECLARE_DELEGATE_TwoParams(
	FOnOpenMobileSensorBackendFlushComplete,
	const FGuid&,
	const FOpenMobileSensorOperationResult&
);
DECLARE_DELEGATE_TwoParams(
	FOnOpenMobileNativeStepCountBackendQueryComplete,
	const FOpenMobileSensorOperationResult&,
	const FOpenMobileStepsSensorSample&
);

class IOpenMobileSensorsBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileSensorsBackend() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Sensors.Backend"));
		return FeatureName;
	}

	virtual FName GetBackendName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	// Availability queries must not start hardware, request permission, or initialize adapters.
	virtual bool IsAvailable() const { return true; }
	virtual bool RequiresHighSamplingRateDeclaration() const { return false; }
	virtual bool HasHighSamplingRateDeclaration() const { return true; }

	virtual FOpenMobileCapability GetBackendCapability() const
	{
		FOpenMobileCapability Capability;
		Capability.Name = GetModularFeatureName();
		Capability.State = EOpenMobileCapabilityState::Available;
		return Capability;
	}

	virtual TArray<FOpenMobileSensorCapability> GetSensorCapabilities() const
	{
		return {};
	}

	virtual TArray<FOpenMobileSensorBackendMetadata> GetSensorMetadata() const
	{
		return {};
	}

	virtual void RefreshMutableSensorMetadata(
		TArray<FOpenMobileSensorBackendMetadata>& InOutMetadata
	) const
	{
	}

	virtual FOpenMobileSensorOperationResult StartSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	)
	{
		static_cast<void>(Handle);
		static_cast<void>(InOutRequest);
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::NotSupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The backend does not implement sensor streaming."),
			{},
			TEXT("OpenMobileSensors")
		);
		return Result;
	}

	virtual FOpenMobileSensorOperationResult ReconfigureSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	)
	{
		static_cast<void>(Handle);
		static_cast<void>(InOutRequest);
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::NotSupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The backend cannot reconfigure this sensor stream."),
			{},
			TEXT("OpenMobileSensors")
		);
		return Result;
	}

	virtual void StopSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle
	)
	{
		static_cast<void>(Handle);
	}

	virtual FOpenMobileSensorOperationResult FlushSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FGuid& RequestId,
		FOnOpenMobileSensorBackendFlushComplete&& Completion
	)
	{
		static_cast<void>(Handle);
		static_cast<void>(RequestId);
		static_cast<void>(Completion);
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::NotSupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The backend cannot flush this sensor stream."),
			{},
			TEXT("OpenMobileSensors")
		);
		return Result;
	}

	virtual FOpenMobileSensorOperationResult RequestCalibrationPrompt(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorIdentifier& Sensor
	)
	{
		static_cast<void>(Handle);
		static_cast<void>(Sensor);
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::NotSupported;
		Result.Failure.Reason =
			EOpenMobileSensorFailureReason::UnsupportedOperation;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The backend does not provide a native calibration prompt."),
			{},
			TEXT("OpenMobileSensors")
		);
		return Result;
	}

	virtual FOpenMobileSensorOperationResult QueryNativeStepCount(
		const FGuid& RequestId,
		const FOpenMobileNativeStepCountQuery& Query,
		FOnOpenMobileNativeStepCountBackendQueryComplete&& Completion
	)
	{
		static_cast<void>(RequestId);
		static_cast<void>(Query);
		static_cast<void>(Completion);
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::NotSupported;
		Result.Failure.Reason =
			EOpenMobileSensorFailureReason::UnsupportedOperation;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The backend does not provide historical step queries."),
			{},
			TEXT("OpenMobileSensors")
		);
		return Result;
	}

	virtual bool CancelNativeStepCountQuery(const FGuid& RequestId)
	{
		static_cast<void>(RequestId);
		return false;
	}

	virtual void BeginShutdown() {}
};
