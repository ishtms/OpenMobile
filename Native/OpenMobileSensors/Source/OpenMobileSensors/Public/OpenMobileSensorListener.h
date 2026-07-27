#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorSamples.h"
#include "OpenMobileSensorListener.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Sensor Sample Info"))
struct OPENMOBILESENSORS_API FOpenMobileSensorSampleInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sensor capture time in boot-relative monotonic seconds."))
	double TimestampMonotonicSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (Units = "s", ToolTip = "Time from sensor capture to game-thread delivery in seconds."))
	double AgeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Strictly increasing sequence for this listener."))
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Normalized accuracy reported for this sample."))
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileSensors.EOpenMobileSensorSourceFlags", ToolTip = "Typed native, derived, mock, or replay source flags."))
	int32 SourceFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Whether the sample passed normalization and sensor-specific validity checks."))
	bool bValid = false;
};

class UOpenMobileSensorListener;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOpenMobileSensorListenerStartedDynamic,
	UOpenMobileSensorListener*,
	Listener,
	UPARAM(DisplayName = "Applied Rate (Hz)") double,
	AppliedRateHz,
	EOpenMobileSensorAvailabilitySource,
	Source,
	bool,
	bRateAdjusted
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileSensorListenerStateDynamic,
	UOpenMobileSensorListener*,
	Listener
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOpenMobileSensorListenerFailureDynamic,
	UOpenMobileSensorListener*,
	Listener,
	FText,
	Message,
	FText,
	Correction,
	FOpenMobileSensorOperationResult,
	Details
);

UCLASS(Abstract, BlueprintType, Transient, meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileSensorListener
	: public UOpenMobileSensorAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Started", ToolTip = "Broadcast once when the requested sensor listener becomes active."))
	FOpenMobileSensorListenerStartedDynamic Started;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Paused", ToolTip = "Broadcast when lifecycle policy pauses this listener."))
	FOpenMobileSensorListenerStateDynamic Paused;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Resumed", ToolTip = "Broadcast when this listener resumes after a lifecycle pause."))
	FOpenMobileSensorListenerStateDynamic Resumed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Permission Required", ToolTip = "Broadcast when the sensor cannot start until its reported permission is granted."))
	FOpenMobileSensorListenerFailureDynamic PermissionRequired;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Unavailable", ToolTip = "Broadcast when the requested sensor or required input is unavailable."))
	FOpenMobileSensorListenerFailureDynamic Unavailable;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Failed", ToolTip = "Broadcast when this listener fails for a reason other than permission or availability."))
	FOpenMobileSensorListenerFailureDynamic Failed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Stopped", ToolTip = "Broadcast once after explicit stop, owner destruction, or world cleanup."))
	FOpenMobileSensorListenerStateDynamic Stopped;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Stop Sensor Listener", Keywords = "OpenMobile sensors cancel cleanup", ToolTip = "Stops this listener. Calling Stop more than once is safe."))
	void Stop();

	virtual bool IsActive() const override;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Sensor Listener State", ToolTip = "Returns the listener's cached state without querying live service state."))
	EOpenMobileSensorSubscriptionState GetListenerState() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Listener Sensor", ToolTip = "Returns the sensor identifier selected for this listener."))
	FOpenMobileSensorIdentifier GetSensor() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Applied Sensor Listener Options", ToolTip = "Returns the resolved stream options used by this listener."))
	FOpenMobileSensorStreamOptions GetAppliedOptions() const;

	virtual void Activate() override;

#if WITH_DEV_AUTOMATION_TESTS
	bool TickOwnerForTests();
#endif

protected:
	void ConfigureListener(
		const UObject* WorldContextObject,
		UObject* ListenerOwner,
		EOpenMobileSensorType Sensor,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		bool bUseAdvancedOptions
	);
	static FOpenMobileSensorSampleInfo MakeSampleInfo(
		const FOpenMobileSensorSampleHeader& Header
	);
	virtual void HandleVectorSample(
		const FOpenMobileVectorSensorSample& Sample
	);
	virtual void CancelNativeOperation() override;
	virtual void OnActionSucceeded() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void BindEvents();
	void UnbindEvents();
	bool TickOwner(float DeltaSeconds);
	void HandleStateChanged(
		const FOpenMobileSensorSubscriptionStateSnapshot& Snapshot
	);
	void HandleVectorBatch(
		FOpenMobileSensorSubscriptionHandle InHandle,
		const FOpenMobileVectorSensorBatch& Batch
	);
	void FinishFromOperation(
		const FOpenMobileSensorOperationResult& Operation
	);
	void BroadcastFailure();
	EOpenMobileSensorAvailabilitySource ResolveSource() const;

	UPROPERTY(Transient)
	TObjectPtr<UObject> ActivationWorldContext;

	TWeakObjectPtr<UObject> LifetimeOwner;
	TWeakObjectPtr<UOpenMobileSensorsSubsystem> BoundSubsystem;
	FOpenMobileSensorIdentifier RequestedSensor;
	FOpenMobileSensorSubscriptionHandle Handle;
	FOpenMobileSensorStreamOptions RequestedOptions;
	FOpenMobileSensorStreamOptions AppliedOptions;
	FOpenMobileSensorRateResolution RateResolution;
	FOpenMobileSensorOperationResult LastOperation;
	FDelegateHandle StateChangedHandle;
	FDelegateHandle SampleHandle;
	FTSTicker::FDelegateHandle OwnerTickerHandle;
	EOpenMobileSensorSubscriptionState CachedState =
		EOpenMobileSensorSubscriptionState::Invalid;
	bool bStartedBroadcast = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileGyroscopeSampleDynamic,
	UOpenMobileSensorListener*,
	Listener,
	UPARAM(DisplayName = "Angular Velocity (rad/s)") FVector,
	AngularVelocityRadiansPerSecond,
	FOpenMobileSensorSampleInfo,
	SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileGyroscopeListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Sample", ToolTip = "Broadcast the latest coalesced gyroscope sample in radians per second."))
	FOpenMobileGyroscopeSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Gyroscope", Keywords = "OpenMobile sensors gyroscope gyro rotation rate angular velocity aim", ToolTip = "Starts an owner-scoped gyroscope listener that emits samples and stops automatically when its owner is destroyed."))
	static UOpenMobileGyroscopeListener* ListenForGyroscope(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset =
			EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace =
			EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Gyroscope Sample", Keywords = "OpenMobile sensors gyro angular velocity cached", ToolTip = "Returns the last sample delivered to this listener without querying live service state."))
	bool GetLatestAngularVelocity(
		UPARAM(DisplayName = "Angular Velocity (rad/s)") FVector& OutAngularVelocityRadiansPerSecond,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

protected:
	virtual void HandleVectorSample(
		const FOpenMobileVectorSensorSample& InSample
	) override;

private:
	FOpenMobileVectorSensorSample LatestSample;
	bool bHasSample = false;
};
