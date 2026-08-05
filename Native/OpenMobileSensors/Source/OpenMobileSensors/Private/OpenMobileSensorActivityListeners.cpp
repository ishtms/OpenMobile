#include "OpenMobileSensorActivityListeners.h"

UOpenMobileStepCountListener* UOpenMobileStepCountListener::ListenForStepCount(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner)
{
	UOpenMobileStepCountListener* Listener =
		NewObject<UOpenMobileStepCountListener>();
	Listener->ConfigureListener(WorldContextObject, ListenerOwner,
		EOpenMobileSensorType::StepCounter, AdvancedOptions, RatePreset,
		EOpenMobileSensorCoordinateSpace::DeviceFixed, bUseAdvancedOptions);
	Listener->ConfigureResettableStepCountSession();
	return Listener;
}

bool UOpenMobileStepCountListener::GetLatestSteps(
	int64& OutSteps,
	FOpenMobileSensorSampleInfo& OutSampleInfo) const
{
	OutSteps = 0;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutSteps = LatestSample.Count;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobileStepCountListener::ResetStepCount(
	EOpenMobileSensorControlOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details)
{
	ResolveControlOutcome(
		ResetListenerStepCount(), Outcome, Message, Correction, Details);
}

void UOpenMobileStepCountListener::HandleStepsSample(
	const FOpenMobileStepsSensorSample& InSample)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Count,
		MakeSampleInfo(LatestSample.Header));
}

UOpenMobileStepEventListener* UOpenMobileStepEventListener::ListenForStepEvents(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner)
{
	UOpenMobileStepEventListener* Listener =
		NewObject<UOpenMobileStepEventListener>();
	Listener->ConfigureListener(WorldContextObject, ListenerOwner,
		EOpenMobileSensorType::StepDetector, AdvancedOptions, RatePreset,
		EOpenMobileSensorCoordinateSpace::DeviceFixed, bUseAdvancedOptions);
	return Listener;
}

bool UOpenMobileStepEventListener::GetLatestStepDelta(
	int64& OutDetectedStepDelta,
	FOpenMobileSensorSampleInfo& OutSampleInfo) const
{
	OutDetectedStepDelta = 0;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutDetectedStepDelta = LatestSample.DetectedStepDelta;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobileStepEventListener::HandleStepsSample(
	const FOpenMobileStepsSensorSample& InSample)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.DetectedStepDelta,
		MakeSampleInfo(LatestSample.Header));
}

UOpenMobilePedometerListener* UOpenMobilePedometerListener::ListenForPedometer(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner)
{
	UOpenMobilePedometerListener* Listener =
		NewObject<UOpenMobilePedometerListener>();
	Listener->ConfigureListener(WorldContextObject, ListenerOwner,
		EOpenMobileSensorType::Pedometer, AdvancedOptions, RatePreset,
		EOpenMobileSensorCoordinateSpace::DeviceFixed, bUseAdvancedOptions);
	return Listener;
}

bool UOpenMobilePedometerListener::GetLatestPedometer(
	int64& OutSteps,
	FOpenMobilePedometerMetrics& OutMetrics,
	FOpenMobileSensorSampleInfo& OutSampleInfo) const
{
	OutSteps = 0;
	OutMetrics = {};
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutSteps = LatestSample.Count;
	OutMetrics = LatestSample.Metrics;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

void UOpenMobilePedometerListener::HandleStepsSample(
	const FOpenMobileStepsSensorSample& InSample)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Count, LatestSample.Metrics,
		MakeSampleInfo(LatestSample.Header));
}

UOpenMobileMotionActivityListener*
UOpenMobileMotionActivityListener::ListenForMotionActivity(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner)
{
	UOpenMobileMotionActivityListener* Listener =
		NewObject<UOpenMobileMotionActivityListener>();
	Listener->ConfigureListener(WorldContextObject, ListenerOwner,
		EOpenMobileSensorType::MotionActivity, AdvancedOptions, RatePreset,
		EOpenMobileSensorCoordinateSpace::DeviceFixed, bUseAdvancedOptions);
	return Listener;
}

