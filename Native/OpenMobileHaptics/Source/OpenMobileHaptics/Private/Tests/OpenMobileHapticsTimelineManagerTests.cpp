#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsTimelineManager.h"

namespace OpenMobileHapticsTimelineManagerTests
{
	UOpenMobileHapticPatternAsset* MakePattern(
		float FirstIntensity = 0.4f
	)
	{
		UOpenMobileHapticPatternAsset* Pattern =
			NewObject<UOpenMobileHapticPatternAsset>();
#if WITH_EDITORONLY_DATA
		FOpenMobileHapticPatternEvent First;
		First.Type = EOpenMobileHapticPatternEventType::Continuous;
		First.DurationSeconds = 0.03;
		First.Intensity = FirstIntensity;
		FOpenMobileHapticPatternEvent Transient;
		Transient.Type = EOpenMobileHapticPatternEventType::Transient;
		Transient.StartTimeSeconds = 0.03;
		Transient.Intensity = 0.8f;
		FOpenMobileHapticPatternEvent Silence;
		Silence.Type = EOpenMobileHapticPatternEventType::Silence;
		Silence.StartTimeSeconds = 0.031;
		Silence.DurationSeconds = 0.01;
		Pattern->SourcePattern.Events = {First, Transient, Silence};
		TArray<FString> Errors;
		if (!Pattern->RebuildDerivedData(Errors))
		{
			return nullptr;
		}
#endif
		return Pattern;
	}

	FOpenMobileHapticCapabilities AndroidCapabilities()
	{
		FOpenMobileHapticCapabilities Capabilities;
		Capabilities.BackendName = TEXT("Android");
		Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
		Capabilities.WaveformTiming = EOpenMobileHapticSupportState::Supported;
		Capabilities.AmplitudeControl =
			EOpenMobileHapticSupportState::Supported;
		return Capabilities;
	}

