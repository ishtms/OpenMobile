#include "OpenMobileHapticsTimelineManager.h"

#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsSettings.h"

namespace OpenMobileHapticsTimelineManagerPrivate
{
	constexpr uint64 FNVOffset = 14695981039346656037ULL;
	constexpr uint64 FNVPrime = 1099511628211ULL;

	template<typename ValueType>
	void HashValue(uint64& Hash, const ValueType& Value)
	{
		const uint8* Bytes = reinterpret_cast<const uint8*>(&Value);
		for (SIZE_T Index = 0; Index < sizeof(ValueType); ++Index)
		{
			Hash ^= Bytes[Index];
			Hash *= FNVPrime;
		}
	}

	void HashIntegerLimit(
		uint64& Hash,
		const FOpenMobileHapticIntegerLimit& Limit
	)
	{
		HashValue(Hash, Limit.bKnown);
		HashValue(Hash, Limit.Value);
	}

	void HashDurationLimit(
		uint64& Hash,
		const FOpenMobileHapticDurationLimit& Limit
	)
	{
		HashValue(Hash, Limit.bKnown);
		HashValue(Hash, Limit.Seconds);
	}

	uint64 AssetSignature(const UOpenMobileHapticPatternAsset& Pattern)
	{
		uint64 Hash = FNVOffset;
		const FOpenMobileHapticCookedPatternData& Cooked =
			Pattern.GetCookedPattern();
		HashValue(Hash, Pattern.PatternVersion);
		HashValue(Hash, Cooked.DataFormatVersion);
		HashValue(Hash, Cooked.SourceHash);
		HashValue(Hash, Cooked.DurationMicroseconds);
		HashValue(Hash, Cooked.GranularityMicroseconds);
		const int32 EventCount = Cooked.Events.Num();
		const int32 CurveCount = Cooked.ParameterCurves.Num();
		HashValue(Hash, EventCount);
		HashValue(Hash, CurveCount);
		HashValue(Hash, Pattern.Loop.bLoop);
		HashValue(Hash, Pattern.Loop.RepeatCount);
		HashValue(Hash, Pattern.Loop.RepeatStartTimeSeconds);
		HashValue(Hash, Pattern.Loop.MaximumDurationSeconds);
		HashValue(Hash, Pattern.FallbackPolicy);
		return Hash;
	}

	uint64 RequestSignature(
		const FOpenMobileHapticLoopOptions& Loop,
		float RequestIntensity,
		EOpenMobileHapticFallbackPolicy FallbackPolicy
	)
	{
		uint64 Hash = FNVOffset;
		HashValue(Hash, Loop.bLoop);
		HashValue(Hash, Loop.RepeatCount);
		HashValue(Hash, Loop.RepeatStartTimeSeconds);
		HashValue(Hash, Loop.MaximumDurationSeconds);
		HashValue(Hash, RequestIntensity);
		HashValue(Hash, FallbackPolicy);
		return Hash;
	}

	uint64 CapabilitySignature(
		FName BackendName,
		const FOpenMobileHapticCapabilities& Capabilities
	)
	{
		uint64 Hash = FNVOffset;
		HashValue(Hash, Capabilities.RichHaptics);
		HashIntegerLimit(Hash, Capabilities.MaximumEventCount);
		if (BackendName == TEXT("Android"))
		{
			HashValue(Hash, Capabilities.WaveformTiming);
			HashValue(Hash, Capabilities.AmplitudeControl);
			HashValue(
				Hash,
				GetDefault<UOpenMobileHapticsSettings>()
					->MaximumPatternEventCount
			);
			return Hash;
		}

		HashValue(Hash, Capabilities.TransientEvents);
		HashValue(Hash, Capabilities.ContinuousEvents);
		HashIntegerLimit(Hash, Capabilities.MaximumControlPointCount);
		HashDurationLimit(Hash, Capabilities.MaximumDurationSeconds);
		const FOpenMobileHapticsAppleContinuousLimits Limits =
			FOpenMobileHapticsAppleContinuousPolicy::MakeLimits(
				*GetDefault<UOpenMobileHapticsSettings>(),
				Capabilities
			);
		HashValue(Hash, Limits.MaximumEventCount);
		HashValue(Hash, Limits.MaximumCurveCount);
		HashValue(Hash, Limits.MaximumCurvePointCount);
		HashValue(Hash, Limits.MaximumFiniteRepeatCount);
		HashValue(Hash, Limits.MaximumEventDurationSeconds);
		HashValue(Hash, Limits.MaximumDurationSeconds);
		return Hash;
	}

