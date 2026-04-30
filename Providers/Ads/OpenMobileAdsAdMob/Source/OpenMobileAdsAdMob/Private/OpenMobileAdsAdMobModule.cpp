#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsAdMobPlatform.h"
#include "OpenMobileAdsAdMobSettings.h"
#include "OpenMobileAdsDiagnostics.h"

namespace OpenMobileAdsAdMobPrivate
{
	bool IsPersistentDisplayFormat(EOpenMobileAdFormat Format)
	{
		return Format == EOpenMobileAdFormat::Banner
			|| Format == EOpenMobileAdFormat::AnchoredAdaptiveBanner
			|| Format == EOpenMobileAdFormat::MediumRectangle;
	}

	class FProvider final : public IOpenMobileAdsProvider
	{
	public:
		virtual FName GetProviderName() const override { return TEXT("AdMob"); }
		virtual bool IsSupported() const override { return FOpenMobileAdsAdMobPlatform::IsSupported(); }
		virtual FOpenMobileAdsProviderCapabilities GetCapabilities() const override
		{
			FOpenMobileAdFormatCapabilities Banner;
			Banner.Format = EOpenMobileAdFormat::Banner;
			Banner.bCanLoad = true;
			Banner.bCanShow = true;
			Banner.bCanHide = true;
			Banner.bPreservesCachedAdOnHide = true;
			Banner.bSupportsPreload = true;
			Banner.bReportsImpression = true;
			Banner.bReportsClick = true;
			Banner.bReportsRevenue = true;
			Banner.MaxCachedAdsPerPlacement = 1;
			Banner.CacheLifetimeSeconds = 0.0;

			FOpenMobileAdFormatCapabilities AdaptiveBanner = Banner;
			AdaptiveBanner.Format =
				EOpenMobileAdFormat::AnchoredAdaptiveBanner;
			FOpenMobileAdFormatCapabilities MediumRectangle = Banner;
			MediumRectangle.Format = EOpenMobileAdFormat::MediumRectangle;

			FOpenMobileAdFormatCapabilities Interstitial;
			Interstitial.Format = EOpenMobileAdFormat::Interstitial;
			Interstitial.bCanLoad = true;
			Interstitial.bCanShow = true;
			Interstitial.bSupportsPreload = true;
			Interstitial.bReportsImpression = true;
			Interstitial.bReportsClick = true;
			Interstitial.bReportsDismiss = true;
			Interstitial.bReportsRevenue = true;
			Interstitial.MaxCachedAdsPerPlacement = 1;
			Interstitial.CacheLifetimeSeconds = 60.0 * 60.0;

			FOpenMobileAdFormatCapabilities Rewarded;
			Rewarded.Format = EOpenMobileAdFormat::Rewarded;
			Rewarded.bCanLoad = true;
			Rewarded.bCanShow = true;
			Rewarded.bSupportsPreload = true;
			Rewarded.bReportsImpression = true;
			Rewarded.bReportsClick = true;
			Rewarded.bReportsDismiss = true;
			Rewarded.bReportsReward = true;
			Rewarded.bReportsRevenue = true;
			Rewarded.bSupportsServerVerification = true;
			Rewarded.MaxCachedAdsPerPlacement = 1;
			Rewarded.CacheLifetimeSeconds = 60.0 * 60.0;

			FOpenMobileAdFormatCapabilities RewardedInterstitial = Rewarded;
			RewardedInterstitial.Format = EOpenMobileAdFormat::RewardedInterstitial;
			RewardedInterstitial.bRequiresIntroduction = true;

			FOpenMobileAdsProviderCapabilities Capabilities;
			Capabilities.Provider = GetProviderName();
#if PLATFORM_ANDROID
			Capabilities.ProviderVersion = TEXT("25.4.0");
#elif PLATFORM_IOS
			Capabilities.ProviderVersion = TEXT("13.8.0");
#endif
			Capabilities.Formats.Add(Banner);
			Capabilities.Formats.Add(AdaptiveBanner);
			Capabilities.Formats.Add(MediumRectangle);
			Capabilities.Formats.Add(Interstitial);
			Capabilities.Formats.Add(Rewarded);
			Capabilities.Formats.Add(RewardedInterstitial);
			return Capabilities;
		}

		virtual FName GetConsentProviderName() const override
		{
			return TEXT("GoogleUMP");
		}

		virtual bool SupportsPrivacyOptionsForm() const override
		{
			return true;
		}

		virtual bool SupportsConsentResetForTesting() const override
		{
			return true;
		}

