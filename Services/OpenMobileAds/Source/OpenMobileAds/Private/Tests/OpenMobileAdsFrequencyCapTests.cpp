#include "Misc/AutomationTest.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsFrequencyCap.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OpenMobileAdsFrequencyCapTests
{
	class FMemoryStore final : public IOpenMobileAdsFrequencyCapStore
	{
	public:
		virtual bool Load(TArray<uint8>& OutData) override
		{
			if (Data.IsEmpty())
			{
				return false;
			}
			OutData = Data;
			return true;
		}

		virtual bool Save(TConstArrayView<uint8> InData) override
		{
			Data.Reset(InData.Num());
			Data.Append(InData.GetData(), InData.Num());
			++SaveCalls;
			return true;
		}

		TArray<uint8> Data;
		int32 SaveCalls = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsFrequencyCapTrackerTest,
	"OpenMobile.Ads.FrequencyCap.Tracker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsFrequencyCapTrackerTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsFrequencyCapTests;
	const FDateTime BaseUtc(2035, 4, 5, 12, 0, 0);
	const TSharedRef<FMemoryStore> Store = MakeShared<FMemoryStore>();
	FOpenMobileAdsFrequencyCapTracker Tracker(Store);
	Tracker.Initialize(BaseUtc, 100.0);

	FOpenMobileAdsFrequencyCap SessionPolicy;
	SessionPolicy.MaxSessionImpressions = 2;
	Tracker.RecordImpression(TEXT("Session"), SessionPolicy, BaseUtc, 100.0);
	TestFalse(
		TEXT("One session impression remains below the limit"),
		Tracker.Evaluate(TEXT("Session"), SessionPolicy, BaseUtc, 100.0).IsCapped()
	);
	Tracker.RecordImpression(TEXT("Session"), SessionPolicy, BaseUtc, 100.0);
	const FOpenMobileAdsFrequencyCapDecision SessionDecision =
		Tracker.Evaluate(TEXT("Session"), SessionPolicy, BaseUtc, 100.0);
	TestTrue(TEXT("The session limit blocks at its exact count"), SessionDecision.IsCapped());
	TestEqual(
		TEXT("The session blocker is typed"),
		SessionDecision.Scope,
		EOpenMobileAdsFrequencyCapScope::Session
	);
	TestEqual(
		TEXT("A session limit has no wall-clock deadline"),
		SessionDecision.NextEligibleAt,
		FDateTime()
	);

	FOpenMobileAdsFrequencyCap RollingPolicy;
	RollingPolicy.MaxImpressions = 2;
	RollingPolicy.WindowSeconds = 60.0;
	Tracker.RecordImpression(TEXT("Rolling"), RollingPolicy, BaseUtc, 100.0);
	Tracker.RecordImpression(
		TEXT("Rolling"),
		RollingPolicy,
		BaseUtc + FTimespan::FromSeconds(10.0),
		110.0
	);
	const FOpenMobileAdsFrequencyCapDecision RollingDecision = Tracker.Evaluate(
		TEXT("Rolling"),
		RollingPolicy,
		BaseUtc + FTimespan::FromSeconds(10.0),
		110.0
	);
	TestTrue(TEXT("The rolling limit blocks at its exact count"), RollingDecision.IsCapped());
	TestEqual(
		TEXT("The rolling blocker is typed"),
		RollingDecision.Scope,
		EOpenMobileAdsFrequencyCapScope::RollingWindow
	);
	TestEqual(
		TEXT("The oldest counted impression determines eligibility"),
		RollingDecision.NextEligibleAt,
		BaseUtc + FTimespan::FromSeconds(60.0)
	);
	TestTrue(TEXT("Rolling impressions are saved immediately"), Store->SaveCalls > 0);

	FOpenMobileAdsFrequencyCapTracker Restarted(Store);
	Restarted.Initialize(BaseUtc + FTimespan::FromSeconds(20.0), 500.0);
	const FOpenMobileAdsFrequencyCapDecision RestartedDecision = Restarted.Evaluate(
		TEXT("Rolling"),
		RollingPolicy,
		BaseUtc + FTimespan::FromSeconds(20.0),
		500.0
	);
	TestTrue(TEXT("The rolling cap survives a restart"), RestartedDecision.IsCapped());
	TestEqual(
		TEXT("A restart preserves the rolling deadline"),
		RestartedDecision.NextEligibleAt,
		BaseUtc + FTimespan::FromSeconds(60.0)
	);
	TestFalse(
		TEXT("Session counts reset with a new tracker"),
		Restarted.Evaluate(TEXT("Session"), SessionPolicy, BaseUtc, 500.0).IsCapped()
	);
	FOpenMobileAdsFrequencyCapTracker ExactBoundary(Store);
	ExactBoundary.Initialize(BaseUtc + FTimespan::FromSeconds(60.0), 700.0);
	TestFalse(
		TEXT("The oldest impression expires at the exact rolling boundary"),
		ExactBoundary.Evaluate(
			TEXT("Rolling"),
			RollingPolicy,
			BaseUtc + FTimespan::FromSeconds(60.0),
			700.0
		).IsCapped()
	);

	FOpenMobileAdsFrequencyCapTracker BackwardClock(Store);
	const FDateTime BackwardUtc = BaseUtc - FTimespan::FromSeconds(100.0);
	BackwardClock.Initialize(BackwardUtc, 900.0);
	const FOpenMobileAdsFrequencyCapDecision BackwardDecision = BackwardClock.Evaluate(
		TEXT("Rolling"),
		RollingPolicy,
		BackwardUtc,
		900.0
	);
	TestTrue(TEXT("A backward clock change cannot clear the cap"), BackwardDecision.IsCapped());
	TestEqual(
		TEXT("A backward clock change cannot lengthen the saved remaining duration"),
		BackwardDecision.NextEligibleAt,
		BackwardUtc + FTimespan::FromSeconds(50.0)
	);

	FOpenMobileAdsFrequencyCapTracker ForwardClock(Store);
	ForwardClock.Initialize(BaseUtc + FTimespan::FromSeconds(61.0), 1200.0);
	TestFalse(
		TEXT("A forward clock change expires old rolling impressions"),
		ForwardClock.Evaluate(
			TEXT("Rolling"),
			RollingPolicy,
			BaseUtc + FTimespan::FromSeconds(61.0),
			1200.0
		).IsCapped()
	);

	const TSharedRef<FMemoryStore> CorruptStore = MakeShared<FMemoryStore>();
	CorruptStore->Data = {1, 2, 3, 4};
	FOpenMobileAdsFrequencyCapTracker CorruptTracker(CorruptStore);
	CorruptTracker.Initialize(BaseUtc, 100.0);
	TestFalse(
		TEXT("Corrupt saved data is ignored"),
		CorruptTracker.Evaluate(TEXT("Rolling"), RollingPolicy, BaseUtc, 100.0).IsCapped()
	);
	CorruptTracker.RecordImpression(TEXT("Rolling"), RollingPolicy, BaseUtc, 100.0);
	TestTrue(TEXT("A later impression replaces corrupt saved data"), CorruptStore->Data.Num() > 4);
	return true;
}

#endif
