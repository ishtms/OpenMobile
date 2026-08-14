#if WITH_DEV_AUTOMATION_TESTS

#include "Async/TaskGraphInterfaces.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsAppleBridgeService.h"
#include "OpenMobileHapticsAppleContinuousPolicy.h"

namespace OpenMobileHapticsAppleBridgeServiceTests
{
	/** Keeps every bridge result and captured callback mutable, each service test can force one native path without platform code. */
	class FMockAppleBridge final : public IOpenMobileHapticsAppleBridge
	{
	public:
		/** Returns configured probe and counts service cache misses. */
		virtual FOpenMobileHapticsAppleHardwareProbe QueryHardware() override
		{
			++QueryCount;
			return Probe;
		}

		/** Returns configured engine result and records recreation attempts. */
		virtual EOpenMobileHapticsAppleEngineResult CreateEngine() override
		{
			++CreateEngineCount;
			return CreateEngineResult;
		}

		/** Captures generator idle lifetime so service forwarding can be asserted. */
		virtual EOpenMobileHapticsAppleSubmissionResult
		PrepareSemanticGenerators(double IdleLifetimeSeconds) override
		{
			++PrepareSemanticCount;
			LastIdleLifetimeSeconds = IdleLifetimeSeconds;
			return PrepareSemanticResult;
		}

		/** Captures transient preparation payload and limits without native allocation. */
		virtual EOpenMobileHapticsAppleSubmissionResult
		PrepareTransientPattern(
			uint64 ResourceId,
			const FOpenMobileHapticsAppleTransientPattern& Pattern,
			int64 EstimatedBytes,
			const FOpenMobileHapticsPreparedResourceLimits& Limits
		) override
		{
			++PrepareTransientCount;
			LastPreparedResourceId = ResourceId;
			LastTransientPattern = Pattern;
			LastEstimatedBytes = EstimatedBytes;
			LastLimits = Limits;
			return PreparePatternResult;
		}

		/** Captures continuous preparation through the same configurable result path. */
		virtual EOpenMobileHapticsAppleSubmissionResult
		PrepareContinuousPattern(
			uint64 ResourceId,
			const FOpenMobileHapticsAppleContinuousPattern& Pattern,
			int64 EstimatedBytes,
			const FOpenMobileHapticsPreparedResourceLimits& Limits
		) override
		{
			++PrepareContinuousCount;
			LastPreparedResourceId = ResourceId;
			LastContinuousPattern = Pattern;
			LastEstimatedBytes = EstimatedBytes;
			LastLimits = Limits;
			return PreparePatternResult;
		}

		/** Accepts immediate semantic playback because these tests focus on service ownership. */
		virtual EOpenMobileHapticsAppleSubmissionResult PlaySemantic(
			EOpenMobileHapticsSemanticBehavior Behavior,
			float Intensity
		) override
		{
			static_cast<void>(Behavior);
			static_cast<void>(Intensity);
			return EOpenMobileHapticsAppleSubmissionResult::Accepted;
		}

		/** Accepts the basic vibration route without retaining extra mock state. */
		virtual EOpenMobileHapticsAppleSubmissionResult
		PlaySystemVibration() override
		{
			return EOpenMobileHapticsAppleSubmissionResult::Accepted;
		}

		/** Captures scheduled semantic identity, timing, and terminal callback. */
		virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledSemantic(
			uint64 RequestId,
			EOpenMobileHapticsSemanticBehavior Behavior,
			float Intensity,
			const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
			FOpenMobileHapticsApplePlaybackEventCallback Callback
		) override
		{
			LastRequestId = RequestId;
			LastSemanticBehavior = Behavior;
			LastSemanticIntensity = Intensity;
			LastSchedule = Schedule;
			PlaybackCallback = MoveTemp(Callback);
			return EOpenMobileHapticsAppleSubmissionResult::Accepted;
		}

		/** Captures scheduled system vibration timing through the shared playback callback slot. */
		virtual EOpenMobileHapticsAppleSubmissionResult
		PlayScheduledSystemVibration(
			uint64 RequestId,
			const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
			FOpenMobileHapticsApplePlaybackEventCallback Callback
		) override
		{
			LastRequestId = RequestId;
			LastSchedule = Schedule;
			PlaybackCallback = MoveTemp(Callback);
			return EOpenMobileHapticsAppleSubmissionResult::Accepted;
		}

