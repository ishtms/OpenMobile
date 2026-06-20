#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsAppleTransientPolicy.h"

namespace OpenMobileHapticsAppleTransientPolicyTests
{
	FOpenMobileHapticCookedPatternEvent Transient(
		uint32 StartMicroseconds,
		uint16 Intensity,
		uint16 Sharpness
	)
	{
		FOpenMobileHapticCookedPatternEvent Event;
		Event.Type = EOpenMobileHapticPatternEventType::Transient;
		Event.StartTimeMicroseconds = StartMicroseconds;
		Event.Intensity = Intensity;
		Event.Sharpness = Sharpness;
		return Event;
	}

	FOpenMobileHapticCapabilities SupportedCapabilities()
	{
		FOpenMobileHapticCapabilities Capabilities;
		Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
		Capabilities.TransientEvents = EOpenMobileHapticSupportState::Supported;
		return Capabilities;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleTransientTranslationTest,
	"OpenMobile.Haptics.Apple.Transient.Translation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleTransientTranslationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleTransientPolicyTests;

	FOpenMobileHapticCookedPatternData Pattern;
	Pattern.DurationMicroseconds = 125000;
	Pattern.Events = {
		Transient(0, MAX_uint16, 0),
		Transient(0, MAX_uint16 / 2, MAX_uint16),
		Transient(125000, MAX_uint16 / 4, MAX_uint16 / 2)
	};
	const FOpenMobileHapticsAppleTransientResolution Resolution =
		FOpenMobileHapticsAppleTransientPolicy::Resolve(
			Pattern,
			SupportedCapabilities(),
			0.5f
		);
	TestEqual(TEXT("Transient pattern is ready"), Resolution.Outcome,
		EOpenMobileHapticsAppleTransientOutcome::Ready);
	TestEqual(TEXT("All transients share one native pattern"),
		Resolution.Pattern.StartTimesSeconds.Num(), 3);
	TestEqual(TEXT("Equal source times remain equal"),
		Resolution.Pattern.StartTimesSeconds[0],
		Resolution.Pattern.StartTimesSeconds[1]);
	TestEqual(TEXT("Later start time is preserved"),
		Resolution.Pattern.StartTimesSeconds[2], 0.125);
	TestEqual(TEXT("Request intensity scales the first event"),
		Resolution.Pattern.Intensities[0], 0.5f);
	TestTrue(TEXT("Cooked intensity is normalized before scaling"),
		FMath::IsNearlyEqual(Resolution.Pattern.Intensities[1], 0.25f,
			0.0001f));
	TestEqual(TEXT("Minimum sharpness remains zero"),
		Resolution.Pattern.Sharpnesses[0], 0.0f);
	TestEqual(TEXT("Maximum sharpness remains one"),
		Resolution.Pattern.Sharpnesses[1], 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleTransientValidationTest,
	"OpenMobile.Haptics.Apple.Transient.ValidationAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleTransientValidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleTransientPolicyTests;

	FOpenMobileHapticCookedPatternData Pattern;
	Pattern.Events = {Transient(0, MAX_uint16, MAX_uint16 / 2)};
	FOpenMobileHapticCapabilities Capabilities = SupportedCapabilities();

	FOpenMobileHapticsAppleTransientResolution Resolution =
		FOpenMobileHapticsAppleTransientPolicy::Resolve(
			Pattern,
			Capabilities,
			std::numeric_limits<float>::quiet_NaN()
		);
	TestEqual(TEXT("Nonfinite request intensity is invalid"),
		Resolution.Outcome,
		EOpenMobileHapticsAppleTransientOutcome::Invalid);

	Resolution = FOpenMobileHapticsAppleTransientPolicy::Resolve(
		Pattern,
		Capabilities,
		0.0f
	);
	TestEqual(TEXT("Zero intensity avoids native playback"),
		Resolution.Outcome,
		EOpenMobileHapticsAppleTransientOutcome::Suppressed);

	Capabilities.TransientEvents =
		EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsAppleTransientPolicy::Resolve(
		Pattern,
		Capabilities,
		1.0f
	);
	TestEqual(TEXT("Unsupported hardware requires fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsAppleTransientOutcome::FallbackRequired);

	Capabilities = SupportedCapabilities();
	FOpenMobileHapticCookedPatternEvent Continuous;
	Continuous.Type = EOpenMobileHapticPatternEventType::Continuous;
	Continuous.DurationMicroseconds = 100000;
	Pattern.DurationMicroseconds = 100000;
	Pattern.Events.Add(Continuous);
	Resolution = FOpenMobileHapticsAppleTransientPolicy::Resolve(
		Pattern,
		Capabilities,
		1.0f
	);
	TestEqual(TEXT("Mixed continuous patterns use the fallback ladder"),
		Resolution.Outcome,
		EOpenMobileHapticsAppleTransientOutcome::FallbackRequired);

	Pattern.DurationMicroseconds = 999;
	Pattern.Events = {Transient(1000, MAX_uint16, MAX_uint16 / 2)};
	Resolution = FOpenMobileHapticsAppleTransientPolicy::Resolve(
		Pattern,
		Capabilities,
		1.0f
	);
	TestEqual(TEXT("Corrupt cooked timing is invalid"), Resolution.Outcome,
		EOpenMobileHapticsAppleTransientOutcome::Invalid);

	Pattern.Events = {};
	Resolution = FOpenMobileHapticsAppleTransientPolicy::Resolve(
		Pattern,
		Capabilities,
		1.0f
	);
	TestEqual(TEXT("Empty cooked patterns are invalid"), Resolution.Outcome,
		EOpenMobileHapticsAppleTransientOutcome::Invalid);
	return true;
}

#endif
