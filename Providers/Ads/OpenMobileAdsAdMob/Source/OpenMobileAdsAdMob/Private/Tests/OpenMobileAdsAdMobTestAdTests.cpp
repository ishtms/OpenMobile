#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsAdMobBackend.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileAdsAdMobConsentMapper.h"
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

		virtual bool RequestConsentInfo(
			const FOpenMobileAdsConsentRequest& Request,
			int64 RequestId,
			FString& OutError
		) override
		{
			++ConsentRefreshCalls;
			ConsentRequest = Request;
			ConsentRefreshRequestId = RequestId;
			return true;
		}

		virtual bool PresentRequiredConsentForm(
			int64 RequestId,
			FString& OutError
		) override
		{
			++ConsentFormCalls;
			ConsentFormRequestId = RequestId;
			return true;
		}

		virtual bool PresentPrivacyOptionsForm(
			int64 RequestId,
			FString& OutError
		) override
		{
			++PrivacyOptionsFormCalls;
			PrivacyOptionsFormRequestId = RequestId;
			return true;
		}

		virtual bool ApplyConsentSignals(
			const FOpenMobileAdsConsentSignals& Signals,
			int32 SignalMask,
			FString& OutError
		) override
		{
			++ConsentSignalCalls;
			LastConsentSignals = Signals;
			LastConsentSignalMask = SignalMask;
			return true;
		}

		virtual bool ResetConsentForTesting(FString& OutError) override
		{
			++ConsentResetCalls;
			if (!bAcceptConsentReset)
			{
				OutError = ConsentResetError;
				return false;
			}
			return true;
		}

		virtual bool LoadRewardedAd(
			const FString& AdUnitId,
			int64 RequestId,
			EOpenMobileAdsDataProcessingMode DataProcessingMode,
			FString& OutError
		) override
		{
			LoadedAdUnitIds.Add(AdUnitId);
			LoadRequestIds.Add(RequestId);
			LoadDataProcessingModes.Add(DataProcessingMode);
			return true;
		}

		virtual bool LoadInterstitialAd(
			const FString& AdUnitId,
			int64 RequestId,
			EOpenMobileAdsDataProcessingMode DataProcessingMode,
			FString& OutError
		) override
		{
			InterstitialLoadedAdUnitIds.Add(AdUnitId);
			InterstitialLoadRequestIds.Add(RequestId);
			InterstitialLoadDataProcessingModes.Add(DataProcessingMode);
			return true;
		}

		virtual bool LoadRewardedInterstitialAd(
			const FString& AdUnitId,
			int64 RequestId,
			EOpenMobileAdsDataProcessingMode DataProcessingMode,
			FString& OutError
		) override
		{
			RewardedInterstitialLoadedAdUnitIds.Add(AdUnitId);
			RewardedInterstitialLoadRequestIds.Add(RequestId);
			RewardedInterstitialLoadDataProcessingModes.Add(DataProcessingMode);
			return true;
		}

		virtual bool LoadAppOpenAd(
			const FString& AdUnitId,
			int64 RequestId,
			EOpenMobileAdsDataProcessingMode DataProcessingMode,
			FString& OutError
		) override
		{
			AppOpenLoadedAdUnitIds.Add(AdUnitId);
			AppOpenLoadRequestIds.Add(RequestId);
			AppOpenLoadDataProcessingModes.Add(DataProcessingMode);
			return true;
		}

		virtual bool LoadBannerAd(
			const FString& AdUnitId,
			int64 RequestId,
			EOpenMobileAdsDataProcessingMode DataProcessingMode,
			EOpenMobileAdFormat Format,
			const FOpenMobileAdsBannerLayout& Layout,
			FString& OutError
		) override
		{
			BannerLoadedAdUnitIds.Add(AdUnitId);
			BannerLoadRequestIds.Add(RequestId);
			BannerLoadDataProcessingModes.Add(DataProcessingMode);
			BannerLoadFormats.Add(Format);
			BannerLoadLayouts.Add(Layout);
			return true;
		}

		virtual void CancelRewardedAd(int64 RequestId) override
		{
			CancelledRequestIds.Add(RequestId);
		}

		virtual void CancelInterstitialAd(int64 RequestId) override
		{
			CancelledInterstitialRequestIds.Add(RequestId);
		}

		virtual void CancelRewardedInterstitialAd(int64 RequestId) override
		{
			CancelledRewardedInterstitialRequestIds.Add(RequestId);
		}

		virtual void CancelAppOpenAd(int64 RequestId) override
		{
			CancelledAppOpenRequestIds.Add(RequestId);
		}

		virtual void CancelBannerAd(int64 RequestId) override
		{
			CancelledBannerRequestIds.Add(RequestId);
		}

		virtual bool ShowRewardedAd(
			int64 LoadedRequestId,
			int64 InShowRequestId,
			const FString& ServerVerificationUserId,
			const FString& ServerVerificationCustomData,
			FString& OutError
		) override
		{
			++ShowCalls;
			ShownLoadedRequestId = LoadedRequestId;
			ShowRequestId = InShowRequestId;
			ShownServerVerificationUserId = ServerVerificationUserId;
			ShownServerVerificationCustomData = ServerVerificationCustomData;
			return true;
		}

		virtual bool ShowInterstitialAd(
			int64 LoadedRequestId,
			int64 InShowRequestId,
			FString& OutError
		) override
		{
			++InterstitialShowCalls;
			ShownInterstitialLoadedRequestId = LoadedRequestId;
			InterstitialShowRequestId = InShowRequestId;
			return true;
		}

		virtual bool ShowRewardedInterstitialAd(
			int64 LoadedRequestId,
			int64 InShowRequestId,
			const FString& ServerVerificationUserId,
			const FString& ServerVerificationCustomData,
			FString& OutError
		) override
		{
			++RewardedInterstitialShowCalls;
			ShownRewardedInterstitialLoadedRequestId = LoadedRequestId;
			RewardedInterstitialShowRequestId = InShowRequestId;
			ShownRewardedInterstitialVerificationUserId =
				ServerVerificationUserId;
			ShownRewardedInterstitialVerificationData =
				ServerVerificationCustomData;
			return true;
		}

		virtual bool ShowAppOpenAd(
			int64 LoadedRequestId,
			int64 InShowRequestId,
			FString& OutError
		) override
		{
			++AppOpenShowCalls;
			ShownAppOpenLoadedRequestId = LoadedRequestId;
			AppOpenShowRequestId = InShowRequestId;
			return true;
		}

		virtual bool ShowBannerAd(
			int64 LoadedRequestId,
			int64 InShowRequestId,
			const FOpenMobileAdsBannerLayout& Layout,
			FString& OutError
		) override
		{
			++BannerShowCalls;
			ShownBannerLoadedRequestId = LoadedRequestId;
			BannerShowRequestId = InShowRequestId;
			ShownBannerLayout = Layout;
			return true;
		}

		virtual bool HideBannerAd(
			int64 LoadedRequestId,
			int64 InHideRequestId,
			FString& OutError
		) override
		{
			++BannerHideCalls;
			HiddenBannerLoadedRequestId = LoadedRequestId;
			BannerHideRequestId = InHideRequestId;
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
		int32 InterstitialShowCalls = 0;
		int32 RewardedInterstitialShowCalls = 0;
		int32 AppOpenShowCalls = 0;
		int32 BannerShowCalls = 0;
		int32 BannerHideCalls = 0;
		int32 ConsentRefreshCalls = 0;
		int32 ConsentFormCalls = 0;
		int32 PrivacyOptionsFormCalls = 0;
		int32 ConsentSignalCalls = 0;
		int32 ConsentResetCalls = 0;
		int32 LastConsentSignalMask = 0;
		int64 InitializationRequestId = 0;
		int64 LaunchRequestId = 0;
		int64 ShownLoadedRequestId = 0;
		int64 ShowRequestId = 0;
		int64 ShownInterstitialLoadedRequestId = 0;
		int64 InterstitialShowRequestId = 0;
		int64 ShownRewardedInterstitialLoadedRequestId = 0;
		int64 RewardedInterstitialShowRequestId = 0;
		int64 ShownAppOpenLoadedRequestId = 0;
		int64 AppOpenShowRequestId = 0;
		int64 ShownBannerLoadedRequestId = 0;
		int64 HiddenBannerLoadedRequestId = 0;
		int64 BannerShowRequestId = 0;
		int64 BannerHideRequestId = 0;
		int64 ConsentRefreshRequestId = 0;
		int64 ConsentFormRequestId = 0;
		int64 PrivacyOptionsFormRequestId = 0;
		FOpenMobileAdsInitializationRequest InitializationRequest;
		FOpenMobileAdsConsentRequest ConsentRequest;
		FOpenMobileAdsConsentSignals LastConsentSignals;
		FString LaunchedAdUnitId;
		FString ConsentResetError;
		FString ShownServerVerificationUserId;
		FString ShownServerVerificationCustomData;
		FString ShownRewardedInterstitialVerificationUserId;
		FString ShownRewardedInterstitialVerificationData;
		FOpenMobileAdsBannerLayout ShownBannerLayout;
		TArray<FString> LoadedAdUnitIds;
		TArray<FString> InterstitialLoadedAdUnitIds;
		TArray<FString> RewardedInterstitialLoadedAdUnitIds;
		TArray<FString> AppOpenLoadedAdUnitIds;
		TArray<FString> BannerLoadedAdUnitIds;
		bool bAcceptConsentReset = true;
		TArray<int64> LoadRequestIds;
		TArray<int64> InterstitialLoadRequestIds;
		TArray<int64> RewardedInterstitialLoadRequestIds;
		TArray<int64> AppOpenLoadRequestIds;
		TArray<int64> BannerLoadRequestIds;
		TArray<EOpenMobileAdsDataProcessingMode> LoadDataProcessingModes;
		TArray<EOpenMobileAdsDataProcessingMode> InterstitialLoadDataProcessingModes;
		TArray<EOpenMobileAdsDataProcessingMode>
			RewardedInterstitialLoadDataProcessingModes;
		TArray<EOpenMobileAdsDataProcessingMode> AppOpenLoadDataProcessingModes;
		TArray<EOpenMobileAdsDataProcessingMode> BannerLoadDataProcessingModes;
		TArray<EOpenMobileAdFormat> BannerLoadFormats;
		TArray<FOpenMobileAdsBannerLayout> BannerLoadLayouts;
		TArray<int64> CancelledRequestIds;
		TArray<int64> CancelledInterstitialRequestIds;
		TArray<int64> CancelledRewardedInterstitialRequestIds;
		TArray<int64> CancelledAppOpenRequestIds;
		TArray<int64> CancelledBannerRequestIds;
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

	class FConsentSink final : public IOpenMobileAdsConsentProviderSink
	{
	public:
		virtual void Complete(FOpenMobileAdsConsentStatusUpdate Update) override
		{
			++CompletionCalls;
		}

		virtual void Fail(FOpenMobileAdsError Error) override
		{
			++FailureCalls;
		}

		virtual void Invalidate() override
		{
			bInvalidated = true;
		}

		int32 CompletionCalls = 0;
		int32 FailureCalls = 0;
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
			AndroidInterstitialAdUnitId = Settings->AndroidInterstitialAdUnitId;
			AndroidBannerAdUnitId = Settings->AndroidBannerAdUnitId;
			IOSAppId = Settings->IOSAppId;
			IOSRewardedAdUnitId = Settings->IOSRewardedAdUnitId;
			IOSInterstitialAdUnitId = Settings->IOSInterstitialAdUnitId;
			IOSBannerAdUnitId = Settings->IOSBannerAdUnitId;
			TestDeviceIdentifiers = Settings->TestDeviceIdentifiers;
			Settings->AndroidAppId = TEXT("ca-app-pub-3940256099942544~3347511713");
			Settings->AndroidRewardedAdUnitId =
				TEXT("ca-app-pub-3940256099942544/5224354917");
			Settings->AndroidInterstitialAdUnitId =
				TEXT("ca-app-pub-3940256099942544/1033173712");
			Settings->AndroidBannerAdUnitId =
				TEXT("ca-app-pub-3940256099942544/6300978111");
			Settings->IOSAppId = TEXT("ca-app-pub-3940256099942544~1458002511");
			Settings->IOSRewardedAdUnitId =
				TEXT("ca-app-pub-3940256099942544/1712485313");
			Settings->IOSInterstitialAdUnitId =
				TEXT("ca-app-pub-3940256099942544/4411468910");
			Settings->IOSBannerAdUnitId =
				TEXT("ca-app-pub-3940256099942544/2435281174");
		}

		~FScopedSettings()
		{
			Settings->AndroidAppId = MoveTemp(AndroidAppId);
			Settings->AndroidRewardedAdUnitId = MoveTemp(AndroidRewardedAdUnitId);
			Settings->AndroidInterstitialAdUnitId = MoveTemp(AndroidInterstitialAdUnitId);
			Settings->AndroidBannerAdUnitId = MoveTemp(AndroidBannerAdUnitId);
			Settings->IOSAppId = MoveTemp(IOSAppId);
			Settings->IOSRewardedAdUnitId = MoveTemp(IOSRewardedAdUnitId);
			Settings->IOSInterstitialAdUnitId = MoveTemp(IOSInterstitialAdUnitId);
			Settings->IOSBannerAdUnitId = MoveTemp(IOSBannerAdUnitId);
			Settings->TestDeviceIdentifiers = MoveTemp(TestDeviceIdentifiers);
		}

		UOpenMobileAdsAdMobSettings* Settings = nullptr;

	private:
		FString AndroidAppId;
		FString AndroidRewardedAdUnitId;
		FString AndroidInterstitialAdUnitId;
		FString AndroidBannerAdUnitId;
		FString IOSAppId;
		FString IOSRewardedAdUnitId;
		FString IOSInterstitialAdUnitId;
		FString IOSBannerAdUnitId;
		TArray<FString> TestDeviceIdentifiers;
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
	FOpenMobileAdsAdMobGdprMappingTest,
	"OpenMobile.Ads.AdMob.Privacy.GdprMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobGdprMappingTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		EOpenMobileAdsAdMobUMPConsentStatus Input;
		bool bCanRequestAds;
		EOpenMobileAdsConsentStatus Status;
		EOpenMobileAdsGdprApplicability Applicability;
		EOpenMobileAdsConsentRequirement Requirement;
		EOpenMobileAdsConsentRequestState RequestState;
	};
	const FCase Cases[] = {
		{
			EOpenMobileAdsAdMobUMPConsentStatus::Unknown,
			false,
			EOpenMobileAdsConsentStatus::Unknown,
			EOpenMobileAdsGdprApplicability::Unknown,
			EOpenMobileAdsConsentRequirement::Unknown,
			EOpenMobileAdsConsentRequestState::Blocked
		},
		{
			EOpenMobileAdsAdMobUMPConsentStatus::NotRequired,
			true,
			EOpenMobileAdsConsentStatus::NotRequired,
			EOpenMobileAdsGdprApplicability::NotApplicable,
			EOpenMobileAdsConsentRequirement::NotRequired,
			EOpenMobileAdsConsentRequestState::Allowed
		},
		{
			EOpenMobileAdsAdMobUMPConsentStatus::Required,
			false,
			EOpenMobileAdsConsentStatus::Required,
			EOpenMobileAdsGdprApplicability::Applicable,
			EOpenMobileAdsConsentRequirement::Required,
			EOpenMobileAdsConsentRequestState::Blocked
		},
		{
			EOpenMobileAdsAdMobUMPConsentStatus::Obtained,
			true,
			EOpenMobileAdsConsentStatus::Obtained,
			EOpenMobileAdsGdprApplicability::Applicable,
			EOpenMobileAdsConsentRequirement::Required,
			EOpenMobileAdsConsentRequestState::Allowed
		},
		{
			EOpenMobileAdsAdMobUMPConsentStatus::Obtained,
			false,
			EOpenMobileAdsConsentStatus::Obtained,
			EOpenMobileAdsGdprApplicability::Applicable,
			EOpenMobileAdsConsentRequirement::Required,
			EOpenMobileAdsConsentRequestState::Blocked
		}
	};

	for (const FCase& Case : Cases)
	{
		const FOpenMobileAdsConsentStatusUpdate Update =
			FOpenMobileAdsAdMobConsentMapper::MapGdprState(
				Case.Input,
				Case.bCanRequestAds,
				TEXT("GoogleUMP")
			);
		TestEqual(TEXT("UMP status is normalized without overclaiming"), Update.Status, Case.Status);
		TestEqual(TEXT("UMP GDPR applicability is normalized"), Update.GdprApplicability, Case.Applicability);
		TestEqual(TEXT("UMP consent requirement is normalized"), Update.Requirement, Case.Requirement);
		TestEqual(TEXT("UMP ad-request eligibility is normalized"), Update.RequestState, Case.RequestState);
		TestEqual(TEXT("UMP mapping retains its provider source"), Update.Source, FName(TEXT("GoogleUMP")));
		TestTrue(TEXT("A successful UMP update is fresh"), Update.bStatusFresh);
		TestTrue(TEXT("UMP raw status is available for diagnostics"), Update.ProviderDetails.bIsAvailable);
		TestFalse(TEXT("UMP raw status is not empty"), Update.ProviderDetails.RawStatus.IsEmpty());
	}
	const FOpenMobileAdsConsentStatusUpdate CombinedUpdate =
		FOpenMobileAdsAdMobConsentMapper::MapGdprState(
			EOpenMobileAdsAdMobUMPConsentStatus::Obtained,
			true,
			TEXT("GoogleUMP"),
			EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::Required
		);
	TestEqual(
		TEXT("One UMP update includes its privacy-options requirement"),
		CombinedUpdate.UsPrivacy.PrivacyOptionsRequirement,
		EOpenMobileAdsPrivacyOptionsRequirement::Required
	);
	TestEqual(
		TEXT("One UMP update preserves provider-managed GPP"),
		CombinedUpdate.UsPrivacy.DataProcessingMode,
		EOpenMobileAdsDataProcessingMode::ProviderManaged
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobUsPrivacyMappingTest,
	"OpenMobile.Ads.AdMob.Privacy.UsStateMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobUsPrivacyMappingTest::RunTest(
	const FString& Parameters
)
{
	struct FCase
	{
		EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement Input;
		EOpenMobileAdsPrivacyOptionsRequirement Expected;
	};
	const FCase Cases[] = {
		{
			EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::Unknown,
			EOpenMobileAdsPrivacyOptionsRequirement::Unknown
		},
		{
			EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::NotRequired,
			EOpenMobileAdsPrivacyOptionsRequirement::NotRequired
		},
		{
			EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::Required,
			EOpenMobileAdsPrivacyOptionsRequirement::Required
		}
	};

	for (const FCase& Case : Cases)
	{
		const FOpenMobileAdsUsPrivacyState State =
			FOpenMobileAdsAdMobConsentMapper::MapUsPrivacyState(Case.Input);
		TestEqual(
			TEXT("UMP privacy-options requirements are normalized"),
			State.PrivacyOptionsRequirement,
			Case.Expected
		);
		TestEqual(
			TEXT("UMP does not imply a US-state region"),
			State.Applicability,
			EOpenMobileAdsUsPrivacyApplicability::Unknown
		);
		TestEqual(
			TEXT("UMP does not expose an opt-in or opt-out choice directly"),
			State.Choice,
			EOpenMobileAdsUsPrivacyChoice::Unknown
		);
		TestEqual(
			TEXT("UMP-owned GPP remains provider managed"),
			State.DataProcessingMode,
			EOpenMobileAdsDataProcessingMode::ProviderManaged
		);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobConsentFlowTest,
	"OpenMobile.Ads.AdMob.Privacy.ConsentFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobConsentFlowTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	FOpenMobileAdsAdMobPlatform::Shutdown();

	FOpenMobileAdsConsentRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.Platform = EOpenMobileAdsPlatform::Android;
	Request.Privacy.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::Yes;
	Request.Development.bEnableConsentDebug = false;
	Request.Development.TestDeviceIdentifiers = {TEXT("UMP-DEVICE")};
	FOpenMobileAdsConsentStatusUpdate RefreshUpdate;
	FOpenMobileAdsError ConsentError;
	int32 RefreshCompletions = 0;
	int32 Failures = 0;
	FString ImmediateError;
	TestTrue(
		TEXT("AdMob starts a UMP consent-info update"),
		FOpenMobileAdsAdMobPlatform::BeginConsentRefresh(
			Request,
			FOnOpenMobileAdMobConsentCompleted::CreateLambda(
				[&RefreshCompletions, &RefreshUpdate](
					FOpenMobileAdsConsentStatusUpdate Update
				)
				{
					++RefreshCompletions;
					RefreshUpdate = MoveTemp(Update);
				}
			),
			FOnOpenMobileAdMobConsentFailed::CreateLambda(
				[&Failures, &ConsentError](FOpenMobileAdsError Error)
				{
					++Failures;
					ConsentError = MoveTemp(Error);
				}
			),
			ImmediateError
		)
	);
	TestEqual(TEXT("The native backend receives one refresh"), Backend.ConsentRefreshCalls, 1);
	TestEqual(TEXT("TFUA reaches native UMP"), Backend.ConsentRequest.Privacy.UnderAgeOfConsent, EOpenMobileAdsAgeTreatment::Yes);
	FOpenMobileAdsAdMobPlatform::NativeConsentInfoUpdated(
		Backend.ConsentRefreshRequestId,
		2,
		false,
		2
	);
	TestEqual(TEXT("The refresh completes once"), RefreshCompletions, 1);
	TestEqual(TEXT("Required UMP status is normalized"), RefreshUpdate.Status, EOpenMobileAdsConsentStatus::Required);
	TestEqual(TEXT("UMP request eligibility remains blocked"), RefreshUpdate.RequestState, EOpenMobileAdsConsentRequestState::Blocked);
	TestEqual(TEXT("UMP privacy options are normalized"), RefreshUpdate.UsPrivacy.PrivacyOptionsRequirement, EOpenMobileAdsPrivacyOptionsRequirement::Required);

	FOpenMobileAdsConsentStatusUpdate FormUpdate;
	int32 FormCompletions = 0;
	TestTrue(
		TEXT("AdMob starts the required UMP form"),
		FOpenMobileAdsAdMobPlatform::BeginRequiredConsentForm(
			Request,
			FOnOpenMobileAdMobConsentCompleted::CreateLambda(
				[&FormCompletions, &FormUpdate](
					FOpenMobileAdsConsentStatusUpdate Update
				)
				{
					++FormCompletions;
					FormUpdate = MoveTemp(Update);
				}
			),
			FOnOpenMobileAdMobConsentFailed::CreateLambda(
				[&Failures, &ConsentError](FOpenMobileAdsError Error)
				{
					++Failures;
					ConsentError = MoveTemp(Error);
				}
			),
			ImmediateError
		)
	);
	TestEqual(TEXT("The native backend receives one form request"), Backend.ConsentFormCalls, 1);
	FOpenMobileAdsAdMobPlatform::NativeConsentFormDismissed(
		Backend.ConsentFormRequestId,
		3,
		true,
		2
	);
	TestEqual(TEXT("Form dismissal completes once"), FormCompletions, 1);
	TestEqual(TEXT("Obtained UMP status is preserved"), FormUpdate.Status, EOpenMobileAdsConsentStatus::Obtained);
	TestEqual(TEXT("Final UMP request eligibility is allowed"), FormUpdate.RequestState, EOpenMobileAdsConsentRequestState::Allowed);

	Request.RequestId = FGuid::NewGuid();
	TestTrue(
		TEXT("A later UMP refresh starts"),
		FOpenMobileAdsAdMobPlatform::BeginConsentRefresh(
			Request,
			FOnOpenMobileAdMobConsentCompleted(),
			FOnOpenMobileAdMobConsentFailed::CreateLambda(
				[&Failures, &ConsentError](FOpenMobileAdsError Error)
				{
					++Failures;
					ConsentError = MoveTemp(Error);
				}
			),
			ImmediateError
		)
	);
	FOpenMobileAdsAdMobPlatform::NativeConsentFailed(
		Backend.ConsentRefreshRequestId,
		TEXT("ump_network"),
		TEXT("mock UMP network failure")
	);
	TestEqual(TEXT("A native UMP failure completes once"), Failures, 1);
	TestTrue(TEXT("UMP network failures are retryable"), ConsentError.bRetryable);
	TestEqual(TEXT("UMP failure diagnostics retain their code"), ConsentError.NativeDiagnostics.NativeCode, FString(TEXT("ump_network")));
	FOpenMobileAdsAdMobPlatform::Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobPrivacyOptionsTest,
	"OpenMobile.Ads.AdMob.Privacy.PrivacyOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobPrivacyOptionsTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	FOpenMobileAdsAdMobPlatform::Shutdown();
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (Provider)
	{
		TestTrue(
			TEXT("AdMob advertises privacy-options support"),
			Provider->SupportsPrivacyOptionsForm()
		);
	}

	FOpenMobileAdsConsentRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.Platform = EOpenMobileAdsPlatform::Android;
	FOpenMobileAdsConsentStatusUpdate Update;
	FOpenMobileAdsError Error;
	int32 Completions = 0;
	int32 Failures = 0;
	FString ImmediateError;
	TestTrue(
		TEXT("AdMob starts the privacy-options form"),
		FOpenMobileAdsAdMobPlatform::BeginPrivacyOptionsForm(
			Request,
			FOnOpenMobileAdMobConsentCompleted::CreateLambda(
				[&Completions, &Update](
					FOpenMobileAdsConsentStatusUpdate Result
				)
				{
					++Completions;
					Update = MoveTemp(Result);
				}
			),
			FOnOpenMobileAdMobConsentFailed::CreateLambda(
				[&Failures, &Error](FOpenMobileAdsError Result)
				{
					++Failures;
					Error = MoveTemp(Result);
				}
			),
			ImmediateError
		)
	);
	TestEqual(TEXT("The native privacy-options form starts once"), Backend.PrivacyOptionsFormCalls, 1);
	FOpenMobileAdsAdMobPlatform::NativeConsentFormDismissed(
		Backend.PrivacyOptionsFormRequestId,
		3,
		true,
		2
	);
	TestEqual(TEXT("Privacy-options dismissal completes once"), Completions, 1);
	TestEqual(TEXT("Privacy-options dismissal refreshes consent"), Update.Status, EOpenMobileAdsConsentStatus::Obtained);
	TestTrue(TEXT("Privacy-options dismissal preserves availability"), Update.UsPrivacy.bPrivacyOptionsFormAvailable);

	Request.RequestId = FGuid::NewGuid();
	TestTrue(
		TEXT("Privacy options can start again"),
		FOpenMobileAdsAdMobPlatform::BeginPrivacyOptionsForm(
			Request,
			FOnOpenMobileAdMobConsentCompleted(),
			FOnOpenMobileAdMobConsentFailed::CreateLambda(
				[&Failures, &Error](FOpenMobileAdsError Result)
				{
					++Failures;
					Error = MoveTemp(Result);
				}
			),
			ImmediateError
		)
	);
	FOpenMobileAdsAdMobPlatform::NativeConsentFailed(
		Backend.PrivacyOptionsFormRequestId,
		TEXT("form_unavailable"),
		TEXT("mock privacy-options form unavailable")
	);
	TestEqual(TEXT("Privacy-options failure completes once"), Failures, 1);
	TestEqual(TEXT("Unavailable privacy options are typed"), Error.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);

	Request.RequestId = FGuid::NewGuid();
	TestTrue(
		TEXT("A cancellable privacy-options request starts"),
		FOpenMobileAdsAdMobPlatform::BeginPrivacyOptionsForm(
			Request,
			FOnOpenMobileAdMobConsentCompleted::CreateLambda(
				[&Completions](FOpenMobileAdsConsentStatusUpdate Result)
				{
					++Completions;
				}
			),
			FOnOpenMobileAdMobConsentFailed(),
			ImmediateError
		)
	);
	FOpenMobileAdsAdMobPlatform::CancelConsent(Request.RequestId);
	FOpenMobileAdsAdMobPlatform::NativeConsentFormDismissed(
		Backend.PrivacyOptionsFormRequestId,
		3,
		true,
		2
	);
	TestEqual(TEXT("A late privacy-options callback is ignored"), Completions, 1);
	FOpenMobileAdsAdMobPlatform::Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobDebugGeographyTest,
	"OpenMobile.Ads.AdMob.Privacy.DebugGeography",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobDebugGeographyTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->TestDeviceIdentifiers = {TEXT("ADMOB-TEST-DEVICE")};
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	FOpenMobileAdsAdMobPlatform::Shutdown();
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}

	FOpenMobileAdsConsentRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.Platform = EOpenMobileAdsPlatform::Android;
	Request.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(
		true,
		{},
		EOpenMobileAdsDebugGeography::RegulatedUsState
	);
	const TSharedRef<FConsentSink, ESPMode::ThreadSafe> Sink =
		MakeShared<FConsentSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError Error;
	TestTrue(
		TEXT("AdMob accepts regulated-US debug geography"),
		Provider->RefreshConsent(Request, Sink, Error)
	);
	TestEqual(
		TEXT("AdMob merges its configured test device before geography resolution"),
		Backend.ConsentRequest.Development.TestDeviceIdentifiers,
		ScopedSettings.Settings->TestDeviceIdentifiers
	);
	TestEqual(
		TEXT("Provider test devices enable regulated-US geography"),
		Backend.ConsentRequest.Development.GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::RegulatedUsState
	);
	FOpenMobileAdsAdMobPlatform::NativeConsentInfoUpdated(
		Backend.ConsentRefreshRequestId,
		1,
		true,
		1
	);

	ScopedSettings.Settings->TestDeviceIdentifiers.Reset();
	Request.RequestId = FGuid::NewGuid();
	Request.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(
		true,
		{},
		EOpenMobileAdsDebugGeography::Other
	);
	TestTrue(
		TEXT("AdMob accepts an unconfigured debug-geography request"),
		Provider->RefreshConsent(Request, Sink, Error)
	);
	TestEqual(
		TEXT("AdMob disables geography when its merged test-device list is empty"),
		Backend.ConsentRequest.Development.GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::Disabled
	);
	FOpenMobileAdsAdMobPlatform::NativeConsentInfoUpdated(
		Backend.ConsentRefreshRequestId,
		1,
		true,
		1
	);
	FOpenMobileAdsAdMobPlatform::Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobConsentResetTest,
	"OpenMobile.Ads.AdMob.Privacy.ConsentReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobConsentResetTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	FOpenMobileAdsAdMobPlatform::Shutdown();
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}

	TestTrue(
		TEXT("AdMob advertises a development consent reset"),
		Provider->SupportsConsentResetForTesting()
	);
	FOpenMobileAdsError Error;
	TestTrue(
		TEXT("AdMob accepts a development consent reset"),
		Provider->ResetConsentForTesting(Error)
	);
	TestEqual(
		TEXT("AdMob calls the native reset boundary once"),
		Backend.ConsentResetCalls,
		1
	);
	TestFalse(TEXT("Successful AdMob reset has no error"), Error.IsSet());

	Backend.bAcceptConsentReset = false;
	Backend.ConsentResetError = TEXT("mock reset failure");
	TestFalse(
		TEXT("AdMob reports native reset rejection"),
		Provider->ResetConsentForTesting(Error)
	);
	TestEqual(
		TEXT("AdMob maps reset rejection to native failure"),
		Error.Code,
		EOpenMobileAdsErrorCode::NativeFailure
	);
	TestTrue(
		TEXT("AdMob preserves reset diagnostics"),
		Error.Explanation.Contains(TEXT("mock reset failure"))
	);
	FOpenMobileAdsAdMobPlatform::Shutdown();
	return true;
}

