#include "OpenMobileHapticsAppleBridgeService.h"

#include "Async/Async.h"
#include "Misc/ScopeLock.h"

struct FOpenMobileHapticsAppleBridgeService::FCallbackState
{
	FCriticalSection Mutex;
	TSharedPtr<
		FOpenMobileHapticsAppleBridgeEventCallback,
		ESPMode::ThreadSafe
	> Callback;
	bool bShuttingDown = false;
};

FOpenMobileHapticsAppleBridgeService::FOpenMobileHapticsAppleBridgeService(
	TUniquePtr<IOpenMobileHapticsAppleBridge> InBridge
)
	: Bridge(MoveTemp(InBridge))
	, CallbackState(MakeShared<FCallbackState, ESPMode::ThreadSafe>())
{
	check(Bridge);
	TWeakPtr<FCallbackState, ESPMode::ThreadSafe> WeakState(CallbackState);
	Bridge->SetEventCallback(
		[WeakState](EOpenMobileHapticsAppleBridgeEvent Event)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakState, Event]()
			{
				const TSharedPtr<FCallbackState, ESPMode::ThreadSafe> State =
					WeakState.Pin();
				if (!State)
				{
					return;
				}

				TSharedPtr<
					FOpenMobileHapticsAppleBridgeEventCallback,
					ESPMode::ThreadSafe
				> Callback;
				{
					FScopeLock Lock(&State->Mutex);
					if (State->bShuttingDown)
					{
						return;
					}
					Callback = State->Callback;
				}
				if (Callback && *Callback)
				{
					(*Callback)(Event);
				}
			});
		}
	);
}

FOpenMobileHapticsAppleBridgeService::~FOpenMobileHapticsAppleBridgeService()
{
	Shutdown();
}

FOpenMobileHapticsAppleHardwareProbe
FOpenMobileHapticsAppleBridgeService::GetHardwareProbeLocked()
{
	if (StableProbe.IsSet())
	{
		return StableProbe.GetValue();
	}

	const FOpenMobileHapticsAppleHardwareProbe Probe = Bridge->QueryHardware();
	if (Probe.RichHaptics
		!= EOpenMobileHapticsAppleHardwareState::TemporarilyUnavailable)
	{
		StableProbe = Probe;
	}
	return Probe;
}

FOpenMobileHapticsAppleHardwareProbe
FOpenMobileHapticsAppleBridgeService::GetHardwareProbe()
{
	FScopeLock Lock(&Mutex);
	if (bShuttingDown)
	{
		return {};
	}
	return GetHardwareProbeLocked();
}

void FOpenMobileHapticsAppleBridgeService::InvalidateHardwareProbe()
{
	FScopeLock Lock(&Mutex);
	StableProbe.Reset();
}

EOpenMobileHapticsAppleEngineResult
FOpenMobileHapticsAppleBridgeService::EnsureEngine()
{
	FScopeLock Lock(&Mutex);
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleEngineResult::ShuttingDown;
	}
	if (bEngineReady)
	{
		return EOpenMobileHapticsAppleEngineResult::Ready;
	}

	switch (GetHardwareProbeLocked().RichHaptics)
	{
	case EOpenMobileHapticsAppleHardwareState::Unsupported:
		return EOpenMobileHapticsAppleEngineResult::UnsupportedHardware;
	case EOpenMobileHapticsAppleHardwareState::TemporarilyUnavailable:
		return EOpenMobileHapticsAppleEngineResult::TemporarilyUnavailable;
	case EOpenMobileHapticsAppleHardwareState::Supported:
		break;
	}

	const EOpenMobileHapticsAppleEngineResult Result = Bridge->CreateEngine();
	if (Result == EOpenMobileHapticsAppleEngineResult::Ready)
	{
		bEngineReady = true;
	}
	return Result;
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlaySemantic(
	EOpenMobileHapticsSemanticBehavior Behavior,
	float Intensity
)
{
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PlaySemantic(Behavior, Intensity);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlaySystemVibration()
{
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PlaySystemVibration();
}

void FOpenMobileHapticsAppleBridgeService::SetEventCallback(
	FOpenMobileHapticsAppleBridgeEventCallback Callback
)
{
	FScopeLock Lock(&CallbackState->Mutex);
	if (CallbackState->bShuttingDown)
	{
		return;
	}
	if (Callback)
	{
		CallbackState->Callback = MakeShared<
			FOpenMobileHapticsAppleBridgeEventCallback,
			ESPMode::ThreadSafe
		>(MoveTemp(Callback));
	}
	else
	{
		CallbackState->Callback.Reset();
	}
}

void FOpenMobileHapticsAppleBridgeService::Shutdown()
{
	{
		FScopeLock CallbackLock(&CallbackState->Mutex);
		CallbackState->bShuttingDown = true;
		CallbackState->Callback.Reset();
	}

	FScopeLock Lock(&Mutex);
	if (bShuttingDown)
	{
		return;
	}
	bShuttingDown = true;
	Bridge->SetEventCallback({});
	Bridge->Shutdown();
	bEngineReady = false;
	StableProbe.Reset();
}