	uint64 NativeResourceId(
		FName BackendName,
		uint64 AssetHash,
		uint64 RequestHash,
		uint64 CapabilityHash,
		uint64 LifecycleGeneration
	)
	{
		uint64 Hash = FNVOffset;
		HashValue(Hash, BackendName);
		HashValue(Hash, AssetHash);
		HashValue(Hash, RequestHash);
		HashValue(Hash, CapabilityHash);
		HashValue(Hash, LifecycleGeneration);
		return Hash == 0 ? 1 : Hash;
	}

	int64 EstimateBytes(const FOpenMobileHapticsPortableTimeline& Timeline)
	{
		int64 Bytes = sizeof(FOpenMobileHapticsPortableTimeline);
		switch (Timeline.Path)
		{
		case EOpenMobileHapticsTimelinePath::AndroidWaveform:
			Bytes += Timeline.Android.TimingsMilliseconds.GetAllocatedSize();
			Bytes += Timeline.Android.Amplitudes.GetAllocatedSize();
			Bytes += Timeline.AndroidControlBase.TimingsMilliseconds
				.GetAllocatedSize();
			Bytes += Timeline.AndroidControlBase.Amplitudes.GetAllocatedSize();
			break;
		case EOpenMobileHapticsTimelinePath::AppleTransient:
			Bytes += Timeline.AppleTransient.Pattern.StartTimesSeconds
				.GetAllocatedSize();
			Bytes += Timeline.AppleTransient.Pattern.Intensities
				.GetAllocatedSize();
			Bytes += Timeline.AppleTransient.Pattern.Sharpnesses
				.GetAllocatedSize();
			break;
		case EOpenMobileHapticsTimelinePath::AppleContinuous:
			Bytes += Timeline.AppleContinuous.Pattern.Events.GetAllocatedSize();
			Bytes += Timeline.AppleContinuous.Pattern.ParameterCurves
				.GetAllocatedSize();
			for (const FOpenMobileHapticsAppleParameterCurve& Curve :
				Timeline.AppleContinuous.Pattern.ParameterCurves)
			{
				Bytes += Curve.RelativeTimesSeconds.GetAllocatedSize();
				Bytes += Curve.Values.GetAllocatedSize();
			}
			break;
		}
		return FMath::Max<int64>(1, Bytes);
	}

	bool UsesContinuousAppleTranslation(
		const FOpenMobileHapticCookedPatternData& Pattern,
		const FOpenMobileHapticLoopOptions& Loop
	)
	{
		if (Loop.bLoop || !Pattern.ParameterCurves.IsEmpty())
		{
			return true;
		}
		for (const FOpenMobileHapticCookedPatternEvent& Event : Pattern.Events)
		{
			if (Event.Type == EOpenMobileHapticPatternEventType::Continuous)
			{
				return true;
			}
		}
		return false;
	}
}

struct FOpenMobileHapticsTimelineManager::FState
{
	struct FEntry
	{
		TWeakObjectPtr<UOpenMobileHapticPatternAsset> Owner;
		FName BackendName;
		uint64 AssetSignature = 0;
		uint64 RequestSignature = 0;
		uint64 CapabilitySignature = 0;
		uint64 LifecycleGeneration = 0;
		uint64 LastAccessSequence = 0;
		double LastAccessTimeSeconds = 0.0;
		TSharedPtr<
			const FOpenMobileHapticsPortableTimeline,
			ESPMode::ThreadSafe
		> Timeline;
	};

