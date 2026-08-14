#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "OpenMobileSensorListener.h"
#include "OpenMobileSensorComponent.generated.h"

class UOpenMobileSensorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOpenMobileSensorComponentReadyDynamic,
	UOpenMobileSensorComponent*, Component,
	UOpenMobileSensorListener*, Listener,
	FOpenMobileSensorStreamOptions, AppliedOptions,
	UPARAM(DisplayName = "Applied Rate (Hz)") double, AppliedRateHz,
	EOpenMobileSensorAvailabilitySource, Source
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileSensorComponentStateDynamic,
	UOpenMobileSensorComponent*, Component,
	UOpenMobileSensorListener*, Listener
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOpenMobileSensorComponentFailureDynamic,
	UOpenMobileSensorComponent*, Component,
	UOpenMobileSensorListener*, Listener,
	FText, Message,
	FText, Correction,
	FOpenMobileSensorOperationResult, Details
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOpenMobileSensorComponentSampleDynamic,
	UOpenMobileSensorComponent*, Component,
	UOpenMobileSensorListener*, Listener,
	EOpenMobileSensorSampleFamily, SampleFamily,
	FOpenMobileSensorSampleInfo, SampleInfo
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileSensorComponentSamplesDroppedDynamic,
	UOpenMobileSensorComponent*, Component,
	UOpenMobileSensorListener*, Listener,
	FOpenMobileSensorDropInfo, DropInfo
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileSensorComponentErrorDynamic,
	UOpenMobileSensorComponent*, Component,
	UOpenMobileSensorListener*, Listener,
	FOpenMobileSensorRuntimeError, Error
);

