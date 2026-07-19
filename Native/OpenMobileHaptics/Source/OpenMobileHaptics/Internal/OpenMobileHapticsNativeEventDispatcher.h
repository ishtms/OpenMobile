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

	explicit FOpenMobileHapticsNativeEventDispatcher(
		FEventHandler InEventHandler
	);

	void RegisterToken(const FOpenMobileHapticsBackendRequestToken& Token);
	void UnregisterToken(const FOpenMobileHapticsBackendRequestToken& Token);
	void Enqueue(FOpenMobileHapticsBackendCallback Callback);
	void Close();

private:
	struct FEventStream
	{
		FOpenMobileHapticsBackendRequestToken Token;
		uint64 LastDispatchedSequence = 0;
		TMap<uint64, FOpenMobileHapticsBackendCallback> PendingCallbacks;
	};

	void DrainOnGameThread();
	static bool IsPayloadValid(
		const FOpenMobileHapticsBackendCallback& Callback
	);
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