	explicit FState(
		int32 InMaximumCacheEntries,
		int64 InMaximumCacheBytes,
		double InIdleLifetimeSeconds
	)
		: MaximumCacheEntries(FMath::Max(1, InMaximumCacheEntries))
		, MaximumCacheBytes(FMath::Max<int64>(1, InMaximumCacheBytes))
		, IdleLifetimeSeconds(FMath::Max(0.001, InIdleLifetimeSeconds))
	{
	}

	mutable FCriticalSection Mutex;
	TArray<FEntry> Entries;
	int32 MaximumCacheEntries = 32;
	int64 MaximumCacheBytes = 4 * 1024 * 1024;
	double IdleLifetimeSeconds = 30.0;
	int64 TotalCacheBytes = 0;
	uint64 AccessSequence = 0;
};

FOpenMobileHapticsTimelineManager::FOpenMobileHapticsTimelineManager(
	int32 InMaximumCacheEntries,
	int64 InMaximumCacheBytes,
	double InIdleLifetimeSeconds
)
	: State(MakeUnique<FState>(
		InMaximumCacheEntries,
		InMaximumCacheBytes,
		InIdleLifetimeSeconds
	))
{
}

FOpenMobileHapticsTimelineManager::~FOpenMobileHapticsTimelineManager() =
	default;

FOpenMobileHapticsTimelineLookup FOpenMobileHapticsTimelineManager::Resolve(
	FName BackendName,
	const UOpenMobileHapticPatternAsset& Pattern,
	const FOpenMobileHapticLoopOptions& Loop,
	const FOpenMobileHapticCapabilities& Capabilities,
	float RequestIntensity,
	EOpenMobileHapticFallbackPolicy FallbackPolicy,
	uint64 LifecycleGeneration
)
{
	return ResolveAtTime(
		BackendName,
		Pattern,
		Loop,
		Capabilities,
		RequestIntensity,
		FallbackPolicy,
		LifecycleGeneration,
		FPlatformTime::Seconds()
	);
}

