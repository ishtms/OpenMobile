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
			Rewarded.bReportsDismiss = true;
			Rewarded.bReportsReward = true;

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

		virtual bool Initialize(
			const FOpenMobileAdsInitializationRequest& Request,
			TSharedRef<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			FOpenMobileAdsInitializationRequest ProviderRequest = Request;
			const UOpenMobileAdsAdMobSettings* Settings =
				GetDefault<UOpenMobileAdsAdMobSettings>();
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
			}
			return bStarted;
		}

		virtual void Shutdown() override
		{
			FOpenMobileAdsAdMobPlatform::Shutdown();
			bUseTestAdUnitIds = false;
		}

		virtual bool RequestAndShowRewardedAd(
			FOpenMobileRewardedAdCallbacks&& Callbacks,
			FOpenMobileError& OutError
		) override
		{
			const UOpenMobileAdsAdMobSettings* Settings = GetDefault<UOpenMobileAdsAdMobSettings>();
			FString AdUnitId = Settings->ResolveRewardedAdUnitId(
				OpenMobileAdsGetCurrentPlatform(),
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
