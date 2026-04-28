#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsResults.h"

class IOpenMobileAdsFrequencyCapStore
{
public:
	virtual ~IOpenMobileAdsFrequencyCapStore() = default;
	virtual bool Load(TArray<uint8>& OutData) = 0;
	virtual bool Save(TConstArrayView<uint8> Data) = 0;
};

struct FOpenMobileAdsFrequencyCapDecision
{
	EOpenMobileAdsFrequencyCapScope Scope = EOpenMobileAdsFrequencyCapScope::None;
	FDateTime NextEligibleAt;

	bool IsCapped() const
	{
		return Scope != EOpenMobileAdsFrequencyCapScope::None;
	}
};

class FOpenMobileAdsFrequencyCapTracker
{
public:
	explicit FOpenMobileAdsFrequencyCapTracker(
		TSharedRef<IOpenMobileAdsFrequencyCapStore> InStore
	);

	void Initialize(FDateTime UtcNow, double MonotonicSeconds);
	FOpenMobileAdsFrequencyCapDecision Evaluate(
		FName Placement,
		const FOpenMobileAdsFrequencyCap& Policy,
		FDateTime UtcNow,
		double MonotonicSeconds
	) const;
	bool RecordImpression(
		FName Placement,
		const FOpenMobileAdsFrequencyCap& Policy,
		FDateTime UtcNow,
		double MonotonicSeconds
	);
	bool Flush(FDateTime UtcNow, double MonotonicSeconds);

private:
	bool Deserialize(
		TConstArrayView<uint8> Data,
		FDateTime UtcNow,
		double MonotonicSeconds
	);
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

TSharedRef<IOpenMobileAdsFrequencyCapStore>
OpenMobileAdsCreateFrequencyCapStore(bool bPersistent);
