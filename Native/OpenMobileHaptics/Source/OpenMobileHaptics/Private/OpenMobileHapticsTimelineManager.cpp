#include "OpenMobileHapticsTimelineManager.h"

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
		TSharedPtr<
			const FOpenMobileHapticsPortableTimeline,
			ESPMode::ThreadSafe
		> Timeline;
	};

	explicit FState(int32 InMaximumCacheEntries)
		: MaximumCacheEntries(FMath::Max(1, InMaximumCacheEntries))
	{
	}

	mutable FCriticalSection Mutex;
	TArray<FEntry> Entries;
	int32 MaximumCacheEntries = 32;
	uint64 AccessSequence = 0;
};

FOpenMobileHapticsTimelineManager::FOpenMobileHapticsTimelineManager(
	int32 InMaximumCacheEntries
)
	: State(MakeUnique<FState>(InMaximumCacheEntries))
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
	using namespace OpenMobileHapticsTimelineManagerPrivate;
	FOpenMobileHapticsTimelineLookup Lookup;
	if (BackendName != TEXT("Android") && BackendName != TEXT("IOS"))
	{
		return Lookup;
	}

	FScopeLock Lock(&State->Mutex);
	const uint64 CurrentAssetSignature = AssetSignature(Pattern);
	const bool bDerivedDataCurrent = Pattern.IsDerivedDataCurrent();
	for (int32 Index = State->Entries.Num() - 1; Index >= 0; --Index)
	{
		const FState::FEntry& Entry = State->Entries[Index];
		if (!Entry.Owner.IsValid()
			|| Entry.LifecycleGeneration != LifecycleGeneration
			|| (Entry.Owner.Get() == &Pattern
				&& (!bDerivedDataCurrent
					|| Entry.AssetSignature != CurrentAssetSignature)))
		{
			State->Entries.RemoveAt(Index);
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

	while (State->Entries.Num() >= State->MaximumCacheEntries)
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
		State->Entries.RemoveAt(EvictionIndex);
	}

	FState::FEntry& Entry = State->Entries.AddDefaulted_GetRef();
	Entry.Owner = const_cast<UOpenMobileHapticPatternAsset*>(&Pattern);
	Entry.BackendName = BackendName;
	Entry.AssetSignature = CurrentAssetSignature;
	Entry.RequestSignature = CurrentRequestSignature;
	Entry.CapabilitySignature = CurrentCapabilitySignature;
	Entry.LifecycleGeneration = LifecycleGeneration;
	Entry.LastAccessSequence = ++State->AccessSequence;
	Entry.Timeline = Timeline;
	Lookup.Timeline = Entry.Timeline;
	return Lookup;
}

void FOpenMobileHapticsTimelineManager::Clear()
{
	FScopeLock Lock(&State->Mutex);
	State->Entries.Reset();
}

int32 FOpenMobileHapticsTimelineManager::GetCacheEntryCount() const
{
	FScopeLock Lock(&State->Mutex);
	return State->Entries.Num();
}
