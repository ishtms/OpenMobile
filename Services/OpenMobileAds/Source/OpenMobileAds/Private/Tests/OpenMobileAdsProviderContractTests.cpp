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
		int32 InitializationCalls = 0;
		int32 ShutdownCalls = 0;
		FOpenMobileAdsProviderCapabilities Capabilities;
		FOpenMobileAdsInitializationRequest LastInitializationRequest;
		FOpenMobileAdsError InitializationRejection;
		FOpenMobileAdsLoadRequest LastLoadRequest;
		FOpenMobileAdsShowRequest LastShowRequest;
		FOpenMobileAdsDestroyRequest LastDestroyRequest;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> LoadSink;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShowSink;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> DestroySink;
		TSharedPtr<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> InitializationSink;
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
		FMockProvider& Provider
	)
	{
		const FOpenMobileAdsOperationResult Result = Subsystem.InitializeAds();
		if (!Result.bAccepted)
		{
			return false;
		}
		Provider.CompleteInitialization();
		DrainGameThreadTasks();
		return Subsystem.GetServiceState() == EOpenMobileAdsServiceState::Ready;
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
