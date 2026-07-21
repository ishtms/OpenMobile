#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsCapabilityTester.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsCapabilitySnapshotTest,
	"OpenMobile.Haptics.Tester.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsCapabilitySnapshotTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.BackendName = TEXT("Android");
	Capabilities.Availability = EOpenMobileHapticAvailability::RichHaptics;
	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
	Capabilities.Envelopes = EOpenMobileHapticSupportState::Supported;
	Capabilities.Primitives = EOpenMobileHapticSupportState::Supported;
	Capabilities.AHAP = EOpenMobileHapticSupportState::Unsupported;
	Capabilities.PrimitiveSupport.Emplace(
		TEXT("Click"),
		EOpenMobileHapticSupportState::Supported
	);
	Capabilities.PrimitiveSupport.Emplace(
		TEXT("AlicePhoneSerial"),
		EOpenMobileHapticSupportState::Supported
	);
	Capabilities.PresetSupport.Emplace(
		TEXT("HeavyClick"),
		EOpenMobileHapticSupportState::Unsupported
	);
	Capabilities.MaximumEventCount = {true, 128};
	Capabilities.MaximumQueueDepth = {true, 4};
	Capabilities.MaximumDurationSeconds = {true, 5.0};
	Capabilities.Detail = TEXT("Alice's personal device detail");

	FOpenMobileHapticUserPolicy Policy;
	Policy.bEnabled = true;
	Policy.MasterIntensity = std::numeric_limits<float>::quiet_NaN();
	Policy.CategoryScales.Add(TEXT("ProjectSecretCategory"), 0.5f);
	Policy.EffectScales.Add(TEXT("CustomerOrder42"), 0.7f);
	FOpenMobileHapticsDiagnostics Diagnostics;
	Diagnostics.PreparationState = EOpenMobileHapticPreparationState::Prepared;
	Diagnostics.PreparedNamedPatternCount = 3;
	Diagnostics.Performance.TimelineCacheEntryCount = 2;
	Diagnostics.Performance.TimelineCacheMemoryBytes = 4096;
	Diagnostics.LastNamedPattern = TEXT("CustomerOrder42");
	Diagnostics.LastResolvedPath = TEXT("/private/project/pattern");
	Diagnostics.LastError.Message = TEXT("ProjectSecretError");

	const FOpenMobileHapticsCapabilityTesterSnapshot Snapshot =
		FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Build(
			Capabilities,
			Policy,
			Diagnostics
		);
	TestEqual(TEXT("Backend remains provider-neutral"),
		Snapshot.Backend,
		FString(TEXT("Android")));
	TestEqual(TEXT("Native tier follows supported API capability"),
		Snapshot.NativeApiTier,
		FString(TEXT("AndroidEnvelope")));
	TestEqual(TEXT("Preparation reports engine state"),
		Snapshot.EngineState,
		FString(TEXT("Prepared")));
	TestEqual(TEXT("Nonfinite player intensity is sanitized"),
		Snapshot.MasterIntensity,
		0.0f);
	TestEqual(TEXT("Prepared resource count is captured"),
		Snapshot.PreparedNamedPatternCount,
		3);
	TestEqual(TEXT("Cached timeline bytes are captured"),
		Snapshot.CachedTimelineBytes,
		static_cast<int64>(4096));
	TestTrue(TEXT("Unknown native names are omitted"), Snapshot.bTruncated);
	TestEqual(TEXT("Only allowlisted primitive remains"),
		Snapshot.PrimitiveSupport.Num(),
		1);

	FString Json;
	FString Error;
	TestTrue(TEXT("Sanitized snapshot serializes"),
		FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Serialize(
			Snapshot,
			Json,
			Error
		));
	FString RepeatedJson;
	TestTrue(TEXT("Repeated snapshot serializes"),
		FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Serialize(
			Snapshot,
			RepeatedJson,
			Error
		));
	TestEqual(TEXT("Snapshot JSON is deterministic"), Json, RepeatedJson);
	TestTrue(TEXT("Snapshot includes native tier"),
		Json.Contains(TEXT("AndroidEnvelope")));
	TestTrue(TEXT("Snapshot includes supported primitive"),
		Json.Contains(TEXT("Click")));
	for (const TCHAR* Forbidden : {
		TEXT("Alice"),
		TEXT("ProjectSecret"),
		TEXT("CustomerOrder42"),
		TEXT("/private/project"),
		TEXT("deviceDetail"),
		TEXT("osVersion")
	})
	{
		TestFalse(
			FString::Printf(TEXT("Snapshot excludes %s"), Forbidden),
			Json.Contains(Forbidden)
		);
	}
	return true;
}

#endif
