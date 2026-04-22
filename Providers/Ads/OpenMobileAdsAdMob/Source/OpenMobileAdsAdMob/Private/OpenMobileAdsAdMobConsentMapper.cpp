#include "OpenMobileAdsAdMobConsentMapper.h"

namespace
{
	struct FNormalizedStatus
	{
		EOpenMobileAdsConsentStatus Status =
			EOpenMobileAdsConsentStatus::Unknown;
		EOpenMobileAdsGdprApplicability Applicability =
			EOpenMobileAdsGdprApplicability::Unknown;
		EOpenMobileAdsConsentRequirement Requirement =
			EOpenMobileAdsConsentRequirement::Unknown;
		const TCHAR* RawStatus = TEXT("UNKNOWN");
	};

	FNormalizedStatus Normalize(
		EOpenMobileAdsAdMobUMPConsentStatus Status
	)
	{
		switch (Status)
		{
		case EOpenMobileAdsAdMobUMPConsentStatus::NotRequired:
			return {
				EOpenMobileAdsConsentStatus::NotRequired,
				EOpenMobileAdsGdprApplicability::NotApplicable,
				EOpenMobileAdsConsentRequirement::NotRequired,
				TEXT("NOT_REQUIRED")
			};

		case EOpenMobileAdsAdMobUMPConsentStatus::Required:
			return {
				EOpenMobileAdsConsentStatus::Required,
				EOpenMobileAdsGdprApplicability::Applicable,
				EOpenMobileAdsConsentRequirement::Required,
				TEXT("REQUIRED")
			};

		case EOpenMobileAdsAdMobUMPConsentStatus::Obtained:
			return {
				EOpenMobileAdsConsentStatus::Obtained,
				EOpenMobileAdsGdprApplicability::Applicable,
				EOpenMobileAdsConsentRequirement::Required,
				TEXT("OBTAINED")
			};

		case EOpenMobileAdsAdMobUMPConsentStatus::Unknown:
		default:
			return {};
		}
	}
}

FOpenMobileAdsConsentStatusUpdate
FOpenMobileAdsAdMobConsentMapper::MapGdprState(
	EOpenMobileAdsAdMobUMPConsentStatus Status,
	bool bCanRequestAds,
	FName Source
)
{
	const FNormalizedStatus Normalized = Normalize(Status);
	FOpenMobileAdsConsentProviderDetails Details;
	Details.bIsAvailable = true;
	Details.RawStatus = Normalized.RawStatus;
	return FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
		Normalized.Status,
		Normalized.Applicability,
		Normalized.Requirement,
		bCanRequestAds
			? EOpenMobileAdsConsentRequestState::Allowed
			: EOpenMobileAdsConsentRequestState::Blocked,
		Source,
		MoveTemp(Details)
	);
}
