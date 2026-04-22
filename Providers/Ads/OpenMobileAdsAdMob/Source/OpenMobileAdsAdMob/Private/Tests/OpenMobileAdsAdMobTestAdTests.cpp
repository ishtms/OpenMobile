#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsAdMobBackend.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileAdsAdMobPlatform.h"
#include "OpenMobileAdsAdMobSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OpenMobileAdsAdMobTestAdTests
{
	class FMockBackend final : public IOpenMobileAdsAdMobBackend
	{
	public:
		virtual FName GetBackendName() const override { return TEXT("MockAdMob"); }
		virtual bool IsAvailable() const override { return true; }

		virtual bool Initialize(
			const FOpenMobileAdsInitializationRequest& Request,
			int64 RequestId,
			FString& OutError
		) override
		{
			++InitializationCalls;
			InitializationRequest = Request;
			InitializationRequestId = RequestId;
			return true;
		}

		virtual void Shutdown() override
		{
			++ShutdownCalls;
		}

		virtual bool LoadRewardedAd(
			const FString& AdUnitId,
			int64 RequestId,
			FString& OutError
		) override
		{
			LoadedAdUnitIds.Add(AdUnitId);
			LoadRequestIds.Add(RequestId);
			return true;
		}

		virtual void CancelRewardedAd(int64 RequestId) override
		{
			CancelledRequestIds.Add(RequestId);
		}

		virtual bool ShowRewardedAd(
			int64 LoadedRequestId,
			int64 InShowRequestId,
			const FString& ServerVerificationCustomData,
			FString& OutError
		) override
		{
			++ShowCalls;
			ShownLoadedRequestId = LoadedRequestId;
			ShowRequestId = InShowRequestId;
			ShownServerVerificationCustomData = ServerVerificationCustomData;
			return true;
		}

		virtual bool LaunchRewardedAd(
			const FString& AdUnitId,
			int64 RequestId,
			FString& OutError
		) override
		{
			++LaunchCalls;
			LaunchedAdUnitId = AdUnitId;
			LaunchRequestId = RequestId;
			return true;
		}

		int32 InitializationCalls = 0;
		int32 ShutdownCalls = 0;
		int32 LaunchCalls = 0;
		int32 ShowCalls = 0;
		int64 InitializationRequestId = 0;
		int64 LaunchRequestId = 0;
		int64 ShownLoadedRequestId = 0;
		int64 ShowRequestId = 0;
		FOpenMobileAdsInitializationRequest InitializationRequest;
		FString LaunchedAdUnitId;
		FString ShownServerVerificationCustomData;
		TArray<FString> LoadedAdUnitIds;
		TArray<int64> LoadRequestIds;
		TArray<int64> CancelledRequestIds;
	};

	class FScopedBackendRegistration
	{
	public:
		explicit FScopedBackendRegistration(FMockBackend& InBackend)
			: Backend(InBackend)
		{
			IModularFeatures::Get().RegisterModularFeature(
				IOpenMobileAdsAdMobBackend::GetModularFeatureName(),
				&Backend
			);
		}

		~FScopedBackendRegistration()
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsAdMobBackend::GetModularFeatureName(),
				&Backend
			);
		}

	private:
		FMockBackend& Backend;
	};

	class FInitializationSink final : public IOpenMobileAdsProviderInitializationSink
	{
	public:
		virtual void UpdateStatus(FOpenMobileAdsInitializationComponentStatus Status) override
		{
		}

		virtual void Complete(FOpenMobileAdsError Error) override
		{
			++CompletionCalls;
			CompletionError = MoveTemp(Error);
		}

		virtual void Invalidate() override
		{
			bInvalidated = true;
		}

		int32 CompletionCalls = 0;
		bool bInvalidated = false;
		FOpenMobileAdsError CompletionError;
	};

	class FEventSink final : public IOpenMobileAdsProviderEventSink
	{
	public:
		virtual void Submit(FOpenMobileAdsEvent Event) override
		{
			if (!bInvalidated)
			{
				Events.Add(MoveTemp(Event));
			}
		}

		virtual void Invalidate() override
		{
			bInvalidated = true;
		}

		TArray<FOpenMobileAdsEvent> Events;
		bool bInvalidated = false;
	};

	class FScopedSettings
	{
	public:
		FScopedSettings()
		{
			Settings = GetMutableDefault<UOpenMobileAdsAdMobSettings>();
			AndroidAppId = Settings->AndroidAppId;
			AndroidRewardedAdUnitId = Settings->AndroidRewardedAdUnitId;
			IOSAppId = Settings->IOSAppId;
			IOSRewardedAdUnitId = Settings->IOSRewardedAdUnitId;
			Settings->AndroidAppId = TEXT("ca-app-pub-3940256099942544~3347511713");
			Settings->AndroidRewardedAdUnitId =
				TEXT("ca-app-pub-3940256099942544/5224354917");
			Settings->IOSAppId = TEXT("ca-app-pub-3940256099942544~1458002511");
			Settings->IOSRewardedAdUnitId =
				TEXT("ca-app-pub-3940256099942544/1712485313");
		}

		~FScopedSettings()
		{
			Settings->AndroidAppId = MoveTemp(AndroidAppId);
			Settings->AndroidRewardedAdUnitId = MoveTemp(AndroidRewardedAdUnitId);
			Settings->IOSAppId = MoveTemp(IOSAppId);
			Settings->IOSRewardedAdUnitId = MoveTemp(IOSRewardedAdUnitId);
		}

		UOpenMobileAdsAdMobSettings* Settings = nullptr;

	private:
		FString AndroidAppId;
		FString AndroidRewardedAdUnitId;
		FString IOSAppId;
		FString IOSRewardedAdUnitId;
	};

	IOpenMobileAdsProvider* FindProvider()
	{
		const TArray<IOpenMobileAdsProvider*> Providers =
			IModularFeatures::Get().GetModularFeatureImplementations<IOpenMobileAdsProvider>(
				IOpenMobileAdsProvider::GetModularFeatureName()
			);
		for (IOpenMobileAdsProvider* Provider : Providers)
		{
			if (Provider && Provider->GetProviderName() == TEXT("AdMob"))
			{
				return Provider;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobCoppaPropagationTest,
	"OpenMobile.Ads.AdMob.Privacy.CoppaPropagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobCoppaPropagationTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FScopedSettings ScopedSettings;
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}
	Provider->Shutdown();

	const EOpenMobileAdsAgeTreatment Treatments[] = {
		EOpenMobileAdsAgeTreatment::Unspecified,
		EOpenMobileAdsAgeTreatment::Yes,
		EOpenMobileAdsAgeTreatment::No
	};
	for (const EOpenMobileAdsAgeTreatment Treatment : Treatments)
	{
		FOpenMobileAdsInitializationRequest Request;
		Request.Platform = EOpenMobileAdsPlatform::Android;
		Request.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
		Request.Privacy.ChildDirectedTreatment = Treatment;
		const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> Sink =
			MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
		FOpenMobileAdsError Error;
		TestTrue(
			TEXT("AdMob accepts COPPA configuration"),
			Provider->Initialize(Request, Sink, Error)
		);
		TestEqual(
			TEXT("AdMob forwards COPPA configuration to its native backend"),
			Backend.InitializationRequest.Privacy.ChildDirectedTreatment,
			Treatment
		);
		FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
			Backend.InitializationRequestId
		);
		Provider->Shutdown();
	}

	TestEqual(
		TEXT("Each COPPA state initializes the native backend once"),
		Backend.InitializationCalls,
		static_cast<int32>(UE_ARRAY_COUNT(Treatments))
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobUnderAgePropagationTest,
	"OpenMobile.Ads.AdMob.Privacy.UnderAgePropagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobUnderAgePropagationTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FScopedSettings ScopedSettings;
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}
	Provider->Shutdown();

	const EOpenMobileAdsAgeTreatment Treatments[] = {
		EOpenMobileAdsAgeTreatment::Unspecified,
		EOpenMobileAdsAgeTreatment::No,
		EOpenMobileAdsAgeTreatment::Yes
	};
	for (const EOpenMobileAdsAgeTreatment Treatment : Treatments)
	{
		FOpenMobileAdsInitializationRequest Request;
		Request.Platform = EOpenMobileAdsPlatform::Android;
		Request.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
		Request.Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::No;
		Request.Privacy.UnderAgeOfConsent = Treatment;
		const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> Sink =
			MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
		FOpenMobileAdsError Error;
		TestTrue(
			TEXT("AdMob accepts under-age configuration"),
			Provider->Initialize(Request, Sink, Error)
		);
		TestEqual(
			TEXT("AdMob forwards under-age configuration to its native backend"),
			Backend.InitializationRequest.Privacy.UnderAgeOfConsent,
			Treatment
		);
		TestEqual(
			TEXT("AdMob keeps under-age treatment separate from COPPA"),
			Backend.InitializationRequest.Privacy.ChildDirectedTreatment,
			EOpenMobileAdsAgeTreatment::No
		);
		FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
			Backend.InitializationRequestId
		);
		Provider->Shutdown();
	}

	TestEqual(
		TEXT("Each under-age state initializes the native backend once"),
		Backend.InitializationCalls,
		static_cast<int32>(UE_ARRAY_COUNT(Treatments))
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobLoadContractTest,
	"OpenMobile.Ads.AdMob.Load.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobLoadContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FScopedSettings ScopedSettings;
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}
	Provider->Shutdown();

	const FOpenMobileAdsProviderCapabilities Capabilities = Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* Rewarded =
		Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded);
	TestNotNull(TEXT("AdMob reports rewarded capabilities"), Rewarded);
	if (Rewarded)
	{
		TestTrue(TEXT("AdMob advertises rewarded loading"), Rewarded->bCanLoad);
		TestTrue(TEXT("AdMob advertises rewarded destruction"), Rewarded->bCanDestroy);
		TestEqual(TEXT("AdMob caches one rewarded ad per placement"), Rewarded->MaxCachedAdsPerPlacement, 1);
		TestEqual(TEXT("AdMob rewarded caches expire after one hour"), Rewarded->CacheLifetimeSeconds, 3600.0);
	}

	FOpenMobileAdsInitializationRequest Initialization;
	Initialization.RequestId = FGuid::NewGuid();
	Initialization.Platform = EOpenMobileAdsPlatform::Android;
	Initialization.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> InitializationSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError InitializationError;
	TestTrue(
		TEXT("AdMob starts before loads"),
		Provider->Initialize(Initialization, InitializationSink, InitializationError)
	);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);

	FOpenMobileAdsLoadRequest First;
	First.RequestId = FGuid::NewGuid();
	First.Placement.Placement = TEXT("RewardOne");
	First.Placement.Format = EOpenMobileAdFormat::Rewarded;
	First.Placement.AdUnitId = TEXT("production-unit-one");
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> FirstSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError FirstError;
	TestTrue(
		TEXT("The first named rewarded load starts"),
		Provider->Load(First, FirstSink, FirstError)
	);

	FOpenMobileAdsLoadRequest Second;
	Second.RequestId = FGuid::NewGuid();
	Second.Placement.Placement = TEXT("RewardTwo");
	Second.Placement.Format = EOpenMobileAdFormat::Rewarded;
	Second.Placement.AdUnitId = TEXT("production-unit-two");
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> SecondSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError SecondError;
	TestTrue(
		TEXT("A different named rewarded placement can load concurrently"),
		Provider->Load(Second, SecondSink, SecondError)
	);
	TestEqual(TEXT("Both loads reach the backend"), Backend.LoadRequestIds.Num(), 2);
	for (const FString& AdUnitId : Backend.LoadedAdUnitIds)
	{
		TestEqual(
			TEXT("Test mode uses Google's Android rewarded test ID"),
			AdUnitId,
			FString(TEXT("ca-app-pub-3940256099942544/5224354917"))
		);
	}

	Provider->Cancel(Second.RequestId);
	TestEqual(TEXT("Cancellation reaches the native backend"), Backend.CancelledRequestIds.Num(), 1);
	if (Backend.CancelledRequestIds.Num() == 1 && Backend.LoadRequestIds.Num() == 2)
	{
		TestEqual(
			TEXT("Cancellation targets the matching native load"),
			Backend.CancelledRequestIds[0],
			Backend.LoadRequestIds[1]
		);
		FOpenMobileAdsAdMobPlatform::NativeRewardedLoadCompleted(
			Backend.LoadRequestIds[1]
		);
	}
	TestTrue(TEXT("A cancelled load ignores late completion"), SecondSink->Events.IsEmpty());

	if (!Backend.LoadRequestIds.IsEmpty())
	{
		FOpenMobileAdsAdMobPlatform::NativeRewardedLoadCompleted(
			Backend.LoadRequestIds[0]
		);
	}
	TestEqual(TEXT("A successful load emits one terminal event"), FirstSink->Events.Num(), 1);
	FGuid FirstCachedAdId;
	if (FirstSink->Events.Num() == 1)
	{
		TestEqual(
			TEXT("Successful native completion emits Loaded"),
			FirstSink->Events[0].Type,
			EOpenMobileAdsEventType::Loaded
		);
		FirstCachedAdId = FirstSink->Events[0].CachedAdId;
		TestTrue(TEXT("A successful load returns an opaque cache ID"), FirstCachedAdId.IsValid());
	}

	FOpenMobileAdsLoadRequest Replacement = First;
	Replacement.RequestId = FGuid::NewGuid();
	Replacement.Options.bForceReload = true;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ReplacementSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError ReplacementError;
	TestTrue(
		TEXT("A placement can replace its completed native load"),
		Provider->Load(Replacement, ReplacementSink, ReplacementError)
	);
	if (Backend.LoadRequestIds.Num() == 3)
	{
		FOpenMobileAdsAdMobPlatform::NativeRewardedLoadCompleted(
			Backend.LoadRequestIds[2]
		);
	}
	TestEqual(TEXT("A replacement load emits one terminal event"), ReplacementSink->Events.Num(), 1);
	if (ReplacementSink->Events.Num() == 1)
	{
		TestTrue(
			TEXT("Each native load receives a distinct cache ID"),
			ReplacementSink->Events[0].CachedAdId.IsValid()
			&& ReplacementSink->Events[0].CachedAdId != FirstCachedAdId
		);
	}
	Provider->ReleaseCachedAd(FirstCachedAdId);
	TestEqual(TEXT("Releasing a cache ID reaches the backend"), Backend.CancelledRequestIds.Num(), 2);
	if (Backend.CancelledRequestIds.Num() == 2)
	{
		TestEqual(TEXT("Cache release targets its native load"), Backend.CancelledRequestIds[1], Backend.LoadRequestIds[0]);
	}
	FOpenMobileAdsDestroyRequest Destroy;
	Destroy.RequestId = FGuid::NewGuid();
	Destroy.Placement = TEXT("RewardOne");
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> DestroySink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError DestroyError;
	TestTrue(TEXT("AdMob accepts service-owned cache destruction"), Provider->Destroy(Destroy, DestroySink, DestroyError));
	TestEqual(TEXT("AdMob destroy completes once"), DestroySink->Events.Num(), 1);
	if (DestroySink->Events.Num() == 1)
	{
		TestEqual(TEXT("AdMob destroy emits Destroyed"), DestroySink->Events[0].Type, EOpenMobileAdsEventType::Destroyed);
	}

	FOpenMobileAdsLoadRequest Failed;
	Failed.RequestId = FGuid::NewGuid();
	Failed.Placement.Placement = TEXT("RewardFailure");
	Failed.Placement.Format = EOpenMobileAdFormat::Rewarded;
	Failed.Placement.AdUnitId = TEXT("production-unit-failure");
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> FailedSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError FailedError;
	TestTrue(
		TEXT("A later rewarded load starts after completion"),
		Provider->Load(Failed, FailedSink, FailedError)
	);
	if (Backend.LoadRequestIds.Num() == 4)
	{
		FOpenMobileAdsAdMobPlatform::NativeRewardedLoadFailed(
			Backend.LoadRequestIds[3],
			TEXT("test native load failed")
		);
	}
	TestEqual(TEXT("A native load failure emits one terminal event"), FailedSink->Events.Num(), 1);
	if (FailedSink->Events.Num() == 1)
	{
		TestEqual(
			TEXT("Native failure emits LoadFailed"),
			FailedSink->Events[0].Type,
			EOpenMobileAdsEventType::LoadFailed
		);
		TestEqual(
			TEXT("Native load failure is typed"),
			FailedSink->Events[0].Error.Code,
			EOpenMobileAdsErrorCode::NativeFailure
		);
	}

	Provider->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobShowContractTest,
	"OpenMobile.Ads.AdMob.Show.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobShowContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FScopedSettings ScopedSettings;
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}
	Provider->Shutdown();

	const FOpenMobileAdsProviderCapabilities Capabilities = Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* Rewarded =
		Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded);
	TestNotNull(TEXT("AdMob reports rewarded capabilities"), Rewarded);
	if (Rewarded)
	{
		TestTrue(TEXT("AdMob advertises reusable rewarded showing"), Rewarded->bCanShow);
		TestTrue(TEXT("AdMob reports rewarded impressions"), Rewarded->bReportsImpression);
		TestTrue(TEXT("AdMob reports rewarded clicks"), Rewarded->bReportsClick);
		TestTrue(TEXT("AdMob reports rewarded revenue"), Rewarded->bReportsRevenue);
		TestTrue(
			TEXT("AdMob supports rewarded server verification"),
			Rewarded->bSupportsServerVerification
		);
	}

	FOpenMobileAdsInitializationRequest Initialization;
	Initialization.RequestId = FGuid::NewGuid();
	Initialization.Platform = EOpenMobileAdsPlatform::Android;
	Initialization.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> InitializationSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError InitializationError;
	TestTrue(
		TEXT("AdMob initializes before reusable rewarded showing"),
		Provider->Initialize(Initialization, InitializationSink, InitializationError)
	);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);

	FOpenMobileAdsLoadRequest Load;
	Load.RequestId = FGuid::NewGuid();
	Load.Placement.Placement = TEXT("ReusableReward");
	Load.Placement.Format = EOpenMobileAdFormat::Rewarded;
	Load.Placement.AdUnitId = TEXT("production-reusable-reward");
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> LoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError LoadError;
	TestTrue(
		TEXT("The reusable rewarded ad starts loading"),
		Provider->Load(Load, LoadSink, LoadError)
	);
	if (!Backend.LoadRequestIds.IsEmpty())
	{
		FOpenMobileAdsAdMobPlatform::NativeRewardedLoadCompleted(
			Backend.LoadRequestIds.Last()
		);
	}
	TestEqual(TEXT("The reusable load completes once"), LoadSink->Events.Num(), 1);
	if (LoadSink->Events.IsEmpty())
	{
		Provider->Shutdown();
		return false;
	}

	FOpenMobileAdsShowRequest Show;
	Show.RequestId = FGuid::NewGuid();
	Show.CachedAdId = LoadSink->Events[0].CachedAdId;
	Show.Placement = TEXT("ReusableReward");
	Show.Format = EOpenMobileAdFormat::Rewarded;
	Show.Options.ServerVerificationCustomData = TEXT("player-42");
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ShowSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError ShowError;
	TestTrue(
		TEXT("AdMob presents the exact cached rewarded ad"),
		Provider->Show(Show, ShowSink, ShowError)
	);
	TestEqual(TEXT("Reusable show reaches the backend once"), Backend.ShowCalls, 1);
	if (!Backend.LoadRequestIds.IsEmpty())
	{
		TestEqual(
			TEXT("Reusable show consumes the matching native cache"),
			Backend.ShownLoadedRequestId,
			Backend.LoadRequestIds.Last()
		);
	}
	TestEqual(
		TEXT("Reusable show forwards server verification custom data"),
		Backend.ShownServerVerificationCustomData,
		FString(TEXT("player-42"))
	);

	FOpenMobileAdsAdMobPlatform::NativeShown(Backend.ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeImpression(Backend.ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeClicked(Backend.ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
		Backend.ShowRequestId,
		12345,
		TEXT("USD"),
		static_cast<int32>(EOpenMobileAdsRevenuePrecision::Precise)
	);
	FOpenMobileAdsAdMobPlatform::NativeEarned(
		Backend.ShowRequestId,
		7,
		TEXT("coin")
	);
	FOpenMobileAdsAdMobPlatform::NativeEarned(
		Backend.ShowRequestId,
		99,
		TEXT("duplicate")
	);
	FOpenMobileAdsAdMobPlatform::NativeClosed(Backend.ShowRequestId);
	const EOpenMobileAdsEventType ExpectedTypes[] = {
		EOpenMobileAdsEventType::Shown,
		EOpenMobileAdsEventType::Impression,
		EOpenMobileAdsEventType::Clicked,
		EOpenMobileAdsEventType::RevenuePaid,
		EOpenMobileAdsEventType::RewardEarned,
		EOpenMobileAdsEventType::Dismissed
	};
	TestEqual(
		TEXT("Reusable show emits one normalized callback lifecycle"),
		ShowSink->Events.Num(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes))
	);
	if (ShowSink->Events.Num() == static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes)))
	{
		for (int32 Index = 0; Index < ShowSink->Events.Num(); ++Index)
		{
			TestEqual(
				TEXT("Reusable show preserves callback order"),
				ShowSink->Events[Index].Type,
				ExpectedTypes[Index]
			);
		}
		TestEqual(
			TEXT("Reusable show preserves reward amount"),
			ShowSink->Events[4].Reward.Amount,
			static_cast<int64>(7)
		);
		TestEqual(
			TEXT("Reusable show preserves revenue micros"),
			ShowSink->Events[3].Revenue.ValueMicros,
			static_cast<int64>(12345)
		);
	}

	Provider->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobTestAdFlowTest,
	"OpenMobile.Ads.AdMob.TestAds.RewardedFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobTestAdFlowTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FScopedSettings ScopedSettings;
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}
	const FOpenMobileAdsProviderCapabilities Capabilities = Provider->GetCapabilities();
	TestEqual(
		TEXT("Every currently supported format has a test-ad contract"),
		Capabilities.Formats.Num(),
		1
	);
	TestTrue(
		TEXT("The supported rewarded format has a test-ad contract"),
		Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded) != nullptr
	);
	Provider->Shutdown();

	FOpenMobileAdsInitializationRequest ProductionRequest;
	ProductionRequest.Platform = EOpenMobileAdsPlatform::Android;
	ProductionRequest.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(false);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> ProductionSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError ProductionError;
	const bool bProductionStarted = Provider->Initialize(
		ProductionRequest,
		ProductionSink,
		ProductionError
	);
	TestFalse(
		TEXT("Production mode rejects Google sample identifiers before native initialization"),
		bProductionStarted
	);
	TestEqual(
		TEXT("Mixed test and production identifiers use a configuration error"),
		ProductionError.Code,
		EOpenMobileAdsErrorCode::NotConfigured
	);
	TestEqual(
		TEXT("Rejected production configuration never reaches the native SDK"),
		Backend.InitializationCalls,
		0
	);
	if (bProductionStarted)
	{
		Provider->Shutdown();
		return false;
	}

	FOpenMobileAdsInitializationRequest TestRequest;
	TestRequest.Platform = EOpenMobileAdsPlatform::Android;
	TestRequest.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> TestSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError InitializationError;
	TestTrue(
		TEXT("Development mode accepts Google sample identifiers"),
		Provider->Initialize(TestRequest, TestSink, InitializationError)
	);
	TestEqual(TEXT("The native SDK initializes once"), Backend.InitializationCalls, 1);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);
	TestEqual(TEXT("Initialization completes once"), TestSink->CompletionCalls, 1);
	TestFalse(TEXT("Test initialization completes without an error"), TestSink->CompletionError.IsSet());

	int32 LoadedCalls = 0;
	int32 ShownCalls = 0;
	int32 EarnedCalls = 0;
	int32 ClosedCalls = 0;
	int32 RewardAmount = 0;
	FString RewardType;
	FOpenMobileRewardedAdCallbacks Callbacks;
	Callbacks.OnLoaded.BindLambda([&LoadedCalls]() { ++LoadedCalls; });
	Callbacks.OnShown.BindLambda([&ShownCalls]() { ++ShownCalls; });
	Callbacks.OnEarned.BindLambda(
		[&EarnedCalls, &RewardAmount, &RewardType](int32 Amount, FString Type)
		{
			++EarnedCalls;
			RewardAmount = Amount;
			RewardType = MoveTemp(Type);
		}
	);
	Callbacks.OnClosed.BindLambda([&ClosedCalls]() { ++ClosedCalls; });
	FOpenMobileError RequestError;
	TestTrue(
		TEXT("The provider starts the rewarded test ad"),
		Provider->RequestAndShowRewardedAd(MoveTemp(Callbacks), RequestError)
	);
	TestEqual(
		TEXT("Android uses Google's official rewarded test ad-unit ID"),
		Backend.LaunchedAdUnitId,
		FString(TEXT("ca-app-pub-3940256099942544/5224354917"))
	);

	FOpenMobileAdsAdMobPlatform::NativeLoaded(Backend.LaunchRequestId);
	FOpenMobileAdsAdMobPlatform::NativeShown(Backend.LaunchRequestId);
	FOpenMobileAdsAdMobPlatform::NativeEarned(
		Backend.LaunchRequestId,
		7,
		TEXT("coin")
	);
	FOpenMobileAdsAdMobPlatform::NativeEarned(
		Backend.LaunchRequestId,
		99,
		TEXT("duplicate")
	);
	FOpenMobileAdsAdMobPlatform::NativeClosed(Backend.LaunchRequestId);
	TestEqual(TEXT("Loaded is reported once"), LoadedCalls, 1);
	TestEqual(TEXT("Shown is reported once"), ShownCalls, 1);
	TestEqual(TEXT("Reward is reported once"), EarnedCalls, 1);
	TestEqual(TEXT("The provider preserves the network reward amount"), RewardAmount, 7);
	TestEqual(TEXT("The provider preserves the network reward type"), RewardType, FString(TEXT("coin")));
	TestEqual(TEXT("Closed is reported once"), ClosedCalls, 1);

	int32 FailedCalls = 0;
	FOpenMobileError Failure;
	FOpenMobileRewardedAdCallbacks FailureCallbacks;
	FailureCallbacks.OnFailed.BindLambda(
		[&FailedCalls, &Failure](FOpenMobileError Error)
		{
			++FailedCalls;
			Failure = MoveTemp(Error);
		}
	);
	FOpenMobileError SecondRequestError;
	TestTrue(
		TEXT("A new test ad can start after close"),
		Provider->RequestAndShowRewardedAd(
			MoveTemp(FailureCallbacks),
			SecondRequestError
		)
	);
	FOpenMobileAdsAdMobPlatform::NativeFailed(
		Backend.LaunchRequestId,
		TEXT("test load failed")
	);
	TestEqual(TEXT("Native failures are reported once"), FailedCalls, 1);
	TestEqual(TEXT("Native failures keep their category"), Failure.Code, EOpenMobileErrorCode::NativeFailure);
	TestTrue(TEXT("Native failure details are preserved"), Failure.Message.Contains(TEXT("test load failed")));

	Provider->Shutdown();
	TestEqual(TEXT("Provider shutdown reaches the native backend"), Backend.ShutdownCalls, 1);

	FOpenMobileAdsInitializationRequest IOSTestRequest;
	IOSTestRequest.Platform = EOpenMobileAdsPlatform::IOS;
	IOSTestRequest.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> IOSTestSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError IOSInitializationError;
	TestTrue(
		TEXT("iOS development mode accepts Google sample identifiers"),
		Provider->Initialize(IOSTestRequest, IOSTestSink, IOSInitializationError)
	);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);
	TestEqual(TEXT("iOS initialization completes once"), IOSTestSink->CompletionCalls, 1);

	int32 IOSLoadedCalls = 0;
	int32 IOSShownCalls = 0;
	int32 IOSEarnedCalls = 0;
	int32 IOSClosedCalls = 0;
	FOpenMobileRewardedAdCallbacks IOSCallbacks;
	IOSCallbacks.OnLoaded.BindLambda([&IOSLoadedCalls]() { ++IOSLoadedCalls; });
	IOSCallbacks.OnShown.BindLambda([&IOSShownCalls]() { ++IOSShownCalls; });
	IOSCallbacks.OnEarned.BindLambda(
		[&IOSEarnedCalls](int32 Amount, FString Type)
		{
			++IOSEarnedCalls;
		}
	);
	IOSCallbacks.OnClosed.BindLambda([&IOSClosedCalls]() { ++IOSClosedCalls; });
	FOpenMobileError IOSRequestError;
	TestTrue(
		TEXT("The provider starts the iOS rewarded test ad"),
		Provider->RequestAndShowRewardedAd(MoveTemp(IOSCallbacks), IOSRequestError)
	);
	TestEqual(
		TEXT("iOS uses Google's official rewarded test ad-unit ID"),
		Backend.LaunchedAdUnitId,
		FString(TEXT("ca-app-pub-3940256099942544/1712485313"))
	);
	FOpenMobileAdsAdMobPlatform::NativeLoaded(Backend.LaunchRequestId);
	FOpenMobileAdsAdMobPlatform::NativeShown(Backend.LaunchRequestId);
	FOpenMobileAdsAdMobPlatform::NativeEarned(
		Backend.LaunchRequestId,
		1,
		TEXT("reward")
	);
	FOpenMobileAdsAdMobPlatform::NativeClosed(Backend.LaunchRequestId);
	TestEqual(TEXT("iOS loaded is reported once"), IOSLoadedCalls, 1);
	TestEqual(TEXT("iOS shown is reported once"), IOSShownCalls, 1);
	TestEqual(TEXT("iOS reward is reported once"), IOSEarnedCalls, 1);
	TestEqual(TEXT("iOS closed is reported once"), IOSClosedCalls, 1);

	Provider->Shutdown();
	TestEqual(TEXT("Both platform sessions shut down cleanly"), Backend.ShutdownCalls, 2);
	return true;
}

#endif
