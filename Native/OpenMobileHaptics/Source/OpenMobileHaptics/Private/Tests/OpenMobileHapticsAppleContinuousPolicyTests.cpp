#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsAppleContinuousPolicy.h"
#include "OpenMobileHapticsSettings.h"

namespace OpenMobileHapticsAppleContinuousPolicyTests
{
	/** Builds compact cooked events so policy tests don't depend on editor asset compilation. */
	FOpenMobileHapticCookedPatternEvent Event(
		EOpenMobileHapticPatternEventType Type,
		uint32 StartMicroseconds,
		uint32 DurationMicroseconds,
		uint16 Intensity,
		uint16 Sharpness
	)
	{
		FOpenMobileHapticCookedPatternEvent Result;
		Result.Type = Type;
		Result.StartTimeMicroseconds = StartMicroseconds;
		Result.DurationMicroseconds = DurationMicroseconds;
		Result.Intensity = Intensity;
		Result.Sharpness = Sharpness;
		return Result;
	}

	/** Builds one cooked curve from explicit points while keeping setup readable in each case. */
	FOpenMobileHapticCookedParameterCurve Curve(
		EOpenMobileHapticCurveParameter Parameter,
		uint32 StartMicroseconds,
		std::initializer_list<FOpenMobileHapticCookedCurvePoint> Points
	)
	{
		FOpenMobileHapticCookedParameterCurve Result;
		Result.Parameter = Parameter;
		Result.StartTimeMicroseconds = StartMicroseconds;
		Result.ControlPoints = Points;
		return Result;
	}

