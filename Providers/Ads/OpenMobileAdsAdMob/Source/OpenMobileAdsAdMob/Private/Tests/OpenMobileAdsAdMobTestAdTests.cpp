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
		int64 InitializationRequestId = 0;
		int64 LaunchRequestId = 0;
		FOpenMobileAdsInitializationRequest InitializationRequest;
		FString LaunchedAdUnitId;
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
	if (FirstSink->Events.Num() == 1)
	{
		TestEqual(
			TEXT("Successful native completion emits Loaded"),
			FirstSink->Events[0].Type,
			EOpenMobileAdsEventType::Loaded
		);
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
	TestEqual(
		TEXT("A successful replacement releases the previous native ad"),
		Backend.CancelledRequestIds.Num(),
		2
	);
	if (Backend.CancelledRequestIds.Num() == 2)
	{
		TestEqual(
			TEXT("Replacement releases the first completed native load"),
			Backend.CancelledRequestIds[1],
			Backend.LoadRequestIds[0]
		);
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
