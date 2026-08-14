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

struct FOpenMobileHapticsTimelineCacheStatistics
{
	uint64 HitCount = 0;
	uint64 MissCount = 0;
	uint64 EvictionCount = 0;
	int32 EntryCount = 0;
	int64 MemoryBytes = 0;
	int32 MaximumEntryCount = 0;
	int64 MaximumMemoryBytes = 0;
	double IdleLifetimeSeconds = 0.0;
};

class FOpenMobileHapticsTimelineManager final
{
public:
	/** Creates a bounded prepared timeline cache, invalid limits are normalized before the first insert. */
	explicit FOpenMobileHapticsTimelineManager(
		int32 InMaximumCacheEntries = 32,
		int64 InMaximumCacheBytes = 4 * 1024 * 1024,
		double InIdleLifetimeSeconds = 30.0
	);
	/** Releases the private cache state where its complete type is available. */
	~FOpenMobileHapticsTimelineManager();

	/** Resolves against the current platform clock, which is what ordinary playback and cache ageing need. */
	FOpenMobileHapticsTimelineLookup Resolve(
		FName BackendName,
		const UOpenMobileHapticPatternAsset& Pattern,
		const FOpenMobileHapticLoopOptions& Loop,
		const FOpenMobileHapticCapabilities& Capabilities,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy,
		uint64 LifecycleGeneration
	);
	/** Accepts an explicit access time for deterministic tests and lifecycle-controlled cache lookups. */
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
	/** Applies new cache limits immediately and evicts old entries if the current cache no longer fits. */
	void SetLimits(const FOpenMobileHapticsPreparedResourceLimits& Limits);
	/** Removes entries that haven't been used within the idle lifetime, pinned timeline copies can still finish playback. */
	void PruneIdle(double CurrentTimeSeconds);
	/** Drops all cached preparations when native lifecycle generation changes. */
	void Clear();
	/** Reports current prepared entry count without exposing the cache keys. */
	int32 GetCacheEntryCount() const;
	/** Reports estimated native payload bytes used for budget decisions. */
	int64 GetCacheMemoryBytes() const;
	/** Returns counters and current limits together so diagnostics describe the same cache state. */
	FOpenMobileHapticsTimelineCacheStatistics GetStatistics() const;
	/** Clears hit, miss, and eviction counters while retaining prepared entries. */
	void ResetStatistics();

private:
	struct FState;
	TUniquePtr<FState> State;
};