bool UOpenMobileMotionActivityListener::GetLatestActivity(
	EOpenMobileMotionActivity& OutActivity,
	EOpenMobileActivityConfidence& OutConfidence,
	FOpenMobileSensorSampleInfo& OutSampleInfo) const
{
	OutActivity = EOpenMobileMotionActivity::Unknown;
	OutConfidence = EOpenMobileActivityConfidence::Unknown;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutActivity = LatestSample.Activity;
	OutConfidence = LatestSample.Confidence;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

FOpenMobileSensorOperationResult
UOpenMobileMotionActivityListener::SetActivityThresholds(
	EOpenMobileActivityConfidence MinimumConfidence,
	double MinimumStableDurationSeconds)
{
	FOpenMobileSensorStreamOptions Options = GetAppliedOptions();
	Options.MinimumActivityConfidence = MinimumConfidence;
	Options.MinimumActivityStableDurationSeconds =
		MinimumStableDurationSeconds;
	return UpdateListenerOptions(Options);
}

void UOpenMobileMotionActivityListener::HandleActivitySample(
	const FOpenMobileActivitySensorSample& InSample)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Activity, LatestSample.Confidence,
		MakeSampleInfo(LatestSample.Header));
}

UOpenMobileActivityTransitionListener*
UOpenMobileActivityTransitionListener::ListenForActivityTransitions(
	const UObject* WorldContextObject,
	const FOpenMobileSensorStreamOptions& AdvancedOptions,
	EOpenMobileSensorRatePreset RatePreset,
	bool bUseAdvancedOptions,
	UObject* ListenerOwner)
{
	UOpenMobileActivityTransitionListener* Listener =
		NewObject<UOpenMobileActivityTransitionListener>();
	Listener->ConfigureListener(WorldContextObject, ListenerOwner,
		EOpenMobileSensorType::ActivityTransition, AdvancedOptions, RatePreset,
		EOpenMobileSensorCoordinateSpace::DeviceFixed, bUseAdvancedOptions);
	return Listener;
}

bool UOpenMobileActivityTransitionListener::GetLatestTransition(
	EOpenMobileMotionActivity& OutActivity,
	EOpenMobileActivityTransition& OutTransition,
	EOpenMobileActivityConfidence& OutConfidence,
	FOpenMobileSensorSampleInfo& OutSampleInfo) const
{
	OutActivity = EOpenMobileMotionActivity::Unknown;
	OutTransition = EOpenMobileActivityTransition::None;
	OutConfidence = EOpenMobileActivityConfidence::Unknown;
	OutSampleInfo = {};
	if (!bHasSample)
	{
		return false;
	}
	OutActivity = LatestSample.Activity;
	OutTransition = LatestSample.Transition;
	OutConfidence = LatestSample.Confidence;
	OutSampleInfo = MakeSampleInfo(LatestSample.Header);
	return true;
}

FOpenMobileSensorOperationResult
UOpenMobileActivityTransitionListener::SetActivityThresholds(
	EOpenMobileActivityConfidence MinimumConfidence,
	double MinimumStableDurationSeconds)
{
	FOpenMobileSensorStreamOptions Options = GetAppliedOptions();
	Options.MinimumActivityConfidence = MinimumConfidence;
	Options.MinimumActivityStableDurationSeconds =
		MinimumStableDurationSeconds;
	return UpdateListenerOptions(Options);
}

void UOpenMobileActivityTransitionListener::HandleActivitySample(
	const FOpenMobileActivitySensorSample& InSample)
{
	LatestSample = InSample;
	bHasSample = true;
	Sample.Broadcast(this, LatestSample.Activity, LatestSample.Transition,
		LatestSample.Confidence, MakeSampleInfo(LatestSample.Header));
}