FOpenMobileHapticsTimelineLookup
FOpenMobileHapticsTimelineManager::ResolveAtTime(
	FName BackendName,
	const UOpenMobileHapticPatternAsset& Pattern,
	const FOpenMobileHapticLoopOptions& Loop,
	const FOpenMobileHapticCapabilities& Capabilities,
	float RequestIntensity,
	EOpenMobileHapticFallbackPolicy FallbackPolicy,
	uint64 LifecycleGeneration,
	double AccessTimeSeconds
)
{
	using namespace OpenMobileHapticsTimelineManagerPrivate;
	FOpenMobileHapticsTimelineLookup Lookup;
	if (BackendName != TEXT("Android") && BackendName != TEXT("IOS"))
	{
		return Lookup;
	}

	FScopeLock Lock(&State->Mutex);
	const uint64 CurrentAssetSignature = AssetSignature(Pattern);
	const bool bDerivedDataCurrent = Pattern.IsDerivedDataCurrent();
	auto RemoveEntry = [this](int32 Index)
	{
		State->TotalCacheBytes = FMath::Max<int64>(
			0,
			State->TotalCacheBytes
				- State->Entries[Index].Timeline->EstimatedBytes
		);
		State->Entries.RemoveAt(Index);
	};
	for (int32 Index = State->Entries.Num() - 1; Index >= 0; --Index)
	{
		const FState::FEntry& Entry = State->Entries[Index];
		if (!Entry.Owner.IsValid()
			|| Entry.LifecycleGeneration != LifecycleGeneration
			|| AccessTimeSeconds - Entry.LastAccessTimeSeconds
				>= State->IdleLifetimeSeconds
			|| (Entry.Owner.Get() == &Pattern
				&& (!bDerivedDataCurrent
					|| Entry.AssetSignature != CurrentAssetSignature)))
		{
			RemoveEntry(Index);
		}
	}
	if (!bDerivedDataCurrent)
	{
		TSharedRef<FOpenMobileHapticsPortableTimeline, ESPMode::ThreadSafe>
			StaleTimeline = MakeShared<
				FOpenMobileHapticsPortableTimeline,
				ESPMode::ThreadSafe
			>();
		if (BackendName == TEXT("Android"))
		{
			StaleTimeline->Path =
				EOpenMobileHapticsTimelinePath::AndroidWaveform;
			StaleTimeline->Android.Reason = TEXT("InvalidPattern");
		}
		else if (UsesContinuousAppleTranslation(
			Pattern.GetCookedPattern(),
			Loop
		))
		{
			StaleTimeline->Path =
				EOpenMobileHapticsTimelinePath::AppleContinuous;
			StaleTimeline->AppleContinuous.Reason = TEXT("InvalidPattern");
		}
		else
		{
			StaleTimeline->Path =
				EOpenMobileHapticsTimelinePath::AppleTransient;
			StaleTimeline->AppleTransient.Reason = TEXT("InvalidPattern");
		}
		Lookup.Timeline = StaleTimeline;
		return Lookup;
	}

	const uint64 CurrentRequestSignature = RequestSignature(
		Loop,
		RequestIntensity,
		FallbackPolicy
	);
	const uint64 CurrentCapabilitySignature = CapabilitySignature(
		BackendName,
		Capabilities
	);
	for (FState::FEntry& Entry : State->Entries)
	{
		if (Entry.Owner.Get() == &Pattern
			&& Entry.BackendName == BackendName
			&& Entry.AssetSignature == CurrentAssetSignature
			&& Entry.RequestSignature == CurrentRequestSignature
			&& Entry.CapabilitySignature == CurrentCapabilitySignature
			&& Entry.LifecycleGeneration == LifecycleGeneration)
		{
			Entry.LastAccessSequence = ++State->AccessSequence;
			Entry.LastAccessTimeSeconds = AccessTimeSeconds;
			Lookup.Timeline = Entry.Timeline;
			Lookup.bCacheHit = true;
			return Lookup;
		}
	}

	TSharedRef<FOpenMobileHapticsPortableTimeline, ESPMode::ThreadSafe>
		Timeline = MakeShared<
			FOpenMobileHapticsPortableTimeline,
			ESPMode::ThreadSafe
		>();
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const FOpenMobileHapticsRepeatPlanResult Repeat =
		FOpenMobileHapticsRepeatPolicy::Resolve(
			Loop,
			static_cast<double>(
				Pattern.GetCookedPattern().DurationMicroseconds
			) / 1000000.0,
			Settings->MaximumFiniteRepeatCount,
			Settings->MaximumContinuousDurationSeconds
		);
	if (Repeat.IsSuccess())
	{
		Timeline->RepeatPlan = Repeat.Plan;
		Timeline->bHasRepeatPlan = true;
	}
	if (BackendName == TEXT("Android"))
	{
		Timeline->Path = EOpenMobileHapticsTimelinePath::AndroidWaveform;
		Timeline->Android =
			FOpenMobileHapticsAndroidWaveformPolicy::ResolvePortable(
				Pattern,
				Loop,
				Capabilities,
				RequestIntensity,
				FallbackPolicy
			);
		Timeline->AndroidControlBase =
			FOpenMobileHapticsAndroidWaveformPolicy::ResolvePortable(
				Pattern,
				Capabilities,
				RequestIntensity,
				FallbackPolicy
			);
	}
	else if (UsesContinuousAppleTranslation(
		Pattern.GetCookedPattern(),
		Loop
	))
	{
		Timeline->Path = EOpenMobileHapticsTimelinePath::AppleContinuous;
		Timeline->AppleContinuous =
			FOpenMobileHapticsAppleContinuousPolicy::Resolve(
				Pattern.GetCookedPattern(),
				Loop,
				Capabilities,
				RequestIntensity,
				FOpenMobileHapticsAppleContinuousPolicy::MakeLimits(
					*GetDefault<UOpenMobileHapticsSettings>(),
					Capabilities
				)
			);
	}
	else
	{
		Timeline->Path = EOpenMobileHapticsTimelinePath::AppleTransient;
		Timeline->AppleTransient =
			FOpenMobileHapticsAppleTransientPolicy::Resolve(
				Pattern.GetCookedPattern(),
				Capabilities,
				RequestIntensity
			);
	}
	Timeline->ResourceId = NativeResourceId(
		BackendName,
		CurrentAssetSignature,
		CurrentRequestSignature,
		CurrentCapabilitySignature,
		LifecycleGeneration
	);
	Timeline->EstimatedBytes = EstimateBytes(*Timeline);

	while (!State->Entries.IsEmpty()
		&& (State->Entries.Num() >= State->MaximumCacheEntries
			|| State->TotalCacheBytes + Timeline->EstimatedBytes
				> State->MaximumCacheBytes))
	{
		int32 EvictionIndex = 0;
		for (int32 Index = 1; Index < State->Entries.Num(); ++Index)
		{
			if (State->Entries[Index].LastAccessSequence
				< State->Entries[EvictionIndex].LastAccessSequence)
			{
				EvictionIndex = Index;
			}
		}
		RemoveEntry(EvictionIndex);
	}
	if (Timeline->EstimatedBytes > State->MaximumCacheBytes)
	{
		Lookup.Timeline = Timeline;
		return Lookup;
	}

	FState::FEntry& Entry = State->Entries.AddDefaulted_GetRef();
	Entry.Owner = const_cast<UOpenMobileHapticPatternAsset*>(&Pattern);
	Entry.BackendName = BackendName;
	Entry.AssetSignature = CurrentAssetSignature;
	Entry.RequestSignature = CurrentRequestSignature;
	Entry.CapabilitySignature = CurrentCapabilitySignature;
	Entry.LifecycleGeneration = LifecycleGeneration;
	Entry.LastAccessSequence = ++State->AccessSequence;
	Entry.LastAccessTimeSeconds = AccessTimeSeconds;
	Entry.Timeline = Timeline;
	State->TotalCacheBytes += Timeline->EstimatedBytes;
	Lookup.Timeline = Entry.Timeline;
	return Lookup;
}

