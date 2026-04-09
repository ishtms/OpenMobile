#include "OpenMobileAdsSubsystem.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsProvider.h"
#include "Misc/ConfigCacheIni.h"
#include "OpenMobileCoreLog.h"

namespace OpenMobileAdsPrivate
{
	FString GetPreferredProvider()
	{
		FString PreferredProvider;
		GConfig->GetString(
			TEXT("OpenMobileAds"),
			TEXT("PreferredProvider"),
			PreferredProvider,
			GEngineIni
		);
		return PreferredProvider.TrimStartAndEnd();
	}
}

IOpenMobileAdsProvider* UOpenMobileAdsSubsystem::FindProvider() const
{
	TArray<IOpenMobileAdsProvider*> Providers =
		IModularFeatures::Get().GetModularFeatureImplementations<IOpenMobileAdsProvider>(
			IOpenMobileAdsProvider::GetModularFeatureName()
		);
	const FString PreferredProvider = OpenMobileAdsPrivate::GetPreferredProvider();

	IOpenMobileAdsProvider* Best = nullptr;
	for (IOpenMobileAdsProvider* Candidate : Providers)
	{
		if (!Candidate || !Candidate->IsSupported())
		{
			continue;
		}

		if (!PreferredProvider.IsEmpty())
		{
			if (Candidate->GetProviderName().ToString().Equals(PreferredProvider, ESearchCase::IgnoreCase))
			{
				return Candidate;
			}
			continue;
		}

		const bool bHigherPriority = !Best || Candidate->GetPriority() > Best->GetPriority();
		const bool bStableTieBreak = Best
			&& Candidate->GetPriority() == Best->GetPriority()
			&& Candidate->GetProviderName().LexicalLess(Best->GetProviderName());
		if (bHigherPriority || bStableTieBreak)
		{
			Best = Candidate;
		}
	}

	return Best;
}

void UOpenMobileAdsSubsystem::Deinitialize()
{
	State = EOpenMobileRewardedAdState::Idle;
	Super::Deinitialize();
}

bool UOpenMobileAdsSubsystem::RequestAndShowRewardedAd()
{
	if (State != EOpenMobileRewardedAdState::Idle)
	{
		HandleAdFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::Busy,
			TEXT("A rewarded ad is already loading or showing.")
		));
		return false;
	}

	IOpenMobileAdsProvider* Provider = FindProvider();
	if (!Provider)
	{
		const FString PreferredProvider = OpenMobileAdsPrivate::GetPreferredProvider();
		HandleAdFailed(FOpenMobileError::Make(
			PreferredProvider.IsEmpty()
				? EOpenMobileErrorCode::NotSupported
				: EOpenMobileErrorCode::NotConfigured,
			PreferredProvider.IsEmpty()
				? TEXT("No enabled OpenMobile ads provider supports this platform.")
				: FString::Printf(
					TEXT("The preferred ads provider '%s' is unavailable."),
					*PreferredProvider
				)
		));
		return false;
	}

	State = EOpenMobileRewardedAdState::Loading;
	FOpenMobileRewardedAdCallbacks Callbacks;
	Callbacks.OnLoaded = FOpenMobileRewardedAdLoadedCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleAdLoaded
	);
	Callbacks.OnShown = FOpenMobileRewardedAdShownCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleAdShown
	);
	Callbacks.OnEarned = FOpenMobileRewardedAdEarnedCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleRewardEarned
	);
	Callbacks.OnClosed = FOpenMobileRewardedAdClosedCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleAdClosed
	);
	Callbacks.OnFailed = FOpenMobileRewardedAdFailedCallback::CreateUObject(
		this,
		&UOpenMobileAdsSubsystem::HandleAdFailed
	);

	FOpenMobileError Error;
	if (!Provider->RequestAndShowRewardedAd(MoveTemp(Callbacks), Error))
	{
		if (!Error.IsSet())
		{
			Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::Internal,
				TEXT("The ads provider rejected the request without returning an error."),
				FString(),
				Provider->GetProviderName().ToString()
			);
		}
		HandleAdFailed(MoveTemp(Error));
		return false;
	}

	return true;
}

bool UOpenMobileAdsSubsystem::IsSupported() const
{
	return FindProvider() != nullptr;
}

FName UOpenMobileAdsSubsystem::GetActiveProviderName() const
{
	const IOpenMobileAdsProvider* Provider = FindProvider();
	return Provider ? Provider->GetProviderName() : NAME_None;
}

void UOpenMobileAdsSubsystem::HandleAdLoaded()
{
	if (State == EOpenMobileRewardedAdState::Loading)
	{
		OnAdLoaded.Broadcast();
	}
}

void UOpenMobileAdsSubsystem::HandleAdShown()
{
	State = EOpenMobileRewardedAdState::Showing;
	OnAdShown.Broadcast();
}

void UOpenMobileAdsSubsystem::HandleRewardEarned(int32 NetworkAmount, FString NetworkRewardType)
{
	OnRewardEarned.Broadcast(NetworkAmount, NetworkRewardType);
}

void UOpenMobileAdsSubsystem::HandleAdClosed()
{
	State = EOpenMobileRewardedAdState::Idle;
	OnAdClosed.Broadcast();
}

void UOpenMobileAdsSubsystem::HandleAdFailed(FOpenMobileError Error)
{
	State = EOpenMobileRewardedAdState::Idle;
	UE_LOG(LogOpenMobile, Warning, TEXT("Ads request failed: %s"), *Error.Message);
	OnAdFailed.Broadcast(Error);
}
