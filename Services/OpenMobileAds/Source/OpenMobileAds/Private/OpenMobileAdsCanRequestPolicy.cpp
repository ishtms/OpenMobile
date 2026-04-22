#include "OpenMobileAdsCanRequestPolicy.h"

namespace
{
	FOpenMobileAdsCanRequestAdsResult Blocked(
		const FOpenMobileAdsCanRequestAdsContext& Context,
		EOpenMobileAdsCanRequestAdsBlockReason Reason,
		EOpenMobileAdsCanRequestAdsBlockType Type,
		FString Explanation
	)
	{
		FOpenMobileAdsCanRequestAdsResult Result;
		Result.BlockReason = Reason;
		Result.BlockType = Type;
		Result.Explanation = MoveTemp(Explanation);
		Result.Provider = Context.Provider;
		return Result;
	}

	FOpenMobileAdsCanRequestAdsResult ProviderBlocked(
		const FOpenMobileAdsCanRequestAdsContext& Context,
		EOpenMobileAdsCanRequestAdsBlockType Type,
		const TCHAR* FallbackExplanation
	)
	{
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ProviderPolicy,
			Type,
			Context.ProviderPolicy.Explanation.IsEmpty()
				? FString(FallbackExplanation)
				: Context.ProviderPolicy.Explanation
		);
	}
}

FOpenMobileAdsCanRequestAdsResult FOpenMobileAdsCanRequestPolicy::Evaluate(
	const FOpenMobileAdsCanRequestAdsContext& Context
)
{
	switch (Context.ServiceState)
	{
	case EOpenMobileAdsServiceState::Uninitialized:
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::InitializationNotStarted,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			TEXT("The ads service has not started initialization.")
		);

	case EOpenMobileAdsServiceState::Initializing:
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::InitializationPending,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			TEXT("The ads service is still initializing.")
		);

	case EOpenMobileAdsServiceState::Failed:
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::InitializationFailed,
			EOpenMobileAdsCanRequestAdsBlockType::Terminal,
			TEXT("The ads service failed to initialize.")
		);

	case EOpenMobileAdsServiceState::ShuttingDown:
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ShuttingDown,
			EOpenMobileAdsCanRequestAdsBlockType::Terminal,
			TEXT("The ads service is shutting down.")
		);

	case EOpenMobileAdsServiceState::Ready:
		break;
	}

	if (!Context.bProviderAvailable)
	{
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ProviderUnavailable,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			TEXT("The initialized ads provider is unavailable.")
		);
	}
	if (Context.ConsentActivity == EOpenMobileAdsConsentActivity::Resetting)
	{
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ConsentResetting,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			TEXT("Consent state is being reset.")
		);
	}
	if (
		Context.ConsentActivity
		== EOpenMobileAdsConsentActivity::PresentingForm
	)
	{
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ConsentFormPresenting,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			TEXT("A consent form is being presented.")
		);
	}
	if (
		Context.ConsentActivity == EOpenMobileAdsConsentActivity::Refreshing
		&& !Context.bConsentStatusFresh
	)
	{
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ConsentRefreshing,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			TEXT("Consent status is being refreshed before requests can continue.")
		);
	}
	if (Context.ConsentStatus == EOpenMobileAdsConsentStatus::Unknown)
	{
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ConsentUnknown,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			TEXT("The current consent status is unknown.")
		);
	}
	if (!Context.bConsentStatusFresh)
	{
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ConsentStale,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			TEXT("The last consent status is no longer fresh.")
		);
	}

	switch (Context.ConsentStatus)
	{
	case EOpenMobileAdsConsentStatus::Unknown:
		break;

	case EOpenMobileAdsConsentStatus::Required:
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ConsentRequired,
			EOpenMobileAdsCanRequestAdsBlockType::UserDecision,
			TEXT("A consent decision is required before requesting ads.")
		);

	case EOpenMobileAdsConsentStatus::Denied:
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ConsentDenied,
			EOpenMobileAdsCanRequestAdsBlockType::UserDecision,
			TEXT("The current consent decision does not allow ad requests.")
		);

	case EOpenMobileAdsConsentStatus::Granted:
	case EOpenMobileAdsConsentStatus::NotRequired:
	case EOpenMobileAdsConsentStatus::Obtained:
		break;
	}
	if (
		Context.ConsentRequestState == EOpenMobileAdsConsentRequestState::Blocked
		|| (
			Context.ConsentStatus == EOpenMobileAdsConsentStatus::Obtained
			&& Context.ConsentRequestState
				!= EOpenMobileAdsConsentRequestState::Allowed
		)
	)
	{
		return Blocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockReason::ConsentProviderBlocked,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			Context.ConsentRequestState == EOpenMobileAdsConsentRequestState::Blocked
				? TEXT("The consent provider currently does not allow ad requests.")
				: TEXT("The consent provider has not confirmed ad-request eligibility.")
		);
	}

	switch (Context.ProviderPolicy.State)
	{
	case EOpenMobileAdsProviderRequestPolicyState::Allowed:
		break;

	case EOpenMobileAdsProviderRequestPolicyState::TemporarilyBlocked:
		return ProviderBlocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockType::Temporary,
			TEXT("The ads provider temporarily does not allow requests.")
		);

	case EOpenMobileAdsProviderRequestPolicyState::UserDecisionRequired:
		return ProviderBlocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockType::UserDecision,
			TEXT("The ads provider requires a user decision before requests.")
		);

	case EOpenMobileAdsProviderRequestPolicyState::Blocked:
		return ProviderBlocked(
			Context,
			EOpenMobileAdsCanRequestAdsBlockType::Configuration,
			TEXT("The ads provider policy does not allow requests.")
		);
	}

	FOpenMobileAdsCanRequestAdsResult Result;
	Result.bCanRequestAds = true;
	Result.BlockReason = EOpenMobileAdsCanRequestAdsBlockReason::None;
	Result.BlockType = EOpenMobileAdsCanRequestAdsBlockType::None;
	Result.Provider = Context.Provider;
	return Result;
}
