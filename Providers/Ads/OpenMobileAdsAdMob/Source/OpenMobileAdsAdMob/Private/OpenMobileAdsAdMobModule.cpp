#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsAdMobPlatform.h"
#include "OpenMobileAdsAdMobSettings.h"

namespace OpenMobileAdsAdMobPrivate
{
	class FProvider final : public IOpenMobileAdsProvider
	{
	public:
		virtual FName GetProviderName() const override { return TEXT("AdMob"); }
		virtual bool IsSupported() const override { return FOpenMobileAdsAdMobPlatform::IsSupported(); }

		virtual bool RequestAndShowRewardedAd(
			FOpenMobileRewardedAdCallbacks&& Callbacks,
			FOpenMobileError& OutError
		) override
		{
			const UOpenMobileAdsAdMobSettings* Settings = GetDefault<UOpenMobileAdsAdMobSettings>();
			FString AdUnitId;
#if PLATFORM_ANDROID
			AdUnitId = Settings->AndroidRewardedAdUnitId;
#elif PLATFORM_IOS
			AdUnitId = Settings->IOSRewardedAdUnitId;
#endif
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