	FOpenMobileHapticCapabilities AppleCapabilities()
	{
		FOpenMobileHapticCapabilities Capabilities;
		Capabilities.BackendName = TEXT("IOS");
		Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
		Capabilities.TransientEvents =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.ContinuousEvents =
			EOpenMobileHapticSupportState::Supported;
		return Capabilities;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsTimelineRoutingAndCacheTest,
	"OpenMobile.Haptics.Timeline.Manager.RoutingAndCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsTimelineRoutingAndCacheTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTimelineManagerTests;
	UOpenMobileHapticPatternAsset* Pattern = MakePattern();
	TestNotNull(TEXT("Mixed timeline compiles"), Pattern);
	if (!Pattern)
	{
		return false;
	}

	FOpenMobileHapticsTimelineManager Manager(4);
	const FOpenMobileHapticLoopOptions Loop;
	const FOpenMobileHapticsTimelineLookup First = Manager.Resolve(
		TEXT("Android"),
		*Pattern,
		Loop,
		AndroidCapabilities(),
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic,
		7
	);
	TestFalse(TEXT("First translation is a cache miss"), First.bCacheHit);
	TestTrue(TEXT("Android route returns a timeline"), First.Timeline.IsValid());
	if (First.Timeline)
	{
		TestTrue(TEXT("Prepared timeline has a native resource identity"),
			First.Timeline->ResourceId != 0);
		TestTrue(TEXT("Prepared timeline reports bounded memory ownership"),
			First.Timeline->EstimatedBytes > 0);
		TestEqual(TEXT("Android route is selected"), First.Timeline->Path,
			EOpenMobileHapticsTimelinePath::AndroidWaveform);
		TestEqual(TEXT("Mixed events preserve their amplitudes"),
			First.Timeline->Android.Amplitudes,
			TArray<int32>({102, 204, 0}));
		TestEqual(TEXT("Silence remains an explicit segment"),
			First.Timeline->Android.TimingsMilliseconds,
			TArray<int64>({30, 1, 10}));
		TestTrue(TEXT("Portable timelines retain their control plan"),
			First.Timeline->bHasRepeatPlan);
		TestTrue(TEXT("Android retains one control compilation"),
			First.Timeline->AndroidControlBase.Outcome
				== EOpenMobileHapticsAndroidWaveformOutcome::Ready);
	}

	const FOpenMobileHapticsTimelineLookup Second = Manager.Resolve(
		TEXT("Android"),
		*Pattern,
		Loop,
		AndroidCapabilities(),
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic,
		7
	);
	TestTrue(TEXT("Identical translation is a cache hit"), Second.bCacheHit);
	TestTrue(TEXT("Cache hit reuses the immutable timeline"),
		First.Timeline == Second.Timeline);
	const FOpenMobileHapticsTimelineCacheStatistics CacheStatistics =
		Manager.GetStatistics();
	TestEqual(TEXT("First translation records one cache miss"),
		CacheStatistics.MissCount, static_cast<uint64>(1));
	TestEqual(TEXT("Repeated translation records one cache hit"),
		CacheStatistics.HitCount, static_cast<uint64>(1));
	TestEqual(TEXT("Cache statistics expose current entry count"),
		CacheStatistics.EntryCount, 1);
	TestTrue(TEXT("Cache statistics expose bounded memory ownership"),
		CacheStatistics.MemoryBytes > 0);

	FOpenMobileHapticLoopOptions FiniteLoop;
	FiniteLoop.bLoop = true;
	FiniteLoop.RepeatCount = 2;
	FiniteLoop.RepeatStartTimeSeconds = 0.03;
	FiniteLoop.MaximumDurationSeconds = 1.0;
	const FOpenMobileHapticsTimelineLookup Looped = Manager.Resolve(
		TEXT("Android"),
		*Pattern,
		FiniteLoop,
		AndroidCapabilities(),
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic,
		7
	);
	TestTrue(TEXT("Finite repeat plan remains available to controls"),
		Looped.Timeline && Looped.Timeline->bHasRepeatPlan
		&& Looped.Timeline->RepeatPlan.RepeatCount == 2);
	TestTrue(TEXT("Control base does not duplicate finite repeats"),
		Looped.Timeline
		&& Looped.Timeline->AndroidControlBase.TimingsMilliseconds.Num()
			< Looped.Timeline->Android.TimingsMilliseconds.Num());

	FOpenMobileHapticCapabilities UnsupportedApple = AppleCapabilities();
	UnsupportedApple.RichHaptics = EOpenMobileHapticSupportState::Unsupported;
	UnsupportedApple.ContinuousEvents =
		EOpenMobileHapticSupportState::Unsupported;
	const FOpenMobileHapticsTimelineLookup Fallback = Manager.Resolve(
		TEXT("IOS"),
		*Pattern,
		Loop,
		UnsupportedApple,
		1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic,
		7
	);
	TestTrue(TEXT("Apple fallback route returns a timeline"),
		Fallback.Timeline.IsValid());
	if (Fallback.Timeline)
	{
		TestEqual(TEXT("Mixed Apple events select continuous translation"),
			Fallback.Timeline->Path,
			EOpenMobileHapticsTimelinePath::AppleContinuous);
		TestEqual(TEXT("Unavailable rich support requests fallback"),
			Fallback.Timeline->AppleContinuous.Outcome,
			EOpenMobileHapticsAppleContinuousOutcome::FallbackRequired);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsTimelineResourceBoundsTest,
	"OpenMobile.Haptics.Timeline.Manager.ResourceBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsTimelineResourceBoundsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTimelineManagerTests;
	FOpenMobileHapticsTimelineManager ExtremeManager;
	FOpenMobileHapticsPreparedResourceLimits ExtremeLimits;
	ExtremeLimits.MaximumCount = MAX_int32;
	ExtremeLimits.MaximumBytes = MAX_int64;
	ExtremeLimits.IdleLifetimeSeconds = MAX_dbl;
	ExtremeManager.SetLimits(ExtremeLimits);
	const FOpenMobileHapticsTimelineCacheStatistics ExtremeStatistics =
		ExtremeManager.GetStatistics();
	TestEqual(TEXT("Timeline cache applies the entry hard ceiling"),
		ExtremeStatistics.MaximumEntryCount, 128);
	TestEqual(TEXT("Timeline cache applies the byte hard ceiling"),
		ExtremeStatistics.MaximumMemoryBytes,
		static_cast<int64>(64 * 1024 * 1024));
	TestEqual(TEXT("Timeline cache applies the idle hard ceiling"),
		ExtremeStatistics.IdleLifetimeSeconds, 300.0);

	UOpenMobileHapticPatternAsset* FirstPattern = MakePattern(0.2f);
	UOpenMobileHapticPatternAsset* SecondPattern = MakePattern(0.8f);
	TestNotNull(TEXT("First bounded timeline compiles"), FirstPattern);
	TestNotNull(TEXT("Second bounded timeline compiles"), SecondPattern);
	if (!FirstPattern || !SecondPattern)
	{
		return false;
	}

	const FOpenMobileHapticCapabilities Capabilities = AndroidCapabilities();
	const FOpenMobileHapticLoopOptions Loop;
	FOpenMobileHapticsTimelineManager ProbeManager(2);
	const FOpenMobileHapticsTimelineLookup Probe = ProbeManager.ResolveAtTime(
		TEXT("Android"), *FirstPattern, Loop, Capabilities, 1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic, 4, 10.0);
	TestTrue(TEXT("Probe timeline reports its owned bytes"),
		Probe.Timeline && Probe.Timeline->EstimatedBytes > 0);
	if (!Probe.Timeline)
	{
		return false;
	}

	const int64 MemoryLimit = Probe.Timeline->EstimatedBytes
		+ Probe.Timeline->EstimatedBytes / 2;
	FOpenMobileHapticsTimelineManager Manager(4, MemoryLimit, 2.0);
	auto ResolveAt = [&](UOpenMobileHapticPatternAsset& Pattern, double Time)
	{
		return Manager.ResolveAtTime(
			TEXT("Android"), Pattern, Loop, Capabilities, 1.0f,
			EOpenMobileHapticFallbackPolicy::Automatic, 4, Time);
	};
	const FOpenMobileHapticsTimelineLookup First = ResolveAt(*FirstPattern, 10.0);
	const uint64 FirstResourceId = First.Timeline
		? First.Timeline->ResourceId
		: 0;
	ResolveAt(*SecondPattern, 10.5);
	TestTrue(TEXT("Prepared timelines stay within their byte budget"),
		Manager.GetCacheMemoryBytes() <= MemoryLimit);
	TestEqual(TEXT("Byte pressure evicts one prepared timeline"),
		Manager.GetCacheEntryCount(), 1);
	TestFalse(TEXT("Byte eviction removes the least recent timeline"),
		ResolveAt(*FirstPattern, 11.0).bCacheHit);
	TestEqual(TEXT("Equivalent recompilation keeps its native identity"),
		ResolveAt(*FirstPattern, 11.5).Timeline->ResourceId,
		FirstResourceId);

	Manager.PruneIdle(14.0);
	TestEqual(TEXT("Idle prepared timelines expire without a ticker"),
		Manager.GetCacheEntryCount(), 0);
	TestEqual(TEXT("Idle eviction releases its memory accounting"),
		Manager.GetCacheMemoryBytes(), static_cast<int64>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsTimelineInvalidationTest,
	"OpenMobile.Haptics.Timeline.Manager.InvalidationAndEviction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsTimelineInvalidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsTimelineManagerTests;
	UOpenMobileHapticPatternAsset* FirstPattern = MakePattern(0.2f);
	UOpenMobileHapticPatternAsset* SecondPattern = MakePattern(0.4f);
	UOpenMobileHapticPatternAsset* ThirdPattern = MakePattern(0.6f);
	TestNotNull(TEXT("First timeline compiles"), FirstPattern);
	TestNotNull(TEXT("Second timeline compiles"), SecondPattern);
	TestNotNull(TEXT("Third timeline compiles"), ThirdPattern);
	if (!FirstPattern || !SecondPattern || !ThirdPattern)
	{
		return false;
	}

	FOpenMobileHapticsTimelineManager Manager(2);
	const FOpenMobileHapticCapabilities Capabilities = AndroidCapabilities();
	const FOpenMobileHapticLoopOptions Loop;
	auto Resolve = [&](UOpenMobileHapticPatternAsset& Pattern, uint64 Generation)
	{
		return Manager.Resolve(
			TEXT("Android"), Pattern, Loop, Capabilities, 1.0f,
			EOpenMobileHapticFallbackPolicy::Automatic, Generation);
	};
	Resolve(*FirstPattern, 10);
	Resolve(*SecondPattern, 10);
	TestTrue(TEXT("Recent access hits before eviction"),
		Resolve(*FirstPattern, 10).bCacheHit);
	Resolve(*ThirdPattern, 10);
	TestEqual(TEXT("Cache stays within its fixed bound"),
		Manager.GetCacheEntryCount(), 2);
	TestFalse(TEXT("Least recently used timeline is evicted first"),
		Resolve(*SecondPattern, 10).bCacheHit);

	const TSharedPtr<const FOpenMobileHapticsPortableTimeline>
		BeforeRevision = Resolve(*SecondPattern, 10).Timeline;
#if WITH_EDITORONLY_DATA
	SecondPattern->SourcePattern.Events[0].Intensity = 0.9f;
	SecondPattern->PatternVersion++;
	const FOpenMobileHapticsTimelineLookup Stale =
		Resolve(*SecondPattern, 10);
	TestFalse(TEXT("Stale derived data cannot hit the cache"),
		Stale.bCacheHit);
	TestTrue(TEXT("Stale derived data is rejected"),
		Stale.Timeline
		&& Stale.Timeline->Android.Outcome
			== EOpenMobileHapticsAndroidWaveformOutcome::Rejected);
	TestEqual(TEXT("Stale derived data is not retained"),
		Manager.GetCacheEntryCount(), 1);
	TArray<FString> Errors;
	TestTrue(TEXT("Revised timeline recompiles"),
		SecondPattern->RebuildDerivedData(Errors));
#endif
	const FOpenMobileHapticsTimelineLookup Revised =
		Resolve(*SecondPattern, 10);
	TestFalse(TEXT("Asset revision invalidates its cached translation"),
		Revised.bCacheHit);
	TestTrue(TEXT("Revision creates a new immutable representation"),
		Revised.Timeline != BeforeRevision);

	const FOpenMobileHapticsTimelineLookup NewLifecycle =
		Resolve(*SecondPattern, 11);
	TestFalse(TEXT("Backend lifecycle change invalidates translations"),
		NewLifecycle.bCacheHit);
	TestEqual(TEXT("Old lifecycle entries are released"),
		Manager.GetCacheEntryCount(), 1);

	FOpenMobileHapticCapabilities BasicCapabilities = Capabilities;
	BasicCapabilities.AmplitudeControl =
		EOpenMobileHapticSupportState::Unsupported;
	const FOpenMobileHapticsTimelineLookup CapabilityChange = Manager.Resolve(
		TEXT("Android"), *SecondPattern, Loop, BasicCapabilities, 1.0f,
		EOpenMobileHapticFallbackPolicy::Automatic, 11);
	TestFalse(TEXT("Capability signature change misses the cache"),
		CapabilityChange.bCacheHit);
	if (CapabilityChange.Timeline)
	{
		TestTrue(TEXT("Capability-specific translation uses default amplitude"),
			CapabilityChange.Timeline->Android.bUsesDefaultAmplitude);
	}

	Manager.Clear();
	TestEqual(TEXT("Backend release clears cached representations"),
		Manager.GetCacheEntryCount(), 0);
	TestTrue(TEXT("Cache release records evicted representations"),
		Manager.GetStatistics().EvictionCount > 0);
	return true;
}

#endif