#if PLATFORM_ANDROID || PLATFORM_IOS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobConsentResetDeviceTest,
	"OpenMobile.Ads.AdMob.Privacy.ConsentReset.Device",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobConsentResetDeviceTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The device AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}
	TestTrue(
		TEXT("The device AdMob provider supports consent reset"),
		Provider->SupportsConsentResetForTesting()
	);
	FOpenMobileAdsError Error;
	TestTrue(
		TEXT("The device resets UMP consent state to unknown"),
		Provider->ResetConsentForTesting(Error)
	);
	TestFalse(TEXT("The device reset has no native error"), Error.IsSet());
	return true;
}

#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobConsentSignalsTest,
	"OpenMobile.Ads.AdMob.Privacy.ConsentSignals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobConsentSignalsTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	FMockBackend Backend;
	FScopedBackendRegistration BackendRegistration(Backend);
	FOpenMobileAdsAdMobPlatform::Shutdown();
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}

	const int32 AllSignals = FOpenMobileAdsConsentSignals::AllSignalMask;
	const int32 GdprSignal = static_cast<int32>(
		EOpenMobileAdsConsentSignal::Gdpr
	);
	const int32 UsPrivacySignal = static_cast<int32>(
		EOpenMobileAdsConsentSignal::UsPrivacy
	);
	TestEqual(
		TEXT("AdMob accepts every normalized consent signal"),
		Provider->GetSupportedConsentSignalMask(),
		AllSignals
	);
	TestEqual(
		TEXT("AdMob confirms every accepted signal"),
		Provider->GetConfirmableConsentSignalMask(),
		AllSignals
	);
	TestEqual(
		TEXT("AdMob updates consent and US privacy choices at runtime"),
		Provider->GetRuntimeUpdatableConsentSignalMask(),
		GdprSignal | UsPrivacySignal
	);

	FOpenMobileAdsConsentSignals Signals;
	Signals.ConsentStatus = EOpenMobileAdsConsentStatus::Obtained;
	Signals.GdprApplicability = EOpenMobileAdsGdprApplicability::Applicable;
	Signals.ConsentRequirement = EOpenMobileAdsConsentRequirement::Required;
	Signals.ConsentRequestState = EOpenMobileAdsConsentRequestState::Allowed;
	Signals.bConsentStatusFresh = true;
	Signals.UsPrivacy.Applicability =
		EOpenMobileAdsUsPrivacyApplicability::Applicable;
	Signals.UsPrivacy.Choice = EOpenMobileAdsUsPrivacyChoice::OptedIn;
	Signals.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Standard;
	Signals.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::No;
	Signals.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::No;
	Signals.Source = TEXT("GoogleUMP");
	const FOpenMobileAdsConsentSignalApplyResult InitialResult =
		Provider->ApplyConsentSignals(Signals, AllSignals);
	TestEqual(TEXT("AdMob applies the initial signal set"), InitialResult.AppliedSignals, AllSignals);
	TestEqual(TEXT("AdMob confirms the initial signal set"), InitialResult.ConfirmedSignals, AllSignals);
	TestEqual(TEXT("The native boundary receives the initial signals"), Backend.ConsentSignalCalls, 1);
	TestEqual(TEXT("The native boundary receives standard processing"), Backend.LastConsentSignals.UsPrivacy.DataProcessingMode, EOpenMobileAdsDataProcessingMode::Standard);

	Signals.UsPrivacy.Choice = EOpenMobileAdsUsPrivacyChoice::OptedOut;
	Signals.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Restricted;
	const FOpenMobileAdsConsentSignalApplyResult ChangedResult =
		Provider->ApplyConsentSignals(Signals, UsPrivacySignal);
	TestEqual(TEXT("AdMob applies the changed US privacy signal"), ChangedResult.AppliedSignals, UsPrivacySignal);
	TestEqual(TEXT("AdMob confirms the changed US privacy signal"), ChangedResult.ConfirmedSignals, UsPrivacySignal);
	TestEqual(TEXT("The native boundary receives the changed signal"), Backend.ConsentSignalCalls, 2);
	TestEqual(TEXT("The native boundary receives restricted processing"), Backend.LastConsentSignals.UsPrivacy.DataProcessingMode, EOpenMobileAdsDataProcessingMode::Restricted);

	FOpenMobileAdsAdMobPlatform::Shutdown();
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
	First.PrivacyContext.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Restricted;
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
	Second.PrivacyContext.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::ProviderManaged;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> SecondSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError SecondError;
	TestTrue(
		TEXT("A different named rewarded placement can load concurrently"),
		Provider->Load(Second, SecondSink, SecondError)
	);
	TestEqual(TEXT("Both loads reach the backend"), Backend.LoadRequestIds.Num(), 2);
	TestEqual(
		TEXT("Each load carries one data-processing mode"),
		Backend.LoadDataProcessingModes.Num(),
		2
	);
	if (Backend.LoadDataProcessingModes.Num() == 2)
	{
		TestEqual(
			TEXT("An opt-out requests restricted processing"),
			Backend.LoadDataProcessingModes[0],
			EOpenMobileAdsDataProcessingMode::Restricted
		);
		TestEqual(
			TEXT("UMP-owned GPP stays provider managed"),
			Backend.LoadDataProcessingModes[1],
			EOpenMobileAdsDataProcessingMode::ProviderManaged
		);
	}
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
	Replacement.PrivacyContext.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Standard;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ReplacementSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError ReplacementError;
	TestTrue(
		TEXT("A placement can replace its completed native load"),
		Provider->Load(Replacement, ReplacementSink, ReplacementError)
	);
	if (Backend.LoadDataProcessingModes.Num() == 3)
	{
		TestEqual(
			TEXT("A changed opt-in clears restricted processing"),
			Backend.LoadDataProcessingModes[2],
			EOpenMobileAdsDataProcessingMode::Standard
		);
	}
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
	Show.ServerVerification.bEnabled = true;
	Show.Options.ServerVerificationUserId = TEXT("player-42");
	Show.Options.ServerVerificationCustomData = TEXT("grant-42");
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
		TEXT("Reusable show forwards the server verification user ID"),
		Backend.ShownServerVerificationUserId,
		FString(TEXT("player-42"))
	);
	TestEqual(
		TEXT("Reusable show forwards server verification custom data"),
		Backend.ShownServerVerificationCustomData,
		FString(TEXT("grant-42"))
	);

	FOpenMobileAdsAdMobPlatform::NativeShown(Backend.ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeImpression(Backend.ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeClicked(Backend.ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
		Backend.ShowRequestId,
		-1,
		TEXT("USD"),
		static_cast<int32>(EOpenMobileAdsRevenuePrecision::Precise)
	);
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
	FOpenMobileAdsAdMobInterstitialContractTest,
	"OpenMobile.Ads.AdMob.Interstitial.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobInterstitialContractTest::RunTest(
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

	const FOpenMobileAdsProviderCapabilities Capabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* Interstitial =
		Capabilities.FindFormat(EOpenMobileAdFormat::Interstitial);
	TestNotNull(TEXT("AdMob reports interstitial capabilities"), Interstitial);
	if (!Interstitial)
	{
		return false;
	}
	TestTrue(TEXT("AdMob advertises interstitial loading"), Interstitial->bCanLoad);
	TestTrue(TEXT("AdMob advertises interstitial showing"), Interstitial->bCanShow);
	TestTrue(TEXT("AdMob advertises interstitial preloading"), Interstitial->bSupportsPreload);
	TestTrue(TEXT("AdMob reports interstitial impressions"), Interstitial->bReportsImpression);
	TestTrue(TEXT("AdMob reports interstitial clicks"), Interstitial->bReportsClick);
	TestTrue(TEXT("AdMob reports interstitial dismissal"), Interstitial->bReportsDismiss);
	TestFalse(TEXT("Interstitials never report rewards"), Interstitial->bReportsReward);

	FOpenMobileAdsInitializationRequest Initialization;
	Initialization.RequestId = FGuid::NewGuid();
	Initialization.Platform = EOpenMobileAdsPlatform::Android;
	Initialization.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> InitializationSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError InitializationError;
	TestTrue(
		TEXT("AdMob initializes before interstitial loading"),
		Provider->Initialize(Initialization, InitializationSink, InitializationError)
	);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);

	FOpenMobileAdsLoadRequest Load;
	Load.RequestId = FGuid::NewGuid();
	Load.Placement.Placement = TEXT("LevelCompleteInterstitial");
	Load.Placement.Format = EOpenMobileAdFormat::Interstitial;
	Load.Placement.AdUnitId = TEXT("production-interstitial");
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> LoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError LoadError;
	TestTrue(
		TEXT("AdMob starts an interstitial load"),
		Provider->Load(Load, LoadSink, LoadError)
	);
	TestEqual(
		TEXT("The interstitial load reaches its own native path"),
		Backend.InterstitialLoadRequestIds.Num(),
		1
	);
	if (Backend.InterstitialLoadRequestIds.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	TestEqual(
		TEXT("Development mode selects Google's Android interstitial test ID"),
		Backend.InterstitialLoadedAdUnitIds[0],
		FString(TEXT("ca-app-pub-3940256099942544/1033173712"))
	);
	FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadCompleted(
		Backend.InterstitialLoadRequestIds[0]
	);
	TestEqual(TEXT("The interstitial load completes once"), LoadSink->Events.Num(), 1);
	if (LoadSink->Events.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	TestEqual(
		TEXT("The interstitial load emits Loaded"),
		LoadSink->Events[0].Type,
		EOpenMobileAdsEventType::Loaded
	);

	FOpenMobileAdsShowRequest Show;
	Show.RequestId = FGuid::NewGuid();
	Show.CachedAdId = LoadSink->Events[0].CachedAdId;
	Show.Placement = Load.Placement.Placement;
	Show.Format = EOpenMobileAdFormat::Interstitial;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ShowSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError ShowError;
	FOpenMobileAdsShowRequest MismatchedShow = Show;
	MismatchedShow.RequestId = FGuid::NewGuid();
	MismatchedShow.Format = EOpenMobileAdFormat::Rewarded;
	TestFalse(
		TEXT("A rewarded show cannot consume an interstitial cache"),
		Provider->Show(MismatchedShow, ShowSink, ShowError)
	);
	TestEqual(
		TEXT("A cache format mismatch does not reach either native show path"),
		Backend.ShowCalls + Backend.InterstitialShowCalls,
		0
	);
	TestTrue(
		TEXT("AdMob presents the exact cached interstitial"),
		Provider->Show(Show, ShowSink, ShowError)
	);
	TestEqual(
		TEXT("The interstitial show reaches its own native path"),
		Backend.InterstitialShowCalls,
		1
	);
	TestEqual(
		TEXT("The interstitial show consumes its matching native cache"),
		Backend.ShownInterstitialLoadedRequestId,
		Backend.InterstitialLoadRequestIds[0]
	);

	FOpenMobileAdsAdMobPlatform::NativeShown(Backend.InterstitialShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeImpression(Backend.InterstitialShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeClicked(Backend.InterstitialShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
		Backend.InterstitialShowRequestId,
		12345,
		TEXT("USD"),
		static_cast<int32>(EOpenMobileAdsRevenuePrecision::Precise)
	);
	FOpenMobileAdsAdMobPlatform::NativeEarned(
		Backend.InterstitialShowRequestId,
		1,
		TEXT("invalid")
	);
	FOpenMobileAdsAdMobPlatform::NativeClosed(Backend.InterstitialShowRequestId);
	const EOpenMobileAdsEventType ExpectedTypes[] = {
		EOpenMobileAdsEventType::Shown,
		EOpenMobileAdsEventType::Impression,
		EOpenMobileAdsEventType::Clicked,
		EOpenMobileAdsEventType::RevenuePaid,
		EOpenMobileAdsEventType::Dismissed
	};
	TestEqual(
		TEXT("Interstitial callbacks emit one non-rewarded lifecycle"),
		ShowSink->Events.Num(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes))
	);
	if (ShowSink->Events.Num() == static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes)))
	{
		for (int32 Index = 0; Index < ShowSink->Events.Num(); ++Index)
		{
			TestEqual(
				TEXT("Interstitial callback order is preserved"),
				ShowSink->Events[Index].Type,
				ExpectedTypes[Index]
			);
		}
	}

	FOpenMobileAdsLoadRequest FailedLoad = Load;
	FailedLoad.RequestId = FGuid::NewGuid();
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> FailedLoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("Another interstitial loads for the failure path"),
		Provider->Load(FailedLoad, FailedLoadSink, LoadError)
	);
	FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadCompleted(
		Backend.InterstitialLoadRequestIds.Last()
	);
	TestEqual(
		TEXT("The failure-path interstitial finishes loading"),
		FailedLoadSink->Events.Num(),
		1
	);
	if (FailedLoadSink->Events.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	FOpenMobileAdsShowRequest FailedShow = Show;
	FailedShow.RequestId = FGuid::NewGuid();
	FailedShow.CachedAdId = FailedLoadSink->Events[0].CachedAdId;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> FailedShowSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("The failure-path interstitial starts showing"),
		Provider->Show(FailedShow, FailedShowSink, ShowError)
	);
	FOpenMobileAdsAdMobPlatform::NativeFailed(
		Backend.InterstitialShowRequestId,
		TEXT("test interstitial show failure")
	);
	TestEqual(
		TEXT("Interstitial presentation failure emits one terminal event"),
		FailedShowSink->Events.Num(),
		1
	);
	if (FailedShowSink->Events.Num() == 1)
	{
		TestEqual(
			TEXT("Interstitial presentation failure is normalized"),
			FailedShowSink->Events[0].Type,
			EOpenMobileAdsEventType::Failed
		);
	}

	FOpenMobileAdsLoadRequest ReleasedLoad = Load;
	ReleasedLoad.RequestId = FGuid::NewGuid();
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ReleasedLoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("Another interstitial loads for explicit release"),
		Provider->Load(ReleasedLoad, ReleasedLoadSink, LoadError)
	);
	const int64 ReleasedNativeRequestId =
		Backend.InterstitialLoadRequestIds.Last();
	FOpenMobileAdsAdMobPlatform::NativeInterstitialLoadCompleted(
		ReleasedNativeRequestId
	);
	TestEqual(
		TEXT("The release-path interstitial finishes loading"),
		ReleasedLoadSink->Events.Num(),
		1
	);
	if (ReleasedLoadSink->Events.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	Provider->ReleaseCachedAd(ReleasedLoadSink->Events[0].CachedAdId);
	TestTrue(
		TEXT("Interstitial release reaches the matching native cache"),
		Backend.CancelledInterstitialRequestIds.Contains(ReleasedNativeRequestId)
	);

	Provider->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobRewardedInterstitialContractTest,
	"OpenMobile.Ads.AdMob.RewardedInterstitial.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobRewardedInterstitialContractTest::RunTest(
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

	const FOpenMobileAdsProviderCapabilities ProviderCapabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* Capabilities =
		ProviderCapabilities.FindFormat(
			EOpenMobileAdFormat::RewardedInterstitial
		);
	TestNotNull(
		TEXT("AdMob reports rewarded-interstitial capabilities"),
		Capabilities
	);
	if (!Capabilities)
	{
		return false;
	}
	TestTrue(TEXT("Rewarded interstitials can load"), Capabilities->bCanLoad);
	TestTrue(TEXT("Rewarded interstitials can show"), Capabilities->bCanShow);
	TestTrue(TEXT("Rewarded interstitials can preload"), Capabilities->bSupportsPreload);
	TestTrue(TEXT("Rewarded interstitials report impressions"), Capabilities->bReportsImpression);
	TestTrue(TEXT("Rewarded interstitials report clicks"), Capabilities->bReportsClick);
	TestTrue(TEXT("Rewarded interstitials report dismissal"), Capabilities->bReportsDismiss);
	TestTrue(TEXT("Rewarded interstitials report rewards"), Capabilities->bReportsReward);
	TestTrue(TEXT("Rewarded interstitials report revenue"), Capabilities->bReportsRevenue);
	TestTrue(TEXT("Rewarded interstitials support server verification"), Capabilities->bSupportsServerVerification);
	TestTrue(TEXT("Rewarded interstitials require an introduction"), Capabilities->bRequiresIntroduction);
	const FOpenMobileAdFormatCapabilities* Rewarded =
		ProviderCapabilities.FindFormat(EOpenMobileAdFormat::Rewarded);
	TestTrue(
		TEXT("Normal rewarded video does not require the interstitial introduction"),
		Rewarded && !Rewarded->bRequiresIntroduction
	);

	FOpenMobileAdsInitializationRequest Initialization;
	Initialization.RequestId = FGuid::NewGuid();
	Initialization.Platform = EOpenMobileAdsPlatform::Android;
	Initialization.Development =
		FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> InitializationSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError Error;
	TestTrue(
		TEXT("AdMob initializes before rewarded-interstitial loading"),
		Provider->Initialize(Initialization, InitializationSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);

	FOpenMobileAdsLoadRequest Load;
	Load.RequestId = FGuid::NewGuid();
	Load.Placement.Placement = TEXT("LevelCompleteReward");
	Load.Placement.Format = EOpenMobileAdFormat::RewardedInterstitial;
	Load.Placement.AdUnitId = TEXT("production-rewarded-interstitial");
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> LoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob starts a rewarded-interstitial load"),
		Provider->Load(Load, LoadSink, Error)
	);
	TestEqual(
		TEXT("Rewarded interstitial loading uses its native path"),
		Backend.RewardedInterstitialLoadRequestIds.Num(),
		1
	);
	if (Backend.RewardedInterstitialLoadRequestIds.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	TestEqual(
		TEXT("Development mode uses Google's Android rewarded-interstitial test ID"),
		Backend.RewardedInterstitialLoadedAdUnitIds[0],
		FString(TEXT("ca-app-pub-3940256099942544/5354046379"))
	);
	FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadCompleted(
		Backend.RewardedInterstitialLoadRequestIds[0],
		25,
		TEXT("coin")
	);
	TestEqual(TEXT("Rewarded interstitial loading completes once"), LoadSink->Events.Num(), 1);
	if (LoadSink->Events.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	TestTrue(
		TEXT("Loaded rewarded interstitial exposes reward metadata"),
		LoadSink->Events[0].bHasReward
	);
	TestEqual(
		TEXT("Loaded rewarded interstitial preserves reward type"),
		LoadSink->Events[0].Reward.Type,
		FString(TEXT("coin"))
	);
	TestEqual(
		TEXT("Loaded rewarded interstitial preserves reward amount"),
		LoadSink->Events[0].Reward.Amount,
		static_cast<int64>(25)
	);

	FOpenMobileAdsShowRequest Show;
	Show.RequestId = FGuid::NewGuid();
	Show.CachedAdId = LoadSink->Events[0].CachedAdId;
	Show.Placement = Load.Placement.Placement;
	Show.Format = EOpenMobileAdFormat::RewardedInterstitial;
	Show.ServerVerification.bEnabled = true;
	Show.Options.bRewardedInterstitialIntroductionPresented = true;
	Show.Options.ServerVerificationUserId = TEXT("player-42");
	Show.Options.ServerVerificationCustomData = TEXT("grant-42");
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ShowSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob shows the exact cached rewarded interstitial"),
		Provider->Show(Show, ShowSink, Error)
	);
	TestEqual(
		TEXT("Rewarded interstitial showing uses its native path"),
		Backend.RewardedInterstitialShowCalls,
		1
	);
	TestEqual(
		TEXT("Rewarded interstitial forwards the server verification user ID"),
		Backend.ShownRewardedInterstitialVerificationUserId,
		FString(TEXT("player-42"))
	);
	TestEqual(
		TEXT("Rewarded interstitial forwards server verification data"),
		Backend.ShownRewardedInterstitialVerificationData,
		FString(TEXT("grant-42"))
	);

	const int64 ShowRequestId = Backend.RewardedInterstitialShowRequestId;
	FOpenMobileAdsAdMobPlatform::NativeShown(ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeImpression(ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeClicked(ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
		ShowRequestId,
		12345,
		TEXT("USD"),
		static_cast<int32>(EOpenMobileAdsRevenuePrecision::Precise)
	);
	FOpenMobileAdsAdMobPlatform::NativeEarned(ShowRequestId, 25, TEXT("coin"));
	FOpenMobileAdsAdMobPlatform::NativeEarned(ShowRequestId, 25, TEXT("coin"));
	FOpenMobileAdsAdMobPlatform::NativeClosed(ShowRequestId);
	const EOpenMobileAdsEventType ExpectedTypes[] = {
		EOpenMobileAdsEventType::Shown,
		EOpenMobileAdsEventType::Impression,
		EOpenMobileAdsEventType::Clicked,
		EOpenMobileAdsEventType::RevenuePaid,
		EOpenMobileAdsEventType::RewardEarned,
		EOpenMobileAdsEventType::Dismissed
	};
	TestEqual(
		TEXT("Rewarded interstitial emits one complete callback lifecycle"),
		ShowSink->Events.Num(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes))
	);
	if (ShowSink->Events.Num() == static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes)))
	{
		for (int32 Index = 0; Index < ShowSink->Events.Num(); ++Index)
		{
			TestEqual(
				TEXT("Rewarded-interstitial callback order is preserved"),
				ShowSink->Events[Index].Type,
				ExpectedTypes[Index]
			);
		}
	}

	FOpenMobileAdsLoadRequest ReleasedLoad = Load;
	ReleasedLoad.RequestId = FGuid::NewGuid();
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ReleasedSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("Another rewarded interstitial loads for release"),
		Provider->Load(ReleasedLoad, ReleasedSink, Error)
	);
	const int64 ReleasedRequestId =
		Backend.RewardedInterstitialLoadRequestIds.Last();
	FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadCompleted(
		ReleasedRequestId,
		10,
		TEXT("gem")
	);
	if (!ReleasedSink->Events.IsEmpty())
	{
		Provider->ReleaseCachedAd(ReleasedSink->Events[0].CachedAdId);
	}
	TestTrue(
		TEXT("Rewarded-interstitial release reaches its native cache"),
		Backend.CancelledRewardedInterstitialRequestIds.Contains(
			ReleasedRequestId
		)
	);

	FOpenMobileAdsLoadRequest FailedLoad = Load;
	FailedLoad.RequestId = FGuid::NewGuid();
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> FailedLoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("Another rewarded interstitial starts for load failure"),
		Provider->Load(FailedLoad, FailedLoadSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadFailed(
		Backend.RewardedInterstitialLoadRequestIds.Last(),
		TEXT("test rewarded-interstitial load failure")
	);
	TestEqual(
		TEXT("Rewarded-interstitial load failure emits one terminal event"),
		FailedLoadSink->Events.Num(),
		1
	);
	if (!FailedLoadSink->Events.IsEmpty())
	{
		TestEqual(
			TEXT("Rewarded-interstitial load failure is normalized"),
			FailedLoadSink->Events[0].Type,
			EOpenMobileAdsEventType::LoadFailed
		);
	}

	FOpenMobileAdsLoadRequest FailedShowLoad = Load;
	FailedShowLoad.RequestId = FGuid::NewGuid();
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> FailedShowLoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("Another rewarded interstitial loads for show failure"),
		Provider->Load(FailedShowLoad, FailedShowLoadSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeRewardedInterstitialLoadCompleted(
		Backend.RewardedInterstitialLoadRequestIds.Last(),
		5,
		TEXT("coin")
	);
	if (!FailedShowLoadSink->Events.IsEmpty())
	{
		FOpenMobileAdsShowRequest FailedShow = Show;
		FailedShow.RequestId = FGuid::NewGuid();
		FailedShow.CachedAdId = FailedShowLoadSink->Events[0].CachedAdId;
		const TSharedRef<FEventSink, ESPMode::ThreadSafe> FailedShowSink =
			MakeShared<FEventSink, ESPMode::ThreadSafe>();
		TestTrue(
			TEXT("The failure-path rewarded interstitial starts showing"),
			Provider->Show(FailedShow, FailedShowSink, Error)
		);
		FOpenMobileAdsAdMobPlatform::NativeFailed(
			Backend.RewardedInterstitialShowRequestId,
			TEXT("test rewarded-interstitial show failure")
		);
		TestEqual(
			TEXT("Rewarded-interstitial show failure emits one terminal event"),
			FailedShowSink->Events.Num(),
			1
		);
		if (!FailedShowSink->Events.IsEmpty())
		{
			TestEqual(
				TEXT("Rewarded-interstitial show failure is normalized"),
				FailedShowSink->Events[0].Type,
				EOpenMobileAdsEventType::Failed
			);
		}
	}

	Provider->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobAppOpenContractTest,
	"OpenMobile.Ads.AdMob.AppOpen.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobAppOpenContractTest::RunTest(
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

	const FOpenMobileAdsProviderCapabilities ProviderCapabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* Capabilities =
		ProviderCapabilities.FindFormat(EOpenMobileAdFormat::AppOpen);
	TestNotNull(TEXT("AdMob reports app-open capabilities"), Capabilities);
	if (!Capabilities)
	{
		return false;
	}
	TestTrue(TEXT("App-open ads can load"), Capabilities->bCanLoad);
	TestTrue(TEXT("App-open ads can show"), Capabilities->bCanShow);
	TestTrue(TEXT("App-open ads can be destroyed"), Capabilities->bCanDestroy);
	TestTrue(TEXT("App-open ads can preload"), Capabilities->bSupportsPreload);
	TestTrue(TEXT("App-open ads report impressions"), Capabilities->bReportsImpression);
	TestTrue(TEXT("App-open ads report clicks"), Capabilities->bReportsClick);
	TestTrue(TEXT("App-open ads report dismissal"), Capabilities->bReportsDismiss);
	TestTrue(TEXT("App-open ads report revenue"), Capabilities->bReportsRevenue);
	TestFalse(TEXT("App-open ads do not report rewards"), Capabilities->bReportsReward);
	TestEqual(
		TEXT("App-open cache lifetime matches the SDK freshness limit"),
		Capabilities->CacheLifetimeSeconds,
		60.0 * 60.0 * 4.0
	);
	TestEqual(
		TEXT("Development mode has Google's iOS app-open test ID"),
		ScopedSettings.Settings->ResolveAppOpenAdUnitId(
			EOpenMobileAdsPlatform::IOS,
			true
		),
		FString(TEXT("ca-app-pub-3940256099942544/5575463023"))
	);

	FOpenMobileAdsInitializationRequest Initialization;
	Initialization.RequestId = FGuid::NewGuid();
	Initialization.Platform = EOpenMobileAdsPlatform::Android;
	Initialization.Development =
		FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> InitializationSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError Error;
	TestTrue(
		TEXT("AdMob initializes before app-open loading"),
		Provider->Initialize(Initialization, InitializationSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);

	FOpenMobileAdsLoadRequest Load;
	Load.RequestId = FGuid::NewGuid();
	Load.Placement.Placement = TEXT("ForegroundAppOpen");
	Load.Placement.Format = EOpenMobileAdFormat::AppOpen;
	Load.Placement.AdUnitId = TEXT("production-app-open");
	Load.PrivacyContext.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Restricted;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> LoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob starts an app-open load"),
		Provider->Load(Load, LoadSink, Error)
	);
	TestEqual(
		TEXT("App-open loading uses its own native path"),
		Backend.AppOpenLoadRequestIds.Num(),
		1
	);
	if (Backend.AppOpenLoadRequestIds.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	TestEqual(
		TEXT("Development mode uses Google's Android app-open test ID"),
		Backend.AppOpenLoadedAdUnitIds[0],
		FString(TEXT("ca-app-pub-3940256099942544/9257395921"))
	);
	TestEqual(
		TEXT("App-open loading preserves the current privacy mode"),
		Backend.AppOpenLoadDataProcessingModes[0],
		EOpenMobileAdsDataProcessingMode::Restricted
	);
	FOpenMobileAdsAdMobPlatform::NativeAppOpenLoadCompleted(
		Backend.AppOpenLoadRequestIds[0]
	);
	TestEqual(TEXT("App-open loading completes once"), LoadSink->Events.Num(), 1);
	if (LoadSink->Events.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	TestEqual(
		TEXT("App-open loading emits Loaded"),
		LoadSink->Events[0].Type,
		EOpenMobileAdsEventType::Loaded
	);

	FOpenMobileAdsShowRequest Show;
	Show.RequestId = FGuid::NewGuid();
	Show.CachedAdId = LoadSink->Events[0].CachedAdId;
	Show.Placement = Load.Placement.Placement;
	Show.Format = EOpenMobileAdFormat::AppOpen;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ShowSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob shows the exact cached app-open ad"),
		Provider->Show(Show, ShowSink, Error)
	);
	TestEqual(
		TEXT("App-open showing uses its own native path"),
		Backend.AppOpenShowCalls,
		1
	);
	TestEqual(
		TEXT("App-open showing consumes its matching native cache"),
		Backend.ShownAppOpenLoadedRequestId,
		Backend.AppOpenLoadRequestIds[0]
	);

	const int64 ShowRequestId = Backend.AppOpenShowRequestId;
	FOpenMobileAdsAdMobPlatform::NativeShown(ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeImpression(ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeClicked(ShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
		ShowRequestId,
		12345,
		TEXT("USD"),
		static_cast<int32>(EOpenMobileAdsRevenuePrecision::Precise)
	);
	FOpenMobileAdsAdMobPlatform::NativeEarned(ShowRequestId, 1, TEXT("invalid"));
	FOpenMobileAdsAdMobPlatform::NativeClosed(ShowRequestId);
	const EOpenMobileAdsEventType ExpectedTypes[] = {
		EOpenMobileAdsEventType::Shown,
		EOpenMobileAdsEventType::Impression,
		EOpenMobileAdsEventType::Clicked,
		EOpenMobileAdsEventType::RevenuePaid,
		EOpenMobileAdsEventType::Dismissed
	};
	TestEqual(
		TEXT("App-open callbacks emit one non-rewarded lifecycle"),
		ShowSink->Events.Num(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes))
	);
	if (ShowSink->Events.Num() == static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes)))
	{
		for (int32 Index = 0; Index < ShowSink->Events.Num(); ++Index)
		{
			TestEqual(
				TEXT("App-open callback order is preserved"),
				ShowSink->Events[Index].Type,
				ExpectedTypes[Index]
			);
		}
	}

	FOpenMobileAdsLoadRequest ReleasedLoad = Load;
	ReleasedLoad.RequestId = FGuid::NewGuid();
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ReleasedSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("Another app-open ad loads for release"),
		Provider->Load(ReleasedLoad, ReleasedSink, Error)
	);
	const int64 ReleasedRequestId = Backend.AppOpenLoadRequestIds.Last();
	FOpenMobileAdsAdMobPlatform::NativeAppOpenLoadCompleted(ReleasedRequestId);
	if (!ReleasedSink->Events.IsEmpty())
	{
		Provider->ReleaseCachedAd(ReleasedSink->Events[0].CachedAdId);
	}
	TestTrue(
		TEXT("App-open release reaches its native cache"),
		Backend.CancelledAppOpenRequestIds.Contains(ReleasedRequestId)
	);

	FOpenMobileAdsLoadRequest FailedLoad = Load;
	FailedLoad.RequestId = FGuid::NewGuid();
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> FailedLoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("Another app-open ad starts for load failure"),
		Provider->Load(FailedLoad, FailedLoadSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeAppOpenLoadFailed(
		Backend.AppOpenLoadRequestIds.Last(),
		TEXT("test app-open load failure")
	);
	TestEqual(
		TEXT("App-open load failure emits one terminal event"),
		FailedLoadSink->Events.Num(),
		1
	);
	if (!FailedLoadSink->Events.IsEmpty())
	{
		TestEqual(
			TEXT("App-open load failure is normalized"),
			FailedLoadSink->Events[0].Type,
			EOpenMobileAdsEventType::LoadFailed
		);
	}

	FOpenMobileAdsLoadRequest FailedShowLoad = Load;
	FailedShowLoad.RequestId = FGuid::NewGuid();
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> FailedShowLoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("Another app-open ad loads for show failure"),
		Provider->Load(FailedShowLoad, FailedShowLoadSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeAppOpenLoadCompleted(
		Backend.AppOpenLoadRequestIds.Last()
	);
	if (!FailedShowLoadSink->Events.IsEmpty())
	{
		FOpenMobileAdsShowRequest FailedShow = Show;
		FailedShow.RequestId = FGuid::NewGuid();
		FailedShow.CachedAdId = FailedShowLoadSink->Events[0].CachedAdId;
		const TSharedRef<FEventSink, ESPMode::ThreadSafe> FailedShowSink =
			MakeShared<FEventSink, ESPMode::ThreadSafe>();
		TestTrue(
			TEXT("The failure-path app-open ad starts showing"),
			Provider->Show(FailedShow, FailedShowSink, Error)
		);
		FOpenMobileAdsAdMobPlatform::NativeFailed(
			Backend.AppOpenShowRequestId,
			TEXT("test app-open show failure")
		);
		TestEqual(
			TEXT("App-open show failure emits one terminal event"),
			FailedShowSink->Events.Num(),
			1
		);
		if (!FailedShowSink->Events.IsEmpty())
		{
			TestEqual(
				TEXT("App-open show failure is normalized"),
				FailedShowSink->Events[0].Type,
				EOpenMobileAdsEventType::Failed
			);
		}
	}

	Provider->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobFixedBannerCapabilitiesTest,
	"OpenMobile.Ads.AdMob.FixedBanner.Capabilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobFixedBannerCapabilitiesTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}

	const FOpenMobileAdsProviderCapabilities Capabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* Banner =
		Capabilities.FindFormat(EOpenMobileAdFormat::Banner);
	TestNotNull(TEXT("AdMob reports fixed-banner capabilities"), Banner);
	if (!Banner)
	{
		return false;
	}
	TestTrue(TEXT("Fixed banners can load"), Banner->bCanLoad);
	TestTrue(TEXT("Fixed banners can show"), Banner->bCanShow);
	TestTrue(TEXT("Fixed banners can hide"), Banner->bCanHide);
	TestTrue(TEXT("Fixed banners preserve their cache when hidden"), Banner->bPreservesCachedAdOnHide);
	TestTrue(TEXT("Fixed banners can preload"), Banner->bSupportsPreload);
	TestTrue(TEXT("Fixed banners report impressions"), Banner->bReportsImpression);
	TestTrue(TEXT("Fixed banners report clicks"), Banner->bReportsClick);
	TestTrue(TEXT("Fixed banners report revenue"), Banner->bReportsRevenue);
	TestFalse(TEXT("Fixed banners do not report rewards"), Banner->bReportsReward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobAdaptiveBannerCapabilitiesTest,
	"OpenMobile.Ads.AdMob.AdaptiveBanner.Capabilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobAdaptiveBannerCapabilitiesTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}

	const FOpenMobileAdsProviderCapabilities Capabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* Adaptive =
		Capabilities.FindFormat(EOpenMobileAdFormat::AnchoredAdaptiveBanner);
	TestNotNull(TEXT("AdMob reports adaptive-banner capabilities"), Adaptive);
	if (!Adaptive)
	{
		return false;
	}
	TestTrue(TEXT("Adaptive banners can load"), Adaptive->bCanLoad);
	TestTrue(TEXT("Adaptive banners can show"), Adaptive->bCanShow);
	TestTrue(TEXT("Adaptive banners can hide"), Adaptive->bCanHide);
	TestTrue(
		TEXT("Adaptive banners preserve their cache when hidden"),
		Adaptive->bPreservesCachedAdOnHide
	);
	TestTrue(TEXT("Adaptive banners can preload"), Adaptive->bSupportsPreload);
	TestTrue(TEXT("Adaptive banners report impressions"), Adaptive->bReportsImpression);
	TestTrue(TEXT("Adaptive banners report clicks"), Adaptive->bReportsClick);
	TestTrue(TEXT("Adaptive banners report revenue"), Adaptive->bReportsRevenue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobAdaptiveBannerContractTest,
	"OpenMobile.Ads.AdMob.AdaptiveBanner.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobAdaptiveBannerContractTest::RunTest(
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

	FOpenMobileAdsInitializationRequest Initialization;
	Initialization.RequestId = FGuid::NewGuid();
	Initialization.Platform = EOpenMobileAdsPlatform::Android;
	Initialization.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> InitializationSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError Error;
	TestTrue(
		TEXT("AdMob initializes before adaptive-banner loading"),
		Provider->Initialize(Initialization, InitializationSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);

	FOpenMobileAdsLoadRequest Load;
	Load.RequestId = FGuid::NewGuid();
	Load.Placement.Placement = TEXT("AdaptiveFooter");
	Load.Placement.Format = EOpenMobileAdFormat::AnchoredAdaptiveBanner;
	Load.Placement.AdUnitId = TEXT("production-adaptive");
	Load.Placement.BannerLayout.Anchor = EOpenMobileAdsBannerAnchor::Bottom;
	Load.Placement.BannerLayout.bRespectSafeArea = true;
	Load.Placement.BannerLayout.AvailableWidth = 360.0f;
	Load.Placement.BannerLayout.Margins.Left = 8.0f;
	Load.Placement.BannerLayout.Margins.Right = 12.0f;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> LoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob starts an adaptive-banner load"),
		Provider->Load(Load, LoadSink, Error)
	);
	TestEqual(
		TEXT("Adaptive loading uses the banner native path"),
		Backend.BannerLoadRequestIds.Num(),
		1
	);
	if (Backend.BannerLoadRequestIds.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	TestEqual(
		TEXT("The native load is marked anchored adaptive"),
		Backend.BannerLoadFormats[0],
		EOpenMobileAdFormat::AnchoredAdaptiveBanner
	);
	TestEqual(
		TEXT("The native load receives the requested maximum width"),
		Backend.BannerLoadLayouts[0].AvailableWidth,
		360.0f
	);
	TestEqual(
		TEXT("Adaptive development mode uses Google's banner test ID"),
		Backend.BannerLoadedAdUnitIds[0],
		FString(TEXT("ca-app-pub-3940256099942544/6300978111"))
	);
	FOpenMobileAdsAdMobPlatform::NativeBannerLoadCompleted(
		Backend.BannerLoadRequestIds[0]
	);
	if (LoadSink->Events.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	const FGuid CachedAdId = LoadSink->Events[0].CachedAdId;

	FOpenMobileAdsShowRequest Show;
	Show.RequestId = FGuid::NewGuid();
	Show.CachedAdId = CachedAdId;
	Show.Placement = Load.Placement.Placement;
	Show.Format = EOpenMobileAdFormat::AnchoredAdaptiveBanner;
	Show.BannerLayout = Load.Placement.BannerLayout;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ShowSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob shows the cached adaptive banner"),
		Provider->Show(Show, ShowSink, Error)
	);
	TestEqual(
		TEXT("Adaptive show preserves the requested width"),
		Backend.ShownBannerLayout.AvailableWidth,
		360.0f
	);
	FOpenMobileAdsAdMobPlatform::NativeBannerShown(Backend.BannerShowRequestId);

	FOpenMobileAdsHideRequest Hide;
	Hide.RequestId = FGuid::NewGuid();
	Hide.CachedAdId = CachedAdId;
	Hide.Placement = Load.Placement.Placement;
	Hide.Format = EOpenMobileAdFormat::AnchoredAdaptiveBanner;
	Hide.bPreserveCachedAd = true;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> HideSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob hides the adaptive banner without consuming it"),
		Provider->Hide(Hide, HideSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeBannerHidden(Backend.BannerHideRequestId);
	TestEqual(TEXT("Adaptive hide completes once"), HideSink->Events.Num(), 1);
	Provider->ReleaseCachedAd(CachedAdId);
	Provider->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobMrecCapabilitiesTest,
	"OpenMobile.Ads.AdMob.Mrec.Capabilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobMrecCapabilitiesTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsAdMobTestAdTests;
	IOpenMobileAdsProvider* Provider = FindProvider();
	TestNotNull(TEXT("The AdMob provider is registered"), Provider);
	if (!Provider)
	{
		return false;
	}

	const FOpenMobileAdsProviderCapabilities Capabilities =
		Provider->GetCapabilities();
	const FOpenMobileAdFormatCapabilities* Mrec =
		Capabilities.FindFormat(EOpenMobileAdFormat::MediumRectangle);
	TestNotNull(TEXT("AdMob reports MREC capabilities"), Mrec);
	if (!Mrec)
	{
		return false;
	}
	TestTrue(TEXT("MREC can load"), Mrec->bCanLoad);
	TestTrue(TEXT("MREC can show"), Mrec->bCanShow);
	TestTrue(TEXT("MREC can hide"), Mrec->bCanHide);
	TestTrue(TEXT("MREC preserves its cache when hidden"), Mrec->bPreservesCachedAdOnHide);
	TestTrue(TEXT("MREC can preload"), Mrec->bSupportsPreload);
	TestTrue(TEXT("MREC reports impressions"), Mrec->bReportsImpression);
	TestTrue(TEXT("MREC reports clicks"), Mrec->bReportsClick);
	TestTrue(TEXT("MREC reports revenue"), Mrec->bReportsRevenue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobMrecContractTest,
	"OpenMobile.Ads.AdMob.Mrec.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobMrecContractTest::RunTest(const FString& Parameters)
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

	FOpenMobileAdsInitializationRequest Initialization;
	Initialization.RequestId = FGuid::NewGuid();
	Initialization.Platform = EOpenMobileAdsPlatform::Android;
	Initialization.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> InitializationSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError Error;
	TestTrue(
		TEXT("AdMob initializes before MREC loading"),
		Provider->Initialize(Initialization, InitializationSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);

	FOpenMobileAdsLoadRequest Load;
	Load.RequestId = FGuid::NewGuid();
	Load.Placement.Placement = TEXT("MenuOffer");
	Load.Placement.Format = EOpenMobileAdFormat::MediumRectangle;
	Load.Placement.AdUnitId = TEXT("production-mrec");
	Load.Placement.BannerLayout.Anchor = EOpenMobileAdsBannerAnchor::Center;
	Load.Placement.BannerLayout.HorizontalAlignment =
		EOpenMobileAdsBannerHorizontalAlignment::Left;
	Load.Placement.BannerLayout.Margins.Left = 20.0f;
	Load.Placement.BannerLayout.Margins.Top = 16.0f;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> LoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(TEXT("AdMob starts an MREC load"), Provider->Load(Load, LoadSink, Error));
	TestEqual(TEXT("MREC uses the persistent native path"), Backend.BannerLoadRequestIds.Num(), 1);
	if (Backend.BannerLoadRequestIds.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	TestEqual(
		TEXT("The native load receives the MREC format"),
		Backend.BannerLoadFormats[0],
		EOpenMobileAdFormat::MediumRectangle
	);
	TestEqual(
		TEXT("MREC development mode uses Google's banner test ID"),
		Backend.BannerLoadedAdUnitIds[0],
		FString(TEXT("ca-app-pub-3940256099942544/6300978111"))
	);
	FOpenMobileAdsAdMobPlatform::NativeBannerLoadCompleted(
		Backend.BannerLoadRequestIds[0]
	);
	if (LoadSink->Events.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	const FGuid CachedAdId = LoadSink->Events[0].CachedAdId;

	FOpenMobileAdsShowRequest Show;
	Show.RequestId = FGuid::NewGuid();
	Show.CachedAdId = CachedAdId;
	Show.Placement = Load.Placement.Placement;
	Show.Format = EOpenMobileAdFormat::MediumRectangle;
	Show.BannerLayout = Load.Placement.BannerLayout;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ShowSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(TEXT("AdMob shows the cached MREC"), Provider->Show(Show, ShowSink, Error));
	TestEqual(
		TEXT("MREC show preserves horizontal positioning"),
		Backend.ShownBannerLayout.HorizontalAlignment,
		EOpenMobileAdsBannerHorizontalAlignment::Left
	);
	FOpenMobileAdsAdMobPlatform::NativeBannerShown(Backend.BannerShowRequestId);

	FOpenMobileAdsHideRequest Hide;
	Hide.RequestId = FGuid::NewGuid();
	Hide.CachedAdId = CachedAdId;
	Hide.Placement = Load.Placement.Placement;
	Hide.Format = EOpenMobileAdFormat::MediumRectangle;
	Hide.bPreserveCachedAd = true;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> HideSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(TEXT("AdMob hides the MREC without consuming it"), Provider->Hide(Hide, HideSink, Error));
	FOpenMobileAdsAdMobPlatform::NativeBannerHidden(Backend.BannerHideRequestId);
	TestEqual(TEXT("MREC hide completes once"), HideSink->Events.Num(), 1);
	Provider->ReleaseCachedAd(CachedAdId);
	Provider->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobFixedBannerContractTest,
	"OpenMobile.Ads.AdMob.FixedBanner.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobFixedBannerContractTest::RunTest(
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

	FOpenMobileAdsInitializationRequest Initialization;
	Initialization.RequestId = FGuid::NewGuid();
	Initialization.Platform = EOpenMobileAdsPlatform::Android;
	Initialization.Development = FOpenMobileAdsDevelopmentConfiguration::FromMode(true);
	const TSharedRef<FInitializationSink, ESPMode::ThreadSafe> InitializationSink =
		MakeShared<FInitializationSink, ESPMode::ThreadSafe>();
	FOpenMobileAdsError Error;
	TestTrue(
		TEXT("AdMob initializes before fixed-banner loading"),
		Provider->Initialize(Initialization, InitializationSink, Error)
	);
	FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(
		Backend.InitializationRequestId
	);

	FOpenMobileAdsLoadRequest Load;
	Load.RequestId = FGuid::NewGuid();
	Load.Placement.Placement = TEXT("MenuBanner");
	Load.Placement.Format = EOpenMobileAdFormat::Banner;
	Load.Placement.AdUnitId = TEXT("production-banner");
	Load.PrivacyContext.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Restricted;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> LoadSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob starts a fixed-banner load"),
		Provider->Load(Load, LoadSink, Error)
	);
	TestEqual(
		TEXT("The fixed-banner load reaches its native path"),
		Backend.BannerLoadRequestIds.Num(),
		1
	);
	if (Backend.BannerLoadRequestIds.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	TestEqual(
		TEXT("Development mode selects Google's Android banner test ID"),
		Backend.BannerLoadedAdUnitIds[0],
		FString(TEXT("ca-app-pub-3940256099942544/6300978111"))
	);
	TestEqual(
		TEXT("Banner loads preserve the current privacy mode"),
		Backend.BannerLoadDataProcessingModes[0],
		EOpenMobileAdsDataProcessingMode::Restricted
	);
	FOpenMobileAdsAdMobPlatform::NativeBannerLoadCompleted(
		Backend.BannerLoadRequestIds[0]
	);
	TestEqual(TEXT("The fixed-banner load completes once"), LoadSink->Events.Num(), 1);
	if (LoadSink->Events.Num() != 1)
	{
		Provider->Shutdown();
		return false;
	}
	const FGuid CachedAdId = LoadSink->Events[0].CachedAdId;
	TestTrue(TEXT("The fixed banner receives a cache identity"), CachedAdId.IsValid());

	FOpenMobileAdsShowRequest Show;
	Show.RequestId = FGuid::NewGuid();
	Show.CachedAdId = CachedAdId;
	Show.Placement = Load.Placement.Placement;
	Show.Format = EOpenMobileAdFormat::Banner;
	Show.BannerLayout.Anchor = EOpenMobileAdsBannerAnchor::Top;
	Show.BannerLayout.bRespectSafeArea = true;
	Show.BannerLayout.Margins.Left = 6.0f;
	Show.BannerLayout.Margins.Top = 10.0f;
	Show.BannerLayout.Margins.Right = 14.0f;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ShowSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob shows the cached fixed banner"),
		Provider->Show(Show, ShowSink, Error)
	);
	TestEqual(TEXT("Fixed-banner show reaches the native path"), Backend.BannerShowCalls, 1);
	TestEqual(
		TEXT("Fixed-banner show retains its native cache"),
		Backend.ShownBannerLoadedRequestId,
		Backend.BannerLoadRequestIds[0]
	);
	TestEqual(
		TEXT("Fixed-banner show forwards top anchoring"),
		Backend.ShownBannerLayout.Anchor,
		EOpenMobileAdsBannerAnchor::Top
	);
	TestTrue(
		TEXT("Fixed-banner show forwards safe-area handling"),
		Backend.ShownBannerLayout.bRespectSafeArea
	);
	TestEqual(
		TEXT("Fixed-banner show forwards asymmetric margins"),
		Backend.ShownBannerLayout.Margins.Right,
		14.0f
	);
	FOpenMobileAdsAdMobPlatform::NativeBannerShown(Backend.BannerShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeImpression(Backend.BannerShowRequestId);
	FOpenMobileAdsAdMobPlatform::NativeClicked(Backend.BannerShowRequestId);
	FOpenMobileAdsRevenueSource RevenueSource;
	RevenueSource.SourceName = TEXT("Google Ads");
	RevenueSource.SourceId = TEXT("5450213213286189855");
	RevenueSource.AdapterClassName =
		TEXT("com.google.ads.mediation.admob.AdMobAdapter");
	RevenueSource.InstanceName = TEXT("AdMob Network");
	RevenueSource.InstanceId = TEXT("4665218928925097");
	FOpenMobileAdsAdMobPlatform::NativeRevenuePaid(
		Backend.BannerShowRequestId,
		2500,
		TEXT("USD"),
		static_cast<int32>(EOpenMobileAdsRevenuePrecision::Estimated),
		RevenueSource
	);
	const EOpenMobileAdsEventType ExpectedShowEvents[] = {
		EOpenMobileAdsEventType::Shown,
		EOpenMobileAdsEventType::Impression,
		EOpenMobileAdsEventType::Clicked,
		EOpenMobileAdsEventType::RevenuePaid
	};
	TestEqual(
		TEXT("The visible fixed banner emits its callback lifecycle"),
		ShowSink->Events.Num(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedShowEvents))
	);
	for (
		int32 Index = 0;
		Index < ShowSink->Events.Num()
			&& Index < static_cast<int32>(UE_ARRAY_COUNT(ExpectedShowEvents));
		++Index
	)
	{
		TestEqual(
			TEXT("Fixed-banner callback order is preserved"),
			ShowSink->Events[Index].Type,
			ExpectedShowEvents[Index]
		);
	}
	if (ShowSink->Events.Num() == static_cast<int32>(UE_ARRAY_COUNT(ExpectedShowEvents)))
	{
		const FOpenMobileAdsRevenueSource& ReportedSource =
			ShowSink->Events[3].Revenue.Source;
		TestEqual(
			TEXT("AdMob preserves the winning source name"),
			ReportedSource.SourceName,
			RevenueSource.SourceName
		);
		TestEqual(
			TEXT("AdMob preserves the winning source ID"),
			ReportedSource.SourceId,
			RevenueSource.SourceId
		);
		TestEqual(
			TEXT("AdMob preserves the winning adapter class"),
			ReportedSource.AdapterClassName,
			RevenueSource.AdapterClassName
		);
		TestEqual(
			TEXT("AdMob preserves the winning instance name"),
			ReportedSource.InstanceName,
			RevenueSource.InstanceName
		);
		TestEqual(
			TEXT("AdMob preserves the winning instance ID"),
			ReportedSource.InstanceId,
			RevenueSource.InstanceId
		);
	}

	FOpenMobileAdsHideRequest Hide;
	Hide.RequestId = FGuid::NewGuid();
	Hide.CachedAdId = CachedAdId;
	Hide.Placement = Load.Placement.Placement;
	Hide.Format = EOpenMobileAdFormat::Banner;
	Hide.bPreserveCachedAd = true;
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> HideSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("AdMob hides the visible fixed banner"),
		Provider->Hide(Hide, HideSink, Error)
	);
	TestEqual(TEXT("Fixed-banner hide reaches the native path"), Backend.BannerHideCalls, 1);
	TestEqual(
		TEXT("Fixed-banner hide targets the same native cache"),
		Backend.HiddenBannerLoadedRequestId,
		Backend.BannerLoadRequestIds[0]
	);
	FOpenMobileAdsAdMobPlatform::NativeBannerHidden(Backend.BannerHideRequestId);
	TestEqual(TEXT("Fixed-banner hide completes once"), HideSink->Events.Num(), 1);
	if (HideSink->Events.Num() == 1)
	{
		TestEqual(
			TEXT("Fixed-banner hide emits Hidden"),
			HideSink->Events[0].Type,
			EOpenMobileAdsEventType::Hidden
		);
	}

	FOpenMobileAdsShowRequest Reshow = Show;
	Reshow.RequestId = FGuid::NewGuid();
	const TSharedRef<FEventSink, ESPMode::ThreadSafe> ReshowSink =
		MakeShared<FEventSink, ESPMode::ThreadSafe>();
	TestTrue(
		TEXT("A hidden fixed banner can be shown again without reloading"),
		Provider->Show(Reshow, ReshowSink, Error)
	);
	TestEqual(TEXT("Reshow uses the same native cache"), Backend.BannerShowCalls, 2);
	TestEqual(
		TEXT("Reshow retains the original native load"),
		Backend.ShownBannerLoadedRequestId,
		Backend.BannerLoadRequestIds[0]
	);

	const int32 CancelledBannersBeforeRelease =
		Backend.CancelledBannerRequestIds.Num();
	Provider->ReleaseCachedAd(CachedAdId);
	TestTrue(
		TEXT("Fixed-banner release destroys the matching native view"),
		Backend.CancelledBannerRequestIds.Contains(Backend.BannerLoadRequestIds[0])
	);
	TestEqual(
		TEXT("Fixed-banner release destroys one native view"),
		Backend.CancelledBannerRequestIds.Num(),
		CancelledBannersBeforeRelease + 1
	);
	Provider->ReleaseCachedAd(CachedAdId);
	TestEqual(
		TEXT("Repeated fixed-banner release is safe"),
		Backend.CancelledBannerRequestIds.Num(),
		CancelledBannersBeforeRelease + 1
	);
	FOpenMobileAdsAdMobPlatform::NativeBannerShown(Backend.BannerShowRequestId);
	TestTrue(
		TEXT("A late callback cannot revive a destroyed fixed banner"),
		ReshowSink->Events.IsEmpty()
	);
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
		7
	);
	TestTrue(
		TEXT("The supported app-open format has a test-ad contract"),
		Capabilities.FindFormat(EOpenMobileAdFormat::AppOpen) != nullptr
	);
	TestTrue(
		TEXT("The supported fixed-banner format has a test-ad contract"),
		Capabilities.FindFormat(EOpenMobileAdFormat::Banner) != nullptr
	);
	TestTrue(
		TEXT("The supported adaptive-banner format has a test-ad contract"),
		Capabilities.FindFormat(
			EOpenMobileAdFormat::AnchoredAdaptiveBanner
		) != nullptr
	);
	TestTrue(
		TEXT("The supported MREC format has a test-ad contract"),
		Capabilities.FindFormat(EOpenMobileAdFormat::MediumRectangle) != nullptr
	);
	TestTrue(
		TEXT("The supported rewarded-interstitial format has a test-ad contract"),
		Capabilities.FindFormat(EOpenMobileAdFormat::RewardedInterstitial)
			!= nullptr
	);
	TestTrue(
		TEXT("The supported interstitial format has a test-ad contract"),
		Capabilities.FindFormat(EOpenMobileAdFormat::Interstitial) != nullptr
	);
	TestTrue(
		TEXT("The supported rewarded format has a test-ad contract"),
		Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded) != nullptr
	);
	if (const FOpenMobileAdFormatCapabilities* Rewarded =
		Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded))
	{
		TestTrue(
			TEXT("The rewarded format supports automatic preloading"),
			Rewarded->bSupportsPreload
		);
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdMobRevenuePrecisionMappingTest,
	"OpenMobile.Ads.AdMob.Revenue.Precision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdMobRevenuePrecisionMappingTest::RunTest(
	const FString& Parameters
)
{
	const TPair<int32, EOpenMobileAdsRevenuePrecision> KnownMappings[] = {
		{0, EOpenMobileAdsRevenuePrecision::Unknown},
		{1, EOpenMobileAdsRevenuePrecision::Estimated},
		{2, EOpenMobileAdsRevenuePrecision::PublisherProvided},
		{3, EOpenMobileAdsRevenuePrecision::Precise}
	};
	for (const TPair<int32, EOpenMobileAdsRevenuePrecision>& Mapping : KnownMappings)
	{
		TestEqual(
			TEXT("AdMob precision maps explicitly"),
			FOpenMobileAdsAdMobPlatform::MapRevenuePrecision(Mapping.Key),
			Mapping.Value
		);
	}

	for (const int32 UnknownValue : {-1, 4, MAX_int32})
	{
		TestEqual(
			TEXT("Unknown AdMob precision does not overstate certainty"),
			FOpenMobileAdsAdMobPlatform::MapRevenuePrecision(UnknownValue),
			EOpenMobileAdsRevenuePrecision::Unknown
		);
	}
	return true;
}

#endif