		/** Captures transient playback, prepared id, initial parameters, and configurable native result. */
		virtual EOpenMobileHapticsAppleSubmissionResult PlayTransientPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleTransientPattern& Pattern,
			FOpenMobileHapticsApplePlaybackEventCallback Callback,
			const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
			uint64 PreparedResourceId
		) override
		{
			++TransientSubmissionCount;
			LastRequestId = RequestId;
			LastPlaybackResourceId = PreparedResourceId;
			LastTransientPattern = Pattern;
			if (InitialParameters)
			{
				LastInitialParameters = *InitialParameters;
				bHadInitialParameters = true;
			}
			PlaybackCallback = MoveTemp(Callback);
			return TransientSubmissionResult;
		}

		/** Captures continuous playback independently so tests can distinguish translation routes. */
		virtual EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleContinuousPattern& Pattern,
			FOpenMobileHapticsApplePlaybackEventCallback Callback,
			const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
			uint64 PreparedResourceId
		) override
		{
			++ContinuousSubmissionCount;
			LastRequestId = RequestId;
			LastPlaybackResourceId = PreparedResourceId;
			LastContinuousPattern = Pattern;
			if (InitialParameters)
			{
				LastInitialParameters = *InitialParameters;
				bHadInitialParameters = true;
			}
			PlaybackCallback = MoveTemp(Callback);
			return ContinuousSubmissionResult;
		}

		/** Retains schedule then reuses immediate transient capture, avoiding duplicate mock rules. */
		virtual EOpenMobileHapticsAppleSubmissionResult
		PlayScheduledTransientPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleTransientPattern& Pattern,
			const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
			FOpenMobileHapticsApplePlaybackEventCallback Callback,
			const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
			uint64 PreparedResourceId
		) override
		{
			LastSchedule = Schedule;
			return PlayTransientPattern(
				RequestId,
				Pattern,
				MoveTemp(Callback),
				InitialParameters,
				PreparedResourceId
			);
		}

		/** Retains schedule then reuses continuous capture for the rest of the payload. */
		virtual EOpenMobileHapticsAppleSubmissionResult
		PlayScheduledContinuousPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleContinuousPattern& Pattern,
			const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
			FOpenMobileHapticsApplePlaybackEventCallback Callback,
			const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
			uint64 PreparedResourceId
		) override
		{
			LastSchedule = Schedule;
			return PlayContinuousPattern(
				RequestId,
				Pattern,
				MoveTemp(Callback),
				InitialParameters,
				PreparedResourceId
			);
		}

		/** Captures normalized AHAP and callback with a separately configurable result. */
		virtual EOpenMobileHapticsAppleSubmissionResult PlayAHAPPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleAHAPPattern& Pattern,
			FOpenMobileHapticsApplePlaybackEventCallback Callback
		) override
		{
			++AHAPSubmissionCount;
			LastRequestId = RequestId;
			LastAHAPPattern = Pattern;
			PlaybackCallback = MoveTemp(Callback);
			return AHAPSubmissionResult;
		}

		/** Retains schedule and reuses AHAP payload capture. */
		virtual EOpenMobileHapticsAppleSubmissionResult PlayScheduledAHAPPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleAHAPPattern& Pattern,
			const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
			FOpenMobileHapticsApplePlaybackEventCallback Callback
		) override
		{
			LastSchedule = Schedule;
			return PlayAHAPPattern(
				RequestId,
				Pattern,
				MoveTemp(Callback)
			);
		}

		/** Records stop target and returns the result selected by the test. */
		virtual EOpenMobileHapticsAppleSubmissionResult StopPattern(
			uint64 RequestId
		) override
		{
			++StopCount;
			LastRequestId = RequestId;
			return StopResult;
		}

		/** Records pause target without changing mock playback automatically. */
		virtual EOpenMobileHapticsAppleSubmissionResult PausePattern(
			uint64 RequestId
		) override
		{
			++PauseCount;
			LastRequestId = RequestId;
			return PauseResult;
		}

		/** Records resume target so service routing can be checked separately from callback state. */
		virtual EOpenMobileHapticsAppleSubmissionResult ResumePattern(
			uint64 RequestId
		) override
		{
			++ResumeCount;
			LastRequestId = RequestId;
			return ResumeResult;
		}

		/** Captures request and resolved seek position for control forwarding checks. */
		virtual EOpenMobileHapticsAppleSubmissionResult SeekPattern(
			uint64 RequestId,
			double PositionSeconds
		) override
		{
			++SeekCount;
			LastRequestId = RequestId;
			LastSeekPositionSeconds = PositionSeconds;
			return SeekResult;
		}

		/** Captures dynamic update exactly as service forwarded it. */
		virtual EOpenMobileHapticsAppleSubmissionResult UpdatePattern(
			uint64 RequestId,
			const FOpenMobileHapticDynamicParameterUpdate& Update
		) override
		{
			++UpdateCount;
			LastRequestId = RequestId;
			LastUpdate = Update;
			return UpdateResult;
		}

		/** Stores the latest engine callback so tests can fire it after invalidation. */
		virtual void SetEventCallback(
			FOpenMobileHapticsAppleBridgeEventCallback Callback
		) override
		{
			EventCallback = MoveTemp(Callback);
		}

		/** Clears callback ownership and counts idempotent service shutdown. */
		virtual void Shutdown() override
		{
			++ShutdownCount;
			EventCallback = {};
		}

		/** Counts prepared-cache release without disturbing playback callback fixtures. */
		virtual void ReleasePreparedResources() override
		{
			++ReleasePreparedCount;
		}

		/** Drives a retained engine callback after service state has changed, including late-event cases. */
		void Emit(EOpenMobileHapticsAppleBridgeEvent Event)
		{
			if (EventCallback)
			{
				EventCallback(Event);
			}
		}

		/** Drives whichever playback callback the latest mock submission captured. */
		void EmitPlayback(EOpenMobileHapticsApplePlaybackEvent Event)
		{
			if (PlaybackCallback)
			{
				PlaybackCallback(Event);
			}
		}

		FOpenMobileHapticsAppleHardwareProbe Probe;
		EOpenMobileHapticsAppleEngineResult CreateEngineResult =
			EOpenMobileHapticsAppleEngineResult::Ready;
		EOpenMobileHapticsAppleSubmissionResult TransientSubmissionResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult ContinuousSubmissionResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult AHAPSubmissionResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult StopResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult UpdateResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult PauseResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult ResumeResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult SeekResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult PrepareSemanticResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult PreparePatternResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		int32 QueryCount = 0;
		int32 CreateEngineCount = 0;
		int32 ShutdownCount = 0;
		int32 TransientSubmissionCount = 0;
		int32 ContinuousSubmissionCount = 0;
		int32 AHAPSubmissionCount = 0;
		int32 StopCount = 0;
		int32 UpdateCount = 0;
		int32 PauseCount = 0;
		int32 ResumeCount = 0;
		int32 SeekCount = 0;
		int32 PrepareSemanticCount = 0;
		int32 PrepareTransientCount = 0;
		int32 PrepareContinuousCount = 0;
		int32 ReleasePreparedCount = 0;
		uint64 LastRequestId = 0;
		uint64 LastPreparedResourceId = 0;
		uint64 LastPlaybackResourceId = 0;
		int64 LastEstimatedBytes = 0;
		double LastIdleLifetimeSeconds = 0.0;
		double LastSeekPositionSeconds = 0.0;
		FOpenMobileHapticsPreparedResourceLimits LastLimits;
		FOpenMobileHapticsAppleTransientPattern LastTransientPattern;
		FOpenMobileHapticsAppleContinuousPattern LastContinuousPattern;
		FOpenMobileHapticsAppleAHAPPattern LastAHAPPattern;
		FOpenMobileHapticsApplePlaybackSchedule LastSchedule;
		EOpenMobileHapticsSemanticBehavior LastSemanticBehavior =
			EOpenMobileHapticsSemanticBehavior::Selection;
		float LastSemanticIntensity = 0.0f;
		FOpenMobileHapticDynamicParameterUpdate LastUpdate;
		FOpenMobileHapticDynamicParameterUpdate LastInitialParameters;
		bool bHadInitialParameters = false;
		FOpenMobileHapticsAppleBridgeEventCallback EventCallback;
		FOpenMobileHapticsApplePlaybackEventCallback PlaybackCallback;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleEngineOwnershipTest,
	"OpenMobile.Haptics.Apple.Bridge.EngineOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleEngineOwnershipTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleBridgeServiceTests;

	TUniquePtr<FMockAppleBridge> UnsupportedBridge =
		MakeUnique<FMockAppleBridge>();
	FMockAppleBridge* Unsupported = UnsupportedBridge.Get();
	Unsupported->Probe.RichHaptics =
		EOpenMobileHapticsAppleHardwareState::Unsupported;
	FOpenMobileHapticsAppleBridgeService UnsupportedService(
		MoveTemp(UnsupportedBridge)
	);
	UnsupportedService.GetHardwareProbe();
	UnsupportedService.GetHardwareProbe();
	TestEqual(TEXT("Stable probes are cached"), Unsupported->QueryCount, 1);
	TestEqual(
		TEXT("Capability queries do not create an engine"),
		Unsupported->CreateEngineCount,
		0
	);
	TestEqual(
		TEXT("Unsupported hardware rejects engine creation"),
		UnsupportedService.EnsureEngine(),
		EOpenMobileHapticsAppleEngineResult::UnsupportedHardware
	);
	TestEqual(
		TEXT("Unsupported hardware never reaches native engine creation"),
		Unsupported->CreateEngineCount,
		0
	);

	TUniquePtr<FMockAppleBridge> RecoveringBridge =
		MakeUnique<FMockAppleBridge>();
	FMockAppleBridge* Recovering = RecoveringBridge.Get();
	FOpenMobileHapticsAppleBridgeService RecoveringService(
		MoveTemp(RecoveringBridge)
	);
	TestEqual(
		TEXT("A temporary probe defers engine creation"),
		RecoveringService.EnsureEngine(),
		EOpenMobileHapticsAppleEngineResult::TemporarilyUnavailable
	);
	Recovering->Probe.RichHaptics =
		EOpenMobileHapticsAppleHardwareState::Supported;
	TestEqual(
		TEXT("A temporary probe is retried"),
		RecoveringService.EnsureEngine(),
		EOpenMobileHapticsAppleEngineResult::Ready
	);
	TestEqual(TEXT("Temporary probes are not cached"),
		Recovering->QueryCount, 2);
	TestEqual(TEXT("Recovery creates one engine"),
		Recovering->CreateEngineCount, 1);

	TUniquePtr<FMockAppleBridge> SupportedBridge =
		MakeUnique<FMockAppleBridge>();
	FMockAppleBridge* Supported = SupportedBridge.Get();
	Supported->Probe.RichHaptics =
		EOpenMobileHapticsAppleHardwareState::Supported;
	FOpenMobileHapticsAppleBridgeService SupportedService(
		MoveTemp(SupportedBridge)
	);
	TestEqual(
		TEXT("Supported hardware creates its engine"),
		SupportedService.EnsureEngine(),
		EOpenMobileHapticsAppleEngineResult::Ready
	);
	TestEqual(
		TEXT("A ready engine is reused"),
		SupportedService.EnsureEngine(),
		EOpenMobileHapticsAppleEngineResult::Ready
	);
	TestEqual(
		TEXT("One service owns one native engine"),
		Supported->CreateEngineCount,
		1
	);
	SupportedService.InvalidateEngine();
	TestEqual(TEXT("A stale prepared engine is restarted"),
		SupportedService.EnsureEngine(),
		EOpenMobileHapticsAppleEngineResult::Ready);
	TestEqual(TEXT("Engine invalidation reaches native startup again"),
		Supported->CreateEngineCount, 2);
	TestEqual(TEXT("Semantic generators prewarm without playback"),
		SupportedService.PrepareSemanticGenerators(4.0),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("Semantic preparation reaches native once"),
		Supported->PrepareSemanticCount, 1);
	TestEqual(TEXT("Semantic preparation uses the idle lifetime"),
		Supported->LastIdleLifetimeSeconds, 4.0);
	FOpenMobileHapticsAppleTransientPattern PreparedPattern;
	PreparedPattern.StartTimesSeconds = {0.0};
	PreparedPattern.Intensities = {1.0f};
	PreparedPattern.Sharpnesses = {0.5f};
	FOpenMobileHapticsPreparedResourceLimits Limits;
	Limits.MaximumCount = 3;
	Limits.MaximumBytes = 2048;
	Limits.IdleLifetimeSeconds = 4.0;
	TestEqual(TEXT("Native pattern compilation does not play"),
		SupportedService.PrepareTransientPattern(
			17, PreparedPattern, 256, Limits),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("Prepared resource identity reaches native"),
		Supported->LastPreparedResourceId, static_cast<uint64>(17));
	TestEqual(TEXT("Pattern preparation does not submit playback"),
		Supported->TransientSubmissionCount, 0);
	SupportedService.ReleasePreparedResources();
	TestEqual(TEXT("Explicit release clears native prepared resources"),
		Supported->ReleasePreparedCount, 1);
	SupportedService.Shutdown();
	SupportedService.Shutdown();
	TestEqual(TEXT("Shutdown releases once"), Supported->ShutdownCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleBridgeCallbackTest,
	"OpenMobile.Haptics.Apple.Bridge.GameThreadCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleBridgeCallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleBridgeServiceTests;

	TUniquePtr<FMockAppleBridge> Bridge = MakeUnique<FMockAppleBridge>();
	FMockAppleBridge* Mock = Bridge.Get();
	FOpenMobileHapticsAppleBridgeService Service(MoveTemp(Bridge));
	int32 CallbackCount = 0;
	bool bCallbackWasOnGameThread = false;
	Service.SetEventCallback(
		[&CallbackCount, &bCallbackWasOnGameThread](
			EOpenMobileHapticsAppleBridgeEvent Event
		)
		{
			static_cast<void>(Event);
			++CallbackCount;
			bCallbackWasOnGameThread = IsInGameThread();
		}
	);
	Mock->Emit(EOpenMobileHapticsAppleBridgeEvent::EngineReset);
	TestEqual(TEXT("Native callbacks never run inline"), CallbackCount, 0);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Native callback is delivered once"), CallbackCount, 1);
	TestTrue(TEXT("Native callback reaches the game thread"),
		bCallbackWasOnGameThread);
	Mock->Emit(EOpenMobileHapticsAppleBridgeEvent::AudioSessionChanged);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Audio-session changes use the same relay"),
		CallbackCount, 2);

	Mock->Emit(EOpenMobileHapticsAppleBridgeEvent::EngineStopped);
	Service.Shutdown();
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Shutdown drops queued callbacks"), CallbackCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsApplePlaybackCallbackTest,
	"OpenMobile.Haptics.Apple.Bridge.PlaybackCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsApplePlaybackCallbackTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleBridgeServiceTests;

	TUniquePtr<FMockAppleBridge> Bridge = MakeUnique<FMockAppleBridge>();
	FMockAppleBridge* Mock = Bridge.Get();
	FOpenMobileHapticsAppleBridgeService Service(MoveTemp(Bridge));
	FOpenMobileHapticsAppleTransientPattern Pattern;
	Pattern.StartTimesSeconds = {0.0, 0.05};
	Pattern.Intensities = {1.0f, 0.5f};
	Pattern.Sharpnesses = {0.25f, 0.75f};
	FOpenMobileHapticDynamicParameterUpdate InitialParameters;
	InitialParameters.bUpdateIntensity = true;
	InitialParameters.Intensity = 0.6f;
	int32 CallbackCount = 0;
	bool bCallbackWasOnGameThread = false;
	TestEqual(
		TEXT("Transient pattern reaches the injected bridge"),
		Service.PlayTransientPattern(
			42,
			Pattern,
			[&CallbackCount, &bCallbackWasOnGameThread](
				EOpenMobileHapticsApplePlaybackEvent Event
			)
			{
				static_cast<void>(Event);
				++CallbackCount;
				bCallbackWasOnGameThread = IsInGameThread();
			},
			&InitialParameters,
			42
		),
		EOpenMobileHapticsAppleSubmissionResult::Accepted
	);
	TestEqual(TEXT("One native pattern is submitted"),
		Mock->TransientSubmissionCount, 1);
	TestEqual(TEXT("Request identity crosses the bridge"),
		Mock->LastRequestId, static_cast<uint64>(42));
	TestEqual(TEXT("Prepared identity is reused for playback"),
		Mock->LastPlaybackResourceId, static_cast<uint64>(42));
	TestEqual(TEXT("All events stay in one bridge call"),
		Mock->LastTransientPattern.StartTimesSeconds.Num(), 2);
	TestTrue(TEXT("Request parameters stay outside the cached pattern"),
		!Mock->LastTransientPattern.bHasInitialDynamicParameters
		&& Mock->bHadInitialParameters);
	TestEqual(TEXT("Initial intensity reaches the native request"),
		Mock->LastInitialParameters.Intensity, 0.6f);

	Mock->EmitPlayback(EOpenMobileHapticsApplePlaybackEvent::Completed);
	TestEqual(TEXT("Playback completion never runs inline"), CallbackCount, 0);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Playback completion is delivered once"), CallbackCount, 1);
	TestTrue(TEXT("Playback completion reaches the game thread"),
		bCallbackWasOnGameThread);

	TestEqual(TEXT("Handle-scoped stop reaches the bridge"),
		Service.StopPattern(42),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("Stop keeps request identity"), Mock->LastRequestId,
		static_cast<uint64>(42));
	Mock->EmitPlayback(EOpenMobileHapticsApplePlaybackEvent::Failed);
	Service.Shutdown();
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Shutdown drops queued playback callbacks"),
		CallbackCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleAHAPBridgeTest,
	"OpenMobile.Haptics.Apple.Bridge.AHAPOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleAHAPBridgeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleBridgeServiceTests;
	TUniquePtr<FMockAppleBridge> Bridge = MakeUnique<FMockAppleBridge>();
	FMockAppleBridge* Mock = Bridge.Get();
	FOpenMobileHapticsAppleBridgeService Service(MoveTemp(Bridge));
	FOpenMobileHapticsAppleAHAPPattern Pattern;
	Pattern.NormalizedJson = TEXT(
		"{\"Version\":1,\"Pattern\":[{\"Event\":{"
		"\"EventType\":\"HapticTransient\",\"Time\":0}}]}"
	);
	Pattern.DurationSeconds = 0.1;
	Pattern.SafetyDurationSeconds = 0.1;
	int32 CallbackCount = 0;
	TestEqual(TEXT("AHAP reaches the injected bridge"),
		Service.PlayAHAPPattern(
			91,
			Pattern,
			[&CallbackCount](EOpenMobileHapticsApplePlaybackEvent Event)
			{
				static_cast<void>(Event);
				++CallbackCount;
			}
		),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("One request-owned AHAP is submitted"),
		Mock->AHAPSubmissionCount, 1);
	TestEqual(TEXT("AHAP request identity crosses the bridge"),
		Mock->LastRequestId, static_cast<uint64>(91));
	TestEqual(TEXT("Normalized AHAP crosses unchanged"),
		Mock->LastAHAPPattern.NormalizedJson, Pattern.NormalizedJson);
	TestFalse(TEXT("Fixed AHAP retains standard player selection"),
		Mock->LastAHAPPattern.bRequiresAdvancedPlayer);
	Mock->EmitPlayback(EOpenMobileHapticsApplePlaybackEvent::Completed);
	TestEqual(TEXT("AHAP completion is not inline"), CallbackCount, 0);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread);
	TestEqual(TEXT("AHAP completion reaches the game thread"),
		CallbackCount, 1);

	Pattern.bRequiresAdvancedPlayer = true;
	Pattern.bLoop = true;
	Pattern.SafetyDurationSeconds = 1.0;
	FOpenMobileHapticsAppleAudioResource AudioResource;
	AudioResource.RelativePath = TEXT("Audio/Explosion.caf");
	AudioResource.Data = {'c', 'a', 'f', 'f', 0, 1, 0, 0};
	Pattern.AudioResources.Add(AudioResource);
	Pattern.bScheduled = true;
	Pattern.ScheduledPlatformTimeSeconds = 123.5;
	Pattern.MaximumLatenessSeconds = 0.05;
	TestEqual(TEXT("Advanced AHAP controls reach the bridge"),
		Service.PlayAHAPPattern(92, Pattern, {}),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestTrue(TEXT("Advanced selection is retained"),
		Mock->LastAHAPPattern.bRequiresAdvancedPlayer);
	TestTrue(TEXT("AHAP loop ownership is retained"),
		Mock->LastAHAPPattern.bLoop);
	TestEqual(TEXT("AHAP safety duration is retained"),
		Mock->LastAHAPPattern.SafetyDurationSeconds, 1.0);
	TestEqual(TEXT("Cooked audio resources stay request owned"),
		Mock->LastAHAPPattern.AudioResources.Num(), 1);
	TestEqual(TEXT("The native target time stays monotonic"),
		Mock->LastAHAPPattern.ScheduledPlatformTimeSeconds, 123.5);
	TestEqual(TEXT("The late-start bound reaches native playback"),
		Mock->LastAHAPPattern.MaximumLatenessSeconds, 0.05);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleContinuousBridgeTest,
	"OpenMobile.Haptics.Apple.Bridge.ContinuousOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleContinuousBridgeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleBridgeServiceTests;

	TUniquePtr<FMockAppleBridge> Bridge = MakeUnique<FMockAppleBridge>();
	FMockAppleBridge* Mock = Bridge.Get();
	FOpenMobileHapticsAppleBridgeService Service(MoveTemp(Bridge));
	FOpenMobileHapticsAppleContinuousPattern Pattern;
	Pattern.Events.Add({
		EOpenMobileHapticPatternEventType::Continuous,
		0.0,
		0.25,
		1.0f,
		0.5f
	});
	Pattern.DurationSeconds = 0.25;
	Pattern.bLoop = true;
	Pattern.LoopEndSeconds = 0.25;
	Pattern.SafetyDurationSeconds = 1.0;
	int32 CallbackCount = 0;
	TestEqual(TEXT("Continuous pattern reaches the injected bridge"),
		Service.PlayContinuousPattern(
			84,
			Pattern,
			[&CallbackCount](EOpenMobileHapticsApplePlaybackEvent Event)
			{
				static_cast<void>(Event);
				++CallbackCount;
			}
		),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("One request-owned continuous pattern is submitted"),
		Mock->ContinuousSubmissionCount, 1);
	TestEqual(TEXT("Continuous request identity crosses the bridge"),
		Mock->LastRequestId, static_cast<uint64>(84));
	TestTrue(TEXT("Native loop ownership crosses the bridge"),
		Mock->LastContinuousPattern.bLoop);
	TestEqual(TEXT("Safety duration crosses the bridge"),
		Mock->LastContinuousPattern.SafetyDurationSeconds, 1.0);
	Mock->EmitPlayback(EOpenMobileHapticsApplePlaybackEvent::Completed);
	TestEqual(TEXT("Continuous completion is not inline"), CallbackCount, 0);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Continuous completion reaches the game thread"),
		CallbackCount, 1);

	TestEqual(TEXT("A second continuous request is accepted"),
		Service.PlayContinuousPattern(
			85,
			Pattern,
			[&CallbackCount](EOpenMobileHapticsApplePlaybackEvent Event)
			{
				static_cast<void>(Event);
				++CallbackCount;
			}
		),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("Continuous cancellation is request scoped"),
		Service.StopPattern(85),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("Continuous cancellation keeps request identity"),
		Mock->LastRequestId, static_cast<uint64>(85));
	FOpenMobileHapticDynamicParameterUpdate Update;
	Update.Intensity = 0.4f;
	Update.bUpdateSharpness = true;
	Update.Sharpness = 0.8f;
	TestEqual(TEXT("Runtime parameters reach the injected bridge"),
		Service.UpdatePattern(85, Update),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("Runtime parameters use one native batch"),
		Mock->UpdateCount, 1);
	TestEqual(TEXT("Runtime parameters preserve request identity"),
		Mock->LastRequestId, static_cast<uint64>(85));
	TestEqual(TEXT("Runtime intensity crosses the bridge"),
		Mock->LastUpdate.Intensity, 0.4f);
	TestEqual(TEXT("Runtime sharpness crosses the bridge"),
		Mock->LastUpdate.Sharpness, 0.8f);

	int32 ResetCount = 0;
	bool bResetWasOnGameThread = false;
	Service.SetEventCallback(
		[&ResetCount, &bResetWasOnGameThread](
			EOpenMobileHapticsAppleBridgeEvent Event
		)
		{
			static_cast<void>(Event);
			++ResetCount;
			bResetWasOnGameThread = IsInGameThread();
		}
	);
	TestEqual(TEXT("Continuous playback can restart after cancellation"),
		Service.PlayContinuousPattern(
			86,
			Pattern,
			[&CallbackCount](EOpenMobileHapticsApplePlaybackEvent Event)
			{
				static_cast<void>(Event);
				++CallbackCount;
			}
		),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	Mock->EmitPlayback(EOpenMobileHapticsApplePlaybackEvent::Failed);
	Mock->Emit(EOpenMobileHapticsAppleBridgeEvent::EngineReset);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Reset failure reaches the continuous owner once"),
		CallbackCount, 2);
	TestEqual(TEXT("Engine reset notification is delivered once"),
		ResetCount, 1);
	TestTrue(TEXT("Engine reset notification reaches the game thread"),
		bResetWasOnGameThread);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsApplePlaybackControlBridgeTest,
	"OpenMobile.Haptics.Apple.Bridge.PlaybackControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsApplePlaybackControlBridgeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleBridgeServiceTests;
	TUniquePtr<FMockAppleBridge> Bridge = MakeUnique<FMockAppleBridge>();
	FMockAppleBridge* Mock = Bridge.Get();
	FOpenMobileHapticsAppleBridgeService Service(MoveTemp(Bridge));

	TestEqual(TEXT("Pause reaches the request-owned advanced player"),
		Service.PausePattern(101),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("Pause preserves request identity"),
		Mock->LastRequestId, static_cast<uint64>(101));
	TestEqual(TEXT("Pause crosses the bridge once"), Mock->PauseCount, 1);

	TestEqual(TEXT("Resume reaches the same request-owned player"),
		Service.ResumePattern(101),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("Resume crosses the bridge once"), Mock->ResumeCount, 1);

	TestEqual(TEXT("Seek reaches the same request-owned player"),
		Service.SeekPattern(101, 0.375),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("Seek preserves the native offset"),
		Mock->LastSeekPositionSeconds, 0.375);
	TestEqual(TEXT("Seek crosses the bridge once"), Mock->SeekCount, 1);

	Mock->PauseResult = EOpenMobileHapticsAppleSubmissionResult::StaleRequest;
	TestEqual(TEXT("Stale advanced players remain distinguishable"),
		Service.PausePattern(102),
		EOpenMobileHapticsAppleSubmissionResult::StaleRequest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAppleScheduledBridgeTest,
	"OpenMobile.Haptics.Apple.Bridge.ScheduledPlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAppleScheduledBridgeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileHapticsAppleBridgeServiceTests;

	TUniquePtr<FMockAppleBridge> Bridge = MakeUnique<FMockAppleBridge>();
	FMockAppleBridge* Mock = Bridge.Get();
	FOpenMobileHapticsAppleBridgeService Service(MoveTemp(Bridge));
	FOpenMobileHapticsApplePlaybackSchedule Schedule;
	Schedule.PlatformTimeSeconds = 123.5;
	Schedule.MaximumLatenessSeconds = 0.05;
	Schedule.Guard = MakeShared<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	>(9);
	int32 CallbackCount = 0;
	bool bCallbackWasOnGameThread = false;
	TestEqual(TEXT("Scheduled semantic playback reaches native"),
		Service.PlayScheduledSemantic(
			71,
			EOpenMobileHapticsSemanticBehavior::Selection,
			0.6f,
			Schedule,
			[&CallbackCount, &bCallbackWasOnGameThread](
				EOpenMobileHapticsApplePlaybackEvent Event
			)
			{
				static_cast<void>(Event);
				++CallbackCount;
				bCallbackWasOnGameThread = IsInGameThread();
			}
		),
		EOpenMobileHapticsAppleSubmissionResult::Accepted);
	TestEqual(TEXT("The request identity reaches scheduling"),
		Mock->LastRequestId, static_cast<uint64>(71));
	TestEqual(TEXT("The monotonic target crosses unchanged"),
		Mock->LastSchedule.PlatformTimeSeconds, 123.5);
	TestEqual(TEXT("The late bound crosses unchanged"),
		Mock->LastSchedule.MaximumLatenessSeconds, 0.05);
	TestTrue(TEXT("The delayed-start guard stays request scoped"),
		Mock->LastSchedule.Guard == Schedule.Guard);

	Mock->EmitPlayback(EOpenMobileHapticsApplePlaybackEvent::Interrupted);
	TestEqual(TEXT("Scheduled completion is never delivered inline"),
		CallbackCount, 0);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread);
	TestEqual(TEXT("Scheduled completion reaches the owner once"),
		CallbackCount, 1);
	TestTrue(TEXT("Scheduled completion reaches the game thread"),
		bCallbackWasOnGameThread);
	return true;
}

#endif
