#include "OpenMobileAdsCanShowPolicy.h"

namespace
{
	FOpenMobileAdsCanShowResult Blocked(
		EOpenMobileAdsCanShowBlockReason Reason,
		const TCHAR* Explanation,
		FDateTime NextEligibleAt = FDateTime()
	)
	{
		FOpenMobileAdsCanShowResult Result;
		Result.BlockReason = Reason;
		Result.Explanation = Explanation;
		Result.NextEligibleAt = NextEligibleAt;
		return Result;
	}
}

FOpenMobileAdsCanShowResult FOpenMobileAdsCanShowPolicy::Evaluate(
	const FOpenMobileAdsCanShowPolicyContext& Context
)
{
	if (!Context.bPlacementConfigured)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::UnknownPlacement,
			TEXT("The requested ads placement is not configured.")
		);
	}
	if (!Context.bPlacementEnabled)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::Disabled,
			TEXT("The requested ads placement is disabled.")
		);
	}
	if (Context.ServiceState != EOpenMobileAdsServiceState::Ready)
	{
		FOpenMobileAdsCanShowResult Result = Blocked(
			EOpenMobileAdsCanShowBlockReason::NotInitialized,
			TEXT("The ads service is not ready.")
		);
		if (!Context.ServiceExplanation.IsEmpty())
		{
			Result.Explanation = Context.ServiceExplanation;
		}
		return Result;
	}
	if (!Context.bProviderAvailable)
	{
		FOpenMobileAdsCanShowResult Result = Blocked(
			EOpenMobileAdsCanShowBlockReason::ProviderUnavailable,
			TEXT("No supported ads provider is available.")
		);
		if (!Context.ProviderExplanation.IsEmpty())
		{
			Result.Explanation = Context.ProviderExplanation;
		}
		return Result;
	}
	if (!Context.bFormatSupported)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::UnsupportedFormat,
			TEXT("The selected provider cannot show this placement format.")
		);
	}
	if (!Context.bPrivacyAllowed)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::PrivacyBlocked,
			TEXT("The current privacy state does not allow ads to be shown.")
		);
	}
	if (Context.PlacementState == EOpenMobileAdPlacementState::Loading)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::Loading,
			TEXT("The placement is still loading.")
		);
	}
	if (
		Context.PlacementState != EOpenMobileAdPlacementState::Ready
		|| !Context.bHasCachedAd
	)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::NotLoaded,
			TEXT("The placement does not have a ready ad.")
		);
	}
	if (Context.bExpired)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::Expired,
			TEXT("The cached ad has expired.")
		);
	}
	if (Context.bFrequencyCapped)
	{
		FOpenMobileAdsCanShowResult Result = Blocked(
			EOpenMobileAdsCanShowBlockReason::FrequencyCap,
			TEXT("The placement has reached its frequency cap."),
			Context.FrequencyCapEndsAt
		);
		Result.FrequencyCapScope = Context.FrequencyCapScope;
		return Result;
	}
	if (Context.bCooldownActive)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::Cooldown,
			TEXT("The placement is still in its cooldown period."),
			Context.CooldownEndsAt
		);
	}
	if (Context.bOffline)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::Offline,
			TEXT("The platform reports no active network connection.")
		);
	}
	if (Context.bLifecycleConflict)
	{
		return Blocked(
			EOpenMobileAdsCanShowBlockReason::LifecycleConflict,
			TEXT("The application lifecycle does not currently allow another ad to be shown.")
		);
	}

	FOpenMobileAdsCanShowResult Result;
	Result.bCanShow = true;
	Result.BlockReason = EOpenMobileAdsCanShowBlockReason::None;
	return Result;
}
