#pragma once

#include "Containers/Ticker.h"
#include "IOpenMobilePermissionProvider.h"
#include "IOpenMobileSensorsBackend.h"
#include "OpenMobileSensorsDevelopmentInputService.h"

class FOpenMobileSensorsEditorMockBackend final
	: public IOpenMobileSensorsBackend
	, public IOpenMobilePermissionProvider
	, public IOpenMobileSensorsDevelopmentInputProvider
{
public:
	void Activate();

	virtual FName GetBackendName() const override;
	virtual int32 GetPriority() const override;
	virtual bool IsAvailable() const override;
	virtual FOpenMobileCapability GetBackendCapability() const override;
	virtual TArray<FOpenMobileSensorCapability>
	GetSensorCapabilities() const override;
	virtual FOpenMobileSensorOperationResult StartSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	) override;
	virtual FOpenMobileSensorOperationResult ReconfigureSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FOpenMobileSensorPhysicalStreamRequest& InOutRequest
	) override;
	virtual void StopSensorStream(
		const FOpenMobileSensorBackendStreamHandle& Handle
	) override;
	virtual void BeginShutdown() override;

	virtual FName GetProviderName() const override;
	virtual bool SupportsPermission(FName Permission) const override;
	virtual FOpenMobilePermissionResult GetStatus(
		FName Permission
	) const override;
	virtual bool RequestPermission(
		FName Permission,
		const FGuid& RequestIdentifier,
		FOpenMobileNativePermissionCompletion&& Completion,
		FOpenMobileError& OutError
	) override;

	virtual FOpenMobileSensorOperationResult ApplyInput(
		const FOpenMobileSensorsMockInput& Input
	) override;
	virtual FOpenMobileSensorOperationResult ApplyPreset(
		EOpenMobileSensorsMockPreset Preset
	) override;
	virtual FOpenMobileSensorOperationResult PlayTimeline(
		const FOpenMobileSensorsMockTimeline& Timeline
	) override;
	virtual FOpenMobileSensorOperationResult StopTimeline() override;
	virtual FOpenMobileSensorOperationResult AdvanceTimeline(
		double DeltaSeconds
	) override;
	virtual FOpenMobileSensorOperationResult InjectError(
		EOpenMobileSensorType Sensor,
		EOpenMobileSensorFailureReason FailureReason,
		const FString& NativeCode
	) override;
	virtual bool IsActive() const override;

private:
	struct FActiveStream
	{
		FOpenMobileSensorPhysicalStreamRequest Request;
		double NextEmissionSeconds = 0.0;
		int64 LastStepCount = 0;
	};

	static bool ValidateInput(const FOpenMobileSensorsMockInput& Input);
	static bool ValidateTimeline(
		const FOpenMobileSensorsMockTimeline& Timeline
	);
	static FOpenMobileSensorsMockInput MakePreset(
		EOpenMobileSensorsMockPreset Preset
	);
	static bool IsSupportedSensor(EOpenMobileSensorType Sensor);
	static FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Sensor
	);
	static FOpenMobileSensorOperationResult MakeSuccess();
	static FOpenMobileSensorOperationResult MakeFailure(
		EOpenMobileSensorFailureReason Reason,
		const TCHAR* NativeCode
	);
	static FOpenMobileSensorSampleHeader MakeHeader(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorsMockInput& Input,
		double TimestampSeconds,
		int32 SourceFlags
	);

	EOpenMobilePermissionStatus GetPermissionStatus(FName Permission) const;
	void EnsureTicker();
	void StopTicker();
	bool Tick(float DeltaSeconds);
	void ApplyPermissionChanges(
		const FOpenMobileSensorsMockInput& Previous,
		const FOpenMobileSensorsMockInput& Next
	);
	void ResolveTimeline(double NowSeconds);
	void ResolveTimelinePosition(double PlaybackSeconds);
	void PublishAll(double NowSeconds, bool bForce);
	void PublishStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		FActiveStream& Stream,
		double TimestampSeconds,
		bool bForce
	);

	TMap<FOpenMobileSensorBackendStreamHandle, FActiveStream> ActiveStreams;
	FOpenMobileSensorsMockInput CurrentInput;
	FOpenMobileSensorsMockTimeline ActiveTimeline;
	double TimelineStartSeconds = 0.0;
	double ManualTimelineElapsedSeconds = 0.0;
	bool bTimelinePlaying = false;
	bool bActive = false;
	FTSTicker::FDelegateHandle TickHandle;
};
