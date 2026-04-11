#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/GameInstance.h"
#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileAdsAsyncAction.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OpenMobileAdsProviderContractTests
{
	class FMockProvider final : public IOpenMobileAdsProvider
	{
	public:
		explicit FMockProvider(FName InName, bool bInSupported = true)
			: Name(InName)
			, bSupported(bInSupported)
		{
			FOpenMobileAdFormatCapabilities Rewarded;
			Rewarded.Format = EOpenMobileAdFormat::Rewarded;
			Rewarded.bCanLoad = true;
			Rewarded.bCanShow = true;
			Rewarded.bReportsDismiss = true;
			Rewarded.bReportsReward = true;
			Capabilities.Provider = Name;
			Capabilities.Formats.Add(Rewarded);
		}

		virtual FName GetProviderName() const override { return Name; }
		virtual bool IsSupported() const override { return bSupported; }
		virtual FOpenMobileAdsProviderCapabilities GetCapabilities() const override
		{
			return Capabilities;
		}

		virtual bool Load(
			const FOpenMobileAdsLoadRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			LastLoadRequest = Request;
			LoadSink = EventSink;
			return true;
		}

		virtual bool Show(
			const FOpenMobileAdsShowRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			LastShowRequest = Request;
			ShowSink = EventSink;
			return true;
		}

		virtual bool Destroy(
			const FOpenMobileAdsDestroyRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			LastDestroyRequest = Request;
			DestroySink = EventSink;
			return true;
		}

		virtual bool RequestAndShowRewardedAd(
			FOpenMobileRewardedAdCallbacks&& Callbacks,
			FOpenMobileError& OutError
		) override
		{
			return false;
		}

		virtual void Cancel(FGuid RequestId) override
		{
			CancelledRequests.Add(RequestId);
		}

		FName Name;
		bool bSupported = true;
		FOpenMobileAdsProviderCapabilities Capabilities;
		FOpenMobileAdsLoadRequest LastLoadRequest;
		FOpenMobileAdsShowRequest LastShowRequest;
		FOpenMobileAdsDestroyRequest LastDestroyRequest;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> LoadSink;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShowSink;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> DestroySink;
		TArray<FGuid> CancelledRequests;
	};

	class FScopedProviderRegistration
	{
	public:
		explicit FScopedProviderRegistration(IOpenMobileAdsProvider& InProvider)
			: Provider(InProvider)
		{
			IModularFeatures::Get().RegisterModularFeature(
				IOpenMobileAdsProvider::GetModularFeatureName(),
				&Provider
			);
		}

		~FScopedProviderRegistration()
		{
			Unregister();
		}

		void Unregister()
		{
			if (!bRegistered)
			{
				return;
			}
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsProvider::GetModularFeatureName(),
				&Provider
			);
			bRegistered = false;
		}

	private:
		IOpenMobileAdsProvider& Provider;
		bool bRegistered = true;
	};

	class FScopedSettings
	{
	public:
		FScopedSettings()
		{
			Settings = GetMutableDefault<UOpenMobileAdsSettings>();
			SavedProvider = Settings->PreferredProvider;
			SavedPlacements = Settings->Placements;
		}

		~FScopedSettings()
		{
			Settings->PreferredProvider = SavedProvider;
			Settings->Placements = MoveTemp(SavedPlacements);
		}

		UOpenMobileAdsSettings* Settings = nullptr;

	private:
		FName SavedProvider;
		TArray<FOpenMobileAdsPlacementSettings> SavedPlacements;
	};

	void DrainGameThreadTasks()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsProviderSelectionContractTest,
	"OpenMobile.Ads.ProviderContract.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsProviderSelectionContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FMockProvider Alpha(TEXT("AlphaAds"));
	FMockProvider Beta(TEXT("BetaAds"));
	TArray<IOpenMobileAdsProvider*> Providers{&Beta, &Alpha};

	const FOpenMobileAdsProviderSelection Conflict =
		FOpenMobileAdsProviderResolver::Resolve(Providers, NAME_None);
	TestNull(TEXT("Conflicts do not pick by registration order"), Conflict.Provider);
	TestEqual(TEXT("Conflict is typed"), Conflict.Error.Code, EOpenMobileAdsErrorCode::ProviderConflict);

	const FOpenMobileAdsProviderSelection Preferred =
		FOpenMobileAdsProviderResolver::Resolve(Providers, TEXT("alphaads"));
	TestTrue(TEXT("Provider names match without case sensitivity"), Preferred.Provider == &Alpha);
	TestFalse(TEXT("Successful selection has no error"), Preferred.Error.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsProviderErrorContractTest,
	"OpenMobile.Ads.ProviderContract.Errors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsProviderErrorContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("UnsupportedInterstitial");
	Placement.Format = EOpenMobileAdFormat::Interstitial;
	Placement.Android.AdUnitId = TEXT("android-interstitial");
	Placement.IOS.AdUnitId = TEXT("ios-interstitial");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);

	TestEqual(
		TEXT("Empty placement errors are typed"),
		Subsystem->LoadAd(NAME_None).Error.Code,
		EOpenMobileAdsErrorCode::InvalidPlacement
	);
	TestEqual(
		TEXT("Unknown placement errors are typed"),
		Subsystem->LoadAd(TEXT("Unknown")).Error.Code,
		EOpenMobileAdsErrorCode::UnknownPlacement
	);
	TestEqual(
		TEXT("Unsupported format errors are typed"),
		Subsystem->LoadAd(TEXT("UnsupportedInterstitial")).Error.Code,
		EOpenMobileAdsErrorCode::UnsupportedFormat
	);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsProviderEventContractTest,
	"OpenMobile.Ads.ProviderContract.EventsAndTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsProviderEventContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ContinueReward");
	Placement.Format = EOpenMobileAdFormat::Rewarded;
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TestNotNull(TEXT("Subsystem can be created"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	TArray<FOpenMobileAdsEvent> Events;
	bool bEveryCallbackWasOnGameThread = true;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events, &bEveryCallbackWasOnGameThread](const FOpenMobileAdsEvent& Event)
		{
			bEveryCallbackWasOnGameThread &= IsInGameThread();
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult LoadResult =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestTrue(TEXT("Configured load is accepted"), LoadResult.bAccepted);
	TestTrue(TEXT("Provider receives a request ID"), Provider.LastLoadRequest.RequestId.IsValid());
	TestEqual(TEXT("Provider receives the placement"), Provider.LastLoadRequest.Placement.Placement, FName(TEXT("ContinueReward")));
	TestTrue(TEXT("Provider receives an event sink"), Provider.LoadSink.IsValid());
	if (!Provider.LoadSink.IsValid())
	{
		AddError(FString::Printf(
			TEXT("Load was rejected: %s"),
			*LoadResult.Error.Explanation
		));
		Subsystem->Deinitialize();
		Subsystem->OnNativeAdsEvent().Remove(EventHandle);
		return false;
	}

	TFuture<void> Callback = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		FOpenMobileAdsEvent PrematureDismiss;
		PrematureDismiss.Type = EOpenMobileAdsEventType::Dismissed;
		Provider.LoadSink->Submit(PrematureDismiss);
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Provider.LoadSink->Submit(Loaded);
	});
	Callback.Wait();
	DrainGameThreadTasks();

	TestTrue(TEXT("Off-thread callbacks are marshaled to the game thread"), bEveryCallbackWasOnGameThread);
	TestTrue(TEXT("Placement becomes ready"), Subsystem->IsReady(TEXT("ContinueReward")));
	TestEqual(TEXT("Load emits started then loaded"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		TestEqual(TEXT("First event is load started"), Events[0].Type, EOpenMobileAdsEventType::LoadStarted);
		TestEqual(TEXT("Second event is loaded"), Events[1].Type, EOpenMobileAdsEventType::Loaded);
		TestTrue(TEXT("Event sequence is increasing"), Events[0].Sequence < Events[1].Sequence);
	}

	FOpenMobileAdsEvent DuplicateLoaded;
	DuplicateLoaded.Type = EOpenMobileAdsEventType::Loaded;
	Provider.LoadSink->Submit(DuplicateLoaded);
	DrainGameThreadTasks();
	TestEqual(TEXT("Duplicate terminal callbacks are ignored"), Events.Num(), 2);

	const FOpenMobileAdsOperationResult ShowResult =
		Subsystem->ShowAd(TEXT("ContinueReward"));
	TestTrue(TEXT("Ready placements can be shown"), ShowResult.bAccepted);
	TestTrue(TEXT("Provider receives a show sink"), Provider.ShowSink.IsValid());
	if (!Provider.ShowSink.IsValid())
	{
		Subsystem->Deinitialize();
		Subsystem->OnNativeAdsEvent().Remove(EventHandle);
		return false;
	}

	FOpenMobileAdsEvent Shown;
	Shown.Type = EOpenMobileAdsEventType::Shown;
	Provider.ShowSink->Submit(Shown);
	TFuture<void> ClickCallback = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		FOpenMobileAdsEvent Clicked;
		Clicked.Type = EOpenMobileAdsEventType::Clicked;
		Provider.ShowSink->Submit(Clicked);
	});
	TFuture<void> RevenueCallback = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		FOpenMobileAdsEvent Revenue;
		Revenue.Type = EOpenMobileAdsEventType::RevenuePaid;
		Provider.ShowSink->Submit(Revenue);
	});
	ClickCallback.Wait();
	RevenueCallback.Wait();
	FOpenMobileAdsEvent Reward;
	Reward.Type = EOpenMobileAdsEventType::RewardEarned;
	Reward.bHasReward = true;
	Reward.Reward.Amount = 10;
	Provider.ShowSink->Submit(Reward);
	Provider.ShowSink->Submit(Reward);
	FOpenMobileAdsEvent Dismissed;
	Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
	Provider.ShowSink->Submit(Dismissed);
	FOpenMobileAdsEvent LateClick;
	LateClick.Type = EOpenMobileAdsEventType::Clicked;
	Provider.ShowSink->Submit(LateClick);
	FOpenMobileAdsEvent LateRevenue;
	LateRevenue.Type = EOpenMobileAdsEventType::RevenuePaid;
	Provider.ShowSink->Submit(LateRevenue);
	DrainGameThreadTasks();

	TestEqual(TEXT("Duplicate rewards are ignored"), Events.Num(), 8);
	if (Events.Num() == 8)
	{
		TestEqual(TEXT("Show acceptance precedes provider events"), Events[2].Type, EOpenMobileAdsEventType::ShowAccepted);
		TestEqual(TEXT("Shown precedes reward"), Events[3].Type, EOpenMobileAdsEventType::Shown);
		TestEqual(TEXT("Reward precedes dismissal"), Events[6].Type, EOpenMobileAdsEventType::RewardEarned);
		TestEqual(TEXT("Dismissal is terminal"), Events[7].Type, EOpenMobileAdsEventType::Dismissed);
		for (int32 Index = 1; Index < Events.Num(); ++Index)
		{
			TestTrue(
				TEXT("Concurrent callbacks retain one total event order"),
				Events[Index - 1].Sequence < Events[Index].Sequence
			);
		}
	}
	TestEqual(
		TEXT("Dismissal restores idle state"),
		Subsystem->GetPlacementStatus(TEXT("ContinueReward")).State,
		EOpenMobileAdPlacementState::Idle
	);

	Subsystem->Deinitialize();
	FOpenMobileAdsEvent LateFailure;
	LateFailure.Type = EOpenMobileAdsEventType::LoadFailed;
	Provider.LoadSink->Submit(LateFailure);
	DrainGameThreadTasks();
	TestEqual(TEXT("Post-destroy callbacks are ignored"), Events.Num(), 8);
	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsProviderUnregistrationContractTest,
	"OpenMobile.Ads.ProviderContract.Unregistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsProviderUnregistrationContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ContinueReward");
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	const FOpenMobileAdsOperationResult LoadResult =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestTrue(TEXT("Load begins before unregistration"), LoadResult.bAccepted);
	DrainGameThreadTasks();

	AddExpectedError(
		TEXT("The ads provider was unregistered during an active placement operation."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Registration.Unregister();
	DrainGameThreadTasks();
	const FOpenMobileAdsPlacementStatus Status =
		Subsystem->GetPlacementStatus(TEXT("ContinueReward"));
	TestEqual(TEXT("Unregistration fails active state"), Status.State, EOpenMobileAdPlacementState::Failed);
	TestEqual(TEXT("Unregistration has a typed error"), Status.LastError.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);

	if (Provider.LoadSink)
	{
		FOpenMobileAdsEvent LateLoaded;
		LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
		Provider.LoadSink->Submit(LateLoaded);
		DrainGameThreadTasks();
	}
	TestFalse(TEXT("Late load does not recover an unregistered provider"), Subsystem->IsReady(TEXT("ContinueReward")));
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRequestCancellationContractTest,
	"OpenMobile.Ads.ProviderContract.Cancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRequestCancellationContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("CancelableReward");
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult LoadResult =
		Subsystem->LoadAd(TEXT("CancelableReward"));
	TestTrue(TEXT("Load begins before cancellation"), LoadResult.bAccepted);
	DrainGameThreadTasks();

	const FOpenMobileAdsOperationResult CancelResult =
		Subsystem->CancelRequest(LoadResult.RequestId);
	TestTrue(TEXT("Active request cancellation is accepted"), CancelResult.bAccepted);
	DrainGameThreadTasks();
	TestEqual(TEXT("Provider cancellation runs once"), Provider.CancelledRequests.Num(), 1);
	if (Provider.CancelledRequests.Num() == 1)
	{
		TestEqual(TEXT("Provider receives the active request ID"), Provider.CancelledRequests[0], LoadResult.RequestId);
	}
	TestEqual(TEXT("Cancellation emits one terminal event"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		TestEqual(TEXT("Cancellation uses the failed terminal event"), Events[1].Type, EOpenMobileAdsEventType::Failed);
		TestEqual(TEXT("Cancellation has a typed error"), Events[1].Error.Code, EOpenMobileAdsErrorCode::Cancelled);
	}
	TestEqual(
		TEXT("Cancelled load returns placement to idle"),
		Subsystem->GetPlacementStatus(TEXT("CancelableReward")).State,
		EOpenMobileAdPlacementState::Idle
	);

	FOpenMobileAdsEvent LateLoaded;
	LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
	Provider.LoadSink->Submit(LateLoaded);
	DrainGameThreadTasks();
	TestEqual(TEXT("Late callback after cancellation is ignored"), Events.Num(), 2);
	TestFalse(TEXT("Late callback cannot restore readiness"), Subsystem->IsReady(TEXT("CancelableReward")));

	TestFalse(
		TEXT("Completed request cannot be cancelled again"),
		Subsystem->CancelRequest(LoadResult.RequestId).bAccepted
	);
	const FOpenMobileAdsOperationResult TeardownLoad =
		Subsystem->LoadAd(TEXT("CancelableReward"));
	TestTrue(TEXT("A new load can begin after cancellation"), TeardownLoad.bAccepted);
	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	TestEqual(TEXT("Subsystem teardown cancels active provider work"), Provider.CancelledRequests.Num(), 2);
	if (Provider.CancelledRequests.Num() == 2)
	{
		TestEqual(TEXT("Teardown cancels the current request"), Provider.CancelledRequests[1], TeardownLoad.RequestId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsDestroyAllFailureContractTest,
	"OpenMobile.Ads.ProviderContract.DestroyAllFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsDestroyAllFailureContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult DestroyResult = Subsystem->DestroyAllAds();
	TestTrue(TEXT("Service-wide destroy starts"), DestroyResult.bAccepted);
	TestTrue(TEXT("Provider receives a service-wide destroy sink"), Provider.DestroySink.IsValid());
	if (Provider.DestroySink)
	{
		FOpenMobileAdsEvent Failed;
		Failed.Type = EOpenMobileAdsEventType::Failed;
		Failed.Error = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderFailure,
			EOpenMobileAdsFailureStage::Teardown,
			NAME_None,
			TEXT("The provider could not destroy all ads."),
			TEXT("MockAds")
		);
		AddExpectedError(
			TEXT("The provider could not destroy all ads."),
			EAutomationExpectedErrorFlags::Contains,
			1
		);
		Provider.DestroySink->Submit(Failed);
		DrainGameThreadTasks();
	}

	TestEqual(TEXT("Destroy-all failure emits one terminal event"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		TestEqual(TEXT("Destroy-all terminal event is failed"), Events[0].Type, EOpenMobileAdsEventType::Failed);
		TestEqual(TEXT("Destroy-all failure keeps the request ID"), Events[0].RequestId, DestroyResult.RequestId);
	}
	TestFalse(
		TEXT("Terminal destroy-all request is no longer cancellable"),
		Subsystem->CancelRequest(DestroyResult.RequestId).bAccepted
	);

	const FOpenMobileAdsOperationResult UnregisteredDestroy = Subsystem->DestroyAllAds();
	TestTrue(TEXT("A second service-wide destroy starts"), UnregisteredDestroy.bAccepted);
	AddExpectedError(
		TEXT("The ads provider was unregistered during a service-wide operation."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Registration.Unregister();
	DrainGameThreadTasks();
	TestEqual(TEXT("Unregistration emits one service-wide terminal event"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		TestEqual(TEXT("Unregistration keeps the service-wide request ID"), Events[1].RequestId, UnregisteredDestroy.RequestId);
		TestEqual(TEXT("Unregistration is typed"), Events[1].Error.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);
	}
	TestFalse(
		TEXT("Unregistered service-wide request is terminal"),
		Subsystem->CancelRequest(UnregisteredDestroy.RequestId).bAccepted
	);
	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAsyncWorldCleanupTest,
	"OpenMobile.Ads.Blueprint.AsyncWorldCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAsyncWorldCleanupTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("WorldCleanupReward");
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	const FOpenMobileAdsOperationResult LoadResult =
		Subsystem->LoadAd(TEXT("WorldCleanupReward"));
	TestTrue(TEXT("Load begins before world cleanup"), LoadResult.bAccepted);

	UWorld* World = NewObject<UWorld>();
	UOpenMobileAdsAsyncAction* Action = NewObject<UOpenMobileAdsAsyncAction>();
	Action->Subsystem = Subsystem;
	Action->TargetWorld = World;
	Action->RequestId = LoadResult.RequestId;
	Action->HandleWorldCleanup(World, true, true);
	DrainGameThreadTasks();

	TestTrue(TEXT("World cleanup finishes the async proxy"), Action->bFinished);
	TestEqual(TEXT("World cleanup cancels provider work once"), Provider.CancelledRequests.Num(), 1);
	if (Provider.CancelledRequests.Num() == 1)
	{
		TestEqual(TEXT("World cleanup cancels the active request"), Provider.CancelledRequests[0], LoadResult.RequestId);
	}
	TestFalse(TEXT("World cleanup leaves no ready ad"), Subsystem->IsReady(TEXT("WorldCleanupReward")));
	Subsystem->Deinitialize();
	return true;
}

#endif
