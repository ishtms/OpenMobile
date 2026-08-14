#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsResults.h"

/** Stores rolling impression history without tying policy code to platform persistence. */
class IOpenMobileAdsFrequencyCapStore
{
public:
	virtual ~IOpenMobileAdsFrequencyCapStore() = default;
	/** Reads the last opaque payload and leaves validation to the tracker. */
	virtual bool Load(TArray<uint8>& OutData) = 0;
	/** Replaces the opaque payload only after the tracker has serialized bounded history. */
	virtual bool Save(TConstArrayView<uint8> Data) = 0;
};

struct FOpenMobileAdsFrequencyCapDecision
{
	EOpenMobileAdsFrequencyCapScope Scope = EOpenMobileAdsFrequencyCapScope::None;
	FDateTime NextEligibleAt;

	/** Treats any explicit scope as capped even when no future wall time is available. */
	bool IsCapped() const
	{
		return Scope != EOpenMobileAdsFrequencyCapScope::None;
	}
};

class FOpenMobileAdsFrequencyCapTracker
{
public:
	/** Takes one store for the tracker's full lifetime so load and flush use the same persistence path. */
	explicit FOpenMobileAdsFrequencyCapTracker(
		TSharedRef<IOpenMobileAdsFrequencyCapStore> InStore
	);

	/** Restores bounded rolling history and starts fresh session counters for this run. */
	void Initialize(FDateTime UtcNow, double MonotonicSeconds);
	/** Checks session first, then rolling history, and returns the earliest wall-clock eligibility. */
	FOpenMobileAdsFrequencyCapDecision Evaluate(
		FName Placement,
		const FOpenMobileAdsFrequencyCap& Policy,
		FDateTime UtcNow,
		double MonotonicSeconds
	) const;
	/** Adds one counted impression and persists rolling history when the policy uses it. */
	bool RecordImpression(
		FName Placement,
		const FOpenMobileAdsFrequencyCap& Policy,
		FDateTime UtcNow,
		double MonotonicSeconds
	);
	/** Saves all bounded rolling history after pruning entries outside their active windows. */
	bool Flush(FDateTime UtcNow, double MonotonicSeconds);

private:
	/** Rejects malformed, future-dated, oversized, or clock-incompatible persisted history. */
	bool Deserialize(
		TConstArrayView<uint8> Data,
		FDateTime UtcNow,
		double MonotonicSeconds
	);
	/** Converts monotonic ages to persisted wall times without writing session-only counters. */
	bool Serialize(
		TArray<uint8>& OutData,
		FDateTime UtcNow,
		double MonotonicSeconds
	) const;

	TSharedRef<IOpenMobileAdsFrequencyCapStore> Store;
	TMap<FName, int32> SessionImpressionCounts;
	TMap<FName, TArray<double>> RollingImpressionMonotonicTimes;
	bool bInitialized = false;
};

/** Creates persistent config storage or an in-memory store according to project policy. */
TSharedRef<IOpenMobileAdsFrequencyCapStore>
OpenMobileAdsCreateFrequencyCapStore(bool bPersistent);
