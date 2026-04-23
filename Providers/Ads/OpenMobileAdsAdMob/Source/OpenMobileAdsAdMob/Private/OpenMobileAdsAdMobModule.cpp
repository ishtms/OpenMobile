#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsAdMobPlatform.h"
#include "OpenMobileAdsAdMobSettings.h"
#include "OpenMobileAdsDiagnostics.h"

namespace OpenMobileAdsAdMobPrivate
{
	class FProvider final : public IOpenMobileAdsProvider
	{
	public:
		virtual FName GetProviderName() const override { return TEXT("AdMob"); }
		virtual bool IsSupported() const override { return FOpenMobileAdsAdMobPlatform::IsSupported(); }
		virtual FOpenMobileAdsProviderCapabilities GetCapabilities() const override
		{
			FOpenMobileAdFormatCapabilities Rewarded;
			Rewarded.Format = EOpenMobileAdFormat::Rewarded;
			Rewarded.bCanLoad = true;
			Rewarded.bCanShow = true;
			Rewarded.bReportsImpression = true;
			Rewarded.bReportsClick = true;
			Rewarded.bReportsDismiss = true;
			Rewarded.bReportsReward = true;
			Rewarded.bReportsRevenue = true;
			Rewarded.bSupportsServerVerification = true;
			Rewarded.MaxCachedAdsPerPlacement = 1;
			Rewarded.CacheLifetimeSeconds = 60.0 * 60.0;

			FOpenMobileAdsProviderCapabilities Capabilities;
			Capabilities.Provider = GetProviderName();
#if PLATFORM_ANDROID
			Capabilities.ProviderVersion = TEXT("25.4.0");
#elif PLATFORM_IOS
			Capabilities.ProviderVersion = TEXT("13.8.0");
#endif
			Capabilities.Formats.Add(Rewarded);
			return Capabilities;
		}

		virtual FName GetConsentProviderName() const override
		{
			return TEXT("GoogleUMP");
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
			if (Request.Placement.Format != EOpenMobileAdFormat::Rewarded)
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
				ProviderRequest.Placement.AdUnitId =
					GetDefault<UOpenMobileAdsAdMobSettings>()->ResolveRewardedAdUnitId(
						InitializedPlatform,
						true
					);
			}
			ProviderRequest.Placement.AdUnitId.TrimStartAndEndInline();
			if (ProviderRequest.Placement.AdUnitId.IsEmpty())
			{
				OutError = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::NotConfigured,
					EOpenMobileAdsFailureStage::Load,
					Request.Placement.Placement,
					TEXT("No AdMob rewarded-ad unit ID is configured for this placement."),
					GetProviderName()
				);
				return false;
			}

			FString NativeError;
			const bool bStarted = FOpenMobileAdsAdMobPlatform::BeginLoad(
				ProviderRequest,
				FOnOpenMobileAdMobRewardedCached::CreateLambda([EventSink](FGuid CachedAdId)
				{
					FOpenMobileAdsEvent Loaded;
					Loaded.Type = EOpenMobileAdsEventType::Loaded;
					Loaded.CachedAdId = CachedAdId;
					EventSink->Submit(MoveTemp(Loaded));
				}),
				FOnOpenMobileAdMobRewardedFailed::CreateLambda(
					[EventSink](FString ErrorMessage)
					{
						FOpenMobileAdsEvent Failed;
						Failed.Type = EOpenMobileAdsEventType::LoadFailed;
						Failed.Error = FOpenMobileAdsError::Make(
							EOpenMobileAdsErrorCode::NativeFailure,
							EOpenMobileAdsFailureStage::Load,
							NAME_None,
							ErrorMessage.IsEmpty()
								? TEXT("AdMob failed to load a rewarded ad.")
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
						? TEXT("AdMob could not start the rewarded-ad load.")
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
			if (Request.Format != EOpenMobileAdFormat::Rewarded)
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
						? TEXT("AdMob could not present the cached rewarded ad.")
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
