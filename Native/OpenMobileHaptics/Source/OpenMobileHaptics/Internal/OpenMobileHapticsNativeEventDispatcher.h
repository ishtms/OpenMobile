#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileHapticsBackend.h"

class FOpenMobileHapticsNativeEventDispatcher final
	: public TSharedFromThis<
		FOpenMobileHapticsNativeEventDispatcher,
		ESPMode::ThreadSafe
	>
{
public:
	using FEventHandler =
		TFunction<void(const FOpenMobileHapticsBackendCallback&)>;

	/** Keeps the game-thread handler separate from backend callback threads from the moment the dispatcher is created. */
	explicit FOpenMobileHapticsNativeEventDispatcher(
		FEventHandler InEventHandler
	);

	/** Opens an ordered callback stream for one accepted backend token. */
	void RegisterToken(const FOpenMobileHapticsBackendRequestToken& Token);
	/** Removes the token and any buffered late callbacks once playback ownership ends. */
	void UnregisterToken(const FOpenMobileHapticsBackendRequestToken& Token);
	/** Buffers an out-of-thread callback by sequence and schedules one game-thread drain. */
	void Enqueue(FOpenMobileHapticsBackendCallback Callback);
	/** Seals the dispatcher during teardown so queued tasks can't call a dead subsystem. */
	void Close();

private:
	struct FEventStream
	{
		FOpenMobileHapticsBackendRequestToken Token;
		uint64 LastDispatchedSequence = 0;
		TMap<uint64, FOpenMobileHapticsBackendCallback> PendingCallbacks;
	};

	/** Delivers only contiguous callback sequences, leaving gaps buffered till their earlier event arrives. */
	void DrainOnGameThread();
	/** Rejects malformed tokens and payloads before they can disturb stream ordering. */
	static bool IsPayloadValid(
		const FOpenMobileHapticsBackendCallback& Callback
	);
	/** Removes the next contiguous event only, a later sequence has to wait. */
	static bool TryTakeNext(
		FEventStream& Stream,
		FOpenMobileHapticsBackendCallback& OutCallback
	);

	static constexpr int32 MaximumBufferedCallbacksPerToken = 16;
	FCriticalSection Mutex;
	TMap<uint64, FEventStream> Streams;
	FEventHandler EventHandler;
	bool bDrainScheduled = false;
	bool bClosed = false;
};
