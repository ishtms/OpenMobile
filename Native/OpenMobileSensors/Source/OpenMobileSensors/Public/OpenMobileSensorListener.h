#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "OpenMobileSensorAsyncActionBase.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorErrorReport.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorSamples.h"
#include "OpenMobileSensorListener.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileSensorControlOutcome : uint8
{
	Succeeded UMETA(DisplayName = "Succeeded", ToolTip = "The listener control was applied."),
	NotSupported UMETA(DisplayName = "Not Supported", ToolTip = "The active sensor backend does not support this control."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "The listener control failed for another reason.")
};

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOpenMobileSensorListenerStartedDynamic,
	UOpenMobileSensorListener*,
	Listener,
	FOpenMobileSensorStreamOptions,
	AppliedOptions,
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileSensorListenerSamplesDroppedDynamic,
	UOpenMobileSensorListener*,
	Listener,
	FOpenMobileSensorDropInfo,
	DropInfo
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileSensorListenerErrorDynamic,
	UOpenMobileSensorListener*,
	Listener,
	FOpenMobileSensorRuntimeError,
	Error
);

UCLASS(Abstract, BlueprintType, Transient, meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileSensorListener
	: public UOpenMobileSensorAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Started", ToolTip = "Broadcast once when the requested sensor listener becomes active, including every resolved stream option and the final native rate."))
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

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Samples Dropped", ToolTip = "Broadcast one coalesced loss report per game-thread dispatch cycle when this listener's bounded sample queue overflows."))
	FOpenMobileSensorListenerSamplesDroppedDynamic SamplesDropped;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Error", ToolTip = "Broadcast a compact listener-scoped runtime error with a direct correction and retryability."))
	FOpenMobileSensorListenerErrorDynamic SensorError;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Stop Sensor Listener", Keywords = "OpenMobile sensors cancel cleanup", ToolTip = "Stops this listener. Calling Stop more than once is safe."))
	void Stop();

	virtual bool IsActive() const override;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Sensor Listener State", ToolTip = "Returns the listener's cached state without querying live service state."))
	EOpenMobileSensorSubscriptionState GetListenerState() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Listener Sensor", ToolTip = "Returns the sensor identifier selected for this listener."))
	FOpenMobileSensorIdentifier GetSensor() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Applied Sensor Listener Options", ToolTip = "Returns the resolved stream options used by this listener."))
	FOpenMobileSensorStreamOptions GetAppliedOptions() const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Options", meta = (DisplayName = "Set Sensor Rate Preset", Keywords = "OpenMobile sensors listener update frequency hertz", AdvancedDisplay = "CustomFrequencyHz", ToolTip = "Updates only this listener's rate preset. Custom Frequency is used only for the Custom preset."))
	FOpenMobileSensorOperationResult SetSensorRatePreset(
		EOpenMobileSensorRatePreset RatePreset,
		double CustomFrequencyHz = 15.0
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Options", meta = (DisplayName = "Set Sensor Coordinate Space", Keywords = "OpenMobile sensors listener update device screen coordinates", ToolTip = "Updates only this listener's coordinate space without resetting its other options."))
	FOpenMobileSensorOperationResult SetSensorCoordinateSpace(
		EOpenMobileSensorCoordinateSpace CoordinateSpace
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Options", meta = (DisplayName = "Set Sensor Lifecycle Policy", Keywords = "OpenMobile sensors listener update background suspend stop", ToolTip = "Updates only this listener's foreground and background lifecycle policy."))
	FOpenMobileSensorOperationResult SetSensorLifecyclePolicy(
		EOpenMobileSensorLifecyclePolicy LifecyclePolicy
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Options", meta = (DisplayName = "Set Sensor Filter Options", Keywords = "OpenMobile sensors listener update low pass high pass smoothing dead zone", ToolTip = "Updates only this listener's filter settings without resetting its rate, coordinates, or lifecycle policy."))
	FOpenMobileSensorOperationResult SetSensorFilterOptions(
		const FOpenMobileSensorFilterOptions& Filters
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Last Sensor Sample Drop", ToolTip = "Returns the most recent sample-loss report cached by this listener."))
	bool GetLastSampleDrop(FOpenMobileSensorDropInfo& OutDropInfo) const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Last Sensor Error", ToolTip = "Returns the most recent compact runtime error cached by this listener."))
	bool GetLastSensorError(FOpenMobileSensorRuntimeError& OutError) const;

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
	virtual void HandleAttitudeSample(const FOpenMobileAttitudeSensorSample& Sample);
	virtual void HandleScalarSample(const FOpenMobileScalarSensorSample& Sample);
	virtual void HandleHeadingSample(const FOpenMobileHeadingSensorSample& Sample);
	virtual void HandleStepsSample(const FOpenMobileStepsSensorSample& Sample);
	virtual void HandleActivitySample(const FOpenMobileActivitySensorSample& Sample);
	virtual void HandleOrientationSample(const FOpenMobileOrientationSensorSample& Sample);
	virtual void HandleProximitySample(const FOpenMobileProximitySensorSample& Sample);
	FOpenMobileSensorOperationResult UpdateListenerOptions(
		const FOpenMobileSensorStreamOptions& Options
	);
	FOpenMobileSensorOperationResult RecenterListenerAttitude(
		EOpenMobileSensorRecenterMode Mode
	);
	FOpenMobileSensorOperationResult RequestListenerCalibration();
	static void ResolveControlOutcome(
		const FOpenMobileSensorOperationResult& Operation,
		EOpenMobileSensorControlOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
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
	void HandleAttitudeBatch(FOpenMobileSensorSubscriptionHandle InHandle,
		const FOpenMobileAttitudeSensorBatch& Batch);
	void HandleScalarBatch(FOpenMobileSensorSubscriptionHandle InHandle,
		const FOpenMobileScalarSensorBatch& Batch);
	void HandleHeadingBatch(FOpenMobileSensorSubscriptionHandle InHandle,
		const FOpenMobileHeadingSensorBatch& Batch);
	void HandleStepsBatch(FOpenMobileSensorSubscriptionHandle InHandle,
		const FOpenMobileStepsSensorBatch& Batch);
	void HandleActivityBatch(FOpenMobileSensorSubscriptionHandle InHandle,
		const FOpenMobileActivitySensorBatch& Batch);
	void HandleOrientationBatch(FOpenMobileSensorSubscriptionHandle InHandle,
		const FOpenMobileOrientationSensorBatch& Batch);
	void HandleProximityBatch(FOpenMobileSensorSubscriptionHandle InHandle,
		const FOpenMobileProximitySensorBatch& Batch);
	void HandleSamplesDropped(
		FOpenMobileSensorSubscriptionHandle InHandle,
		const FOpenMobileSensorDropInfo& DropInfo
	);
	void FinishFromOperation(
		const FOpenMobileSensorOperationResult& Operation
	);
	void BroadcastFailure();
	void PublishRuntimeError(
		const FOpenMobileSensorOperationResult& Operation
	);
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
	FDelegateHandle SamplesDroppedHandle;
	FTSTicker::FDelegateHandle OwnerTickerHandle;
	EOpenMobileSensorSubscriptionState CachedState =
		EOpenMobileSensorSubscriptionState::Invalid;
	bool bStartedBroadcast = false;
	FOpenMobileSensorDropInfo LastDropInfo;
	bool bHasDropInfo = false;
	FOpenMobileSensorRuntimeError LastRuntimeError;
	bool bHasRuntimeError = false;
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

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Gyroscope Sample", Keywords = "OpenMobile sensors gyro angular velocity cached", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileAccelerometerSampleDynamic,
	UOpenMobileSensorListener*,
	Listener,
	UPARAM(DisplayName = "Acceleration (m/s2)") FVector,
	AccelerationMetresPerSecondSquared,
	FOpenMobileSensorSampleInfo,
	SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileAccelerometerListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Sample", ToolTip = "Broadcast acceleration in metres per second squared."))
	FOpenMobileAccelerometerSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Accelerometer", Keywords = "OpenMobile sensors accelerometer acceleration motion movement tilt", ToolTip = "Starts an owner-scoped accelerometer listener with automatic cleanup."))
	static UOpenMobileAccelerometerListener* ListenForAccelerometer(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Accelerometer Sample", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
	bool GetLatestAcceleration(
		UPARAM(DisplayName = "Acceleration (m/s2)") FVector& OutAccelerationMetresPerSecondSquared,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

protected:
	virtual void HandleVectorSample(const FOpenMobileVectorSensorSample& InSample) override;

private:
	FOpenMobileVectorSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileMagnetometerSampleDynamic,
	UOpenMobileSensorListener*,
	Listener,
	UPARAM(DisplayName = "Magnetic Field (uT)") FVector,
	MagneticFieldMicroteslas,
	FOpenMobileSensorSampleInfo,
	SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileMagnetometerListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Sample", ToolTip = "Broadcast magnetic field strength in microteslas."))
	FOpenMobileMagnetometerSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Magnetometer", Keywords = "OpenMobile sensors magnetometer magnetic field compass microtesla", ToolTip = "Starts an owner-scoped magnetometer listener with automatic cleanup."))
	static UOpenMobileMagnetometerListener* ListenForMagnetometer(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Magnetometer Sample", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
	bool GetLatestMagneticField(
		UPARAM(DisplayName = "Magnetic Field (uT)") FVector& OutMagneticFieldMicroteslas,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Request Magnetometer Calibration", ExpandEnumAsExecs = "Outcome", ToolTip = "Explicitly requests native calibration UI for this magnetometer listener when supported."))
	void RequestMagnetometerCalibration(
		EOpenMobileSensorControlOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

protected:
	virtual void HandleVectorSample(const FOpenMobileVectorSensorSample& InSample) override;

private:
	FOpenMobileVectorSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileGravitySampleDynamic,
	UOpenMobileSensorListener*,
	Listener,
	UPARAM(DisplayName = "Gravity (m/s2)") FVector,
	GravityMetresPerSecondSquared,
	FOpenMobileSensorSampleInfo,
	SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileGravityListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Sample", ToolTip = "Broadcast the gravity vector in metres per second squared."))
	FOpenMobileGravitySampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Gravity", Keywords = "OpenMobile sensors gravity down vector tilt motion", ToolTip = "Starts an owner-scoped gravity listener with automatic cleanup."))
	static UOpenMobileGravityListener* ListenForGravity(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Gravity Sample", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
	bool GetLatestGravity(
		UPARAM(DisplayName = "Gravity (m/s2)") FVector& OutGravityMetresPerSecondSquared,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

protected:
	virtual void HandleVectorSample(const FOpenMobileVectorSensorSample& InSample) override;

private:
	FOpenMobileVectorSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileLinearAccelerationSampleDynamic,
	UOpenMobileSensorListener*,
	Listener,
	UPARAM(DisplayName = "Linear Acceleration (m/s2)") FVector,
	LinearAccelerationMetresPerSecondSquared,
	FOpenMobileSensorSampleInfo,
	SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileLinearAccelerationListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Sample", ToolTip = "Broadcast acceleration with gravity removed in metres per second squared."))
	FOpenMobileLinearAccelerationSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Linear Acceleration", Keywords = "OpenMobile sensors linear acceleration user acceleration movement gravity removed", ToolTip = "Starts an owner-scoped linear-acceleration listener with automatic cleanup."))
	static UOpenMobileLinearAccelerationListener* ListenForLinearAcceleration(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Linear Acceleration Sample", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
	bool GetLatestLinearAcceleration(
		UPARAM(DisplayName = "Linear Acceleration (m/s2)") FVector& OutLinearAccelerationMetresPerSecondSquared,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

protected:
	virtual void HandleVectorSample(const FOpenMobileVectorSensorSample& InSample) override;

private:
	FOpenMobileVectorSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOpenMobileShakeSampleDynamic,
	UOpenMobileSensorListener*,
	Listener,
	UPARAM(DisplayName = "Strength (m/s2)") double,
	StrengthMetresPerSecondSquared,
	UPARAM(DisplayName = "Duration (s)") double,
	DurationSeconds,
	int32,
	ImpulseCount,
	FOpenMobileSensorSampleInfo,
	SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileShakeListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Sample", ToolTip = "Broadcast a detected shake with strength, duration, and impulse count."))
	FOpenMobileShakeSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Shake", Keywords = "OpenMobile sensors shake gesture impulse movement", ToolTip = "Starts an owner-scoped shake detector with automatic cleanup."))
	static UOpenMobileShakeListener* ListenForShake(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Shake", ToolTip = "Copies one coherent snapshot of the last shake delivered to this listener."))
	bool GetLatestShake(
		UPARAM(DisplayName = "Shake") FOpenMobileShakeEventData& OutShake,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

protected:
	virtual void HandleVectorSample(const FOpenMobileVectorSensorSample& InSample) override;

private:
	FOpenMobileVectorSensorSample LatestSample;
	bool bHasSample = false;
};
