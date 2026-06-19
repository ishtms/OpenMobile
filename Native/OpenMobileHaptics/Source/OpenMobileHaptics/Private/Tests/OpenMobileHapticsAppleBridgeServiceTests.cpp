#if WITH_DEV_AUTOMATION_TESTS

#include "Async/TaskGraphInterfaces.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsAppleBridgeService.h"

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

		FOpenMobileHapticsAppleHardwareProbe Probe;
		EOpenMobileHapticsAppleEngineResult CreateEngineResult =
			EOpenMobileHapticsAppleEngineResult::Ready;
		int32 QueryCount = 0;
		int32 CreateEngineCount = 0;
		int32 ShutdownCount = 0;
		FOpenMobileHapticsAppleBridgeEventCallback EventCallback;
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

#endif
