#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsAndroidWaveformPolicy.h"
#include "OpenMobileHapticsAppleContinuousPolicy.h"
#include "OpenMobileHapticsAppleTransientPolicy.h"

class UOpenMobileHapticPatternAsset;

enum class EOpenMobileHapticsTimelinePath : uint8
{
	AndroidWaveform,
	AppleTransient,
	AppleContinuous
};

struct FOpenMobileHapticsPortableTimeline
{
	EOpenMobileHapticsTimelinePath Path =
		EOpenMobileHapticsTimelinePath::AndroidWaveform;
	FOpenMobileHapticsAndroidWaveformResolution Android;
	FOpenMobileHapticsAppleTransientResolution AppleTransient;
	FOpenMobileHapticsAppleContinuousResolution AppleContinuous;
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
		int32 InMaximumCacheEntries = 32
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
	void Clear();
	int32 GetCacheEntryCount() const;

private:
	struct FState;
	TUniquePtr<FState> State;
};
