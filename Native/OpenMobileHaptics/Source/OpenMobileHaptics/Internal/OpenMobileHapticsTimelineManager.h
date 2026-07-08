#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsAndroidWaveformPolicy.h"
#include "OpenMobileHapticsAppleContinuousPolicy.h"
#include "OpenMobileHapticsAppleTransientPolicy.h"
#include "OpenMobileHapticsRepeatPolicy.h"

class UOpenMobileHapticPatternAsset;

enum class EOpenMobileHapticsTimelinePath : uint8
{
	AndroidWaveform,
	AppleTransient,
	AppleContinuous
};

struct FOpenMobileHapticsPortableTimeline
{
	uint64 ResourceId = 0;
	int64 EstimatedBytes = 0;
	EOpenMobileHapticsTimelinePath Path =
		EOpenMobileHapticsTimelinePath::AndroidWaveform;
	FOpenMobileHapticsAndroidWaveformResolution Android;
	FOpenMobileHapticsAndroidWaveformResolution AndroidControlBase;
	FOpenMobileHapticsAppleTransientResolution AppleTransient;
	FOpenMobileHapticsAppleContinuousResolution AppleContinuous;
	FOpenMobileHapticsRepeatPlan RepeatPlan;
	bool bHasRepeatPlan = false;
};

struct FOpenMobileHapticsTimelineLookup
{
	TSharedPtr<const FOpenMobileHapticsPortableTimeline, ESPMode::ThreadSafe>
		Timeline;
	bool bCacheHit = false;
};

class FOpenMobileHapticsTimelineManager final
{
public:
	explicit FOpenMobileHapticsTimelineManager(
		int32 InMaximumCacheEntries = 32,
		int64 InMaximumCacheBytes = 4 * 1024 * 1024,
		double InIdleLifetimeSeconds = 30.0
	);
	~FOpenMobileHapticsTimelineManager();

	FOpenMobileHapticsTimelineLookup Resolve(
		FName BackendName,
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticLoopOptions& Loop,
		const FOpenMobileHapticCapabilities& Capabilities,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy,
		uint64 LifecycleGeneration
	);
	FOpenMobileHapticsTimelineLookup ResolveAtTime(
		FName BackendName,
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticLoopOptions& Loop,
		const FOpenMobileHapticCapabilities& Capabilities,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy,
		uint64 LifecycleGeneration,
		double AccessTimeSeconds
	);
	void SetLimits(const FOpenMobileHapticsPreparedResourceLimits& Limits);
	void PruneIdle(double CurrentTimeSeconds);
	void Clear();
	int32 GetCacheEntryCount() const;
	int64 GetCacheMemoryBytes() const;

private:
	struct FState;
	TUniquePtr<FState> State;
};
