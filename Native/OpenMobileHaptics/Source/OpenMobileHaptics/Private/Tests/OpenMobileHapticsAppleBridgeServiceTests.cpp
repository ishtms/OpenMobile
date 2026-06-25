#if WITH_DEV_AUTOMATION_TESTS

#include "Async/TaskGraphInterfaces.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsAppleBridgeService.h"
#include "OpenMobileHapticsAppleContinuousPolicy.h"

namespace OpenMobileHapticsAppleBridgeServiceTests
{
	class FMockAppleBridge final : public IOpenMobileHapticsAppleBridge
	{
	public:
		virtual FOpenMobileHapticsAppleHardwareProbe QueryHardware() override
		{
			++QueryCount;
			return Probe;
		}

		virtual EOpenMobileHapticsAppleEngineResult CreateEngine() override
		{
			++CreateEngineCount;
			return CreateEngineResult;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult PlaySemantic(
			EOpenMobileHapticsSemanticBehavior Behavior,
			float Intensity
		) override
		{
			static_cast<void>(Behavior);
			static_cast<void>(Intensity);
			return EOpenMobileHapticsAppleSubmissionResult::Accepted;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult
		PlaySystemVibration() override
		{
			return EOpenMobileHapticsAppleSubmissionResult::Accepted;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult PlayTransientPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleTransientPattern& Pattern,
			FOpenMobileHapticsApplePlaybackEventCallback Callback
		) override
		{
			++TransientSubmissionCount;
			LastRequestId = RequestId;
			LastTransientPattern = Pattern;
			PlaybackCallback = MoveTemp(Callback);
			return TransientSubmissionResult;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult PlayContinuousPattern(
			uint64 RequestId,
			const FOpenMobileHapticsAppleContinuousPattern& Pattern,
			FOpenMobileHapticsApplePlaybackEventCallback Callback
		) override
		{
			++ContinuousSubmissionCount;
			LastRequestId = RequestId;
			LastContinuousPattern = Pattern;
			PlaybackCallback = MoveTemp(Callback);
			return ContinuousSubmissionResult;
		}

		virtual EOpenMobileHapticsAppleSubmissionResult StopPattern(
			uint64 RequestId
		) override
		{
			++StopCount;
			LastRequestId = RequestId;
			return StopResult;
		}

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

		virtual void SetEventCallback(
			FOpenMobileHapticsAppleBridgeEventCallback Callback
		) override
		{
			EventCallback = MoveTemp(Callback);
		}

		virtual void Shutdown() override
		{
			++ShutdownCount;
			EventCallback = {};
		}

		void Emit(EOpenMobileHapticsAppleBridgeEvent Event)
		{
			if (EventCallback)
			{
				EventCallback(Event);
			}
		}

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
		EOpenMobileHapticsAppleSubmissionResult StopResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		EOpenMobileHapticsAppleSubmissionResult UpdateResult =
			EOpenMobileHapticsAppleSubmissionResult::Accepted;
		int32 QueryCount = 0;
		int32 CreateEngineCount = 0;
		int32 ShutdownCount = 0;
		int32 TransientSubmissionCount = 0;
		int32 ContinuousSubmissionCount = 0;
		int32 StopCount = 0;
		int32 UpdateCount = 0;
		uint64 LastRequestId = 0;
		FOpenMobileHapticsAppleTransientPattern LastTransientPattern;
		FOpenMobileHapticsAppleContinuousPattern LastContinuousPattern;
		FOpenMobileHapticDynamicParameterUpdate LastUpdate;
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

	Mock->Emit(EOpenMobileHapticsAppleBridgeEvent::EngineStopped);
	Service.Shutdown();
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Shutdown drops queued callbacks"), CallbackCount, 1);
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
			}
		),
		EOpenMobileHapticsAppleSubmissionResult::Accepted
	);
	TestEqual(TEXT("One native pattern is submitted"),
		Mock->TransientSubmissionCount, 1);
	TestEqual(TEXT("Request identity crosses the bridge"),
		Mock->LastRequestId, static_cast<uint64>(42));
	TestEqual(TEXT("All events stay in one bridge call"),
		Mock->LastTransientPattern.StartTimesSeconds.Num(), 2);

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

#endif