void FOpenMobileHapticsTimelineManager::SetLimits(
	const FOpenMobileHapticsPreparedResourceLimits& Limits
)
{
	FScopeLock Lock(&State->Mutex);
	State->MaximumCacheEntries = FMath::Max(1, Limits.MaximumCount);
	State->MaximumCacheBytes = FMath::Max<int64>(1, Limits.MaximumBytes);
	State->IdleLifetimeSeconds = FMath::Max(0.001, Limits.IdleLifetimeSeconds);
	while (!State->Entries.IsEmpty()
		&& (State->Entries.Num() > State->MaximumCacheEntries
			|| State->TotalCacheBytes > State->MaximumCacheBytes))
	{
		int32 EvictionIndex = 0;
		for (int32 Index = 1; Index < State->Entries.Num(); ++Index)
		{
			if (State->Entries[Index].LastAccessSequence
				< State->Entries[EvictionIndex].LastAccessSequence)
			{
				EvictionIndex = Index;
			}
		}
		State->TotalCacheBytes -=
			State->Entries[EvictionIndex].Timeline->EstimatedBytes;
		State->Entries.RemoveAt(EvictionIndex);
	}
}

void FOpenMobileHapticsTimelineManager::PruneIdle(double CurrentTimeSeconds)
{
	FScopeLock Lock(&State->Mutex);
	for (int32 Index = State->Entries.Num() - 1; Index >= 0; --Index)
	{
		if (!State->Entries[Index].Owner.IsValid()
			|| CurrentTimeSeconds
				- State->Entries[Index].LastAccessTimeSeconds
				>= State->IdleLifetimeSeconds)
		{
			State->TotalCacheBytes -=
				State->Entries[Index].Timeline->EstimatedBytes;
			State->Entries.RemoveAt(Index);
		}
	}
	State->TotalCacheBytes = FMath::Max<int64>(0, State->TotalCacheBytes);
}

void FOpenMobileHapticsTimelineManager::Clear()
{
	FScopeLock Lock(&State->Mutex);
	State->Entries.Reset();
	State->TotalCacheBytes = 0;
}

int64 FOpenMobileHapticsTimelineManager::GetCacheMemoryBytes() const
{
	FScopeLock Lock(&State->Mutex);
	return State->TotalCacheBytes;
}

int32 FOpenMobileHapticsTimelineManager::GetCacheEntryCount() const
{
	FScopeLock Lock(&State->Mutex);
	return State->Entries.Num();
}
