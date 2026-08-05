#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorListener.h"
#include "OpenMobileSensorActivityListeners.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileStepCountSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Steps") int64, Steps,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileStepCountListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Sample", ToolTip = "Broadcast the current resettable step-count session total."))
	FOpenMobileStepCountSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Step Count", Keywords = "OpenMobile sensors step count walking fitness session", ToolTip = "Starts an owner-scoped step-count listener with automatic cleanup."))
	static UOpenMobileStepCountListener* ListenForStepCount(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Step Count", ToolTip = "Copies one coherent snapshot of the current step-count session total."))
	bool GetLatestSteps(int64& OutSteps,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Reset Step Count", ExpandEnumAsExecs = "Outcome", Keywords = "OpenMobile sensors steps session reset zero baseline", ToolTip = "Resets only this typed step-count listener. The next accepted native total becomes zero."))
	void ResetStepCount(
		EOpenMobileSensorControlOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

protected:
	virtual void HandleStepsSample(const FOpenMobileStepsSensorSample& InSample) override;

private:
	FOpenMobileStepsSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileStepEventSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Detected Step Delta") int64, DetectedStepDelta,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileStepEventListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Sample", ToolTip = "Broadcast newly detected step deltas."))
	FOpenMobileStepEventSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Step Events", Keywords = "OpenMobile sensors step event detector walking footstep", ToolTip = "Starts an owner-scoped step-event listener with automatic cleanup."))
	static UOpenMobileStepEventListener* ListenForStepEvents(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Step Event", ToolTip = "Copies one coherent snapshot of the last detected step delta."))
	bool GetLatestStepDelta(int64& OutDetectedStepDelta,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleStepsSample(const FOpenMobileStepsSensorSample& InSample) override;

private:
	FOpenMobileStepsSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOpenMobilePedometerSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Steps") int64, Steps,
	FOpenMobilePedometerMetrics, Metrics,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobilePedometerListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Sample", ToolTip = "Broadcast step count with optional distance, floors, pace, and cadence metrics."))
	FOpenMobilePedometerSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Pedometer", Keywords = "OpenMobile sensors pedometer steps distance pace cadence floors fitness", ToolTip = "Starts an owner-scoped pedometer listener with automatic cleanup."))
	static UOpenMobilePedometerListener* ListenForPedometer(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Pedometer Sample", ToolTip = "Copies one coherent snapshot of the latest pedometer total and metrics."))
	bool GetLatestPedometer(int64& OutSteps,
		FOpenMobilePedometerMetrics& OutMetrics,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleStepsSample(const FOpenMobileStepsSensorSample& InSample) override;

private:
	FOpenMobileStepsSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOpenMobileMotionActivitySampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Activity") EOpenMobileMotionActivity, Activity,
	EOpenMobileActivityConfidence, Confidence,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileMotionActivityListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Sample", ToolTip = "Broadcast the current classified motion activity and confidence."))
	FOpenMobileMotionActivitySampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Motion Activity", Keywords = "OpenMobile sensors motion activity walking running cycling automotive stationary", ToolTip = "Starts an owner-scoped motion-activity listener with automatic cleanup."))
	static UOpenMobileMotionActivityListener* ListenForMotionActivity(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Motion Activity", ToolTip = "Copies one coherent snapshot of the latest motion activity."))
	bool GetLatestActivity(EOpenMobileMotionActivity& OutActivity,
		EOpenMobileActivityConfidence& OutConfidence,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Set Motion Activity Thresholds", ToolTip = "Updates only the minimum activity confidence and stable duration for this listener."))
	FOpenMobileSensorOperationResult SetActivityThresholds(
		EOpenMobileActivityConfidence MinimumConfidence,
		double MinimumStableDurationSeconds
	);

protected:
	virtual void HandleActivitySample(const FOpenMobileActivitySensorSample& InSample) override;

private:
	FOpenMobileActivitySensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOpenMobileActivityTransitionSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Activity") EOpenMobileMotionActivity, Activity,
	UPARAM(DisplayName = "Transition") EOpenMobileActivityTransition, Transition,
	EOpenMobileActivityConfidence, Confidence,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileActivityTransitionListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Sample", ToolTip = "Broadcast activity start and stop transitions with confidence."))
	FOpenMobileActivityTransitionSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Activity Transitions", Keywords = "OpenMobile sensors activity transition started stopped walking running", ToolTip = "Starts an owner-scoped activity-transition listener with automatic cleanup."))
	static UOpenMobileActivityTransitionListener* ListenForActivityTransitions(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Activity Transition", ToolTip = "Copies one coherent snapshot of the latest activity transition."))
	bool GetLatestTransition(EOpenMobileMotionActivity& OutActivity,
		EOpenMobileActivityTransition& OutTransition,
		EOpenMobileActivityConfidence& OutConfidence,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Set Activity Transition Thresholds", ToolTip = "Updates only the minimum activity confidence and stable duration for this listener."))
	FOpenMobileSensorOperationResult SetActivityThresholds(
		EOpenMobileActivityConfidence MinimumConfidence,
		double MinimumStableDurationSeconds
	);

protected:
	virtual void HandleActivitySample(const FOpenMobileActivitySensorSample& InSample) override;

private:
	FOpenMobileActivitySensorSample LatestSample;
	bool bHasSample = false;
};
