#include "Misc/AutomationTest.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "OpenMobileHapticsNativeEventDispatcher.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsNativeEventDispatcherTest,
	"OpenMobile.Haptics.Scheduling.NativeEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsNativeEventDispatcherTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	TArray<FOpenMobileHapticsBackendCallback> Observed;
	bool bAllEventsOnGameThread = true;
	const TSharedRef<
		FOpenMobileHapticsNativeEventDispatcher,
		ESPMode::ThreadSafe
	> Dispatcher = MakeShared<
		FOpenMobileHapticsNativeEventDispatcher,
		ESPMode::ThreadSafe
	>([&Observed, &bAllEventsOnGameThread](
		const FOpenMobileHapticsBackendCallback& Callback
	)
	{
		bAllEventsOnGameThread &= IsInGameThread();
		Observed.Add(Callback);
	});

	auto MakeToken = [](uint64 RequestId)
	{
		FOpenMobileHapticsBackendRequestToken Token;
		Token.RegistryGeneration = 1;
		Token.RequestId = RequestId;
		Token.BackendName = TEXT("NativeEventTest");
		Token.PlaybackHandle.Id = FGuid::NewGuid();
		return Token;
	};
	auto MakeCallback = [](
		const FOpenMobileHapticsBackendRequestToken& Token,
		uint64 Sequence,
		uint64 PreviousSequence,
		EOpenMobileHapticPlaybackState State
	)
	{
		FOpenMobileHapticsBackendCallback Callback;
		Callback.Token = Token;
		Callback.Sequence = Sequence;
		Callback.PreviousSequence = PreviousSequence;
		Callback.Event.Handle = Token.PlaybackHandle;
		Callback.Event.State = State;
		Callback.Event.TimestampSeconds = static_cast<double>(Sequence);
		return Callback;
	};

	const FOpenMobileHapticsBackendRequestToken FirstToken = MakeToken(1);
	const FOpenMobileHapticsBackendRequestToken SecondToken = MakeToken(2);
	Dispatcher->RegisterToken(FirstToken);
	Dispatcher->RegisterToken(SecondToken);

	TFuture<void> LaterFirstEvent = Async(
		EAsyncExecution::ThreadPool,
		[Dispatcher, Callback = MakeCallback(
			FirstToken,
			2,
			1,
			EOpenMobileHapticPlaybackState::Completed
		)]() mutable
		{
			Dispatcher->Enqueue(MoveTemp(Callback));
		}
	);
	LaterFirstEvent.Wait();
	TFuture<void> EarlierFirstEvent = Async(
		EAsyncExecution::ThreadPool,
		[Dispatcher, Callback = MakeCallback(
			FirstToken,
			1,
			0,
			EOpenMobileHapticPlaybackState::Started
		)]() mutable
		{
			Dispatcher->Enqueue(MoveTemp(Callback));
		}
	);
	TFuture<void> ConcurrentSecondEvent = Async(
		EAsyncExecution::ThreadPool,
		[Dispatcher, Callback = MakeCallback(
			SecondToken,
			1,
			0,
			EOpenMobileHapticPlaybackState::Completed
		)]() mutable
		{
			Dispatcher->Enqueue(MoveTemp(Callback));
		}
	);
	EarlierFirstEvent.Wait();
	ConcurrentSecondEvent.Wait();

	TestEqual(TEXT("A stalled game thread receives nothing early"),
		Observed.Num(), 0);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Both handles deliver their normalized events"),
		Observed.Num(), 3);
	TestTrue(TEXT("Every native callback returns on the game thread"),
		bAllEventsOnGameThread);

	int32 FirstStartIndex = INDEX_NONE;
	int32 FirstCompletionIndex = INDEX_NONE;
	int32 SecondIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Observed.Num(); ++Index)
	{
		const FOpenMobileHapticsBackendCallback& Callback = Observed[Index];
		if (Callback.Token.RequestId == FirstToken.RequestId)
		{
			if (Callback.Sequence == 1)
			{
				FirstStartIndex = Index;
			}
			else if (Callback.Sequence == 2)
			{
				FirstCompletionIndex = Index;
			}
		}
		else if (Callback.Token.RequestId == SecondToken.RequestId)
		{
			SecondIndex = Index;
		}
	}
	TestTrue(TEXT("The first handle starts before it completes"),
		FirstStartIndex != INDEX_NONE
			&& FirstCompletionIndex > FirstStartIndex);
	TestTrue(TEXT("The concurrent handle remains independent"),
		SecondIndex != INDEX_NONE);

	Dispatcher->Enqueue(MakeCallback(
		FirstToken,
		2,
		1,
		EOpenMobileHapticPlaybackState::Completed
	));
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Duplicate native callbacks are ignored"),
		Observed.Num(), 3);

	const FOpenMobileHapticsBackendRequestToken TeardownToken = MakeToken(3);
	Dispatcher->RegisterToken(TeardownToken);
	Dispatcher->Enqueue(MakeCallback(
		TeardownToken,
		2,
		1,
		EOpenMobileHapticPlaybackState::Completed
	));
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("A missing predecessor remains buffered"),
		Observed.Num(), 3);
	Dispatcher->Close();

	TFuture<void> CallbackAfterClose = Async(
		EAsyncExecution::ThreadPool,
		[Dispatcher, Callback = MakeCallback(
			TeardownToken,
			1,
			0,
			EOpenMobileHapticPlaybackState::Started
		)]() mutable
		{
			Dispatcher->Enqueue(MoveTemp(Callback));
		}
	);
	CallbackAfterClose.Wait();
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(
		ENamedThreads::GameThread
	);
	TestEqual(TEXT("Teardown drops buffered and late callbacks"),
		Observed.Num(), 3);
	return true;
}

#endif
