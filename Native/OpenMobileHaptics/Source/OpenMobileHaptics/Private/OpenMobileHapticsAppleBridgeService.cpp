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

FOpenMobileHapticsApplePlaybackEventCallback
FOpenMobileHapticsAppleBridgeService::RelayPlaybackCallback(
	FOpenMobileHapticsApplePlaybackEventCallback Callback
)
{
	const TSharedRef<
		FOpenMobileHapticsApplePlaybackEventCallback,
		ESPMode::ThreadSafe
	> SharedCallback = MakeShared<
		FOpenMobileHapticsApplePlaybackEventCallback,
		ESPMode::ThreadSafe
	>(MoveTemp(Callback));
	TWeakPtr<FCallbackState, ESPMode::ThreadSafe> WeakState(CallbackState);
	return [WeakState, SharedCallback](
		EOpenMobileHapticsApplePlaybackEvent Event
	)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakState, SharedCallback, Event]()
		{
			const TSharedPtr<FCallbackState, ESPMode::ThreadSafe> State =
				WeakState.Pin();
			if (!State)
			{
				return;
			}
			{
				FScopeLock StateLock(&State->Mutex);
				if (State->bShuttingDown)
				{
					return;
				}
			}
			if (*SharedCallback)
			{
				(*SharedCallback)(Event);
			}
		});
	};
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