UCLASS(
	ClassGroup = (OpenMobile),
	meta = (BlueprintSpawnableComponent, DisplayName = "OpenMobile Sensor")
)
class OPENMOBILESENSORS_API UOpenMobileSensorComponent final
	: public UActorComponent
{
	GENERATED_BODY()

public:
	/** The component starts with safe sensor defaults and Auto Activate enabled. You can change the exposed fields before play begins. */
	UOpenMobileSensorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ValidEnumValues = "Accelerometer,Gyroscope,Magnetometer,Gravity,LinearAcceleration,Attitude,MagneticHeading,TrueHeading,BarometricPressure,RelativeAltitude,AbsoluteAltitude,AmbientLight,Proximity,StepCounter,StepDetector,Pedometer,MotionActivity,ActivityTransition,PhysicalOrientation,Shake", ToolTip = "Sensor started by this component. Raw and uncalibrated variants remain available through advanced subscription APIs."))
	EOpenMobileSensorType Sensor = EOpenMobileSensorType::Accelerometer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Configurable project rate preset used by the component listener."))
	EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Coordinate space used by motion, pose, and heading sensors. Other sensors ignore this value."))
	EOpenMobileSensorCoordinateSpace CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::DeviceFixed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors|Lifecycle", meta = (ToolTip = "Stops the active listener when the owning actor ends play."))
	bool bStopWhenOwnerEndsPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "OpenMobile|Sensors|Advanced", meta = (ToolTip = "Uses Advanced Options as the listener base instead of current Project Settings defaults."))
	bool bUseAdvancedOptions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "OpenMobile|Sensors|Advanced", meta = (EditCondition = "bUseAdvancedOptions", ToolTip = "Full stream options used when Use Advanced Options is enabled. Rate Preset and Coordinate Space above remain authoritative."))
	FOpenMobileSensorStreamOptions AdvancedOptions;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Sensor Ready", ToolTip = "Broadcast when the configured listener becomes active with its resolved options and rate."))
	FOpenMobileSensorComponentReadyDynamic SensorReady;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Sample", ToolTip = "Broadcast when the configured listener delivers a sample. Use the reported family with the matching cached sample getter."))
	FOpenMobileSensorComponentSampleDynamic Sample;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Sensor Paused", ToolTip = "Broadcast when lifecycle policy pauses the component listener."))
	FOpenMobileSensorComponentStateDynamic SensorPaused;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Sensor Resumed", ToolTip = "Broadcast when the component listener resumes after a lifecycle pause."))
	FOpenMobileSensorComponentStateDynamic SensorResumed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Permission Required", ToolTip = "Broadcast when the configured sensor needs a requestable permission."))
	FOpenMobileSensorComponentFailureDynamic PermissionRequired;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Sensor Unavailable", ToolTip = "Broadcast when the configured sensor or a required input is unavailable."))
	FOpenMobileSensorComponentFailureDynamic SensorUnavailable;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Sensor Failed", ToolTip = "Broadcast when startup or delivery fails, including a direct correction."))
	FOpenMobileSensorComponentFailureDynamic SensorFailed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Sensor Stopped", ToolTip = "Broadcast after explicit stop, owner end play, or listener cleanup."))
	FOpenMobileSensorComponentStateDynamic SensorStopped;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Samples Dropped", ToolTip = "Broadcast when bounded delivery loses samples for this component listener."))
	FOpenMobileSensorComponentSamplesDroppedDynamic SamplesDropped;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Events", meta = (DisplayName = "Sensor Error", ToolTip = "Broadcast a compact runtime error scoped to this component listener."))
	FOpenMobileSensorComponentErrorDynamic SensorError;

	/** Call this after changing the component configuration, or when Auto Activate is off. It'll safely replace the component's current listener. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Start Sensor Component", Keywords = "OpenMobile sensors component activate start", ToolTip = "Starts or restarts this component's configured owner-scoped listener."))
	void StartSensor();

	/** Call this when the component should stop before its owner ends. Calling it again is safe. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors", meta = (DisplayName = "Stop Sensor Component", Keywords = "OpenMobile sensors component deactivate stop", ToolTip = "Stops this component's active listener. Calling Stop more than once is safe."))
	void StopSensor();

	/** You'll get the typed listener owned by this component. It returns null when startup hasn't succeeded or the listener has finished. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Active Sensor Component Listener", ToolTip = "Returns the typed listener created for the configured sensor, or null when inactive."))
	UOpenMobileSensorListener* GetActiveListener() const;

	/** Use this when component logic needs to branch on the owned listener's current state. It returns false before startup and after stop. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Is Sensor Component Active", ToolTip = "Returns whether the component currently owns an active sensor listener."))
	bool IsSensorActive() const;

	/** Call this after the component's Sample event when you need the latest vector value. You'll get false till that family has delivered once. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Component Vector Sample", ToolTip = "Copies the latest vector-family sample delivered to this component."))
	bool GetLatestVectorSample(FOpenMobileVectorSensorSample& OutSample) const;

	/** Call this after the component's Sample event when you need the latest attitude value. You'll get false till that family has delivered once. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Component Attitude Sample", ToolTip = "Copies the latest attitude-family sample delivered to this component."))
	bool GetLatestAttitudeSample(FOpenMobileAttitudeSensorSample& OutSample) const;

	/** Call this after the component's Sample event when you need the latest scalar value. You'll get false till that family has delivered once. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Component Scalar Sample", ToolTip = "Copies the latest scalar-family sample delivered to this component."))
	bool GetLatestScalarSample(FOpenMobileScalarSensorSample& OutSample) const;

	/** Call this after the component's Sample event when you need the latest heading value. You'll get false till that family has delivered once. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Component Heading Sample", ToolTip = "Copies the latest heading-family sample delivered to this component."))
	bool GetLatestHeadingSample(FOpenMobileHeadingSensorSample& OutSample) const;

	/** Call this after the component's Sample event when you need the latest steps value. You'll get false till that family has delivered once. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Component Steps Sample", ToolTip = "Copies the latest steps-family sample delivered to this component."))
	bool GetLatestStepsSample(FOpenMobileStepsSensorSample& OutSample) const;

	/** Call this after the component's Sample event when you need the latest activity value. You'll get false till that family has delivered once. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Component Activity Sample", ToolTip = "Copies the latest activity-family sample delivered to this component."))
	bool GetLatestActivitySample(FOpenMobileActivitySensorSample& OutSample) const;

	/** Call this after the component's Sample event when you need the latest orientation value. You'll get false till that family has delivered once. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Component Orientation Sample", ToolTip = "Copies the latest orientation-family sample delivered to this component."))
	bool GetLatestOrientationSample(FOpenMobileOrientationSensorSample& OutSample) const;

	/** Call this after the component's Sample event when you need the latest proximity value. You'll get false till that family has delivered once. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Latest Component Proximity Sample", ToolTip = "Copies the latest proximity-family sample delivered to this component."))
	bool GetLatestProximitySample(FOpenMobileProximitySensorSample& OutSample) const;

	/** Unreal calls this for Auto Activate and manual component activation. Reset forces the configured listener to be rebuilt. */
	virtual void Activate(bool bReset = false) override;

	/** Unreal calls this for component deactivation. It stops the owned listener before marking the component inactive. */
	virtual void Deactivate() override;

	/** Owner End Play reaches here before the actor disappears. The configured cleanup policy decides whether the listener stops. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UOpenMobileSensorListener* CreateListener() const;
	void BindListener(UOpenMobileSensorListener& Listener);
	void UnbindListener(UOpenMobileSensorListener& Listener);
	void ClearListener(UOpenMobileSensorListener* Listener);
	void BroadcastConfigurationFailure();
	void BroadcastSample(
		EOpenMobileSensorSampleFamily SampleFamily,
		const FOpenMobileSensorSampleHeader& Header
	);

	/** This forwards the listener's start result through Sensor Ready. You won't get a second subscription path here. */
	UFUNCTION()
	void HandleStarted(
		UOpenMobileSensorListener* Listener,
		FOpenMobileSensorStreamOptions AppliedOptions,
		double AppliedRateHz,
		EOpenMobileSensorAvailabilitySource Source,
		bool bRateAdjusted,
		EOpenMobileSensorLifecyclePolicy BackgroundBehavior
	);

	/** This forwards the owned listener's pause event, so component users don't have to bind the listener also. */
	UFUNCTION()
	void HandlePaused(UOpenMobileSensorListener* Listener);

	/** This forwards resume only after the owned listener reports it. You won't see a made-up component state change. */
	UFUNCTION()
	void HandleResumed(UOpenMobileSensorListener* Listener);

	/** This keeps permission handling on the component's own event pins. The component won't open a system prompt by itself. */
	UFUNCTION()
	void HandlePermissionRequired(
		UOpenMobileSensorListener* Listener,
		FText Message,
		FText Correction,
		FOpenMobileSensorOperationResult Details
	);

	/** This forwards the listener's unavailable result with its message and correction. You'll get the same answer from component and listener workflows. */
	UFUNCTION()
	void HandleUnavailable(
		UOpenMobileSensorListener* Listener,
		FText Message,
		FText Correction,
		FOpenMobileSensorOperationResult Details
	);

	/** This forwards terminal startup failure without starting another listener. Retry stays with the caller only. */
	UFUNCTION()
	void HandleFailed(
		UOpenMobileSensorListener* Listener,
		FText Message,
		FText Correction,
		FOpenMobileSensorOperationResult Details
	);

	/** This clears the component's listener reference before broadcasting Sensor Stopped. A callback can't read an already finished listener then. */
	UFUNCTION()
	void HandleStopped(UOpenMobileSensorListener* Listener);

	/** This forwards loss reported for this component's listener only. You'll know the event stream has a gap without reading global diagnostics. */
	UFUNCTION()
	void HandleSamplesDropped(
		UOpenMobileSensorListener* Listener,
		FOpenMobileSensorDropInfo DropInfo
	);

	/** This forwards runtime errors from this component's listener only. The report keeps the original recovery hint also. */
	UFUNCTION()
	void HandleSensorError(
		UOpenMobileSensorListener* Listener,
		FOpenMobileSensorRuntimeError Error
	);

	void HandleVectorSample(const FOpenMobileVectorSensorSample& InSample);
	void HandleAttitudeSample(const FOpenMobileAttitudeSensorSample& InSample);
	void HandleScalarSample(const FOpenMobileScalarSensorSample& InSample);
	void HandleHeadingSample(const FOpenMobileHeadingSensorSample& InSample);
	void HandleStepsSample(const FOpenMobileStepsSensorSample& InSample);
	void HandleActivitySample(const FOpenMobileActivitySensorSample& InSample);
	void HandleOrientationSample(const FOpenMobileOrientationSensorSample& InSample);
	void HandleProximitySample(const FOpenMobileProximitySensorSample& InSample);

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileSensorListener> ActiveListener;

	FOpenMobileVectorSensorSample LatestVectorSample;
	FOpenMobileAttitudeSensorSample LatestAttitudeSample;
	FOpenMobileScalarSensorSample LatestScalarSample;
	FOpenMobileHeadingSensorSample LatestHeadingSample;
	FOpenMobileStepsSensorSample LatestStepsSample;
	FOpenMobileActivitySensorSample LatestActivitySample;
	FOpenMobileOrientationSensorSample LatestOrientationSample;
	FOpenMobileProximitySensorSample LatestProximitySample;
	bool bHasVectorSample = false;
	bool bHasAttitudeSample = false;
	bool bHasScalarSample = false;
	bool bHasHeadingSample = false;
	bool bHasStepsSample = false;
	bool bHasActivitySample = false;
	bool bHasOrientationSample = false;
	bool bHasProximitySample = false;
};
