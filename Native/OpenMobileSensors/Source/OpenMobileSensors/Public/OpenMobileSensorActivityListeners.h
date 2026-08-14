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

	/** Use this when you want resettable step-count totals tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Step Count", Keywords = "OpenMobile sensors step count walking fitness session", ToolTip = "Starts an owner-scoped step-count listener with automatic cleanup."))
	static UOpenMobileStepCountListener* ListenForStepCount(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you want the current step-count session total. You won't mix fields from two deliveries. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Step Count", ToolTip = "Copies one coherent snapshot of the current step-count session total."))
	bool GetLatestSteps(int64& OutSteps,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

	/** Use this to reset only this typed step-count listener. The next accepted native total becomes zero. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Reset Step Count", ExpandEnumAsExecs = "Outcome", Keywords = "OpenMobile sensors steps session reset zero baseline", ToolTip = "Resets only this typed step-count listener. The next accepted native total becomes zero."))
	void ResetStepCount(
		EOpenMobileSensorControlOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	);

protected:
	/** This rebases one accepted native total into the listener's resettable session count. */
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

	/** Use this when you want individual step events tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Step Events", Keywords = "OpenMobile sensors step event detector walking footstep", ToolTip = "Starts an owner-scoped step-event listener with automatic cleanup."))
	static UOpenMobileStepEventListener* ListenForStepEvents(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you want the last detected step delta. You won't mix fields from two deliveries. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Step Event", ToolTip = "Copies one coherent snapshot of the last detected step delta."))
	bool GetLatestStepDelta(int64& OutDetectedStepDelta,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	/** This publishes the accepted step delta only. Cumulative totals belong to Step Count or Pedometer. */
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

	/** Use this when you want step totals and pedometer metrics tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Pedometer", Keywords = "OpenMobile sensors pedometer steps distance pace cadence floors fitness", ToolTip = "Starts an owner-scoped pedometer listener with automatic cleanup."))
	static UOpenMobilePedometerListener* ListenForPedometer(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you want the latest pedometer total and metrics. You won't mix fields from two deliveries. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Pedometer Sample", ToolTip = "Copies one coherent snapshot of the latest pedometer total and metrics."))
	bool GetLatestPedometer(int64& OutSteps,
		FOpenMobilePedometerMetrics& OutMetrics,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	/** This caches the accepted pedometer total with whichever optional metrics the provider supplied. Missing metrics stay unavailable. */
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

	/** Use this when you want motion-activity updates tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Motion Activity", Keywords = "OpenMobile sensors motion activity walking running cycling automotive stationary", ToolTip = "Starts an owner-scoped motion-activity listener with automatic cleanup."))
	static UOpenMobileMotionActivityListener* ListenForMotionActivity(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you want the latest motion activity. You won't mix fields from two deliveries. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Motion Activity", ToolTip = "Copies one coherent snapshot of the latest motion activity."))
	bool GetLatestActivity(EOpenMobileMotionActivity& OutActivity,
		EOpenMobileActivityConfidence& OutConfidence,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

	/** Use this when motion activity feels too noisy or too slow to settle. It changes confidence and stable duration without rebuilding the listener. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Set Motion Activity Thresholds", ToolTip = "Updates only the minimum activity confidence and stable duration for this listener."))
	FOpenMobileSensorOperationResult SetActivityThresholds(
		EOpenMobileActivityConfidence MinimumConfidence,
		double MinimumStableDurationSeconds
	);

protected:
	/** This applies the listener's confidence and stable-duration rules before broadcasting activity. */
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

	/** Use this when you want activity start and stop transitions tied to one Blueprint owner. It'll stop itself when that owner goes away. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Activity Transitions", Keywords = "OpenMobile sensors activity transition started stopped walking running", ToolTip = "Starts an owner-scoped activity-transition listener with automatic cleanup."))
	static UOpenMobileActivityTransitionListener* ListenForActivityTransitions(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	/** Read this after Sample when you want the latest activity transition. You won't mix fields from two deliveries. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Get Latest Activity Transition", ToolTip = "Copies one coherent snapshot of the latest activity transition."))
	bool GetLatestTransition(EOpenMobileMotionActivity& OutActivity,
		EOpenMobileActivityTransition& OutTransition,
		EOpenMobileActivityConfidence& OutConfidence,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

	/** Use this when transition events need stricter confidence or a longer stable duration. The active listener and its other options stay put. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Set Activity Transition Thresholds", ToolTip = "Updates only the minimum activity confidence and stable duration for this listener."))
	FOpenMobileSensorOperationResult SetActivityThresholds(
		EOpenMobileActivityConfidence MinimumConfidence,
		double MinimumStableDurationSeconds
	);

protected:
	/** This publishes a transition only after its confidence and stability rules accept it. Raw classifications won't leak through. */
	virtual void HandleActivitySample(const FOpenMobileActivitySensorSample& InSample) override;

private:
	FOpenMobileActivitySensorSample LatestSample;
	bool bHasSample = false;
};
