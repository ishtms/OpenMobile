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