void FOpenMobileHapticsAppleBridgeService::InvalidateEngine()
{
	FScopeLock Lock(&Mutex);
	bEngineReady = false;
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
FOpenMobileHapticsAppleBridgeService::PrepareSemanticGenerators(
	double IdleLifetimeSeconds
)
{
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PrepareSemanticGenerators(IdleLifetimeSeconds);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PrepareTransientPattern(
	uint64 ResourceId,
	const FOpenMobileHapticsAppleTransientPattern& Pattern,
	int64 EstimatedBytes,
	const FOpenMobileHapticsPreparedResourceLimits& Limits
)
{
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PrepareTransientPattern(
			ResourceId,
			Pattern,
			EstimatedBytes,
			Limits
		);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PrepareContinuousPattern(
	uint64 ResourceId,
	const FOpenMobileHapticsAppleContinuousPattern& Pattern,
	int64 EstimatedBytes,
	const FOpenMobileHapticsPreparedResourceLimits& Limits
)
{
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PrepareContinuousPattern(
			ResourceId,
			Pattern,
			EstimatedBytes,
			Limits
		);
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

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlayScheduledSemantic(
	uint64 RequestId,
	EOpenMobileHapticsSemanticBehavior Behavior,
	float Intensity,
	const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
	FOpenMobileHapticsApplePlaybackEventCallback Callback
)
{
	FOpenMobileHapticsApplePlaybackEventCallback RelayedCallback =
		RelayPlaybackCallback(MoveTemp(Callback));
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PlayScheduledSemantic(
			RequestId,
			Behavior,
			Intensity,
			Schedule,
			MoveTemp(RelayedCallback)
		);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlayScheduledSystemVibration(
	uint64 RequestId,
	const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
	FOpenMobileHapticsApplePlaybackEventCallback Callback
)
{
	FOpenMobileHapticsApplePlaybackEventCallback RelayedCallback =
		RelayPlaybackCallback(MoveTemp(Callback));
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PlayScheduledSystemVibration(
			RequestId,
			Schedule,
			MoveTemp(RelayedCallback)
		);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlayTransientPattern(
	uint64 RequestId,
	const FOpenMobileHapticsAppleTransientPattern& Pattern,
	FOpenMobileHapticsApplePlaybackEventCallback Callback,
	const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
	uint64 PreparedResourceId
)
{
	const TSharedRef<
		FOpenMobileHapticsApplePlaybackEventCallback,
		ESPMode::ThreadSafe
	> SharedCallback = MakeShared<
		FOpenMobileHapticsApplePlaybackEventCallback,
		ESPMode::ThreadSafe
	>(MoveTemp(Callback));
	TWeakPtr<FCallbackState, ESPMode::ThreadSafe> WeakState(CallbackState);
	FScopeLock Lock(&Mutex);
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	return Bridge->PlayTransientPattern(
		RequestId,
		Pattern,
		[WeakState, SharedCallback](
			EOpenMobileHapticsApplePlaybackEvent Event
		)
		{
			AsyncTask(
				ENamedThreads::GameThread,
				[WeakState, SharedCallback, Event]()
				{
					const TSharedPtr<
						FCallbackState,
						ESPMode::ThreadSafe
					> State = WeakState.Pin();
					if (!State)
					{
						return;
					}
					{
						FScopeLock StateLock(&State->Mutex);
						if (State->bShuttingDown)
						{
							return;
						}
					}
					if (*SharedCallback)
					{
						(*SharedCallback)(Event);
					}
				}
			);
		},
		InitialParameters,
		PreparedResourceId
	);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlayContinuousPattern(
	uint64 RequestId,
	const FOpenMobileHapticsAppleContinuousPattern& Pattern,
	FOpenMobileHapticsApplePlaybackEventCallback Callback,
	const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
	uint64 PreparedResourceId
)
{
	const TSharedRef<
		FOpenMobileHapticsApplePlaybackEventCallback,
		ESPMode::ThreadSafe
	> SharedCallback = MakeShared<
		FOpenMobileHapticsApplePlaybackEventCallback,
		ESPMode::ThreadSafe
	>(MoveTemp(Callback));
	TWeakPtr<FCallbackState, ESPMode::ThreadSafe> WeakState(CallbackState);
	FScopeLock Lock(&Mutex);
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	return Bridge->PlayContinuousPattern(
		RequestId,
		Pattern,
		[WeakState, SharedCallback](
			EOpenMobileHapticsApplePlaybackEvent Event
		)
		{
			AsyncTask(
				ENamedThreads::GameThread,
				[WeakState, SharedCallback, Event]()
				{
					const TSharedPtr<
						FCallbackState,
						ESPMode::ThreadSafe
					> State = WeakState.Pin();
					if (!State)
					{
						return;
					}
					{
						FScopeLock StateLock(&State->Mutex);
						if (State->bShuttingDown)
						{
							return;
						}
					}
					if (*SharedCallback)
					{
						(*SharedCallback)(Event);
					}
				}
			);
		},
		InitialParameters,
		PreparedResourceId
	);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlayScheduledTransientPattern(
	uint64 RequestId,
	const FOpenMobileHapticsAppleTransientPattern& Pattern,
	const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
	FOpenMobileHapticsApplePlaybackEventCallback Callback,
	const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
	uint64 PreparedResourceId
)
{
	FOpenMobileHapticsApplePlaybackEventCallback RelayedCallback =
		RelayPlaybackCallback(MoveTemp(Callback));
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PlayScheduledTransientPattern(
			RequestId,
			Pattern,
			Schedule,
			MoveTemp(RelayedCallback),
			InitialParameters,
			PreparedResourceId
		);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlayScheduledContinuousPattern(
	uint64 RequestId,
	const FOpenMobileHapticsAppleContinuousPattern& Pattern,
	const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
	FOpenMobileHapticsApplePlaybackEventCallback Callback,
	const FOpenMobileHapticDynamicParameterUpdate* InitialParameters,
	uint64 PreparedResourceId
)
{
	FOpenMobileHapticsApplePlaybackEventCallback RelayedCallback =
		RelayPlaybackCallback(MoveTemp(Callback));
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PlayScheduledContinuousPattern(
			RequestId,
			Pattern,
			Schedule,
			MoveTemp(RelayedCallback),
			InitialParameters,
			PreparedResourceId
		);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlayAHAPPattern(
	uint64 RequestId,
	const FOpenMobileHapticsAppleAHAPPattern& Pattern,
	FOpenMobileHapticsApplePlaybackEventCallback Callback
)
{
	const TSharedRef<
		FOpenMobileHapticsApplePlaybackEventCallback,
		ESPMode::ThreadSafe
	> SharedCallback = MakeShared<
		FOpenMobileHapticsApplePlaybackEventCallback,
		ESPMode::ThreadSafe
	>(MoveTemp(Callback));
	TWeakPtr<FCallbackState, ESPMode::ThreadSafe> WeakState(CallbackState);
	FScopeLock Lock(&Mutex);
	if (bShuttingDown)
	{
		return EOpenMobileHapticsAppleSubmissionResult::ShuttingDown;
	}
	return Bridge->PlayAHAPPattern(
		RequestId,
		Pattern,
		[WeakState, SharedCallback](
			EOpenMobileHapticsApplePlaybackEvent Event
		)
		{
			AsyncTask(
				ENamedThreads::GameThread,
				[WeakState, SharedCallback, Event]()
				{
					const TSharedPtr<
						FCallbackState,
						ESPMode::ThreadSafe
					> State = WeakState.Pin();
					if (!State)
					{
						return;
					}
					{
						FScopeLock StateLock(&State->Mutex);
						if (State->bShuttingDown)
						{
							return;
						}
					}
					if (*SharedCallback)
					{
						(*SharedCallback)(Event);
					}
				}
			);
		}
	);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::PlayScheduledAHAPPattern(
	uint64 RequestId,
	const FOpenMobileHapticsAppleAHAPPattern& Pattern,
	const FOpenMobileHapticsApplePlaybackSchedule& Schedule,
	FOpenMobileHapticsApplePlaybackEventCallback Callback
)
{
	FOpenMobileHapticsApplePlaybackEventCallback RelayedCallback =
		RelayPlaybackCallback(MoveTemp(Callback));
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->PlayScheduledAHAPPattern(
			RequestId,
			Pattern,
			Schedule,
			MoveTemp(RelayedCallback)
		);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::StopPattern(uint64 RequestId)
{
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->StopPattern(RequestId);
}

EOpenMobileHapticsAppleSubmissionResult
FOpenMobileHapticsAppleBridgeService::UpdatePattern(
	uint64 RequestId,
	const FOpenMobileHapticDynamicParameterUpdate& Update
)
{
	FScopeLock Lock(&Mutex);
	return bShuttingDown
		? EOpenMobileHapticsAppleSubmissionResult::ShuttingDown
		: Bridge->UpdatePattern(RequestId, Update);
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

void FOpenMobileHapticsAppleBridgeService::ReleasePreparedResources()
{
	FScopeLock Lock(&Mutex);
	if (!bShuttingDown)
	{
		Bridge->ReleasePreparedResources();
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
