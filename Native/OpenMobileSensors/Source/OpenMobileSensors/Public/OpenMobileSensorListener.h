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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(
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
	bRateAdjusted,
	EOpenMobileSensorLifecyclePolicy,
	BackgroundBehavior
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOpenMobileSensorListenerSharedRateWarningDynamic,
	UOpenMobileSensorListener*,
	Listener,
	UPARAM(DisplayName = "Listener Rate (Hz)") double,
	ListenerRateHz,
	UPARAM(DisplayName = "Shared Physical Rate (Hz)") double,
	SharedPhysicalRateHz,
	FText,
	Message,
	FText,
	Correction
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

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorVectorSampleNative,
	const FOpenMobileVectorSensorSample&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorAttitudeSampleNative,
	const FOpenMobileAttitudeSensorSample&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorScalarSampleNative,
	const FOpenMobileScalarSensorSample&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorHeadingSampleNative,
	const FOpenMobileHeadingSensorSample&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorStepsSampleNative,
	const FOpenMobileStepsSensorSample&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorActivitySampleNative,
	const FOpenMobileActivitySensorSample&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorOrientationSampleNative,
	const FOpenMobileOrientationSensorSample&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileSensorProximitySampleNative,
	const FOpenMobileProximitySensorSample&
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

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Shared Stream Rate Raised", ToolTip = "Broadcast when another compatible listener raises the shared physical sensor rate above this listener's resolved rate, which can increase power use."))
	FOpenMobileSensorListenerSharedRateWarningDynamic SharedStreamRateRaised;

	/** Use this to stop this listener. Calling Stop more than once is safe. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Stop Sensor Listener", Keywords = "OpenMobile sensors cancel cleanup", ToolTip = "Stops this listener. Calling Stop more than once is safe."))
	void Stop();

	/** You'll get true while this listener is starting, active, or paused. Finished listeners stay false even if Blueprint still holds the object. */
	virtual bool IsActive() const override;

	/** You'll get the listener's cached state without querying live service state. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Sensor Listener State", ToolTip = "Returns the listener's cached state without querying live service state."))
	EOpenMobileSensorSubscriptionState GetListenerState() const;

	/** You'll get the sensor identifier selected for this listener. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Listener Sensor", ToolTip = "Returns the sensor identifier selected for this listener."))
	FOpenMobileSensorIdentifier GetSensor() const;

	/** You'll get the resolved stream options used by this listener. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Applied Sensor Listener Options", ToolTip = "Returns the resolved stream options used by this listener."))
	FOpenMobileSensorStreamOptions GetAppliedOptions() const;

	/** Use this when you want to change only this listener's rate preset. Custom Frequency is used only for the Custom preset. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Options", meta = (DisplayName = "Set Sensor Rate Preset", Keywords = "OpenMobile sensors listener update frequency hertz", AdvancedDisplay = "CustomFrequencyHz", ToolTip = "Updates only this listener's rate preset. Custom Frequency is used only for the Custom preset."))
	FOpenMobileSensorOperationResult SetSensorRatePreset(
		EOpenMobileSensorRatePreset RatePreset,
		double CustomFrequencyHz = 15.0
	);

	/** Use this when you want to change only this listener's coordinate space without resetting its other options. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Options", meta = (DisplayName = "Set Sensor Coordinate Space", Keywords = "OpenMobile sensors listener update device screen coordinates", ToolTip = "Updates only this listener's coordinate space without resetting its other options."))
	FOpenMobileSensorOperationResult SetSensorCoordinateSpace(
		EOpenMobileSensorCoordinateSpace CoordinateSpace
	);

	/** Use this when you want to change only this listener's foreground and background lifecycle policy. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Options", meta = (DisplayName = "Set Sensor Lifecycle Policy", Keywords = "OpenMobile sensors listener update background suspend stop", ToolTip = "Updates only this listener's foreground and background lifecycle policy."))
	FOpenMobileSensorOperationResult SetSensorLifecyclePolicy(
		EOpenMobileSensorLifecyclePolicy LifecyclePolicy
	);

	/** Use this when you want to change only this listener's filter settings without resetting its rate, coordinates, or lifecycle policy. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Options", meta = (DisplayName = "Set Sensor Filter Options", Keywords = "OpenMobile sensors listener update low pass high pass smoothing dead zone", ToolTip = "Updates only this listener's filter settings without resetting its rate, coordinates, or lifecycle policy."))
	FOpenMobileSensorOperationResult SetSensorFilterOptions(
		const FOpenMobileSensorFilterOptions& Filters
	);

	/** You'll get the most recent sample-loss report cached by this listener. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Last Sensor Sample Drop", ToolTip = "Returns the most recent sample-loss report cached by this listener."))
	bool GetLastSampleDrop(FOpenMobileSensorDropInfo& OutDropInfo) const;

	/** You'll get the most recent compact runtime error cached by this listener. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Last Sensor Error", ToolTip = "Returns the most recent compact runtime error cached by this listener."))
	bool GetLastSensorError(FOpenMobileSensorRuntimeError& OutError) const;

	/** Unreal calls this after the async listener node is wired. It validates owner lifetime before submitting one stream request. */
	virtual void Activate() override;

	/** Use this to compress the common header into the pins every typed Sample event shares. It copies one delivery only. */
	static FOpenMobileSensorSampleInfo MakeSampleInfo(
		const FOpenMobileSensorSampleHeader& Header
	);

	/** Bind here when native code needs vector samples from this listener only. The delegate stops with the listener. */
	FOnOpenMobileSensorVectorSampleNative& OnVectorSampleNative()
	{
		return VectorSampleNative;
	}

	/** Bind here when native code needs attitude samples from this listener only. You won't receive subsystem-wide traffic. */
	FOnOpenMobileSensorAttitudeSampleNative& OnAttitudeSampleNative()
	{
		return AttitudeSampleNative;
	}

	/** Bind here when native code needs scalar samples from this listener only. The delegate follows owner cleanup also. */
	FOnOpenMobileSensorScalarSampleNative& OnScalarSampleNative()
	{
		return ScalarSampleNative;
	}

	/** Bind here when native code needs heading samples from this listener only. Magnetic and true-heading listeners stay separate. */
	FOnOpenMobileSensorHeadingSampleNative& OnHeadingSampleNative()
	{
		return HeadingSampleNative;
	}

	/** Bind here when native code needs step-family samples from this listener only. The typed listener decides the count semantics. */
	FOnOpenMobileSensorStepsSampleNative& OnStepsSampleNative()
	{
		return StepsSampleNative;
	}

	/** Bind here when native code needs activity-family samples from this listener only. Normal activity and transitions keep their typed payload. */
	FOnOpenMobileSensorActivitySampleNative& OnActivitySampleNative()
	{
		return ActivitySampleNative;
	}

	/** Bind here when native code needs physical-orientation samples from this listener only. Lifecycle cleanup removes the binding with the listener. */
	FOnOpenMobileSensorOrientationSampleNative& OnOrientationSampleNative()
	{
		return OrientationSampleNative;
	}

	/** Bind here when native code needs proximity samples from this listener only. Optional distance stays in the typed sample. */
	FOnOpenMobileSensorProximitySampleNative& OnProximitySampleNative()
	{
		return ProximitySampleNative;
	}

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests call this to check owner teardown without waiting for the engine ticker. Shipping builds don't expose it. */
	bool TickOwnerForTests();
#endif

protected:
	/** Every typed listener calls this before Activate. It stores one owner, one sensor, and either project defaults or the supplied advanced options. */
	void ConfigureListener(
		const UObject* WorldContextObject,
		UObject* ListenerOwner,
		EOpenMobileSensorType Sensor,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset,
		EOpenMobileSensorCoordinateSpace CoordinateSpace,
		bool bUseAdvancedOptions
	);
	/** Step Count calls this before activation so native totals become an owner-scoped resettable session. Other step listeners shouldn't use it. */
	void ConfigureResettableStepCountSession();

	/** Override this when a vector listener needs to publish sensor-specific pins. The base has already checked handle ownership and order. */
	virtual void HandleVectorSample(
		const FOpenMobileVectorSensorSample& Sample
	);
	/** Override this when an attitude listener needs typed output. One accepted delivery reaches this hook only. */
	virtual void HandleAttitudeSample(const FOpenMobileAttitudeSensorSample& Sample);

	/** Override this when a scalar listener needs typed output. One accepted delivery reaches this hook only. */
	virtual void HandleScalarSample(const FOpenMobileScalarSensorSample& Sample);

	/** Override this when a heading listener needs typed output. The reference and accuracy fields are already coherent. */
	virtual void HandleHeadingSample(const FOpenMobileHeadingSensorSample& Sample);

	/** Override this when a step listener needs typed output. Session rebasing has already happened when enabled. */
	virtual void HandleStepsSample(const FOpenMobileStepsSensorSample& Sample);

	/** Override this when an activity listener needs typed output. Filtering and transition stability are already applied. */
	virtual void HandleActivitySample(const FOpenMobileActivitySensorSample& Sample);

	/** Override this when a physical-orientation listener needs typed output. Debounce has already accepted the posture. */
	virtual void HandleOrientationSample(const FOpenMobileOrientationSensorSample& Sample);

	/** Override this when a proximity listener needs typed output. Near state and optional distance come from one sample. */
	virtual void HandleProximitySample(const FOpenMobileProximitySensorSample& Sample);

	/** Use this from a focused listener control after changing a copy of Applied Options. It preserves the listener and returns the provider result. */
	FOpenMobileSensorOperationResult UpdateListenerOptions(
		const FOpenMobileSensorStreamOptions& Options
	);
	/** Use this from typed attitude controls so recentering stays scoped to this listener. Unsupported providers return their real result. */
	FOpenMobileSensorOperationResult RecenterListenerAttitude(
		EOpenMobileSensorRecenterMode Mode
	);
	/** Use this from Step Count only. It keeps the native baseline and reported session total in sync. */
	FOpenMobileSensorOperationResult ResetListenerStepCount();

	/** Use this from Relative Altitude only. The next accepted altitude becomes the local zero. */
	FOpenMobileSensorOperationResult RecenterListenerRelativeAltitude();

	/** Use this from typed calibration nodes only. It asks the provider tied to this listener, not every sensor. */
	FOpenMobileSensorOperationResult RequestListenerCalibration();

	/** This turns one provider result into the standard control execution pins and text. Typed controls shouldn't each invent their own mapping. */
	static void ResolveControlOutcome(
		const FOpenMobileSensorOperationResult& Operation,
		EOpenMobileSensorControlOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);
	/** This stops the owned stream when the listener or its world is cancelled. Repeated stop remains harmless. */
	virtual void CancelNativeOperation() override;

	/** This broadcasts Started only after the subsystem confirms an active listener. */
	virtual void OnActionSucceeded() override;

	/** This selects Permission Required, Unavailable, or Failed from the typed startup result. */
	virtual void OnActionFailed(const FOpenMobileError& Error) override;

	/** This broadcasts Stopped once after explicit stop or owner cleanup. */
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
	void BroadcastSharedStreamRateWarning();
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
	bool bResettableStepCountSession = false;
	FOpenMobileSensorDropInfo LastDropInfo;
	bool bHasDropInfo = false;
	FOpenMobileSensorRuntimeError LastRuntimeError;
	bool bHasRuntimeError = false;
	FOnOpenMobileSensorVectorSampleNative VectorSampleNative;
	FOnOpenMobileSensorAttitudeSampleNative AttitudeSampleNative;
	FOnOpenMobileSensorScalarSampleNative ScalarSampleNative;
	FOnOpenMobileSensorHeadingSampleNative HeadingSampleNative;
	FOnOpenMobileSensorStepsSampleNative StepsSampleNative;
	FOnOpenMobileSensorActivitySampleNative ActivitySampleNative;
	FOnOpenMobileSensorOrientationSampleNative OrientationSampleNative;
	FOnOpenMobileSensorProximitySampleNative ProximitySampleNative;
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
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileGyroscopeListenerSampleNative,
	const FVector&
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileGyroscopeListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Sample", ToolTip = "Broadcast the latest coalesced gyroscope sample in radians per second."))
	FOpenMobileGyroscopeSampleDynamic Sample;

	/** Use this when you want gyroscope samples tied to one Blueprint owner. It'll stop itself when that owner goes away. */
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

	/** Read this after Sample when you need angular velocity in radians per second. Value and Sample Info come from the same gyroscope delivery. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Gyroscope Sample", Keywords = "OpenMobile sensors gyro angular velocity cached", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
	bool GetLatestAngularVelocity(
		UPARAM(DisplayName = "Angular Velocity (rad/s)") FVector& OutAngularVelocityRadiansPerSecond,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

	/** Bind here when native code wants angular velocity with the compact sample info. The delegate belongs to this gyroscope listener only. */
	FOnOpenMobileGyroscopeListenerSampleNative& OnSampleNative()
	{
		return SampleNative;
	}

protected:
	/** This caches one accepted gyroscope sample and broadcasts angular velocity in radians per second. It won't publish another vector sensor's units. */
	virtual void HandleVectorSample(
		const FOpenMobileVectorSensorSample& InSample
	) override;

private:
	FOnOpenMobileGyroscopeListenerSampleNative SampleNative;
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

	/** Use this when you want accelerometer samples tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Accelerometer", Keywords = "OpenMobile sensors accelerometer acceleration motion movement tilt", ToolTip = "Starts an owner-scoped accelerometer listener with automatic cleanup."))
	static UOpenMobileAccelerometerListener* ListenForAccelerometer(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you need acceleration including gravity in metres per second squared. Value and Sample Info stay on one delivery. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Accelerometer Sample", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
	bool GetLatestAcceleration(
		UPARAM(DisplayName = "Acceleration (m/s2)") FVector& OutAccelerationMetresPerSecondSquared,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

protected:
	/** This caches one accepted accelerometer sample and broadcasts acceleration in metres per second squared. */
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

	/** Use this when you want magnetometer samples tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Magnetometer", Keywords = "OpenMobile sensors magnetometer magnetic field compass microtesla", ToolTip = "Starts an owner-scoped magnetometer listener with automatic cleanup."))
	static UOpenMobileMagnetometerListener* ListenForMagnetometer(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you need magnetic field in microteslas. Value, accuracy, and Sample Info won't come from different deliveries. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Magnetometer Sample", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
	bool GetLatestMagneticField(
		UPARAM(DisplayName = "Magnetic Field (uT)") FVector& OutMagneticFieldMicroteslas,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

	/** Use this when you want to ask the platform for native calibration UI for this magnetometer listener when supported. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Request Magnetometer Calibration", ExpandEnumAsExecs = "Outcome", ToolTip = "Explicitly requests native calibration UI for this magnetometer listener when supported."))
	void RequestMagnetometerCalibration(
		EOpenMobileSensorControlOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

protected:
	/** This caches one accepted magnetometer sample and broadcasts magnetic field in microteslas. Calibration state stays attached. */
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
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnOpenMobileGravityListenerSampleNative,
	const FVector&
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileGravityListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Sample", ToolTip = "Broadcast the gravity vector in metres per second squared."))
	FOpenMobileGravitySampleDynamic Sample;

	/** Use this when you want gravity samples tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Gravity", Keywords = "OpenMobile sensors gravity down vector tilt motion", ToolTip = "Starts an owner-scoped gravity listener with automatic cleanup."))
	static UOpenMobileGravityListener* ListenForGravity(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you need the gravity vector in metres per second squared. Value and Sample Info stay on one delivery. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Gravity Sample", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
	bool GetLatestGravity(
		UPARAM(DisplayName = "Gravity (m/s2)") FVector& OutGravityMetresPerSecondSquared,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

	/** Bind here when native code wants gravity with the compact sample info. The delegate belongs to this gravity listener only. */
	FOnOpenMobileGravityListenerSampleNative& OnSampleNative()
	{
		return SampleNative;
	}

protected:
	/** This caches one accepted gravity sample and broadcasts it in metres per second squared. */
	virtual void HandleVectorSample(const FOpenMobileVectorSensorSample& InSample) override;

private:
	FOnOpenMobileGravityListenerSampleNative SampleNative;
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

	/** Use this when you want linear-acceleration samples tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Linear Acceleration", Keywords = "OpenMobile sensors linear acceleration user acceleration movement gravity removed", ToolTip = "Starts an owner-scoped linear-acceleration listener with automatic cleanup."))
	static UOpenMobileLinearAccelerationListener* ListenForLinearAcceleration(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you need acceleration with gravity removed. Value and Sample Info stay on one delivery only. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Linear Acceleration Sample", ToolTip = "Copies one coherent snapshot of the last sample delivered to this listener."))
	bool GetLatestLinearAcceleration(
		UPARAM(DisplayName = "Linear Acceleration (m/s2)") FVector& OutLinearAccelerationMetresPerSecondSquared,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

protected:
	/** This caches one accepted linear-acceleration sample after gravity removal. The typed event keeps its metres-per-second-squared units. */
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

	/** Use this when you want shake events tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Shake", Keywords = "OpenMobile sensors shake gesture impulse movement", ToolTip = "Starts an owner-scoped shake detector with automatic cleanup."))
	static UOpenMobileShakeListener* ListenForShake(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you want the last shake delivered to this listener. You won't mix fields from two deliveries. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Motion", meta = (DisplayName = "Get Latest Shake", ToolTip = "Copies one coherent snapshot of the last shake delivered to this listener."))
	bool GetLatestShake(
		UPARAM(DisplayName = "Shake") FOpenMobileShakeEventData& OutShake,
		FOpenMobileSensorSampleInfo& OutSampleInfo
	) const;

protected:
	/** This caches one accepted shake result and broadcasts strength, duration, and impulse count together. */
	virtual void HandleVectorSample(const FOpenMobileVectorSensorSample& InSample) override;

private:
	FOpenMobileVectorSensorSample LatestSample;
	bool bHasSample = false;
};
