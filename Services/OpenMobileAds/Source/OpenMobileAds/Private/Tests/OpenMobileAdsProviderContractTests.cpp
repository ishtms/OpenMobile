#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Features/IModularFeatures.h"
#include "HAL/PlatformMisc.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/AutomationTest.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileAdsAsyncAction.h"
#include "OpenMobileAdsCanShowPolicy.h"
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

		virtual bool Initialize(
			const FOpenMobileAdsInitializationRequest& Request,
			TSharedRef<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++InitializationCalls;
			LastInitializationRequest = Request;
			InitializationSink = CompletionSink;
			if (!bAcceptInitialization)
			{
				OutError = InitializationRejection;
				return false;
			}
			return true;
		}

		virtual bool Load(
			const FOpenMobileAdsLoadRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++LoadCalls;
			LastLoadRequest = Request;
			LoadSink = EventSink;
			if (!bAcceptLoad)
			{
				OutError = LoadRejection;
				LoadSink.Reset();
				return false;
			}
			return true;
		}

		virtual bool Show(
			const FOpenMobileAdsShowRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++ShowCalls;
			LastShowRequest = Request;
			ShowSink = EventSink;
			if (!bAcceptShow)
			{
				OutError = ShowRejection;
				ShowSink.Reset();
				return false;
			}
			return true;
		}

		virtual bool Destroy(
			const FOpenMobileAdsDestroyRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++DestroyCalls;
			LastDestroyRequest = Request;
			DestroySink = EventSink;
			if (!bAcceptDestroy)
			{
				OutError = DestroyRejection;
				DestroySink.Reset();
				return false;
			}
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

		virtual void ReleaseCachedAd(FGuid CachedAdId) override
		{
			ReleasedCachedAds.Add(CachedAdId);
		}

		virtual void Shutdown() override
		{
			++ShutdownCalls;
		}

		void CompleteInitialization(FOpenMobileAdsError Error = FOpenMobileAdsError())
		{
			if (InitializationSink)
			{
				InitializationSink->Complete(MoveTemp(Error));
			}
		}

		void ReportInitializationStatus(
			FOpenMobileAdsInitializationComponentStatus Status
		)
		{
			if (InitializationSink)
			{
				InitializationSink->UpdateStatus(MoveTemp(Status));
			}
		}

		FName Name;
		bool bSupported = true;
		bool bAcceptInitialization = true;
		bool bAcceptLoad = true;
		bool bAcceptShow = true;
		bool bAcceptDestroy = true;
		int32 InitializationCalls = 0;
		int32 LoadCalls = 0;
		int32 ShowCalls = 0;
		int32 DestroyCalls = 0;
		int32 ShutdownCalls = 0;
		FOpenMobileAdsProviderCapabilities Capabilities;
		FOpenMobileAdsInitializationRequest LastInitializationRequest;
		FOpenMobileAdsError InitializationRejection;
		FOpenMobileAdsError LoadRejection;
		FOpenMobileAdsError ShowRejection;
		FOpenMobileAdsError DestroyRejection;
		FOpenMobileAdsLoadRequest LastLoadRequest;
		FOpenMobileAdsShowRequest LastShowRequest;
		FOpenMobileAdsDestroyRequest LastDestroyRequest;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> LoadSink;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShowSink;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> DestroySink;
		TSharedPtr<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> InitializationSink;
		TArray<FGuid> CancelledRequests;
		TArray<FGuid> ReleasedCachedAds;
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
			bSavedDevelopmentTestMode = Settings->bDevelopmentTestMode;
			SavedTestDeviceIdentifiers = Settings->TestDeviceIdentifiers;
			SavedPrivacy = Settings->Privacy;
			SavedRequestConfiguration = Settings->RequestConfiguration;
			SavedPlacements = Settings->Placements;
		}

		~FScopedSettings()
		{
			Settings->PreferredProvider = SavedProvider;
			Settings->bDevelopmentTestMode = bSavedDevelopmentTestMode;
			Settings->TestDeviceIdentifiers = MoveTemp(SavedTestDeviceIdentifiers);
			Settings->Privacy = SavedPrivacy;
			Settings->RequestConfiguration = SavedRequestConfiguration;
			Settings->Placements = MoveTemp(SavedPlacements);
		}

		UOpenMobileAdsSettings* Settings = nullptr;

	private:
		FName SavedProvider;
		bool bSavedDevelopmentTestMode = false;
		TArray<FString> SavedTestDeviceIdentifiers;
		FOpenMobileAdsPrivacyConfiguration SavedPrivacy;
		FOpenMobileAdsRequestConfiguration SavedRequestConfiguration;
		TArray<FOpenMobileAdsPlacementSettings> SavedPlacements;
	};

	void DrainGameThreadTasks()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	}

	bool InitializeSuccessfully(
		UOpenMobileAdsSubsystem& Subsystem,
		FMockProvider& Provider,
		bool bAllowAdRequests = true
	)
	{
		const FOpenMobileAdsOperationResult Result = Subsystem.InitializeAds();
		if (!Result.bAccepted)
		{
			return false;
		}
		Provider.CompleteInitialization();
		DrainGameThreadTasks();
		if (Subsystem.GetServiceState() != EOpenMobileAdsServiceState::Ready)
		{
			return false;
		}
		if (bAllowAdRequests)
		{
			FOpenMobileAdsPrivacySnapshot Privacy;
			Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::NotRequired;
			Privacy.bCanRequestAds = true;
			Privacy.Source = TEXT("MockConsent");
			Subsystem.UpdatePrivacySnapshot(MoveTemp(Privacy));
		}
		return true;
	}

	const FOpenMobileAdsInitializationComponentStatus* FindInitializationComponent(
		const FOpenMobileAdsInitializationStatusSnapshot& Snapshot,
		EOpenMobileAdsInitializationComponentType Type,
		FName Name
	)
	{
		return Snapshot.Components.FindByPredicate(
			[Type, Name](const FOpenMobileAdsInitializationComponentStatus& Component)
			{
				return Component.Type == Type && Component.Name == Name;
			}
		);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsLoadPolicyContractTest,
	"OpenMobile.Ads.ProviderContract.Load.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsLoadPolicyContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent = true;
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ContinueReward");
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");
	FOpenMobileAdsPlacementSettings& Disabled =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Disabled.Placement = TEXT("DisabledReward");
	Disabled.bEnabled = false;
	Disabled.Android.AdUnitId = TEXT("android-disabled-unit");
	Disabled.IOS.AdUnitId = TEXT("ios-disabled-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before placement operations"),
		InitializeSuccessfully(*Subsystem, Provider, false)
	);

	const FOpenMobileAdsOperationResult PrivacyBlocked =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestFalse(TEXT("Unknown consent rejects the load"), PrivacyBlocked.bAccepted);
	TestEqual(
		TEXT("Privacy rejection is typed"),
		PrivacyBlocked.Error.Code,
		EOpenMobileAdsErrorCode::PrivacyBlocked
	);
	TestFalse(TEXT("Rejected loads have no request ID"), PrivacyBlocked.RequestId.IsValid());
	TestEqual(TEXT("Privacy rejection does not reach the provider"), Provider.LoadCalls, 0);
	TestEqual(
		TEXT("Disabled placement wins before privacy policy"),
		Subsystem->LoadAd(TEXT("DisabledReward")).Error.Code,
		EOpenMobileAdsErrorCode::DisabledPlacement
	);

	FOpenMobileAdsPrivacySnapshot Privacy;
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Privacy.bCanRequestAds = true;
	Privacy.Source = TEXT("MockConsent");
	Subsystem->UpdatePrivacySnapshot(Privacy);

	const FOpenMobileAdsOperationResult First =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestTrue(TEXT("An allowed named placement starts loading"), First.bAccepted);
	TestTrue(TEXT("An accepted load returns a request ID"), First.RequestId.IsValid());
	TestEqual(TEXT("The provider receives one load"), Provider.LoadCalls, 1);
	TestEqual(
		TEXT("The provider receives the named placement"),
		Provider.LastLoadRequest.Placement.Placement,
		FName(TEXT("ContinueReward"))
	);

	const FOpenMobileAdsOperationResult LoadingDuplicate =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestFalse(TEXT("A duplicate in-flight load is rejected"), LoadingDuplicate.bAccepted);
	TestEqual(
		TEXT("An in-flight duplicate is busy"),
		LoadingDuplicate.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);
	TestEqual(TEXT("A duplicate does not call the provider"), Provider.LoadCalls, 1);

	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = FGuid::NewGuid();
	Provider.LoadSink->Submit(Loaded);
	DrainGameThreadTasks();
	const FOpenMobileAdsOperationResult CachedDuplicate =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestFalse(TEXT("A cached placement is not loaded again by default"), CachedDuplicate.bAccepted);
	TestEqual(
		TEXT("A cached duplicate is busy"),
		CachedDuplicate.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);

	FOpenMobileAdsLoadOptions ForceReload;
	ForceReload.bForceReload = true;
	const FOpenMobileAdsOperationResult Forced =
		Subsystem->LoadAd(TEXT("ContinueReward"), ForceReload);
	TestTrue(TEXT("Force reload replaces the ready load operation"), Forced.bAccepted);
	TestNotEqual(TEXT("Force reload gets a new request ID"), Forced.RequestId, First.RequestId);
	TestEqual(TEXT("Force reload reaches the provider once"), Provider.LoadCalls, 2);
	FOpenMobileAdsEvent ReplacementFailed;
	ReplacementFailed.Type = EOpenMobileAdsEventType::LoadFailed;
	ReplacementFailed.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Load,
		TEXT("ContinueReward"),
		TEXT("Replacement load failed.")
	);
	AddExpectedError(
		TEXT("Replacement load failed."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.LoadSink->Submit(MoveTemp(ReplacementFailed));
	DrainGameThreadTasks();
	TestTrue(
		TEXT("A failed force reload preserves the previously ready ad"),
		Subsystem->IsReady(TEXT("ContinueReward"))
	);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsReadinessPolicyContractTest,
	"OpenMobile.Ads.ProviderContract.Readiness.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsReadinessPolicyContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ReadyReward");
	Placement.Android.AdUnitId = TEXT("android-ready-unit");
	Placement.IOS.AdUnitId = TEXT("ios-ready-unit");
	FOpenMobileAdsPlacementSettings& DisabledPlacement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	DisabledPlacement.Placement = TEXT("DisabledReward");
	DisabledPlacement.bEnabled = false;
	DisabledPlacement.Android.AdUnitId = TEXT("android-disabled-unit");
	DisabledPlacement.IOS.AdUnitId = TEXT("ios-disabled-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestFalse(TEXT("An unloaded placement is not ready"), Subsystem->IsReady(TEXT("ReadyReward")));
	TestEqual(
		TEXT("Unknown placement precedes initialization"),
		Subsystem->CanShow(TEXT("MissingReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::UnknownPlacement
	);
	TestEqual(
		TEXT("Disabled placement precedes initialization"),
		Subsystem->CanShow(TEXT("DisabledReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::Disabled
	);
	TestEqual(
		TEXT("Initialization blocks before cache state"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::NotInitialized
	);
	TestTrue(
		TEXT("The provider initializes for readiness checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	Provider.bSupported = false;
	TestEqual(
		TEXT("An unavailable provider blocks before cache state"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::ProviderUnavailable
	);
	Provider.bSupported = true;
	Provider.Capabilities.Formats[0].bCanShow = false;
	TestEqual(
		TEXT("Unsupported show format blocks before cache state"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::UnsupportedFormat
	);
	Provider.Capabilities.Formats[0].bCanShow = true;
	TestEqual(
		TEXT("An eligible but unloaded placement reports not loaded"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::NotLoaded
	);

	TestTrue(
		TEXT("The placement starts loading"),
		Subsystem->LoadAd(TEXT("ReadyReward")).bAccepted
	);
	FOpenMobileAdsPrivacySnapshot Privacy = Subsystem->GetPrivacySnapshot();
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Required;
	Privacy.bCanRequestAds = false;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	TestFalse(TEXT("Loading does not count as ready"), Subsystem->IsReady(TEXT("ReadyReward")));
	TestEqual(
		TEXT("Privacy policy wins while loading"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::PrivacyBlocked
	);

	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Privacy.bCanRequestAds = true;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	TestEqual(
		TEXT("Loading is reported after privacy allows ads"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::Loading
	);

	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = FGuid::NewGuid();
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	TestTrue(TEXT("A valid cached ad is ready"), Subsystem->IsReady(TEXT("ReadyReward")));
	TestTrue(TEXT("An eligible cached ad can show"), Subsystem->CanShow(TEXT("ReadyReward")).bCanShow);

	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Denied;
	Privacy.bCanRequestAds = false;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	TestTrue(TEXT("Privacy does not change cached readiness"), Subsystem->IsReady(TEXT("ReadyReward")));
	TestEqual(
		TEXT("Privacy blocks an otherwise ready ad"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::PrivacyBlocked
	);

	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Privacy.bCanRequestAds = true;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	const ENetworkConnectionType PreviousConnectionType =
		FPlatformMisc::GetNetworkConnectionType();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(ENetworkConnectionType::None);
	TestEqual(
		TEXT("No platform connection blocks showing"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::Offline
	);
	TestTrue(TEXT("Connectivity does not change cached readiness"), Subsystem->IsReady(TEXT("ReadyReward")));
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(ENetworkConnectionType::Unknown);
	TestTrue(TEXT("Unknown connectivity stays eligible"), Subsystem->CanShow(TEXT("ReadyReward")).bCanShow);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	const FOpenMobileAdsPlacementStatus StatusBeforeQueries =
		Subsystem->GetPlacementStatus(TEXT("ReadyReward"));
	TestEqual(
		TEXT("Background state blocks showing"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::LifecycleConflict
	);
	TestEqual(
		TEXT("Repeated background queries keep the same decision"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::LifecycleConflict
	);
	const FOpenMobileAdsPlacementStatus StatusAfterQueries =
		Subsystem->GetPlacementStatus(TEXT("ReadyReward"));
	TestEqual(
		TEXT("CanShow does not change placement state"),
		StatusAfterQueries.State,
		StatusBeforeQueries.State
	);
	TestEqual(
		TEXT("CanShow does not change cache identity"),
		StatusAfterQueries.CachedAdId,
		StatusBeforeQueries.CachedAdId
	);
	TestEqual(
		TEXT("CanShow does not change cache expiration"),
		StatusAfterQueries.ExpiresAt,
		StatusBeforeQueries.ExpiresAt
	);
	TestTrue(TEXT("Lifecycle policy does not change cached readiness"), Subsystem->IsReady(TEXT("ReadyReward")));
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	TestTrue(TEXT("Foreground state restores eligibility"), Subsystem->CanShow(TEXT("ReadyReward")).bCanShow);
	FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
	TestEqual(
		TEXT("Inactive state blocks showing"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::LifecycleConflict
	);
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	TestTrue(TEXT("Reactivation restores eligibility"), Subsystem->CanShow(TEXT("ReadyReward")).bCanShow);

	Subsystem->Deinitialize();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(PreviousConnectionType);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCanShowDecisionContractTest,
	"OpenMobile.Ads.ProviderContract.Readiness.Decision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCanShowDecisionContractTest::RunTest(const FString& Parameters)
{
	auto MakeEligibleContext = []()
	{
		FOpenMobileAdsCanShowPolicyContext Context;
		Context.bPlacementConfigured = true;
		Context.bPlacementEnabled = true;
		Context.ServiceState = EOpenMobileAdsServiceState::Ready;
		Context.bProviderAvailable = true;
		Context.bFormatSupported = true;
		Context.bPrivacyAllowed = true;
		Context.PlacementState = EOpenMobileAdPlacementState::Ready;
		Context.bHasCachedAd = true;
		return Context;
	};
	auto TestReason = [this](
		const TCHAR* What,
		const FOpenMobileAdsCanShowPolicyContext& Context,
		EOpenMobileAdsCanShowBlockReason Expected
	)
	{
		const FOpenMobileAdsCanShowResult Result =
			FOpenMobileAdsCanShowPolicy::Evaluate(Context);
		TestEqual(What, Result.BlockReason, Expected);
		TestFalse(TEXT("A blocked decision cannot show"), Result.bCanShow);
		TestFalse(TEXT("A blocked decision includes an explanation"), Result.Explanation.IsEmpty());
	};

	FOpenMobileAdsCanShowPolicyContext Context = MakeEligibleContext();
	Context.bPlacementConfigured = false;
	Context.bPlacementEnabled = false;
	Context.ServiceState = EOpenMobileAdsServiceState::Uninitialized;
	Context.bProviderAvailable = false;
	Context.bFormatSupported = false;
	Context.bPrivacyAllowed = false;
	Context.PlacementState = EOpenMobileAdPlacementState::Loading;
	Context.bHasCachedAd = false;
	Context.bExpired = true;
	Context.bFrequencyCapped = true;
	Context.bCooldownActive = true;
	Context.bOffline = true;
	Context.bLifecycleConflict = true;
	TestReason(
		TEXT("Unknown placement has highest precedence"),
		Context,
		EOpenMobileAdsCanShowBlockReason::UnknownPlacement
	);

	Context = MakeEligibleContext();
	Context.bPlacementEnabled = false;
	TestReason(TEXT("Disabled placement is typed"), Context, EOpenMobileAdsCanShowBlockReason::Disabled);
	Context = MakeEligibleContext();
	Context.ServiceState = EOpenMobileAdsServiceState::Initializing;
	Context.bPrivacyAllowed = false;
	Context.PlacementState = EOpenMobileAdPlacementState::Loading;
	TestReason(
		TEXT("Initialization precedes privacy and loading"),
		Context,
		EOpenMobileAdsCanShowBlockReason::NotInitialized
	);
	Context = MakeEligibleContext();
	Context.bProviderAvailable = false;
	TestReason(
		TEXT("Provider availability is typed"),
		Context,
		EOpenMobileAdsCanShowBlockReason::ProviderUnavailable
	);
	Context = MakeEligibleContext();
	Context.bFormatSupported = false;
	TestReason(
		TEXT("Unsupported format is typed"),
		Context,
		EOpenMobileAdsCanShowBlockReason::UnsupportedFormat
	);
	Context = MakeEligibleContext();
	Context.bPrivacyAllowed = false;
	Context.PlacementState = EOpenMobileAdPlacementState::Loading;
	TestReason(
		TEXT("Privacy precedes loading"),
		Context,
		EOpenMobileAdsCanShowBlockReason::PrivacyBlocked
	);
	Context = MakeEligibleContext();
	Context.PlacementState = EOpenMobileAdPlacementState::Loading;
	Context.bHasCachedAd = false;
	TestReason(TEXT("Loading is typed"), Context, EOpenMobileAdsCanShowBlockReason::Loading);
	Context = MakeEligibleContext();
	Context.bHasCachedAd = false;
	TestReason(TEXT("Missing cache is typed"), Context, EOpenMobileAdsCanShowBlockReason::NotLoaded);
	Context = MakeEligibleContext();
	Context.bExpired = true;
	Context.bFrequencyCapped = true;
	TestReason(
		TEXT("Expiration precedes pacing policy"),
		Context,
		EOpenMobileAdsCanShowBlockReason::Expired
	);

	const FDateTime CapEndsAt(2030, 1, 2);
	Context = MakeEligibleContext();
	Context.bFrequencyCapped = true;
	Context.bCooldownActive = true;
	Context.bOffline = true;
	Context.bLifecycleConflict = true;
	Context.FrequencyCapEndsAt = CapEndsAt;
	FOpenMobileAdsCanShowResult Result = FOpenMobileAdsCanShowPolicy::Evaluate(Context);
	TestEqual(
		TEXT("Frequency cap precedes cooldown and runtime state"),
		Result.BlockReason,
		EOpenMobileAdsCanShowBlockReason::FrequencyCap
	);
	TestEqual(TEXT("Frequency cap returns its next eligible time"), Result.NextEligibleAt, CapEndsAt);

	const FDateTime CooldownEndsAt(2030, 1, 3);
	Context = MakeEligibleContext();
	Context.bCooldownActive = true;
	Context.bOffline = true;
	Context.bLifecycleConflict = true;
	Context.CooldownEndsAt = CooldownEndsAt;
	Result = FOpenMobileAdsCanShowPolicy::Evaluate(Context);
	TestEqual(
		TEXT("Cooldown precedes connectivity and lifecycle"),
		Result.BlockReason,
		EOpenMobileAdsCanShowBlockReason::Cooldown
	);
	TestEqual(TEXT("Cooldown returns its next eligible time"), Result.NextEligibleAt, CooldownEndsAt);

	Context = MakeEligibleContext();
	Context.bOffline = true;
	Context.bLifecycleConflict = true;
	TestReason(
		TEXT("Connectivity precedes lifecycle"),
		Context,
		EOpenMobileAdsCanShowBlockReason::Offline
	);
	Context = MakeEligibleContext();
	Context.bLifecycleConflict = true;
	TestReason(
		TEXT("Lifecycle conflict is typed"),
		Context,
		EOpenMobileAdsCanShowBlockReason::LifecycleConflict
	);

	Result = FOpenMobileAdsCanShowPolicy::Evaluate(MakeEligibleContext());
	TestTrue(TEXT("An eligible decision can show"), Result.bCanShow);
	TestEqual(
		TEXT("An eligible decision has no block reason"),
		Result.BlockReason,
		EOpenMobileAdsCanShowBlockReason::None
	);
	TestTrue(TEXT("An eligible decision has no explanation"), Result.Explanation.IsEmpty());
	TestEqual(TEXT("An eligible decision has no pacing deadline"), Result.NextEligibleAt, FDateTime());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsShowPolicyContractTest,
	"OpenMobile.Ads.ProviderContract.Show.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsShowPolicyContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ShowReward");
	Placement.Android.AdUnitId = TEXT("android-show-unit");
	Placement.IOS.AdUnitId = TEXT("ios-show-unit");
	FOpenMobileAdsPlacementSettings& OtherPlacement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	OtherPlacement.Placement = TEXT("OtherReward");
	OtherPlacement.Android.AdUnitId = TEXT("android-other-unit");
	OtherPlacement.IOS.AdUnitId = TEXT("ios-other-unit");
	FOpenMobileAdsPlacementSettings& StalePlacement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	StalePlacement.Placement = TEXT("StaleReward");
	StalePlacement.Android.AdUnitId = TEXT("android-stale-unit");
	StalePlacement.IOS.AdUnitId = TEXT("ios-stale-unit");

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].CacheLifetimeSeconds = 60.0;
	FOpenMobileAdFormatCapabilities Interstitial;
	Interstitial.Format = EOpenMobileAdFormat::Interstitial;
	Interstitial.bCanShow = true;
	Provider.Capabilities.Formats.Add(Interstitial);
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before show checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	auto LoadPlacement = [this, Subsystem, &Provider](
		FName PlacementName,
		FGuid CachedAdId,
		FDateTime Timestamp = FDateTime()
	)
	{
		TestTrue(TEXT("The placement starts loading"), Subsystem->LoadAd(PlacementName).bAccepted);
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = CachedAdId;
		Loaded.Timestamp = Timestamp;
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
	};
	const FGuid ShowCachedAdId = FGuid::NewGuid();
	const FGuid OtherCachedAdId = FGuid::NewGuid();
	LoadPlacement(TEXT("ShowReward"), ShowCachedAdId);
	LoadPlacement(TEXT("OtherReward"), OtherCachedAdId);
	TestTrue(TEXT("The loaded placement is ready"), Subsystem->IsReady(TEXT("ShowReward")));

	FOpenMobileAdsPrivacySnapshot Privacy = Subsystem->GetPrivacySnapshot();
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Denied;
	Privacy.bCanRequestAds = false;
	Subsystem->UpdatePrivacySnapshot(MoveTemp(Privacy));
	const FOpenMobileAdsOperationResult Rejected = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Revoked consent rejects showing"), Rejected.bAccepted);
	TestEqual(
		TEXT("The show-time privacy rejection is typed"),
		Rejected.Error.Code,
		EOpenMobileAdsErrorCode::PrivacyBlocked
	);
	TestEqual(TEXT("A policy rejection does not reach the provider"), Provider.ShowCalls, 0);
	TestTrue(TEXT("A policy rejection preserves the ready cache"), Subsystem->IsReady(TEXT("ShowReward")));
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Privacy.bCanRequestAds = true;
	Subsystem->UpdatePrivacySnapshot(MoveTemp(Privacy));

	const ENetworkConnectionType PreviousConnectionType =
		FPlatformMisc::GetNetworkConnectionType();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(ENetworkConnectionType::AirplaneMode);
	const FOpenMobileAdsOperationResult Offline = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Offline state rejects showing"), Offline.bAccepted);
	TestEqual(TEXT("Offline rejection is typed"), Offline.Error.Code, EOpenMobileAdsErrorCode::InvalidState);
	TestEqual(TEXT("Offline rejection does not reach the provider"), Provider.ShowCalls, 0);
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(ENetworkConnectionType::Unknown);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	const FOpenMobileAdsOperationResult Background = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Background state rejects showing"), Background.bAccepted);
	TestEqual(TEXT("Lifecycle rejection is typed"), Background.Error.Code, EOpenMobileAdsErrorCode::InvalidState);
	TestEqual(TEXT("Lifecycle rejection does not reach the provider"), Provider.ShowCalls, 0);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	Provider.bSupported = false;
	const FOpenMobileAdsOperationResult ProviderUnavailable =
		Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Provider availability is rechecked at show time"), ProviderUnavailable.bAccepted);
	TestEqual(
		TEXT("Provider availability rejection is typed"),
		ProviderUnavailable.Error.Code,
		EOpenMobileAdsErrorCode::UnsupportedPlatform
	);
	TestEqual(TEXT("Unavailable provider does not receive a show"), Provider.ShowCalls, 0);
	TestTrue(TEXT("Provider availability rejection preserves the cache"), Subsystem->IsReady(TEXT("ShowReward")));
	Provider.bSupported = true;

	ScopedSettings.Settings->Placements[0].Format = EOpenMobileAdFormat::Interstitial;
	const FOpenMobileAdsOperationResult WrongFormat = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("A cache for the old placement format is rejected"), WrongFormat.bAccepted);
	TestEqual(TEXT("Wrong-format cache rejection is typed"), WrongFormat.Error.Code, EOpenMobileAdsErrorCode::NotReady);
	TestEqual(TEXT("Wrong-format cache does not reach the provider"), Provider.ShowCalls, 0);
	TestTrue(TEXT("Wrong-format rejection preserves the cached ad"), Subsystem->IsReady(TEXT("ShowReward")));
	ScopedSettings.Settings->Placements[0].Format = EOpenMobileAdFormat::Rewarded;

	LoadPlacement(
		TEXT("StaleReward"),
		FGuid::NewGuid(),
		FDateTime::UtcNow() - FTimespan::FromSeconds(120.0)
	);
	const FOpenMobileAdsOperationResult Stale = Subsystem->ShowAd(TEXT("StaleReward"));
	TestFalse(TEXT("An expired cache is rejected at show time"), Stale.bAccepted);
	TestEqual(TEXT("Expired cache rejection is typed"), Stale.Error.Code, EOpenMobileAdsErrorCode::NotReady);
	TestEqual(TEXT("Expired cache does not reach the provider"), Provider.ShowCalls, 0);

	TArray<FOpenMobileAdsEvent> ShowEvents;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&ShowEvents](const FOpenMobileAdsEvent& Event)
		{
			ShowEvents.Add(Event);
		}
	);
	Provider.bAcceptShow = false;
	Provider.ShowRejection = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Show,
		TEXT("ShowReward"),
		TEXT("The mock provider rejected presentation."),
		Provider.Name
	);
	const FOpenMobileAdsOperationResult ProviderRejected =
		Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Provider rejection is immediate"), ProviderRejected.bAccepted);
	TestEqual(
		TEXT("Provider rejection preserves its typed error"),
		ProviderRejected.Error.Code,
		EOpenMobileAdsErrorCode::ProviderFailure
	);
	TestFalse(TEXT("Immediate rejection has no request ID"), ProviderRejected.RequestId.IsValid());
	TestEqual(TEXT("Provider rejection attempts one presentation"), Provider.ShowCalls, 1);
	TestTrue(TEXT("Provider rejection restores the ready cache"), Subsystem->IsReady(TEXT("ShowReward")));
	DrainGameThreadTasks();
	TestTrue(TEXT("Provider rejection does not emit an asynchronous event"), ShowEvents.IsEmpty());

	Provider.bAcceptShow = true;
	const FOpenMobileAdsOperationResult Accepted = Subsystem->ShowAd(TEXT("ShowReward"));
	TestTrue(TEXT("An eligible ready ad is accepted once"), Accepted.bAccepted);
	TestTrue(TEXT("An accepted show has a request ID"), Accepted.RequestId.IsValid());
	TestEqual(TEXT("The provider receives the accepted request ID"), Provider.LastShowRequest.RequestId, Accepted.RequestId);
	TestEqual(TEXT("The provider receives the requested placement"), Provider.LastShowRequest.Placement, FName(TEXT("ShowReward")));
	TestEqual(TEXT("The provider receives the cached ad identity"), Provider.LastShowRequest.CachedAdId, ShowCachedAdId);
	TestEqual(TEXT("The provider receives the cached ad format"), Provider.LastShowRequest.Format, EOpenMobileAdFormat::Rewarded);
	TestFalse(TEXT("An accepted ad is no longer ready for reuse"), Subsystem->IsReady(TEXT("ShowReward")));
	const FOpenMobileAdsOperationResult Reused = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("The same accepted cached ad cannot show twice"), Reused.bAccepted);
	TestEqual(TEXT("A reused cache rejection is typed"), Reused.Error.Code, EOpenMobileAdsErrorCode::NotReady);
	TestEqual(TEXT("A reused cache is not submitted twice"), Provider.ShowCalls, 2);

	const FOpenMobileAdsOperationResult Concurrent = Subsystem->ShowAd(TEXT("OtherReward"));
	TestFalse(TEXT("A concurrent full-screen show is rejected"), Concurrent.bAccepted);
	TestEqual(TEXT("Concurrent show rejection is typed"), Concurrent.Error.Code, EOpenMobileAdsErrorCode::InvalidState);
	TestEqual(TEXT("Concurrent rejection does not reach the provider"), Provider.ShowCalls, 2);
	TestTrue(TEXT("Concurrent rejection preserves the other cache"), Subsystem->IsReady(TEXT("OtherReward")));
	DrainGameThreadTasks();
	TestEqual(TEXT("Accepted show emits one service event"), ShowEvents.Num(), 1);
	if (ShowEvents.Num() == 1)
	{
		TestEqual(TEXT("The immediate service event is show accepted"), ShowEvents[0].Type, EOpenMobileAdsEventType::ShowAccepted);
	}

	FOpenMobileAdsEvent Failed;
	Failed.Type = EOpenMobileAdsEventType::Failed;
	Failed.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Show,
		TEXT("ShowReward"),
		TEXT("The mock presentation failed asynchronously.")
	);
	AddExpectedError(
		TEXT("The mock presentation failed asynchronously."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.ShowSink->Submit(MoveTemp(Failed));
	DrainGameThreadTasks();
	TestEqual(TEXT("Asynchronous failure follows acceptance"), ShowEvents.Num(), 2);
	if (ShowEvents.Num() == 2)
	{
		TestEqual(TEXT("Presentation failure is asynchronous"), ShowEvents[1].Type, EOpenMobileAdsEventType::Failed);
		TestEqual(TEXT("Presentation failure keeps the request ID"), ShowEvents[1].RequestId, Accepted.RequestId);
		TestEqual(TEXT("Presentation failure keeps the cache ID"), ShowEvents[1].CachedAdId, ShowCachedAdId);
	}
	TestFalse(TEXT("Asynchronous failure consumes the failed cache"), Subsystem->IsReady(TEXT("ShowReward")));

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);

	Subsystem->Deinitialize();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(PreviousConnectionType);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsDestroyLifecycleContractTest,
	"OpenMobile.Ads.ProviderContract.Destroy.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsDestroyLifecycleContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("DestroyReward");
	Placement.Android.AdUnitId = TEXT("android-destroy-unit");
	Placement.IOS.AdUnitId = TEXT("ios-destroy-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before destroy checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	auto LoadReady = [this, Subsystem, &Provider](FGuid CachedAdId)
	{
		const FOpenMobileAdsOperationResult Load =
			Subsystem->LoadAd(TEXT("DestroyReward"));
		TestTrue(TEXT("The placement loads before cached destroy"), Load.bAccepted);
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = CachedAdId;
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
		TestTrue(TEXT("The placement is ready before cached destroy"), Subsystem->IsReady(TEXT("DestroyReward")));
	};

	const FOpenMobileAdsOperationResult Load =
		Subsystem->LoadAd(TEXT("DestroyReward"));
	TestTrue(TEXT("The load starts before destroy"), Load.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> LoadSink =
		Provider.LoadSink;

	const FOpenMobileAdsOperationResult Destroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestTrue(TEXT("Destroy can replace an active load"), Destroy.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> FirstDestroySink =
		Provider.DestroySink;
	TestEqual(TEXT("Destroy reaches the provider once"), Provider.DestroyCalls, 1);
	TestEqual(TEXT("Destroy cancels the active native load"), Provider.CancelledRequests.Num(), 1);
	if (Provider.CancelledRequests.Num() == 1)
	{
		TestEqual(TEXT("The cancelled request is the replaced load"), Provider.CancelledRequests[0], Load.RequestId);
	}
	TestFalse(
		TEXT("The replaced load request is no longer active"),
		Subsystem->CancelRequest(Load.RequestId).bAccepted
	);
	const FOpenMobileAdsOperationResult DuplicateDestroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestFalse(TEXT("A placement cannot start two destroy operations"), DuplicateDestroy.bAccepted);
	TestEqual(TEXT("Duplicate destroy is typed as busy"), DuplicateDestroy.Error.Code, EOpenMobileAdsErrorCode::Busy);
	TestEqual(TEXT("Duplicate destroy does not reach the provider"), Provider.DestroyCalls, 1);
	if (LoadSink)
	{
		FOpenMobileAdsEvent LateLoaded;
		LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
		LateLoaded.CachedAdId = FGuid::NewGuid();
		LoadSink->Submit(MoveTemp(LateLoaded));
		DrainGameThreadTasks();
	}
	TestFalse(TEXT("A late load cannot survive destroy"), Subsystem->IsReady(TEXT("DestroyReward")));

	if (FirstDestroySink)
	{
		FOpenMobileAdsEvent Destroyed;
		Destroyed.Type = EOpenMobileAdsEventType::Destroyed;
		FirstDestroySink->Submit(MoveTemp(Destroyed));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("Destroy completion returns the placement to idle"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).State,
		EOpenMobileAdPlacementState::Idle
	);

	const FGuid RejectedCacheId = FGuid::NewGuid();
	LoadReady(RejectedCacheId);
	Provider.bAcceptDestroy = false;
	Provider.DestroyRejection = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Teardown,
		TEXT("DestroyReward"),
		TEXT("The mock provider rejected destroy."),
		Provider.Name
	);
	const FOpenMobileAdsOperationResult Rejected =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestFalse(TEXT("Provider destroy rejection is immediate"), Rejected.bAccepted);
	TestEqual(TEXT("Provider destroy rejection is typed"), Rejected.Error.Code, EOpenMobileAdsErrorCode::ProviderFailure);
	TestEqual(TEXT("Provider rejection has no request ID"), Rejected.RequestId.IsValid(), false);
	TestTrue(TEXT("Provider rejection preserves cached readiness"), Subsystem->IsReady(TEXT("DestroyReward")));
	TestEqual(
		TEXT("Provider rejection preserves the cache identity"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).CachedAdId,
		RejectedCacheId
	);
	Provider.bAcceptDestroy = true;

	const FOpenMobileAdsOperationResult CachedDestroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestTrue(TEXT("A ready cached ad can be destroyed"), CachedDestroy.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> CachedDestroySink =
		Provider.DestroySink;
	if (CachedDestroySink)
	{
		FOpenMobileAdsEvent Destroyed;
		Destroyed.Type = EOpenMobileAdsEventType::Destroyed;
		CachedDestroySink->Submit(MoveTemp(Destroyed));
		DrainGameThreadTasks();
	}
	TestEqual(TEXT("Completed cached destroy releases one native ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Cached destroy releases the matching identity"), Provider.ReleasedCachedAds[0], RejectedCacheId);
	}
	TestFalse(TEXT("Completed destroy invalidates cached readiness"), Subsystem->IsReady(TEXT("DestroyReward")));

	const FGuid ShowingCacheId = FGuid::NewGuid();
	LoadReady(ShowingCacheId);
	const FOpenMobileAdsOperationResult Show =
		Subsystem->ShowAd(TEXT("DestroyReward"));
	TestTrue(TEXT("The placement starts showing before destroy"), Show.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShowSink =
		Provider.ShowSink;
	const FOpenMobileAdsOperationResult ShowingDestroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestTrue(TEXT("Destroy can replace an active show"), ShowingDestroy.bAccepted);
	TestEqual(TEXT("Destroy cancels the active native show"), Provider.CancelledRequests.Num(), 2);
	if (Provider.CancelledRequests.Num() == 2)
	{
		TestEqual(TEXT("The cancelled request is the replaced show"), Provider.CancelledRequests[1], Show.RequestId);
	}
	if (ShowSink)
	{
		FOpenMobileAdsEvent LateDismissed;
		LateDismissed.Type = EOpenMobileAdsEventType::Dismissed;
		ShowSink->Submit(MoveTemp(LateDismissed));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("A late show callback cannot finish the destroy"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).State,
		EOpenMobileAdPlacementState::Destroying
	);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> CancelledDestroySink =
		Provider.DestroySink;
	TestTrue(
		TEXT("An accepted destroy can be cancelled safely"),
		Subsystem->CancelRequest(ShowingDestroy.RequestId).bAccepted
	);
	DrainGameThreadTasks();
	TestEqual(TEXT("Destroy cancellation reaches the provider"), Provider.CancelledRequests.Num(), 3);
	TestEqual(
		TEXT("Destroy cancellation leaves an idle placement"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).State,
		EOpenMobileAdPlacementState::Idle
	);
	TestFalse(TEXT("Destroy cancellation does not restore a stale cache"), Subsystem->IsReady(TEXT("DestroyReward")));
	TestEqual(TEXT("Destroy cancellation releases the shown cache"), Provider.ReleasedCachedAds.Num(), 2);
	if (CancelledDestroySink)
	{
		FOpenMobileAdsEvent LateDestroyed;
		LateDestroyed.Type = EOpenMobileAdsEventType::Destroyed;
		CancelledDestroySink->Submit(MoveTemp(LateDestroyed));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("A late destroy callback cannot change cancelled state"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).State,
		EOpenMobileAdPlacementState::Idle
	);

	const FGuid ShutdownCacheId = FGuid::NewGuid();
	LoadReady(ShutdownCacheId);
	const FOpenMobileAdsOperationResult ShutdownDestroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestTrue(TEXT("Destroy starts before subsystem shutdown"), ShutdownDestroy.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShutdownDestroySink =
		Provider.DestroySink;

	Subsystem->Deinitialize();
	TestEqual(TEXT("Shutdown cancels the active destroy"), Provider.CancelledRequests.Num(), 4);
	if (Provider.CancelledRequests.Num() == 4)
	{
		TestEqual(TEXT("Shutdown cancels the destroy request ID"), Provider.CancelledRequests[3], ShutdownDestroy.RequestId);
	}
	TestEqual(TEXT("Shutdown releases the remaining cached ad"), Provider.ReleasedCachedAds.Num(), 3);
	if (ShutdownDestroySink)
	{
		FOpenMobileAdsEvent LateDestroyed;
		LateDestroyed.Type = EOpenMobileAdsEventType::Destroyed;
		ShutdownDestroySink->Submit(MoveTemp(LateDestroyed));
		DrainGameThreadTasks();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsDestroyAllLifecycleContractTest,
	"OpenMobile.Ads.ProviderContract.Destroy.AllLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsDestroyAllLifecycleContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	for (const FName PlacementName : {FName(TEXT("DestroyOne")), FName(TEXT("DestroyTwo"))})
	{
		FOpenMobileAdsPlacementSettings& Placement =
			ScopedSettings.Settings->Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.Android.AdUnitId = FString::Printf(TEXT("android-%s"), *PlacementName.ToString());
		Placement.IOS.AdUnitId = FString::Printf(TEXT("ios-%s"), *PlacementName.ToString());
	}

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before destroy-all checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	TestTrue(TEXT("The cached placement starts loading"), Subsystem->LoadAd(TEXT("DestroyTwo")).bAccepted);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	const FOpenMobileAdsOperationResult ActiveLoad =
		Subsystem->LoadAd(TEXT("DestroyOne"));
	TestTrue(TEXT("Another placement is loading before destroy all"), ActiveLoad.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> LoadSink =
		Provider.LoadSink;

	const FOpenMobileAdsOperationResult DestroyAll = Subsystem->DestroyAllAds();
	TestTrue(TEXT("Service-wide destroy is accepted"), DestroyAll.bAccepted);
	TestTrue(TEXT("Service-wide destroy has a request ID"), DestroyAll.RequestId.IsValid());
	TestTrue(TEXT("The provider receives an all-placements request"), Provider.LastDestroyRequest.bAllPlacements);
	TestEqual(TEXT("Service-wide destroy cancels active loading"), Provider.CancelledRequests.Num(), 1);
	if (Provider.CancelledRequests.Num() == 1)
	{
		TestEqual(TEXT("Destroy all cancels the loading request"), Provider.CancelledRequests[0], ActiveLoad.RequestId);
	}
	TestEqual(
		TEXT("The loading placement enters destroying state"),
		Subsystem->GetPlacementStatus(TEXT("DestroyOne")).State,
		EOpenMobileAdPlacementState::Destroying
	);
	TestEqual(
		TEXT("The cached placement enters destroying state"),
		Subsystem->GetPlacementStatus(TEXT("DestroyTwo")).State,
		EOpenMobileAdPlacementState::Destroying
	);
	const FOpenMobileAdsOperationResult Duplicate = Subsystem->DestroyAllAds();
	TestFalse(TEXT("Concurrent service-wide destroy is rejected"), Duplicate.bAccepted);
	TestEqual(TEXT("Concurrent service-wide destroy is busy"), Duplicate.Error.Code, EOpenMobileAdsErrorCode::Busy);
	TestEqual(TEXT("Concurrent service-wide destroy does not reach the provider"), Provider.DestroyCalls, 1);
	if (LoadSink)
	{
		FOpenMobileAdsEvent LateLoaded;
		LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
		LateLoaded.CachedAdId = FGuid::NewGuid();
		LoadSink->Submit(MoveTemp(LateLoaded));
		DrainGameThreadTasks();
	}
	TestFalse(TEXT("Late loading cannot escape destroy all"), Subsystem->IsReady(TEXT("DestroyOne")));

	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> DestroySink =
		Provider.DestroySink;
	if (DestroySink)
	{
		FOpenMobileAdsEvent Destroyed;
		Destroyed.Type = EOpenMobileAdsEventType::Destroyed;
		DestroySink->Submit(MoveTemp(Destroyed));
		DrainGameThreadTasks();
	}
	TestEqual(TEXT("Destroy all releases the cached native ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Destroy all releases the matching cache identity"), Provider.ReleasedCachedAds[0], CachedAdId);
	}
	TestEqual(
		TEXT("Destroy all clears the first placement"),
		Subsystem->GetPlacementStatus(TEXT("DestroyOne")).State,
		EOpenMobileAdPlacementState::Idle
	);
	TestEqual(
		TEXT("Destroy all clears the second placement"),
		Subsystem->GetPlacementStatus(TEXT("DestroyTwo")).State,
		EOpenMobileAdPlacementState::Idle
	);
	TestFalse(
		TEXT("Completed service-wide destroy is terminal"),
		Subsystem->CancelRequest(DestroyAll.RequestId).bAccepted
	);
	if (DestroySink)
	{
		FOpenMobileAdsEvent LateDestroyed;
		LateDestroyed.Type = EOpenMobileAdsEventType::Destroyed;
		DestroySink->Submit(MoveTemp(LateDestroyed));
		DrainGameThreadTasks();
	}

	TestTrue(TEXT("A placement can load after destroy all"), Subsystem->LoadAd(TEXT("DestroyTwo")).bAccepted);
	const FGuid CancelledCacheId = FGuid::NewGuid();
	FOpenMobileAdsEvent Reloaded;
	Reloaded.Type = EOpenMobileAdsEventType::Loaded;
	Reloaded.CachedAdId = CancelledCacheId;
	Provider.LoadSink->Submit(MoveTemp(Reloaded));
	DrainGameThreadTasks();
	const FOpenMobileAdsOperationResult CancelledDestroyAll = Subsystem->DestroyAllAds();
	TestTrue(TEXT("A second destroy all can start after completion"), CancelledDestroyAll.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> CancelledSink =
		Provider.DestroySink;
	TestTrue(
		TEXT("Service-wide destroy cancellation is accepted"),
		Subsystem->CancelRequest(CancelledDestroyAll.RequestId).bAccepted
	);
	DrainGameThreadTasks();
	TestFalse(TEXT("Cancelled destroy all does not restore cached readiness"), Subsystem->IsReady(TEXT("DestroyTwo")));
	TestEqual(
		TEXT("Cancelled destroy all leaves the placement idle"),
		Subsystem->GetPlacementStatus(TEXT("DestroyTwo")).State,
		EOpenMobileAdPlacementState::Idle
	);
	TestEqual(TEXT("Cancelled destroy all releases its cached ad"), Provider.ReleasedCachedAds.Num(), 2);
	if (CancelledSink)
	{
		FOpenMobileAdsEvent LateDestroyed;
		LateDestroyed.Type = EOpenMobileAdsEventType::Destroyed;
		CancelledSink->Submit(MoveTemp(LateDestroyed));
		DrainGameThreadTasks();
	}

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCacheContractTest,
	"OpenMobile.Ads.ProviderContract.Cache.Ownership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCacheContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	for (const FName PlacementName : {FName(TEXT("RewardOne")), FName(TEXT("RewardTwo"))})
	{
		FOpenMobileAdsPlacementSettings& Placement =
			ScopedSettings.Settings->Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.Android.AdUnitId = FString::Printf(TEXT("android-%s"), *PlacementName.ToString());
		Placement.IOS.AdUnitId = FString::Printf(TEXT("ios-%s"), *PlacementName.ToString());
	}

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].MaxCachedAdsPerPlacement = 2;
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before cache operations"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	const FOpenMobileAdFormatCapabilities* Rewarded =
		Provider.Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded);
	TestNotNull(TEXT("The mock provider advertises rewarded cache policy"), Rewarded);
	if (Rewarded)
	{
		TestEqual(TEXT("Provider cache capacity remains inspectable"), Rewarded->MaxCachedAdsPerPlacement, 2);
	}

	const FGuid FirstCachedAdId = FGuid::NewGuid();
	TestTrue(TEXT("The first placement load starts"), Subsystem->LoadAd(TEXT("RewardOne")).bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> FirstLoadSink =
		Provider.LoadSink;
	FOpenMobileAdsEvent FirstLoaded;
	FirstLoaded.Type = EOpenMobileAdsEventType::Loaded;
	FirstLoaded.CachedAdId = FirstCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(FirstLoaded));
	DrainGameThreadTasks();

	const FGuid SecondCachedAdId = FGuid::NewGuid();
	TestTrue(TEXT("The second placement load starts independently"), Subsystem->LoadAd(TEXT("RewardTwo")).bAccepted);
	FOpenMobileAdsEvent SecondLoaded;
	SecondLoaded.Type = EOpenMobileAdsEventType::Loaded;
	SecondLoaded.CachedAdId = SecondCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(SecondLoaded));
	DrainGameThreadTasks();
	TestEqual(
		TEXT("The first placement retains its cache identity"),
		Subsystem->GetPlacementStatus(TEXT("RewardOne")).CachedAdId,
		FirstCachedAdId
	);
	TestEqual(
		TEXT("The second placement has an independent cache identity"),
		Subsystem->GetPlacementStatus(TEXT("RewardTwo")).CachedAdId,
		SecondCachedAdId
	);

	FOpenMobileAdsLoadOptions ForceReload;
	ForceReload.bForceReload = true;
	const FOpenMobileAdsOperationResult CancelledReplacement =
		Subsystem->LoadAd(TEXT("RewardOne"), ForceReload);
	TestTrue(TEXT("A replacement load can be cancelled"), CancelledReplacement.bAccepted);
	TestTrue(
		TEXT("Cancelling a replacement load succeeds"),
		Subsystem->CancelRequest(CancelledReplacement.RequestId).bAccepted
	);
	DrainGameThreadTasks();
	TestEqual(
		TEXT("Cancellation restores the prior cache identity"),
		Subsystem->GetPlacementStatus(TEXT("RewardOne")).CachedAdId,
		FirstCachedAdId
	);
	TestTrue(TEXT("Cancellation restores ready state"), Subsystem->IsReady(TEXT("RewardOne")));
	TestTrue(TEXT("Cancellation does not release the prior cache"), Provider.ReleasedCachedAds.IsEmpty());

	TestTrue(
		TEXT("The first placement starts a replacement load"),
		Subsystem->LoadAd(TEXT("RewardOne"), ForceReload).bAccepted
	);
	const FGuid ReplacementCachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent ReplacementLoaded;
	ReplacementLoaded.Type = EOpenMobileAdsEventType::Loaded;
	ReplacementLoaded.CachedAdId = ReplacementCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(ReplacementLoaded));
	DrainGameThreadTasks();
	TestEqual(TEXT("Replacement releases one cached ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Replacement releases the prior cache identity"), Provider.ReleasedCachedAds[0], FirstCachedAdId);
	}
	TestEqual(
		TEXT("Replacement keeps the second placement unchanged"),
		Subsystem->GetPlacementStatus(TEXT("RewardTwo")).CachedAdId,
		SecondCachedAdId
	);

	if (FirstLoadSink)
	{
		FOpenMobileAdsEvent StaleLoaded;
		StaleLoaded.Type = EOpenMobileAdsEventType::Loaded;
		StaleLoaded.CachedAdId = FGuid::NewGuid();
		FirstLoadSink->Submit(MoveTemp(StaleLoaded));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("A stale load callback cannot replace the current cache"),
		Subsystem->GetPlacementStatus(TEXT("RewardOne")).CachedAdId,
		ReplacementCachedAdId
	);

	TestTrue(TEXT("The cached first placement can start showing"), Subsystem->ShowAd(TEXT("RewardOne")).bAccepted);
	FOpenMobileAdsEvent ShowFailed;
	ShowFailed.Type = EOpenMobileAdsEventType::Failed;
	ShowFailed.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Show,
		TEXT("RewardOne"),
		TEXT("Cached ad became unavailable.")
	);
	AddExpectedError(
		TEXT("Cached ad became unavailable."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.ShowSink->Submit(MoveTemp(ShowFailed));
	DrainGameThreadTasks();
	TestFalse(
		TEXT("Provider failure invalidates the affected cache"),
		Subsystem->GetPlacementStatus(TEXT("RewardOne")).CachedAdId.IsValid()
	);
	TestEqual(TEXT("Provider failure releases the affected native cache"), Provider.ReleasedCachedAds.Num(), 2);
	TestTrue(TEXT("Provider failure does not clear another placement"), Subsystem->IsReady(TEXT("RewardTwo")));

	AddExpectedError(
		TEXT("The ads provider was unregistered during an active placement operation."),
		EAutomationExpectedErrorFlags::Contains,
		2
	);
	Registration.Unregister();
	DrainGameThreadTasks();
	TestFalse(
		TEXT("Provider unregistration invalidates remaining cached state"),
		Subsystem->GetPlacementStatus(TEXT("RewardTwo")).CachedAdId.IsValid()
	);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCacheIdentityContractTest,
	"OpenMobile.Ads.ProviderContract.Cache.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCacheIdentityContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("IdentityReward");
	Placement.Android.AdUnitId = TEXT("android-identity");
	Placement.IOS.AdUnitId = TEXT("ios-identity");

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].MaxCachedAdsPerPlacement = 0;
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before cache identity checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	const FOpenMobileAdsOperationResult NoCapacity =
		Subsystem->LoadAd(TEXT("IdentityReward"));
	TestFalse(TEXT("A provider without cache capacity rejects loading"), NoCapacity.bAccepted);
	TestEqual(
		TEXT("Invalid provider cache capacity is typed"),
		NoCapacity.Error.Code,
		EOpenMobileAdsErrorCode::ProviderFailure
	);
	Provider.Capabilities.Formats[0].MaxCachedAdsPerPlacement = 1;
	Provider.Capabilities.Formats[0].CacheLifetimeSeconds = -1.0;
	const FOpenMobileAdsOperationResult InvalidLifetime =
		Subsystem->LoadAd(TEXT("IdentityReward"));
	TestFalse(TEXT("A provider with a negative cache lifetime rejects loading"), InvalidLifetime.bAccepted);
	TestEqual(
		TEXT("Invalid provider cache lifetime is typed"),
		InvalidLifetime.Error.Code,
		EOpenMobileAdsErrorCode::ProviderFailure
	);
	Provider.Capabilities.Formats[0].CacheLifetimeSeconds = 0.0;

	TestTrue(TEXT("The first load starts"), Subsystem->LoadAd(TEXT("IdentityReward")).bAccepted);
	FOpenMobileAdsEvent MissingIdentity;
	MissingIdentity.Type = EOpenMobileAdsEventType::Loaded;
	AddExpectedError(
		TEXT("The ads provider reported a loaded ad without a cache identity."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.LoadSink->Submit(MoveTemp(MissingIdentity));
	DrainGameThreadTasks();
	const FOpenMobileAdsPlacementStatus FailedStatus =
		Subsystem->GetPlacementStatus(TEXT("IdentityReward"));
	TestEqual(TEXT("A missing cache identity fails the load"), FailedStatus.State, EOpenMobileAdPlacementState::Failed);
	TestEqual(
		TEXT("A missing cache identity is a provider failure"),
		FailedStatus.LastError.Code,
		EOpenMobileAdsErrorCode::ProviderFailure
	);
	TestFalse(TEXT("The service does not invent a cache identity"), FailedStatus.CachedAdId.IsValid());

	TestTrue(TEXT("A load can retry after the malformed callback"), Subsystem->LoadAd(TEXT("IdentityReward")).bAccepted);
	const FGuid StableCachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = StableCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();

	FOpenMobileAdsLoadOptions ForceReload;
	ForceReload.bForceReload = true;
	TestTrue(TEXT("A forced replacement starts"), Subsystem->LoadAd(TEXT("IdentityReward"), ForceReload).bAccepted);
	FOpenMobileAdsEvent MalformedReplacement;
	MalformedReplacement.Type = EOpenMobileAdsEventType::Loaded;
	AddExpectedError(
		TEXT("The ads provider reported a loaded ad without a cache identity."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.LoadSink->Submit(MoveTemp(MalformedReplacement));
	DrainGameThreadTasks();
	TestEqual(
		TEXT("A malformed replacement preserves the prior cache"),
		Subsystem->GetPlacementStatus(TEXT("IdentityReward")).CachedAdId,
		StableCachedAdId
	);
	TestTrue(TEXT("A malformed replacement keeps the prior ad ready"), Subsystem->IsReady(TEXT("IdentityReward")));
	TestTrue(TEXT("A malformed replacement does not release the prior ad"), Provider.ReleasedCachedAds.IsEmpty());

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCacheExpirationContractTest,
	"OpenMobile.Ads.ProviderContract.Cache.Expiration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCacheExpirationContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ExpiringReward");
	Placement.Android.AdUnitId = TEXT("android-expiring");
	Placement.IOS.AdUnitId = TEXT("ios-expiring");

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].CacheLifetimeSeconds = 1.0;
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before expiration checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);
	TestTrue(TEXT("The expiring placement starts loading"), Subsystem->LoadAd(TEXT("ExpiringReward")).bAccepted);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Loaded.Timestamp = FDateTime::UtcNow() - FTimespan::FromSeconds(2.0);
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();

	const FOpenMobileAdsPlacementStatus ReadyStatus =
		Subsystem->GetPlacementStatus(TEXT("ExpiringReward"));
	TestEqual(TEXT("The cache records its load timestamp"), ReadyStatus.CachedAt, Events.Last().Timestamp);
	TestEqual(
		TEXT("The cache records provider-specific expiration"),
		ReadyStatus.ExpiresAt,
		ReadyStatus.CachedAt + FTimespan::FromSeconds(1.0)
	);
	TestFalse(TEXT("An overdue cache is not ready before cleanup runs"), Subsystem->IsReady(TEXT("ExpiringReward")));
	TestEqual(
		TEXT("CanShow reports an overdue cache as expired"),
		Subsystem->CanShow(TEXT("ExpiringReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::Expired
	);

	FTSTicker::GetCoreTicker().Tick(0.0f);
	DrainGameThreadTasks();
	const FOpenMobileAdsPlacementStatus ExpiredStatus =
		Subsystem->GetPlacementStatus(TEXT("ExpiringReward"));
	TestEqual(TEXT("Expiration returns the placement to idle"), ExpiredStatus.State, EOpenMobileAdPlacementState::Idle);
	TestFalse(TEXT("Expiration invalidates the public cache ID"), ExpiredStatus.CachedAdId.IsValid());
	TestEqual(TEXT("Expiration clears the load timestamp"), ExpiredStatus.CachedAt, FDateTime());
	TestEqual(TEXT("Expiration clears the deadline"), ExpiredStatus.ExpiresAt, FDateTime());
	TestEqual(TEXT("Expiration releases the native cached ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Expiration releases the matching cache identity"), Provider.ReleasedCachedAds[0], CachedAdId);
	}
	TestTrue(
		TEXT("Expiration broadcasts a normalized event"),
		Events.ContainsByPredicate([](const FOpenMobileAdsEvent& Event)
		{
			return Event.Type == EOpenMobileAdsEventType::Expired;
		})
	);

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsInitializationIdempotencyContractTest,
	"OpenMobile.Ads.ProviderContract.Initialization.IdempotencyAndConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsInitializationIdempotencyContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->bDevelopmentTestMode = true;
	ScopedSettings.Settings->TestDeviceIdentifiers = {
		TEXT("GLOBAL-DEVICE-A"),
		TEXT("GLOBAL-DEVICE-B")
	};
	ScopedSettings.Settings->Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::Yes;
	ScopedSettings.Settings->Privacy.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::No;
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent = false;
	ScopedSettings.Settings->RequestConfiguration.MaxAdContentRating =
		EOpenMobileAdsMaxAdContentRating::ParentalGuidance;

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);

	const FOpenMobileAdsOperationResult First = Subsystem->InitializeAds();
	TestTrue(TEXT("The first initialization request is accepted"), First.bAccepted);
	TestTrue(TEXT("Initialization receives a stable request ID"), First.RequestId.IsValid());
	TestEqual(TEXT("The service enters initializing state"), Subsystem->GetServiceState(), EOpenMobileAdsServiceState::Initializing);
	TestEqual(TEXT("The provider starts once"), Provider.InitializationCalls, 1);
	TestTrue(TEXT("Test mode reaches the provider before initialization"), Provider.LastInitializationRequest.Development.bEnabled);
	TestTrue(TEXT("Test mode enables test devices"), Provider.LastInitializationRequest.Development.bUseTestDevices);
	TestTrue(TEXT("Test mode enables official test IDs"), Provider.LastInitializationRequest.Development.bUseTestAdUnitIds);
	TestTrue(TEXT("Test mode enables consent debug intent"), Provider.LastInitializationRequest.Development.bEnableConsentDebug);
	TestTrue(TEXT("Test mode enables verbose diagnostics"), Provider.LastInitializationRequest.Development.bEnableVerboseDiagnostics);
	TestEqual(
		TEXT("Global test-device identifiers reach the provider before initialization"),
		Provider.LastInitializationRequest.Development.TestDeviceIdentifiers,
		ScopedSettings.Settings->TestDeviceIdentifiers
	);
	TestEqual(
		TEXT("Child-directed treatment reaches the provider"),
		Provider.LastInitializationRequest.Privacy.ChildDirectedTreatment,
		EOpenMobileAdsAgeTreatment::Yes
	);
	TestEqual(
		TEXT("Under-age treatment reaches the provider"),
		Provider.LastInitializationRequest.Privacy.UnderAgeOfConsent,
		EOpenMobileAdsAgeTreatment::No
	);
	TestEqual(
		TEXT("Request configuration reaches the provider"),
		Provider.LastInitializationRequest.RequestConfiguration.MaxAdContentRating,
		EOpenMobileAdsMaxAdContentRating::ParentalGuidance
	);

	const FOpenMobileAdsOperationResult Overlapping = Subsystem->InitializeAds();
	TestTrue(TEXT("An overlapping initialization call is accepted"), Overlapping.bAccepted);
	TestEqual(TEXT("Overlapping calls share the request ID"), Overlapping.RequestId, First.RequestId);
	TestEqual(TEXT("An overlapping call does not restart the provider"), Provider.InitializationCalls, 1);

	TFuture<void> Completion = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		Provider.CompleteInitialization();
	});
	Completion.Wait();
	DrainGameThreadTasks();
	TestEqual(TEXT("Provider completion makes the service ready"), Subsystem->GetServiceState(), EOpenMobileAdsServiceState::Ready);

	const FOpenMobileAdsOperationResult Repeated = Subsystem->InitializeAds();
	TestTrue(TEXT("Initialization remains idempotent after success"), Repeated.bAccepted);
	TestEqual(TEXT("Successful repeated calls share the request ID"), Repeated.RequestId, First.RequestId);
	TestEqual(TEXT("A successful repeated call does not restart the provider"), Provider.InitializationCalls, 1);

	Subsystem->Deinitialize();
	TestEqual(TEXT("Teardown shuts down the initialized provider once"), Provider.ShutdownCalls, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsInitializationStatusContractTest,
	"OpenMobile.Ads.ProviderContract.Initialization.StatusReporting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsInitializationStatusContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.ProviderVersion = TEXT("mock-2.4");
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	TArray<FOpenMobileAdsInitializationStatusSnapshot> Changes;
	const FDelegateHandle StatusHandle =
		Subsystem->OnNativeInitializationStatusChanged().AddLambda(
			[&Changes](const FOpenMobileAdsInitializationStatusSnapshot& Status)
			{
				Changes.Add(Status);
			}
		);

	const FOpenMobileAdsOperationResult Started = Subsystem->InitializeAds();
	TestTrue(TEXT("Initialization starts"), Started.bAccepted);
	const FOpenMobileAdsInitializationStatusSnapshot Initializing =
		Subsystem->GetInitializationStatus();
	TestEqual(TEXT("The snapshot is initializing"), Initializing.ServiceState, EOpenMobileAdsServiceState::Initializing);
	TestEqual(TEXT("The snapshot keeps the request ID"), Initializing.RequestId, Started.RequestId);
	TestTrue(TEXT("The snapshot records its start time"), Initializing.StartedAt != FDateTime());
	const FOpenMobileAdsInitializationComponentStatus* InitialProvider =
		FindInitializationComponent(
			Initializing,
			EOpenMobileAdsInitializationComponentType::Provider,
			TEXT("MockAds")
		);
	TestNotNull(TEXT("The provider has a normalized status"), InitialProvider);
	if (InitialProvider)
	{
		TestEqual(TEXT("The provider starts as initializing"), InitialProvider->State, EOpenMobileAdsInitializationState::Initializing);
		TestEqual(TEXT("The provider version is exposed"), InitialProvider->Version, FString(TEXT("mock-2.4")));
		TestTrue(TEXT("Provider capabilities are exposed"), InitialProvider->bHasCapabilities);
		TestTrue(TEXT("Rewarded capability is retained"), InitialProvider->Capabilities.SupportsFormat(EOpenMobileAdFormat::Rewarded));
	}

	TFuture<void> ComponentUpdates = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		FOpenMobileAdsInitializationComponentStatus Network;
		Network.Type = EOpenMobileAdsInitializationComponentType::Network;
		Network.Name = TEXT("MockNetwork");
		Network.Parent = TEXT("MockAds");
		Network.State = EOpenMobileAdsInitializationState::Ready;
		Network.Version = TEXT("1.2.0");
		Network.LatencyMilliseconds = 12.5;
		Provider.ReportInitializationStatus(MoveTemp(Network));

		FOpenMobileAdsInitializationComponentStatus Adapter;
		Adapter.Type = EOpenMobileAdsInitializationComponentType::Adapter;
		Adapter.Name = TEXT("MockAdapter");
		Adapter.Parent = TEXT("MockNetwork");
		Adapter.State = EOpenMobileAdsInitializationState::Initializing;
		Provider.ReportInitializationStatus(MoveTemp(Adapter));
	});
	ComponentUpdates.Wait();
	DrainGameThreadTasks();
	const FOpenMobileAdsInitializationStatusSnapshot ComponentStatus =
		Subsystem->GetInitializationStatus();
	const FOpenMobileAdsInitializationComponentStatus* Network =
		FindInitializationComponent(
			ComponentStatus,
			EOpenMobileAdsInitializationComponentType::Network,
			TEXT("MockNetwork")
		);
	TestNotNull(TEXT("Network status is reported"), Network);
	if (Network)
	{
		TestEqual(TEXT("Network version is retained"), Network->Version, FString(TEXT("1.2.0")));
		TestEqual(TEXT("Network latency is retained"), Network->LatencyMilliseconds, 12.5);
	}

	Provider.CompleteInitialization();
	DrainGameThreadTasks();
	FOpenMobileAdsInitializationStatusSnapshot Ready = Subsystem->GetInitializationStatus();
	TestEqual(TEXT("The service reports ready"), Ready.ServiceState, EOpenMobileAdsServiceState::Ready);
	TestTrue(TEXT("Service latency is recorded"), Ready.LatencyMilliseconds >= 0.0);
	TestTrue(TEXT("A pending adapter reports partial success"), Ready.bPartialSuccess);

	FOpenMobileAdsInitializationComponentStatus FailedAdapter;
	FailedAdapter.Type = EOpenMobileAdsInitializationComponentType::Adapter;
	FailedAdapter.Name = TEXT("MockAdapter");
	FailedAdapter.Parent = TEXT("MockNetwork");
	FailedAdapter.State = EOpenMobileAdsInitializationState::Failed;
	FailedAdapter.LatencyMilliseconds = 25.0;
	FailedAdapter.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Initialization,
		NAME_None,
		TEXT("The mock adapter did not initialize."),
		TEXT("MockAds")
	);
	Provider.ReportInitializationStatus(FailedAdapter);
	DrainGameThreadTasks();
	Ready = Subsystem->GetInitializationStatus();
	TestEqual(TEXT("Adapter failure does not fail the ready provider"), Ready.ServiceState, EOpenMobileAdsServiceState::Ready);
	TestTrue(TEXT("Adapter failure reports partial success"), Ready.bPartialSuccess);
	const FOpenMobileAdsInitializationComponentStatus* Failed =
		FindInitializationComponent(
			Ready,
			EOpenMobileAdsInitializationComponentType::Adapter,
			TEXT("MockAdapter")
		);
	TestNotNull(TEXT("Failed adapter remains inspectable"), Failed);
	if (Failed)
	{
		TestEqual(TEXT("Adapter failure detail is retained"), Failed->Error.Code, EOpenMobileAdsErrorCode::ProviderFailure);
	}

	FailedAdapter.State = EOpenMobileAdsInitializationState::Ready;
	FailedAdapter.LatencyMilliseconds = 40.0;
	FailedAdapter.Error = FOpenMobileAdsError();
	Provider.ReportInitializationStatus(MoveTemp(FailedAdapter));
	DrainGameThreadTasks();
	Ready = Subsystem->GetInitializationStatus();
	TestFalse(TEXT("A late ready adapter clears partial success"), Ready.bPartialSuccess);
	TestTrue(TEXT("Each accepted transition broadcasts a snapshot"), Changes.Num() >= 6);

	Subsystem->OnNativeInitializationStatusChanged().Remove(StatusHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsInitializationFailureContractTest,
	"OpenMobile.Ads.ProviderContract.Initialization.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsInitializationFailureContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);

	const FOpenMobileAdsOperationResult Started = Subsystem->InitializeAds();
	TestTrue(TEXT("Initialization starts before an asynchronous failure"), Started.bAccepted);
	Provider.CompleteInitialization(FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Initialization,
		NAME_None,
		TEXT("The mock SDK failed to initialize."),
		TEXT("MockAds")
	));
	DrainGameThreadTasks();
	TestEqual(TEXT("Provider failure makes the service failed"), Subsystem->GetServiceState(), EOpenMobileAdsServiceState::Failed);
	const FOpenMobileAdsInitializationStatusSnapshot FailedStatus =
		Subsystem->GetInitializationStatus();
	TestEqual(TEXT("The status snapshot reports failure"), FailedStatus.ServiceState, EOpenMobileAdsServiceState::Failed);
	TestEqual(TEXT("The status snapshot keeps the provider error"), FailedStatus.Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	const FOpenMobileAdsInitializationComponentStatus* FailedProvider =
		FindInitializationComponent(
			FailedStatus,
			EOpenMobileAdsInitializationComponentType::Provider,
			TEXT("MockAds")
		);
	TestNotNull(TEXT("The failed provider remains inspectable"), FailedProvider);
	if (FailedProvider)
	{
		TestEqual(TEXT("The provider status reports failure"), FailedProvider->State, EOpenMobileAdsInitializationState::Failed);
		TestEqual(TEXT("The provider status keeps the failure detail"), FailedProvider->Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	}

	const FOpenMobileAdsOperationResult Repeated = Subsystem->InitializeAds();
	TestFalse(TEXT("A failed initialization is not reported as accepted"), Repeated.bAccepted);
	TestEqual(TEXT("The provider error is retained"), Repeated.Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	TestEqual(TEXT("A repeated failed call does not restart the provider"), Provider.InitializationCalls, 1);

	Subsystem->Deinitialize();
	TestEqual(TEXT("A provider that accepted initialization is shut down"), Provider.ShutdownCalls, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsInitializationSelectionContractTest,
	"OpenMobile.Ads.ProviderContract.Initialization.SelectionFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsInitializationSelectionContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;

	ScopedSettings.Settings->PreferredProvider = TEXT("MissingAds");
	UOpenMobileAdsSubsystem* MissingSubsystem = NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	const FOpenMobileAdsOperationResult Missing = MissingSubsystem->InitializeAds();
	TestFalse(TEXT("A missing preferred provider is rejected"), Missing.bAccepted);
	TestEqual(TEXT("A missing provider has a typed error"), Missing.Error.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);
	MissingSubsystem->Deinitialize();

	FMockProvider UnsupportedProvider(TEXT("UnsupportedAds"), false);
	FScopedProviderRegistration UnsupportedRegistration(UnsupportedProvider);
	ScopedSettings.Settings->PreferredProvider = TEXT("UnsupportedAds");
	UOpenMobileAdsSubsystem* UnsupportedSubsystem = NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	const FOpenMobileAdsOperationResult Unsupported = UnsupportedSubsystem->InitializeAds();
	TestFalse(TEXT("An unsupported platform is rejected"), Unsupported.bAccepted);
	TestEqual(TEXT("Unsupported platform failure is distinct"), Unsupported.Error.Code, EOpenMobileAdsErrorCode::UnsupportedPlatform);
	UnsupportedSubsystem->Deinitialize();

	FMockProvider Alpha(TEXT("AlphaAds"));
	FMockProvider Beta(TEXT("BetaAds"));
	FScopedProviderRegistration AlphaRegistration(Alpha);
	FScopedProviderRegistration BetaRegistration(Beta);
	ScopedSettings.Settings->PreferredProvider = NAME_None;
	UOpenMobileAdsSubsystem* ConflictSubsystem = NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	const FOpenMobileAdsOperationResult Conflict = ConflictSubsystem->InitializeAds();
	TestFalse(TEXT("Ambiguous provider selection is rejected"), Conflict.bAccepted);
	TestEqual(TEXT("Ambiguous selection has a typed error"), Conflict.Error.Code, EOpenMobileAdsErrorCode::ProviderConflict);
	ConflictSubsystem->Deinitialize();
	return true;
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
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));

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
	FOpenMobileAdsLoadCallbackContractTest,
	"OpenMobile.Ads.ProviderContract.Load.Callback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsLoadCallbackContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	for (const FName PlacementName : {FName(TEXT("CallbackSuccess")), FName(TEXT("CallbackFailure"))})
	{
		FOpenMobileAdsPlacementSettings& Placement =
			ScopedSettings.Settings->Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.Format = EOpenMobileAdFormat::Rewarded;
		Placement.Android.AdUnitId = FString::Printf(TEXT("android-%s"), *PlacementName.ToString());
		Placement.IOS.AdUnitId = FString::Printf(TEXT("ios-%s"), *PlacementName.ToString());
	}

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before load callback checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult SuccessfulLoad =
		Subsystem->LoadAd(TEXT("CallbackSuccess"));
	TestTrue(TEXT("The successful callback request is accepted"), SuccessfulLoad.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> SuccessSink =
		Provider.LoadSink;
	FOpenMobileAdsEvent DuplicateStarted;
	DuplicateStarted.Type = EOpenMobileAdsEventType::LoadStarted;
	DuplicateStarted.Provider = TEXT("WrongProvider");
	DuplicateStarted.Placement = TEXT("WrongPlacement");
	DuplicateStarted.Format = EOpenMobileAdFormat::Banner;
	DuplicateStarted.RequestId = FGuid::NewGuid();
	SuccessSink->Submit(MoveTemp(DuplicateStarted));
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.Provider = TEXT("WrongProvider");
	Loaded.Placement = TEXT("WrongPlacement");
	Loaded.Format = EOpenMobileAdFormat::Banner;
	Loaded.RequestId = FGuid::NewGuid();
	Loaded.CachedAdId = CachedAdId;
	SuccessSink->Submit(MoveTemp(Loaded));
	FOpenMobileAdsEvent DuplicateFailure;
	DuplicateFailure.Type = EOpenMobileAdsEventType::LoadFailed;
	DuplicateFailure.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Load,
		TEXT("WrongPlacement"),
		TEXT("This duplicate terminal result must be ignored."),
		TEXT("WrongProvider")
	);
	SuccessSink->Submit(MoveTemp(DuplicateFailure));
	DrainGameThreadTasks();

	TestEqual(TEXT("A successful load broadcasts one start and one terminal result"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		const FOpenMobileAdsEvent& Started = Events[0];
		const FOpenMobileAdsEvent& Completed = Events[1];
		TestEqual(TEXT("The first successful event is load started"), Started.Type, EOpenMobileAdsEventType::LoadStarted);
		TestEqual(TEXT("The successful terminal event is loaded"), Completed.Type, EOpenMobileAdsEventType::Loaded);
		TestEqual(TEXT("Load started has the normalized placement"), Started.Placement, FName(TEXT("CallbackSuccess")));
		TestEqual(TEXT("Loaded has the normalized placement"), Completed.Placement, FName(TEXT("CallbackSuccess")));
		TestEqual(TEXT("Load started has the normalized format"), Started.Format, EOpenMobileAdFormat::Rewarded);
		TestEqual(TEXT("Loaded has the normalized format"), Completed.Format, EOpenMobileAdFormat::Rewarded);
		TestEqual(TEXT("Load started has the normalized provider"), Started.Provider, Provider.Name);
		TestEqual(TEXT("Loaded has the normalized provider"), Completed.Provider, Provider.Name);
		TestEqual(TEXT("Load started has the accepted request ID"), Started.RequestId, SuccessfulLoad.RequestId);
		TestEqual(TEXT("Loaded has the accepted request ID"), Completed.RequestId, SuccessfulLoad.RequestId);
		TestEqual(TEXT("Loaded preserves the provider cache ID"), Completed.CachedAdId, CachedAdId);
		TestEqual(TEXT("Load started reports loading state"), Started.PlacementState, EOpenMobileAdPlacementState::Loading);
		TestEqual(TEXT("Loaded reports ready state"), Completed.PlacementState, EOpenMobileAdPlacementState::Ready);
		TestTrue(TEXT("Load callback sequence is increasing"), Started.Sequence < Completed.Sequence);
	}

	const FOpenMobileAdsOperationResult FailedLoad =
		Subsystem->LoadAd(TEXT("CallbackFailure"));
	TestTrue(TEXT("The failed callback request is accepted"), FailedLoad.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> FailureSink =
		Provider.LoadSink;
	FOpenMobileAdsEvent Failed;
	Failed.Type = EOpenMobileAdsEventType::LoadFailed;
	Failed.Provider = TEXT("WrongProvider");
	Failed.Placement = TEXT("WrongPlacement");
	Failed.Format = EOpenMobileAdFormat::Banner;
	Failed.RequestId = FGuid::NewGuid();
	Failed.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Load,
		TEXT("WrongPlacement"),
		TEXT("The native load failed."),
		TEXT("WrongProvider"),
		FString(),
		true
	);
	FailureSink->Submit(MoveTemp(Failed));
	DrainGameThreadTasks();

	TestEqual(TEXT("A failed load adds one start and one terminal result"), Events.Num(), 4);
	if (Events.Num() == 4)
	{
		const FOpenMobileAdsEvent& Started = Events[2];
		const FOpenMobileAdsEvent& Completed = Events[3];
		TestEqual(TEXT("The first failed event is load started"), Started.Type, EOpenMobileAdsEventType::LoadStarted);
		TestEqual(TEXT("The failed terminal event is load failed"), Completed.Type, EOpenMobileAdsEventType::LoadFailed);
		TestEqual(TEXT("Failed load has the normalized placement"), Completed.Placement, FName(TEXT("CallbackFailure")));
		TestEqual(TEXT("Failed load has the normalized format"), Completed.Format, EOpenMobileAdFormat::Rewarded);
		TestEqual(TEXT("Failed load has the normalized provider"), Completed.Provider, Provider.Name);
		TestEqual(TEXT("Failed load has the accepted request ID"), Completed.RequestId, FailedLoad.RequestId);
		TestEqual(TEXT("Failed load normalizes the error placement"), Completed.Error.Placement, FName(TEXT("CallbackFailure")));
		TestEqual(TEXT("Failed load normalizes the error provider"), Completed.Error.Provider, Provider.Name);
		TestEqual(TEXT("Failed load reports failed state"), Completed.PlacementState, EOpenMobileAdPlacementState::Failed);
	}

	FOpenMobileAdsEvent LateLoaded;
	LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
	LateLoaded.CachedAdId = FGuid::NewGuid();
	FailureSink->Submit(MoveTemp(LateLoaded));
	FOpenMobileAdsEvent LateFailure;
	LateFailure.Type = EOpenMobileAdsEventType::LoadFailed;
	FailureSink->Submit(MoveTemp(LateFailure));
	DrainGameThreadTasks();
	TestEqual(TEXT("Late terminal callbacks are ignored"), Events.Num(), 4);

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
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
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));

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
		Loaded.CachedAdId = FGuid::NewGuid();
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
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));
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
	const FOpenMobileAdsInitializationStatusSnapshot InitializationStatus =
		Subsystem->GetInitializationStatus();
	TestEqual(TEXT("Unregistration fails initialization status"), InitializationStatus.ServiceState, EOpenMobileAdsServiceState::Failed);
	TestEqual(TEXT("Initialization status keeps unregistration detail"), InitializationStatus.Error.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);
	const FOpenMobileAdsInitializationComponentStatus* ProviderStatus =
		FindInitializationComponent(
			InitializationStatus,
			EOpenMobileAdsInitializationComponentType::Provider,
			TEXT("MockAds")
		);
	TestNotNull(TEXT("The unregistered provider remains inspectable"), ProviderStatus);
	if (ProviderStatus)
	{
		TestEqual(TEXT("The unregistered provider reports failure"), ProviderStatus->State, EOpenMobileAdsInitializationState::Failed);
	}

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
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));
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
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("DestroyFailureReward");
	Placement.Android.AdUnitId = TEXT("android-destroy-failure");
	Placement.IOS.AdUnitId = TEXT("ios-destroy-failure");
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));
	TestTrue(
		TEXT("The placement loads before destroy-all failure"),
		Subsystem->LoadAd(TEXT("DestroyFailureReward")).bAccepted
	);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	TestTrue(TEXT("The placement is ready before destroy-all failure"), Subsystem->IsReady(TEXT("DestroyFailureReward")));
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
	const FOpenMobileAdsPlacementStatus FailedStatus =
		Subsystem->GetPlacementStatus(TEXT("DestroyFailureReward"));
	TestEqual(TEXT("Destroy-all failure leaves a typed failed state"), FailedStatus.State, EOpenMobileAdPlacementState::Failed);
	TestEqual(TEXT("Destroy-all failure keeps its typed error"), FailedStatus.LastError.Code, EOpenMobileAdsErrorCode::ProviderFailure);
	TestFalse(TEXT("Destroy-all failure cannot revive cached readiness"), Subsystem->IsReady(TEXT("DestroyFailureReward")));
	TestEqual(TEXT("Destroy-all failure releases the cached native ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Destroy-all failure releases the matching cache"), Provider.ReleasedCachedAds[0], CachedAdId);
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
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));
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
