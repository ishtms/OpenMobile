#include "Misc/AutomationTest.h"
#include "OpenMobileAdsRetry.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OpenMobileAdsRetryTests
{
	/** Returns a prepared jitter sequence and falls back to zero after it runs out. */
	class FControlledRandomSource final
		: public IOpenMobileAdsRetryRandomSource
	{
	public:
		explicit FControlledRandomSource(TArray<double> InValues)
			: Values(MoveTemp(InValues))
		{
		}

		virtual double NextUnit() override
		{
			return Values.IsValidIndex(Index) ? Values[Index++] : 0.0;
		}

	private:
		TArray<double> Values;
		int32 Index = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRetryDelayCalculationTest,
	"OpenMobile.Ads.Reliability.Backoff.DelayCalculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRetryDelayCalculationTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsRetryTests;
	FOpenMobileAdsRetryPolicy Policy;
	Policy.InitialDelaySeconds = 2.0;
	Policy.BackoffMultiplier = 3.0;
	Policy.MaxDelaySeconds = 10.0;
	Policy.bUseJitter = false;
	FControlledRandomSource UnusedRandom({});
	TestEqual(
		TEXT("The first retry uses the initial delay"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(
			Policy,
			1,
			UnusedRandom
		),
		2.0
	);
	TestEqual(
		TEXT("The second retry applies the multiplier"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(
			Policy,
			2,
			UnusedRandom
		),
		6.0
	);
	TestEqual(
		TEXT("Later retries stop at the maximum delay"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(
			Policy,
			3,
			UnusedRandom
		),
		10.0
	);
	TestEqual(
		TEXT("Large attempt numbers remain capped"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(
			Policy,
			1000,
			UnusedRandom
		),
		10.0
	);
	Policy.InitialDelaySeconds = 0.0;
	TestEqual(
		TEXT("A zero initial delay stays zero for large attempt numbers"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(
			Policy,
			TNumericLimits<int32>::Max(),
			UnusedRandom
		),
		0.0
	);
	Policy.InitialDelaySeconds = 2.0;

	Policy.bUseJitter = true;
	FControlledRandomSource Random({0.0, 0.5, 1.0, -2.0, 3.0});
	TestEqual(
		TEXT("Minimum jitter keeps half the backoff delay"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(Policy, 2, Random),
		3.0
	);
	TestEqual(
		TEXT("Midpoint jitter is deterministic"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(Policy, 2, Random),
		4.5
	);
	TestEqual(
		TEXT("Maximum jitter keeps the full backoff delay"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(Policy, 2, Random),
		6.0
	);
	TestEqual(
		TEXT("Random values below zero are clamped"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(Policy, 2, Random),
		3.0
	);
	TestEqual(
		TEXT("Random values above one are clamped"),
		FOpenMobileAdsRetryDelayCalculator::Calculate(Policy, 2, Random),
		6.0
	);
	return true;
}

#endif