	/** Returns the smallest Apple capability set needed for continuous translation to run. */
	FOpenMobileHapticCapabilities SupportedCapabilities()
	{
		FOpenMobileHapticCapabilities Capabilities;
		Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
		Capabilities.TransientEvents = EOpenMobileHapticSupportState::Supported;
		Capabilities.ContinuousEvents = EOpenMobileHapticSupportState::Supported;
		return Capabilities;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleContinuousTranslationTest,
	"OpenMobile.Haptics.Apple.Continuous.Translation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleContinuousTranslationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleContinuousPolicyTests;

	FOpenMobileHapticCookedPatternData Pattern;
	Pattern.DurationMicroseconds = 750000;
	Pattern.Events = {
		Event(EOpenMobileHapticPatternEventType::Continuous, 0, 500000,
			MAX_uint16, MAX_uint16 / 4),
		Event(EOpenMobileHapticPatternEventType::Transient, 500000, 0,
			MAX_uint16 / 2, MAX_uint16 / 2),
		Event(EOpenMobileHapticPatternEventType::Silence, 500000, 250000,
			MAX_uint16, MAX_uint16 / 2)
	};
	Pattern.ParameterCurves = {
		Curve(EOpenMobileHapticCurveParameter::IntensityControl, 0,
			{{0, MAX_uint16}, {500000, MAX_uint16 / 4}}),
		Curve(EOpenMobileHapticCurveParameter::SharpnessControl, 0,
			{{0, MAX_uint16 / 2}, {500000, MAX_uint16}})
	};
	FOpenMobileHapticLoopOptions Loop;
	FOpenMobileHapticsAppleContinuousLimits Limits;
	const FOpenMobileHapticsAppleContinuousResolution Resolution =
		FOpenMobileHapticsAppleContinuousPolicy::Resolve(
			Pattern,
			Loop,
			SupportedCapabilities(),
			0.5f,
			Limits
		);

	TestEqual(TEXT("Mixed rich pattern is ready"), Resolution.Outcome,
		EOpenMobileHapticsAppleContinuousOutcome::Ready);
	TestEqual(TEXT("Mixed rich events and trailing silence stay together"),
		Resolution.Pattern.Events.Num(), 3);
	TestEqual(TEXT("Continuous duration is preserved"),
		Resolution.Pattern.Events[0].DurationSeconds, 0.5);
	TestEqual(TEXT("Request intensity scales event intensity once"),
		Resolution.Pattern.Events[0].Intensity, 0.5f);
	TestEqual(TEXT("Both parameter curves are preserved"),
		Resolution.Pattern.ParameterCurves.Num(), 2);
	TestEqual(TEXT("Intensity control remains multiplicative"),
		Resolution.Pattern.ParameterCurves[0].Values[0], 1.0f);
	TestTrue(TEXT("Sharpness midpoint becomes neutral"),
		FMath::IsNearlyZero(
			Resolution.Pattern.ParameterCurves[1].Values[0], 0.0001f));
	TestEqual(TEXT("Maximum sharpness control becomes additive one"),
		Resolution.Pattern.ParameterCurves[1].Values[1], 1.0f);
	TestTrue(TEXT("Trailing silence has a native timing event"),
		Resolution.Pattern.Events.IsValidIndex(2));
	if (Resolution.Pattern.Events.IsValidIndex(2))
	{
		TestEqual(TEXT("Trailing silence uses a zero-intensity native event"),
			Resolution.Pattern.Events[2].Type,
			EOpenMobileHapticPatternEventType::Continuous);
		TestEqual(TEXT("Trailing silence remains inactive"),
			Resolution.Pattern.Events[2].Intensity, 0.0f);
		TestEqual(TEXT("Trailing silence preserves the authored duration"),
			Resolution.Pattern.Events[2].DurationSeconds, 0.25);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleContinuousValidationTest,
	"OpenMobile.Haptics.Apple.Continuous.ValidationAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleContinuousValidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleContinuousPolicyTests;

	FOpenMobileHapticCookedPatternData Pattern;
	Pattern.DurationMicroseconds = 1000000;
	Pattern.Events = {
		Event(EOpenMobileHapticPatternEventType::Continuous, 0, 1000000,
			MAX_uint16, MAX_uint16 / 2)
	};
	FOpenMobileHapticsAppleContinuousLimits Limits;
	Limits.MaximumDurationSeconds = 1.0;
	FOpenMobileHapticCapabilities Capabilities = SupportedCapabilities();
	Capabilities.MaximumEventCount = {true, 2};
	Capabilities.MaximumControlPointCount = {true, 4};
	Capabilities.MaximumDurationSeconds = {true, 0.75};
	const FOpenMobileHapticsAppleContinuousLimits ResolvedLimits =
		FOpenMobileHapticsAppleContinuousPolicy::MakeLimits(
			*GetDefault<UOpenMobileHapticsSettings>(),
			Capabilities
		);
	TestEqual(TEXT("Hardware event limit narrows Apple translation"),
		ResolvedLimits.MaximumEventCount, 2);
	TestEqual(TEXT("Hardware curve-point limit narrows Apple translation"),
		ResolvedLimits.MaximumCurvePointCount, 4);
	TestEqual(TEXT("Hardware duration narrows Apple translation"),
		ResolvedLimits.MaximumDurationSeconds, 0.75);
	Capabilities.MaximumEventCount = {};
	Capabilities.MaximumControlPointCount = {};
	Capabilities.MaximumDurationSeconds = {};
	FOpenMobileHapticsAppleContinuousResolution Resolution =
		FOpenMobileHapticsAppleContinuousPolicy::Resolve(
			Pattern, {}, Capabilities, 1.0f, Limits);
	TestEqual(TEXT("Exact duration limit is accepted"), Resolution.Outcome,
		EOpenMobileHapticsAppleContinuousOutcome::Ready);

	Pattern.DurationMicroseconds = 1000001;
	Pattern.Events[0].DurationMicroseconds = 1000001;
	Resolution = FOpenMobileHapticsAppleContinuousPolicy::Resolve(
		Pattern, {}, Capabilities, 1.0f, Limits);
	TestEqual(TEXT("Global duration overflow is invalid"), Resolution.Outcome,
		EOpenMobileHapticsAppleContinuousOutcome::Invalid);

	Pattern.DurationMicroseconds = 500000;
	Pattern.Events = {
		Event(EOpenMobileHapticPatternEventType::Continuous, 0, 400000,
			MAX_uint16, MAX_uint16 / 2),
		Event(EOpenMobileHapticPatternEventType::Continuous, 200000, 300000,
			MAX_uint16, MAX_uint16 / 2)
	};
	Resolution = FOpenMobileHapticsAppleContinuousPolicy::Resolve(
		Pattern, {}, Capabilities, 1.0f, Limits);
	TestEqual(TEXT("Overlapping portable events are invalid"),
		Resolution.Outcome, EOpenMobileHapticsAppleContinuousOutcome::Invalid);

	Pattern.Events = {
		Event(EOpenMobileHapticPatternEventType::Continuous, 0, 500000,
			MAX_uint16, MAX_uint16 / 2)
	};
	Pattern.ParameterCurves = {
		Curve(EOpenMobileHapticCurveParameter::IntensityControl, 0,
			{{0, MAX_uint16}, {500001, MAX_uint16 / 2}})
	};
	Resolution = FOpenMobileHapticsAppleContinuousPolicy::Resolve(
		Pattern, {}, Capabilities, 1.0f, Limits);
	TestEqual(TEXT("Curves outside the timeline are invalid"),
		Resolution.Outcome, EOpenMobileHapticsAppleContinuousOutcome::Invalid);

	Pattern.ParameterCurves.Reset();
	Capabilities.RichHaptics = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.ContinuousEvents = EOpenMobileHapticSupportState::Unsupported;
	Resolution = FOpenMobileHapticsAppleContinuousPolicy::Resolve(
		Pattern, {}, Capabilities, 1.0f, Limits);
	TestEqual(TEXT("No-haptics hardware requires fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleContinuousLoopTest,
	"OpenMobile.Haptics.Apple.Continuous.LoopOwnershipAndSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleContinuousLoopTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleContinuousPolicyTests;

	FOpenMobileHapticCookedPatternData Pattern;
	Pattern.DurationMicroseconds = 250000;
	Pattern.Events = {
		Event(EOpenMobileHapticPatternEventType::Continuous, 0, 250000,
			MAX_uint16, MAX_uint16 / 2),
		Event(EOpenMobileHapticPatternEventType::Transient, 250000, 0,
			MAX_uint16, MAX_uint16 / 2)
	};
	FOpenMobileHapticsAppleContinuousLimits Limits;
	Limits.MaximumEventCount = 8;
	Limits.MaximumDurationSeconds = 2.0;
	FOpenMobileHapticLoopOptions Loop;
	Loop.bLoop = true;
	Loop.RepeatCount = 2;
	Loop.RepeatStartTimeSeconds = 0.1;
	Loop.MaximumDurationSeconds = 1.0;
	FOpenMobileHapticsAppleContinuousResolution Resolution =
		FOpenMobileHapticsAppleContinuousPolicy::Resolve(
			Pattern, Loop, SupportedCapabilities(), 1.0f, Limits);
	TestEqual(TEXT("Finite repeat is ready"), Resolution.Outcome,
		EOpenMobileHapticsAppleContinuousOutcome::Ready);
	TestFalse(TEXT("Finite repeat does not leave a native loop"),
		Resolution.Pattern.bLoop);
	TestEqual(TEXT("Finite suffix repeats are expanded"),
		Resolution.Pattern.Events.Num(), 6);
	TestTrue(TEXT("Finite repeat duration is bounded"),
		FMath::IsNearlyEqual(Resolution.Pattern.DurationSeconds, 0.55,
			0.000001));
	TestTrue(TEXT("Expanded repeat contains its first sliced event"),
		Resolution.Pattern.Events.IsValidIndex(2));
	if (Resolution.Pattern.Events.IsValidIndex(2))
	{
		TestTrue(TEXT("Crossing continuous event is sliced at repeat start"),
			FMath::IsNearlyEqual(
				Resolution.Pattern.Events[2].DurationSeconds,
				0.15,
				0.000001));
	}

	Pattern.ParameterCurves = {
		Curve(EOpenMobileHapticCurveParameter::IntensityControl, 0,
			{{0, MAX_uint16}, {250000, MAX_uint16 / 2}})
	};
	Resolution = FOpenMobileHapticsAppleContinuousPolicy::Resolve(
		Pattern, Loop, SupportedCapabilities(), 1.0f, Limits);
	TestEqual(TEXT("Finite repeat with a curve is ready"), Resolution.Outcome,
		EOpenMobileHapticsAppleContinuousOutcome::Ready);
	TestEqual(TEXT("Finite repeats duplicate the suffix curve"),
		Resolution.Pattern.ParameterCurves.Num(), 3);
	TestTrue(TEXT("First repeated curve starts at the repeat boundary"),
		Resolution.Pattern.ParameterCurves.IsValidIndex(1)
		&& FMath::IsNearlyEqual(
			Resolution.Pattern.ParameterCurves[1].StartTimeSeconds,
			0.25,
			0.000001));
	TestTrue(TEXT("Sliced curve begins at the interpolated control value"),
		Resolution.Pattern.ParameterCurves.IsValidIndex(1)
		&& !Resolution.Pattern.ParameterCurves[1].Values.IsEmpty()
		&& FMath::IsNearlyEqual(
			Resolution.Pattern.ParameterCurves[1].Values[0],
			0.8f,
			0.0001f));

	Loop.RepeatCount = 0;
	Loop.RepeatStartTimeSeconds = 0.0;
	Resolution = FOpenMobileHapticsAppleContinuousPolicy::Resolve(
		Pattern, Loop, SupportedCapabilities(), 1.0f, Limits);
	TestEqual(TEXT("Explicit indefinite repeat is ready"), Resolution.Outcome,
		EOpenMobileHapticsAppleContinuousOutcome::Ready);
	TestTrue(TEXT("Indefinite repeat requests one owned native loop"),
		Resolution.Pattern.bLoop);
	TestEqual(TEXT("Native loop ends at the compiled pattern boundary"),
		Resolution.Pattern.LoopEndSeconds, 0.25);
	TestEqual(TEXT("Indefinite repeat keeps its safety deadline"),
		Resolution.Pattern.SafetyDurationSeconds, 1.0);

	Loop.RepeatStartTimeSeconds = 0.1;
	Resolution = FOpenMobileHapticsAppleContinuousPolicy::Resolve(
		Pattern, Loop, SupportedCapabilities(), 1.0f, Limits);
	TestEqual(TEXT("Unsupported indefinite suffix loop requires fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired);

	Pattern.Events = {
		Event(EOpenMobileHapticPatternEventType::Transient, 0, 0,
			MAX_uint16, MAX_uint16 / 2)
	};
	Pattern.ParameterCurves.Reset();
	Loop.RepeatCount = 1;
	Loop.RepeatStartTimeSeconds = 0.0;
	Resolution = FOpenMobileHapticsAppleContinuousPolicy::Resolve(
		Pattern, Loop, SupportedCapabilities(), 1.0f, Limits);
	TestEqual(TEXT("Transient-only loops require a declared fallback"),
		Resolution.Outcome,
		EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired);
	return true;
}

#endif
