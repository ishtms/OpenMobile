#include "OpenMobileAdsPrivacy.h"

namespace
{
	EOpenMobileAdsConsentRequirement RequirementFromStatus(
		EOpenMobileAdsConsentStatus Status
	)
	{
		switch (Status)
		{
		case EOpenMobileAdsConsentStatus::Required:
		case EOpenMobileAdsConsentStatus::Granted:
		case EOpenMobileAdsConsentStatus::Denied:
		case EOpenMobileAdsConsentStatus::Obtained:
			return EOpenMobileAdsConsentRequirement::Required;

		case EOpenMobileAdsConsentStatus::NotRequired:
			return EOpenMobileAdsConsentRequirement::NotRequired;

		case EOpenMobileAdsConsentStatus::Unknown:
		default:
			return EOpenMobileAdsConsentRequirement::Unknown;
		}
	}

	EOpenMobileAdsConsentRequestState RequestStateFromStatus(
		EOpenMobileAdsConsentStatus Status
	)
	{
		switch (Status)
		{
		case EOpenMobileAdsConsentStatus::Granted:
		case EOpenMobileAdsConsentStatus::NotRequired:
			return EOpenMobileAdsConsentRequestState::Allowed;

		case EOpenMobileAdsConsentStatus::Required:
		case EOpenMobileAdsConsentStatus::Denied:
			return EOpenMobileAdsConsentRequestState::Blocked;

		case EOpenMobileAdsConsentStatus::Unknown:
		case EOpenMobileAdsConsentStatus::Obtained:
		default:
			return EOpenMobileAdsConsentRequestState::Unknown;
		}
	}

	FOpenMobileAdsConsentStatusUpdate Begin(
		EOpenMobileAdsConsentStatusUpdateType Type,
		FName Source
	)
	{
		FOpenMobileAdsConsentStatusUpdate Update;
		Update.Type = Type;
		Update.Source = Source;
		return Update;
	}
}

FOpenMobileAdsConsentStatusUpdate
FOpenMobileAdsConsentStatusUpdate::BeginRefresh(FName Source)
{
	return Begin(EOpenMobileAdsConsentStatusUpdateType::RefreshStarted, Source);
}

FOpenMobileAdsConsentStatusUpdate
FOpenMobileAdsConsentStatusUpdate::BeginFormPresentation(FName Source)
{
	return Begin(
		EOpenMobileAdsConsentStatusUpdateType::FormPresentationStarted,
		Source
	);
}

FOpenMobileAdsConsentStatusUpdate
FOpenMobileAdsConsentStatusUpdate::BeginReset(FName Source)
{
	return Begin(EOpenMobileAdsConsentStatusUpdateType::ResetStarted, Source);
}

FOpenMobileAdsConsentStatusUpdate FOpenMobileAdsConsentStatusUpdate::Complete(
	EOpenMobileAdsConsentStatus Status,
	FName Source,
	FOpenMobileAdsConsentProviderDetails ProviderDetails,
	bool bStatusFresh,
	FDateTime ExpiresAt,
	bool bRestoredFromProviderStorage
)
{
	return CompleteProviderState(
		Status,
		EOpenMobileAdsGdprApplicability::Unknown,
		RequirementFromStatus(Status),
		RequestStateFromStatus(Status),
		Source,
		MoveTemp(ProviderDetails),
		bStatusFresh,
		ExpiresAt,
		bRestoredFromProviderStorage
	);
}

FOpenMobileAdsConsentStatusUpdate
FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
	EOpenMobileAdsConsentStatus Status,
	EOpenMobileAdsGdprApplicability GdprApplicability,
	EOpenMobileAdsConsentRequirement Requirement,
	EOpenMobileAdsConsentRequestState RequestState,
	FName Source,
	FOpenMobileAdsConsentProviderDetails ProviderDetails,
	bool bStatusFresh,
	FDateTime ExpiresAt,
	bool bRestoredFromProviderStorage
)
{
	FOpenMobileAdsConsentStatusUpdate Update;
	Update.Type = EOpenMobileAdsConsentStatusUpdateType::Completed;
	Update.Status = Status;
	Update.GdprApplicability = GdprApplicability;
	Update.Requirement = Requirement;
	Update.RequestState = RequestState;
	Update.Source = Source;
	Update.ProviderDetails = MoveTemp(ProviderDetails);
	Update.bStatusFresh = bStatusFresh && !bRestoredFromProviderStorage;
	Update.ExpiresAt = ExpiresAt;
	Update.bRestoredFromProviderStorage = bRestoredFromProviderStorage;
	return Update;
}

FOpenMobileAdsConsentStatusUpdate FOpenMobileAdsConsentStatusUpdate::Fail(
	FName Source,
	FOpenMobileAdsError Error
)
{
	FOpenMobileAdsConsentStatusUpdate Update;
	Update.Type = EOpenMobileAdsConsentStatusUpdateType::Failed;
	Update.Source = Source;
	Update.Error = MoveTemp(Error);
	return Update;
}
