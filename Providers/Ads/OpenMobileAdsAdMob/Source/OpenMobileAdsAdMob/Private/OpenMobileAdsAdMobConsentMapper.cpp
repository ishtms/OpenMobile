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
	FName Source,
	EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement PrivacyOptions
)
{
	const FNormalizedStatus Normalized = Normalize(Status);
	FOpenMobileAdsConsentProviderDetails Details;
	Details.bIsAvailable = true;
	Details.RawStatus = Normalized.RawStatus;
	FOpenMobileAdsConsentStatusUpdate Update =
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
		Normalized.Status,
		Normalized.Applicability,
		Normalized.Requirement,
		bCanRequestAds
			? EOpenMobileAdsConsentRequestState::Allowed
			: EOpenMobileAdsConsentRequestState::Blocked,
		Source,
		MoveTemp(Details)
	);
	Update.UsPrivacy = MapUsPrivacyState(PrivacyOptions);
	return Update;
}

FOpenMobileAdsUsPrivacyState
FOpenMobileAdsAdMobConsentMapper::MapUsPrivacyState(
	EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement Requirement
)
{
	FOpenMobileAdsUsPrivacyState State;
	State.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::ProviderManaged;
	switch (Requirement)
	{
	case EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::NotRequired:
		State.PrivacyOptionsRequirement =
			EOpenMobileAdsPrivacyOptionsRequirement::NotRequired;
		break;

	case EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::Required:
		State.PrivacyOptionsRequirement =
			EOpenMobileAdsPrivacyOptionsRequirement::Required;
		break;

	case EOpenMobileAdsAdMobUMPPrivacyOptionsRequirement::Unknown:
	default:
		break;
	}
	return State;
}
