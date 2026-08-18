#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"
#include "IOpenMobilePermissionProvider.h"
#include "OpenMobileSensorsBackendRegistry.h"

class FOpenMobileSensorsAndroidBackend;

enum class EOpenMobileSensorsAndroidBridgeFailure : uint8
{
	None,
	ActivityUnavailable,
	BridgeClassMissing,
	BridgeMethodMissing,
	BridgeCreateFailed,
	JavaException,
	InvalidPayload,
	InvalidArgument,
	SensorMissing,
	PermissionDenied,
	RegisterFailed,
	StreamMissing,
	FlushFailed,
	Paused,
	ShuttingDown,
	Timeout
};

struct FOpenMobileSensorsAndroidSensorDescriptor
{
	FOpenMobileSensorIdentifier Sensor;
	FString NativeIdentifier;
	FString NativeName;
	FString Vendor;
	FString NativeStringType;
	int32 NativeType = 0;
	int32 NativeId = 0;
	int32 Version = 0;
	int32 MinimumDelayMicroseconds = 0;
	int32 MaximumDelayMicroseconds = 0;
	int32 FifoCapacitySamples = 0;
	int32 NativeReportingMode = -1;
	double MaximumRange = 0.0;
	double Resolution = 0.0;
	double PowerMilliamps = 0.0;
	bool bWakeUp = false;
	bool bPreferred = false;
	bool bDynamic = false;
};

struct FOpenMobileSensorsAndroidBridgeResult
{
	EOpenMobileSensorsAndroidBridgeFailure Failure =
		EOpenMobileSensorsAndroidBridgeFailure::None;
	int32 NativeResult = 0;

	bool IsSuccess() const
	{
		return Failure == EOpenMobileSensorsAndroidBridgeFailure::None;
	}
};

class FOpenMobileSensorsAndroidBridge final
{
public:
	explicit FOpenMobileSensorsAndroidBridge(
		FOpenMobileSensorsAndroidBackend& InBackend
	);
	~FOpenMobileSensorsAndroidBridge();

	static FOpenMobilePermissionResult
	QueryActivityRecognitionPermissionStatus();
	static bool RefreshApplicationWindowRotation();
	bool RequestActivityRecognitionPermission(
		const FGuid& RequestIdentifier,
		FOpenMobileNativePermissionCompletion&& Completion,
		FOpenMobileError& OutError
	);
	void CancelPermissionRequest(const FGuid& RequestIdentifier);
	FOpenMobileSensorsAndroidBridgeResult QuerySensors(
		TArray<FOpenMobileSensorsAndroidSensorDescriptor>& OutSensors
	);
	FOpenMobileSensorsAndroidBridgeResult StartStream(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor,
		int32 SamplingPeriodMicroseconds,
		int32 MaximumReportLatencyMicroseconds,
		bool bLowLatency,
		EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame
	);
	FOpenMobileSensorsAndroidBridgeResult ReconfigureStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		int32 SamplingPeriodMicroseconds,
		int32 MaximumReportLatencyMicroseconds,
		bool bLowLatency,
		EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame
	);
	bool GetActiveSensorDescriptor(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorsAndroidSensorDescriptor& OutDescriptor
	);
	FOpenMobileSensorsAndroidBridgeResult StopStream(
		const FOpenMobileSensorBackendStreamHandle& Handle
	);
	FOpenMobileSensorsAndroidBridgeResult FlushStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FGuid& RequestId,
		FOnOpenMobileSensorBackendFlushComplete&& Completion
	);
	bool HasHighSamplingRateDeclaration();
	void Shutdown();

	void HandleSampleBatch(
		uint64 BackendGeneration,
		const FGuid& StreamIdentifier,
		int32 NativeSensorType,
		int32 SampleCount,
		int32 ValuesPerSample,
		int32 ValueStride,
		TArray<int64>&& TimestampsNanoseconds,
		TArray<float>&& Values
	);
	void HandleAccuracyChanged(
		uint64 BackendGeneration,
		const FGuid& StreamIdentifier,
		int32 NativeSensorType,
		int32 NativeAccuracy,
		int64 TimestampNanoseconds
	);
	void HandleFlushCompleted(
		uint64 BackendGeneration,
		const FGuid& StreamIdentifier,
		const FGuid& RequestId,
		int32 NativeResult
	);
	void HandleSensorDisconnected(
		uint64 BackendGeneration,
		const FGuid& StreamIdentifier,
		int32 NativeSensorType
	);
	void HandleStreamError(
		uint64 BackendGeneration,
		const FGuid& StreamIdentifier,
		int32 NativeResult
	);
	void HandleStreamRestarted(
		uint64 BackendGeneration,
		const FGuid& StreamIdentifier
	);
	void HandleSensorsChanged();
	void HandlePermissionResult(
		const FGuid& RequestIdentifier,
		int32 NativeStatus
	);

private:
	struct FActiveStream
	{
		FOpenMobileSensorsBackendToken Token;
		FOpenMobileSensorBackendStreamHandle Handle;
		FOpenMobileSensorsAndroidSensorDescriptor Descriptor;
		EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame =
			EOpenMobileAttitudeReferenceFrame::GameRelative;
		bool bResetNextSample = false;
	};

	struct FPendingFlush
	{
		FOpenMobileSensorsBackendToken Token;
		FOpenMobileSensorBackendStreamHandle Handle;
		FOnOpenMobileSensorBackendFlushComplete Completion;
	};

	FOpenMobileSensorsAndroidBridgeResult EnsureInitialized();
	FOpenMobileSensorsAndroidBridgeResult MapNativeResult(
		int32 NativeResult
	) const;
	void ClearException(void* Environment) const;
	FString StreamId(
		const FOpenMobileSensorBackendStreamHandle& Handle
	) const;
	bool FindActiveStream(
		uint64 BackendGeneration,
		const FGuid& StreamIdentifier,
		FActiveStream& OutStream,
		bool bConsumeReset
	);

	FOpenMobileSensorsAndroidBackend& Backend;
	FCriticalSection Mutex;
	TMap<FGuid, FActiveStream> ActiveStreams;
	TMap<FGuid, FPendingFlush> PendingFlushes;
	TMap<FGuid, FOpenMobileNativePermissionCompletion>
		PendingPermissionRequests;
	void* BridgeClass = nullptr;
	void* BridgeObject = nullptr;
	void* CreateMethod = nullptr;
	void* QuerySensorsMethod = nullptr;
	void* StartStreamMethod = nullptr;
	void* ReconfigureStreamMethod = nullptr;
	void* StopStreamMethod = nullptr;
	void* FlushStreamMethod = nullptr;
	void* HasHighSamplingRateDeclarationMethod = nullptr;
	void* RequestActivityRecognitionPermissionMethod = nullptr;
	void* CancelPermissionRequestMethod = nullptr;
	void* ShutdownMethod = nullptr;
	TAtomic<bool> bShuttingDown = false;
};
