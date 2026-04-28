#include "Misc/AutomationTest.h"
#include "OpenMobileAdsCooldown.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCooldownTrackerTest,
	"OpenMobile.Ads.Cooldown.Tracker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCooldownTrackerTest::RunTest(const FString& Parameters)
{
	const FDateTime BaseUtc(2035, 4, 5, 12, 0, 0);
	FOpenMobileAdsCooldownTracker Tracker;
	Tracker.RecordImpression(
		TEXT("First"),
		EOpenMobileAdFormat::Rewarded,
		100.0
	);

	const FOpenMobileAdsCooldownDecision First = Tracker.Evaluate(
		TEXT("First"),
		EOpenMobileAdFormat::Rewarded,
		20.0,
		10.0,
		BaseUtc,
		100.0
	);
	TestTrue(TEXT("The placement cooldown starts from the impression"), First.bPlacementActive);
	TestTrue(TEXT("The global cooldown starts from the impression"), First.bGlobalActive);
	TestEqual(
		TEXT("The longer placement cooldown determines eligibility"),
		First.NextEligibleAt,
		BaseUtc + FTimespan::FromSeconds(20.0)
	);

	const FOpenMobileAdsCooldownDecision OtherPlacement = Tracker.Evaluate(
		TEXT("Second"),
		EOpenMobileAdFormat::Rewarded,
		30.0,
		10.0,
		BaseUtc,
		100.0
	);
	TestFalse(TEXT("Another placement has no local cooldown"), OtherPlacement.bPlacementActive);
	TestTrue(TEXT("Another placement shares the global cooldown"), OtherPlacement.bGlobalActive);
	TestEqual(
		TEXT("The global cooldown exposes its deadline"),
		OtherPlacement.NextEligibleAt,
		BaseUtc + FTimespan::FromSeconds(10.0)
	);

	TestFalse(
		TEXT("The global cooldown expires at its exact boundary"),
		Tracker.Evaluate(
			TEXT("Second"),
			EOpenMobileAdFormat::Rewarded,
			30.0,
			10.0,
			BaseUtc + FTimespan::FromSeconds(10.0),
			110.0
		).IsActive()
	);
	TestFalse(
		TEXT("The placement cooldown expires at its exact boundary"),
		Tracker.Evaluate(
			TEXT("First"),
			EOpenMobileAdFormat::Rewarded,
			20.0,
			10.0,
			BaseUtc + FTimespan::FromSeconds(20.0),
			120.0
		).IsActive()
	);

	const FDateTime BackwardUtc = BaseUtc - FTimespan::FromSeconds(300.0);
	const FOpenMobileAdsCooldownDecision BackwardClock = Tracker.Evaluate(
		TEXT("First"),
		EOpenMobileAdFormat::Rewarded,
		20.0,
		10.0,
		BackwardUtc,
		105.0
	);
	TestTrue(TEXT("A backward wall clock does not clear the cooldown"), BackwardClock.IsActive());
	TestEqual(
		TEXT("A backward wall clock does not lengthen the remaining cooldown"),
		BackwardClock.NextEligibleAt,
		BackwardUtc + FTimespan::FromSeconds(15.0)
	);

	Tracker.RecordImpression(TEXT("Banner"), EOpenMobileAdFormat::Banner, 200.0);
	TestFalse(
		TEXT("A non-full-screen impression does not start cooldown pacing"),
		Tracker.Evaluate(
			TEXT("Banner"),
			EOpenMobileAdFormat::Banner,
			30.0,
			30.0,
			BaseUtc,
			200.0
		).IsActive()
	);
	return true;
}

#endif
