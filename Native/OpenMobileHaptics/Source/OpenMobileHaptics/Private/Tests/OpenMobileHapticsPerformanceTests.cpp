#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsBudgetPolicy.h"
#include "OpenMobileHapticsPatternCompiler.h"
#include "OpenMobileHapticsPerformanceTracker.h"
#include "OpenMobileHapticsSettings.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBudgetPolicyTest,
	"OpenMobile.Haptics.Performance.BudgetPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBudgetPolicyTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	TestEqual(TEXT("Active handles have a hard ceiling"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumActiveHandles(MAX_int32),
		128);
	TestEqual(TEXT("Queued handles have a hard ceiling"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumQueuedHandles(MAX_int32),
		256);
	TestEqual(TEXT("Per-channel queues have a hard ceiling"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumQueueDepthPerChannel(
			MAX_int32
		),
		64);
	TestEqual(TEXT("Invalid global queue depth fails closed"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumQueueDepthPerChannel(-1),
		1);
	TestEqual(TEXT("Prepared native resources have a count ceiling"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPreparedPatterns(
			MAX_int32
		),
		128);
	TestEqual(TEXT("Prepared native resources have a byte ceiling"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPreparedPatternBytes(
			MAX_int64
		),
		static_cast<int64>(64 * 1024 * 1024));
	TestEqual(TEXT("Compiled events have a hard ceiling"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternEvents(MAX_int32),
		4096);
	TestEqual(TEXT("Compiled curves have a hard ceiling"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternCurves(MAX_int32),
		128);
	TestEqual(TEXT("Compiled curve points have a hard ceiling"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternCurvePoints(
			MAX_int32
		),
		4096);
	TestEqual(TEXT("Diagnostic history has a hard ceiling"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumDiagnosticEvents(
			MAX_int32
		),
		512);
	TestEqual(TEXT("Invalid positive-only budgets fail closed"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPreparedPatterns(-1),
		1);
	TestEqual(TEXT("Curve budgets may intentionally disable curves"),
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternCurves(-1),
		0);
	TestEqual(TEXT("Invalid idle lifetime uses the safe minimum"),
		FOpenMobileHapticsBudgetPolicy::ResolvePreparedIdleLifetimeSeconds(
			std::numeric_limits<double>::quiet_NaN()
		),
		1.0);

	UOpenMobileHapticsSettings* Settings =
		NewObject<UOpenMobileHapticsSettings>();
	Settings->MaximumPatternEventCount = MAX_int32;
	Settings->MaximumPatternCurveCount = MAX_int32;
	Settings->MaximumPatternCurvePointCount = MAX_int32;
	const FOpenMobileHapticsPatternCompileLimits CompileLimits =
		FOpenMobileHapticsPatternCompiler::MakeLimits(
			*Settings,
			{}
		);
	TestEqual(TEXT("Compiler applies the event hard ceiling"),
		CompileLimits.MaximumEventCount, 4096);
	TestEqual(TEXT("Compiler applies the curve hard ceiling"),
		CompileLimits.MaximumCurveCount, 128);
	TestEqual(TEXT("Compiler applies the point hard ceiling"),
		CompileLimits.MaximumCurvePointCount, 4096);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPerformanceTrackerTest,
	"OpenMobile.Haptics.Performance.Metrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPerformanceTrackerTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileHapticsPerformanceTracker Tracker;
	Tracker.RecordDroppedRequest();
	Tracker.RecordDroppedRequest();
	Tracker.RecordQueueDepth(7);
	Tracker.RecordQueueDepth(3);
	Tracker.RecordQueueDepth(-1);
	Tracker.RecordPreparationLatencySeconds(0.125);
	Tracker.RecordPreparationLatencySeconds(-1.0);
	Tracker.RecordNativeSubmissionLatencySeconds(0.002);
	Tracker.RecordNativeSubmissionLatencySeconds(0.004);
	Tracker.RecordNativeSubmissionLatencySeconds(
		std::numeric_limits<double>::quiet_NaN()
	);

	const FOpenMobileHapticsPerformanceSnapshot Snapshot =
		Tracker.GetSnapshot();
	TestEqual(TEXT("Dropped requests are counted"),
		Snapshot.DroppedRequestCount, static_cast<uint64>(2));
	TestEqual(TEXT("Peak queue depth is retained"),
		Snapshot.PeakQueuedPlaybackCount, 7);
	TestEqual(TEXT("Only valid preparation samples are counted"),
		Snapshot.PreparationCount, static_cast<uint64>(1));
	TestEqual(TEXT("Preparation latency is reported in milliseconds"),
		Snapshot.LastPreparationLatencyMilliseconds, 125.0);
	TestEqual(TEXT("Native submissions are counted"),
		Snapshot.NativeSubmissionCount, static_cast<uint64>(2));
	TestEqual(TEXT("Latest native latency is retained"),
		Snapshot.LastNativeSubmissionLatencyMilliseconds, 4.0);
	TestEqual(TEXT("Maximum native latency is retained"),
		Snapshot.MaximumNativeSubmissionLatencyMilliseconds, 4.0);

	FOpenMobileHapticsPerformanceDiagnostics PublicDiagnostics;
	TestEqual(TEXT("Public dropped count starts empty"),
		PublicDiagnostics.DroppedRequestCount, static_cast<int64>(0));
	TestEqual(TEXT("Public cache counters start empty"),
		PublicDiagnostics.TimelineCacheHitCount
			+ PublicDiagnostics.TimelineCacheMissCount,
		static_cast<int64>(0));
	TestEqual(TEXT("Public latency starts empty"),
		PublicDiagnostics.LastNativeSubmissionLatencyMilliseconds, 0.0);
	return true;
}

#endif
