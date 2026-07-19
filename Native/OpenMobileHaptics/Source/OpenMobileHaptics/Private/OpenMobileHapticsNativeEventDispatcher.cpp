#include "OpenMobileHapticsNativeEventDispatcher.h"

#include "Async/Async.h"
#include "Misc/ScopeLock.h"

FOpenMobileHapticsNativeEventDispatcher::
	FOpenMobileHapticsNativeEventDispatcher(FEventHandler InEventHandler)
	: EventHandler(MoveTemp(InEventHandler))
{
}

void FOpenMobileHapticsNativeEventDispatcher::RegisterToken(
	const FOpenMobileHapticsBackendRequestToken& Token
)
{
	if (!Token.IsValid())
	{
		return;
	}
	FScopeLock Lock(&Mutex);
	if (bClosed)
	{
		return;
	}
	FEventStream* Existing = Streams.Find(Token.RequestId);
	if (Existing && Existing->Token == Token)
	{
		return;
	}
	FEventStream Stream;
	Stream.Token = Token;
	Streams.Add(Token.RequestId, MoveTemp(Stream));
}

void FOpenMobileHapticsNativeEventDispatcher::UnregisterToken(
	const FOpenMobileHapticsBackendRequestToken& Token
)
{
	FScopeLock Lock(&Mutex);
	FEventStream* Stream = Streams.Find(Token.RequestId);
	if (Stream && Stream->Token == Token)
	{
		Streams.Remove(Token.RequestId);
	}
}

void FOpenMobileHapticsNativeEventDispatcher::Enqueue(
	FOpenMobileHapticsBackendCallback Callback
)
{
	if (!IsPayloadValid(Callback))
	{
		return;
	}

	bool bScheduleDrain = false;
	{
		FScopeLock Lock(&Mutex);
		FEventStream* Stream = bClosed
			? nullptr
			: Streams.Find(Callback.Token.RequestId);
		if (!Stream
			|| !(Stream->Token == Callback.Token)
			|| Callback.Sequence <= Stream->LastDispatchedSequence
			|| Stream->PendingCallbacks.Contains(Callback.Sequence))
		{
			return;
		}

		if (Stream->PendingCallbacks.Num()
			>= MaximumBufferedCallbacksPerToken)
		{
			uint64 LargestSequence = 0;
			for (const TPair<
				uint64,
				FOpenMobileHapticsBackendCallback
			>& Pair : Stream->PendingCallbacks)
			{
				LargestSequence = FMath::Max(LargestSequence, Pair.Key);
			}
			if (Callback.Sequence >= LargestSequence)
			{
				return;
			}
			Stream->PendingCallbacks.Remove(LargestSequence);
		}

		Stream->PendingCallbacks.Add(
			Callback.Sequence,
			MoveTemp(Callback)
		);
		if (!bDrainScheduled)
		{
			bDrainScheduled = true;
			bScheduleDrain = true;
		}
	}

	if (bScheduleDrain)
	{
		const TSharedRef<
			FOpenMobileHapticsNativeEventDispatcher,
			ESPMode::ThreadSafe
		> Self = AsShared();
		AsyncTask(ENamedThreads::GameThread, [Self]()
		{
			Self->DrainOnGameThread();
		});
	}
}

void FOpenMobileHapticsNativeEventDispatcher::Close()
{
	check(IsInGameThread());
	FScopeLock Lock(&Mutex);
	bClosed = true;
	bDrainScheduled = false;
	Streams.Reset();
	EventHandler = {};
}

void FOpenMobileHapticsNativeEventDispatcher::DrainOnGameThread()
{
	check(IsInGameThread());
	TArray<FOpenMobileHapticsBackendCallback> ReadyCallbacks;
	FEventHandler Handler;
	{
		FScopeLock Lock(&Mutex);
		bDrainScheduled = false;
		if (bClosed || !EventHandler)
		{
			return;
		}
		for (TPair<uint64, FEventStream>& Pair : Streams)
		{
			FOpenMobileHapticsBackendCallback Callback;
			while (TryTakeNext(Pair.Value, Callback))
			{
				ReadyCallbacks.Add(MoveTemp(Callback));
			}
		}
		Handler = EventHandler;
	}

	for (const FOpenMobileHapticsBackendCallback& Callback : ReadyCallbacks)
	{
		Handler(Callback);
	}
}

bool FOpenMobileHapticsNativeEventDispatcher::IsPayloadValid(
	const FOpenMobileHapticsBackendCallback& Callback
)
{
	if (!Callback.Token.IsValid()
		|| Callback.Sequence == 0
		|| Callback.Event.State == EOpenMobileHapticPlaybackState::Invalid
		|| (Callback.Event.Handle.IsValid()
			&& Callback.Event.Handle != Callback.Token.PlaybackHandle))
	{
		return false;
	}
	return !Callback.HasExplicitPredecessor()
		|| Callback.PreviousSequence < Callback.Sequence;
}

bool FOpenMobileHapticsNativeEventDispatcher::TryTakeNext(
	FEventStream& Stream,
	FOpenMobileHapticsBackendCallback& OutCallback
)
{
	TOptional<uint64> SelectedSequence;
	for (const TPair<
		uint64,
		FOpenMobileHapticsBackendCallback
	>& Pair : Stream.PendingCallbacks)
	{
		const FOpenMobileHapticsBackendCallback& Candidate = Pair.Value;
		const bool bReady = Candidate.HasExplicitPredecessor()
			? Candidate.PreviousSequence == Stream.LastDispatchedSequence
			: Candidate.Sequence > Stream.LastDispatchedSequence;
		if (bReady
			&& (!SelectedSequence.IsSet()
				|| Candidate.Sequence < SelectedSequence.GetValue()))
		{
			SelectedSequence = Candidate.Sequence;
		}
	}
	if (!SelectedSequence.IsSet())
	{
		return false;
	}
	Stream.PendingCallbacks.RemoveAndCopyValue(
		SelectedSequence.GetValue(),
		OutCallback
	);
	Stream.LastDispatchedSequence = SelectedSequence.GetValue();
	return true;
}