		virtual bool ResetConsentForTesting(
			FOpenMobileAdsError& OutError
		) override
		{
			FString NativeError;
			if (!FOpenMobileAdsAdMobPlatform::ResetConsentForTesting(NativeError))
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NativeFailure,
					EOpenMobileAdsFailureStage::Consent,
					NAME_None,
					NativeError.IsEmpty()
						? TEXT("Google UMP consent state could not be reset.")
						: MoveTemp(NativeError),
					GetConsentProviderName()
				);
				return false;
			}
			LastConsentSignals = FOpenMobileAdsConsentSignals();
			return true;
		}

		virtual int32 GetSupportedConsentSignalMask() const override
		{
			return FOpenMobileAdsConsentSignals::AllSignalMask;
		}

		virtual int32 GetConfirmableConsentSignalMask() const override
		{
			return FOpenMobileAdsConsentSignals::AllSignalMask;
		}

		virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
		{
			return static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr)
				| static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
		}

		virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
			const FOpenMobileAdsConsentSignals& Signals,
			int32 SignalMask
		) override
		{
			SignalMask &= FOpenMobileAdsConsentSignals::AllSignalMask;
			FString NativeError;
			if (!FOpenMobileAdsAdMobPlatform::ApplyConsentSignals(
				Signals,
				SignalMask,
				NativeError
			))
			{
				FOpenMobileAdsConsentSignalApplyResult Result;
				Result.Error = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NativeFailure,
					EOpenMobileAdsFailureStage::Consent,
					NAME_None,
					NativeError.IsEmpty()
						? TEXT("AdMob could not apply consent signals.")
						: MoveTemp(NativeError),
					GetProviderName()
				);
				return Result;
			}
			LastConsentSignals = Signals;
			return FOpenMobileAdsConsentSignalApplyResult::Applied(
				SignalMask,
				SignalMask
			);
		}

		virtual bool RefreshConsent(
			const FOpenMobileAdsConsentRequest& Request,
			TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			FOpenMobileAdsConsentRequest ProviderRequest = Request;
			const UOpenMobileAdsAdMobSettings* Settings =
				GetDefault<UOpenMobileAdsAdMobSettings>();
			ProviderRequest.Development.TestDeviceIdentifiers =
				Request.Development.bEnableConsentDebug
					? Settings->ResolveTestDeviceIdentifiers(
						Request.Development.TestDeviceIdentifiers
					)
					: TArray<FString>();
			FString NativeError;
			const bool bStarted =
				FOpenMobileAdsAdMobPlatform::BeginConsentRefresh(
					ProviderRequest,
					FOnOpenMobileAdMobConsentCompleted::CreateLambda(
						[CompletionSink](
							FOpenMobileAdsConsentStatusUpdate Update
						) mutable
						{
							CompletionSink->Complete(MoveTemp(Update));
						}
					),
					FOnOpenMobileAdMobConsentFailed::CreateLambda(
						[CompletionSink](FOpenMobileAdsError Error) mutable
						{
							CompletionSink->Fail(MoveTemp(Error));
						}
					),
					NativeError
				);
			if (!bStarted)
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NativeFailure,
					EOpenMobileAdsFailureStage::Consent,
					NAME_None,
					NativeError.IsEmpty()
						? TEXT("Google UMP could not start its consent-info update.")
						: MoveTemp(NativeError),
					GetConsentProviderName()
				);
			}
			return bStarted;
		}

		virtual bool PresentRequiredConsentForm(
			const FOpenMobileAdsConsentRequest& Request,
			TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			FString NativeError;
			const bool bStarted =
				FOpenMobileAdsAdMobPlatform::BeginRequiredConsentForm(
					Request,
					FOnOpenMobileAdMobConsentCompleted::CreateLambda(
						[CompletionSink](
							FOpenMobileAdsConsentStatusUpdate Update
						) mutable
						{
							CompletionSink->Complete(MoveTemp(Update));
						}
					),
					FOnOpenMobileAdMobConsentFailed::CreateLambda(
						[CompletionSink](FOpenMobileAdsError Error) mutable
						{
							CompletionSink->Fail(MoveTemp(Error));
						}
					),
					NativeError
				);
			if (!bStarted)
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NativeFailure,
					EOpenMobileAdsFailureStage::Consent,
					NAME_None,
					NativeError.IsEmpty()
						? TEXT("Google UMP could not present its required consent form.")
						: MoveTemp(NativeError),
					GetConsentProviderName()
				);
			}
			return bStarted;
		}

		virtual bool PresentPrivacyOptionsForm(
			const FOpenMobileAdsConsentRequest& Request,
			TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			FString NativeError;
			const bool bStarted =
				FOpenMobileAdsAdMobPlatform::BeginPrivacyOptionsForm(
					Request,
					FOnOpenMobileAdMobConsentCompleted::CreateLambda(
						[CompletionSink](
							FOpenMobileAdsConsentStatusUpdate Update
						) mutable
						{
							CompletionSink->Complete(MoveTemp(Update));
						}
					),
					FOnOpenMobileAdMobConsentFailed::CreateLambda(
						[CompletionSink](FOpenMobileAdsError Error) mutable
						{
							CompletionSink->Fail(MoveTemp(Error));
						}
					),
					NativeError
				);
			if (!bStarted)
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NativeFailure,
					EOpenMobileAdsFailureStage::Consent,
					NAME_None,
					NativeError.IsEmpty()
						? TEXT("Google UMP could not present its privacy-options form.")
						: MoveTemp(NativeError),
					GetConsentProviderName()
				);
			}
			return bStarted;
		}

		virtual void CancelConsent(FGuid RequestId) override
		{
			FOpenMobileAdsAdMobPlatform::CancelConsent(RequestId);
		}

		virtual bool Initialize(
			const FOpenMobileAdsInitializationRequest& Request,
			TSharedRef<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			const UOpenMobileAdsAdMobSettings* Settings =
				GetDefault<UOpenMobileAdsAdMobSettings>();
			FString ConfigurationError;
			if (!Settings->IsConfigurationCompatibleWithMode(
				Request.Platform,
				Request.Development.bUseTestAdUnitIds,
				ConfigurationError
			))
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NotConfigured,
					EOpenMobileAdsFailureStage::Configuration,
					NAME_None,
					MoveTemp(ConfigurationError),
					GetProviderName(),
					TEXT("Enable Development/Test Mode or replace every Google sample identifier with a production identifier.")
				);
				return false;
			}

			FOpenMobileAdsInitializationRequest ProviderRequest = Request;
			ProviderRequest.Development.TestDeviceIdentifiers =
				Request.Development.bUseTestDevices
					? Settings->ResolveTestDeviceIdentifiers(
						Request.Development.TestDeviceIdentifiers
					)
					: TArray<FString>();
			FOpenMobileAdsLog::SetTestDeviceIdentifiers(
				ProviderRequest.Development.TestDeviceIdentifiers
			);
			FString NativeError;
			const bool bStarted = FOpenMobileAdsAdMobPlatform::Initialize(
				ProviderRequest,
				FOnOpenMobileAdMobInitializationStatus::CreateLambda(
					[CompletionSink](
						const FOpenMobileAdsInitializationComponentStatus& Status
					)
					{
						CompletionSink->UpdateStatus(Status);
					}
				),
				FOnOpenMobileAdMobInitialized::CreateLambda(
					[CompletionSink](FOpenMobileAdsError Error)
					{
						CompletionSink->Complete(MoveTemp(Error));
					}
				),
				NativeError
			);
			if (!bStarted)
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NativeFailure,
					EOpenMobileAdsFailureStage::Initialization,
					NAME_None,
					NativeError.IsEmpty()
						? TEXT("AdMob could not start SDK initialization.")
						: MoveTemp(NativeError),
					GetProviderName()
				);
			}
			else
			{
				bUseTestAdUnitIds = Request.Development.bUseTestAdUnitIds;
				InitializedPlatform = Request.Platform;
			}
			return bStarted;
		}

		virtual bool Load(
			const FOpenMobileAdsLoadRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			if (
				!IsPersistentDisplayFormat(Request.Placement.Format)
				&&
				Request.Placement.Format != EOpenMobileAdFormat::Interstitial
				&& Request.Placement.Format != EOpenMobileAdFormat::Rewarded
				&& Request.Placement.Format
					!= EOpenMobileAdFormat::RewardedInterstitial
			)
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::UnsupportedFormat,
					EOpenMobileAdsFailureStage::Load,
					Request.Placement.Placement,
					TEXT("AdMob does not support loading this placement format yet."),
					GetProviderName()
				);
				return false;
			}

			FOpenMobileAdsLoadRequest ProviderRequest = Request;
			if (bUseTestAdUnitIds)
			{
				const UOpenMobileAdsAdMobSettings* Settings =
					GetDefault<UOpenMobileAdsAdMobSettings>();
				switch (Request.Placement.Format)
				{
				case EOpenMobileAdFormat::Banner:
				case EOpenMobileAdFormat::AnchoredAdaptiveBanner:
				case EOpenMobileAdFormat::MediumRectangle:
					ProviderRequest.Placement.AdUnitId =
						Settings->ResolveBannerAdUnitId(InitializedPlatform, true);
					break;
				case EOpenMobileAdFormat::Interstitial:
					ProviderRequest.Placement.AdUnitId =
						Settings->ResolveInterstitialAdUnitId(InitializedPlatform, true);
					break;
				case EOpenMobileAdFormat::RewardedInterstitial:
					ProviderRequest.Placement.AdUnitId =
						Settings->ResolveRewardedInterstitialAdUnitId(
							InitializedPlatform,
							true
						);
					break;
				default:
					ProviderRequest.Placement.AdUnitId =
						Settings->ResolveRewardedAdUnitId(InitializedPlatform, true);
					break;
				}
			}
			ProviderRequest.Placement.AdUnitId.TrimStartAndEndInline();
			if (ProviderRequest.Placement.AdUnitId.IsEmpty())
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NotConfigured,
					EOpenMobileAdsFailureStage::Load,
					Request.Placement.Placement,
					IsPersistentDisplayFormat(Request.Placement.Format)
						? TEXT("No AdMob banner ad-unit ID is configured for this placement.")
						: Request.Placement.Format == EOpenMobileAdFormat::Interstitial
							? TEXT("No AdMob interstitial ad-unit ID is configured for this placement.")
							: TEXT("No AdMob rewarded ad-unit ID is configured for this placement."),
					GetProviderName()
				);
				return false;
			}

			FString NativeError;
			const bool bStarted = FOpenMobileAdsAdMobPlatform::BeginLoad(
				ProviderRequest,
				FOnOpenMobileAdMobAdCached::CreateLambda(
					[EventSink](
						FGuid CachedAdId,
						int64 RewardAmount,
						FString RewardType
					)
					{
						FOpenMobileAdsEvent Loaded;
						Loaded.Type = EOpenMobileAdsEventType::Loaded;
						Loaded.CachedAdId = CachedAdId;
						Loaded.bHasReward = RewardAmount > 0;
						Loaded.Reward.Amount = RewardAmount;
						Loaded.Reward.Type = MoveTemp(RewardType);
						EventSink->Submit(MoveTemp(Loaded));
					}
				),
				FOnOpenMobileAdMobAdLoadFailed::CreateLambda(
					[EventSink](FString ErrorMessage)
					{
						FOpenMobileAdsEvent Failed;
						Failed.Type = EOpenMobileAdsEventType::LoadFailed;
						Failed.Error = FOpenMobileAdsError::Make(
							EOpenMobileAdsErrorCode::NativeFailure,
							EOpenMobileAdsFailureStage::Load,
							NAME_None,
							ErrorMessage.IsEmpty()
								? TEXT("AdMob failed to load an ad.")
								: MoveTemp(ErrorMessage),
							TEXT("AdMob"),
							FString(),
							true
						);
						EventSink->Submit(MoveTemp(Failed));
					}
				),
				NativeError
			);
			if (!bStarted)
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NativeFailure,
					EOpenMobileAdsFailureStage::Load,
					Request.Placement.Placement,
					NativeError.IsEmpty()
						? TEXT("AdMob could not start the ad load.")
						: MoveTemp(NativeError),
					GetProviderName(),
					FString(),
					true
				);
			}
			return bStarted;
		}

		virtual bool Show(
			const FOpenMobileAdsShowRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			if (
				!IsPersistentDisplayFormat(Request.Format)
				&&
				Request.Format != EOpenMobileAdFormat::Interstitial
				&& Request.Format != EOpenMobileAdFormat::Rewarded
				&& Request.Format != EOpenMobileAdFormat::RewardedInterstitial
			)
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::UnsupportedFormat,
					EOpenMobileAdsFailureStage::Show,
					Request.Placement,
					TEXT("AdMob does not support showing this placement format yet."),
					GetProviderName()
				);
				return false;
			}

			FString NativeError;
			const bool bStarted = FOpenMobileAdsAdMobPlatform::BeginShow(
				Request,
				EventSink,
				NativeError
			);
			if (!bStarted)
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NativeFailure,
					EOpenMobileAdsFailureStage::Show,
					Request.Placement,
					NativeError.IsEmpty()
						? TEXT("AdMob could not present the cached ad.")
						: MoveTemp(NativeError),
					GetProviderName()
				);
			}
			return bStarted;
		}

		virtual bool Hide(
			const FOpenMobileAdsHideRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			if (!IsPersistentDisplayFormat(Request.Format))
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::UnsupportedFormat,
					EOpenMobileAdsFailureStage::Hide,
					Request.Placement,
					TEXT("AdMob supports hide only for persistent banner placements."),
					GetProviderName()
				);
				return false;
			}

			FString NativeError;
			const bool bStarted = FOpenMobileAdsAdMobPlatform::BeginHide(
				Request,
				EventSink,
				NativeError
			);
			if (!bStarted)
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NativeFailure,
					EOpenMobileAdsFailureStage::Hide,
					Request.Placement,
					NativeError.IsEmpty()
						? TEXT("AdMob could not hide the cached banner ad.")
						: MoveTemp(NativeError),
					GetProviderName()
				);
			}
			return bStarted;
		}

		virtual void Cancel(FGuid RequestId) override
		{
			FOpenMobileAdsAdMobPlatform::Cancel(RequestId);
		}

		virtual bool Destroy(
			const FOpenMobileAdsDestroyRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			FOpenMobileAdsEvent Destroyed;
			Destroyed.Type = EOpenMobileAdsEventType::Destroyed;
			EventSink->Submit(MoveTemp(Destroyed));
			return true;
		}

		virtual void ReleaseCachedAd(FGuid CachedAdId) override
		{
			FOpenMobileAdsAdMobPlatform::ReleaseCachedAd(CachedAdId);
		}

		virtual void Shutdown() override
		{
			FOpenMobileAdsAdMobPlatform::Shutdown();
			bUseTestAdUnitIds = false;
			InitializedPlatform = EOpenMobileAdsPlatform::Unsupported;
		}

		virtual bool RequestAndShowRewardedAd(
			FOpenMobileRewardedAdCallbacks&& Callbacks,
			FOpenMobileError& OutError
		) override
		{
			const UOpenMobileAdsAdMobSettings* Settings = GetDefault<UOpenMobileAdsAdMobSettings>();
			FString AdUnitId = Settings->ResolveRewardedAdUnitId(
				InitializedPlatform,
				bUseTestAdUnitIds
			);
			AdUnitId.TrimStartAndEndInline();
			if (AdUnitId.IsEmpty())
			{
				OutError = FOpenMobileError::Make(
					EOpenMobileErrorCode::NotConfigured,
					TEXT("No AdMob rewarded-ad unit ID is configured for this platform."),
					FString(),
					TEXT("AdMob")
				);
				return false;
			}

			FString NativeError;
			const bool bStarted = FOpenMobileAdsAdMobPlatform::BeginRequest(
				AdUnitId,
				MoveTemp(Callbacks.OnLoaded),
				MoveTemp(Callbacks.OnShown),
				MoveTemp(Callbacks.OnEarned),
				MoveTemp(Callbacks.OnClosed),
				FOnOpenMobileAdMobRewardedFailed::CreateLambda(
					[Failed = MoveTemp(Callbacks.OnFailed)](FString ErrorMessage) mutable
					{
						Failed.ExecuteIfBound(FOpenMobileError::Make(
							EOpenMobileErrorCode::NativeFailure,
							MoveTemp(ErrorMessage),
							FString(),
							TEXT("AdMob")
						));
					}
				),
				NativeError
			);
			if (!bStarted)
			{
				OutError = FOpenMobileError::Make(
					EOpenMobileErrorCode::NativeFailure,
					NativeError.IsEmpty()
						? TEXT("AdMob could not start the rewarded-ad request.")
						: MoveTemp(NativeError),
					FString(),
					TEXT("AdMob")
				);
			}
			return bStarted;
		}

	private:
		bool bUseTestAdUnitIds = false;
		EOpenMobileAdsPlatform InitializedPlatform = EOpenMobileAdsPlatform::Unsupported;
		FOpenMobileAdsConsentSignals LastConsentSignals;
	};
}

class FOpenMobileAdsAdMobModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Provider = MakeUnique<OpenMobileAdsAdMobPrivate::FProvider>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsProvider::GetModularFeatureName(),
			Provider.Get()
		);
	}

	virtual void ShutdownModule() override
	{
		if (Provider)
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsProvider::GetModularFeatureName(),
				Provider.Get()
			);
			Provider.Reset();
		}
		FOpenMobileAdsAdMobPlatform::Shutdown();
	}

private:
	TUniquePtr<OpenMobileAdsAdMobPrivate::FProvider> Provider;
};

IMPLEMENT_MODULE(FOpenMobileAdsAdMobModule, OpenMobileAdsAdMob)
