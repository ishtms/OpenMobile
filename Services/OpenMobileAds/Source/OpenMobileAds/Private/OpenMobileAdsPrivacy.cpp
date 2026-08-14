#include "OpenMobileAdsPrivacy.h"

namespace
{
	/** Derives whether a provider decision still requires a consent form. */
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

	/** Converts a consent result to the request state used by service-wide policy. */
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

	/** Starts a consent activity update without replacing the last known provider decision. */
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

int32 FOpenMobileAdsConsentSignals::GetConfiguredSignalMask() const
{
	int32 Result = 0;
	if (
		bConsentStatusFresh
		&& (
			ConsentStatus != EOpenMobileAdsConsentStatus::Unknown
			|| GdprApplicability != EOpenMobileAdsGdprApplicability::Unknown
			|| ConsentRequirement != EOpenMobileAdsConsentRequirement::Unknown
			|| ConsentRequestState != EOpenMobileAdsConsentRequestState::Unknown
		)
	)
	{
		Result |= static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	}
	if (
		bConsentStatusFresh
		&& (
			UsPrivacy.Applicability
				!= EOpenMobileAdsUsPrivacyApplicability::Unknown
			|| UsPrivacy.Choice != EOpenMobileAdsUsPrivacyChoice::Unknown
			|| UsPrivacy.DataProcessingMode
				!= EOpenMobileAdsDataProcessingMode::Unspecified
		)
	)
	{
		Result |= static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	}
	if (ChildDirectedTreatment != EOpenMobileAdsAgeTreatment::Unspecified)
	{
		Result |= static_cast<int32>(EOpenMobileAdsConsentSignal::ChildDirected);
	}
	if (UnderAgeOfConsent != EOpenMobileAdsAgeTreatment::Unspecified)
	{
		Result |= static_cast<int32>(
			EOpenMobileAdsConsentSignal::UnderAgeOfConsent
		);
	}
	return Result;
}

int32 FOpenMobileAdsConsentSignals::GetRequiredSignalMask() const
{
	const int32 Configured = GetConfiguredSignalMask();
	int32 Result = Configured & (
		static_cast<int32>(EOpenMobileAdsConsentSignal::ChildDirected)
		| static_cast<int32>(EOpenMobileAdsConsentSignal::UnderAgeOfConsent)
	);
	if (
		GdprApplicability == EOpenMobileAdsGdprApplicability::Applicable
		|| ConsentRequirement == EOpenMobileAdsConsentRequirement::Required
		|| ConsentStatus == EOpenMobileAdsConsentStatus::Required
		|| ConsentStatus == EOpenMobileAdsConsentStatus::Granted
		|| ConsentStatus == EOpenMobileAdsConsentStatus::Denied
		|| ConsentStatus == EOpenMobileAdsConsentStatus::Obtained
	)
	{
		Result |= Configured
			& static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	}
	if (
		UsPrivacy.Applicability
			== EOpenMobileAdsUsPrivacyApplicability::Applicable
		|| UsPrivacy.Choice != EOpenMobileAdsUsPrivacyChoice::Unknown
		|| UsPrivacy.DataProcessingMode
			== EOpenMobileAdsDataProcessingMode::Restricted
	)
	{
		Result |= Configured
			& static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	}
	return Result;
}

int32 FOpenMobileAdsConsentSignals::GetChangedSignalMask(
	const FOpenMobileAdsConsentSignals& Other
) const
{
	int32 Result = 0;
	if (
		ConsentStatus != Other.ConsentStatus
		|| GdprApplicability != Other.GdprApplicability
		|| ConsentRequirement != Other.ConsentRequirement
		|| ConsentRequestState != Other.ConsentRequestState
		|| bConsentStatusFresh != Other.bConsentStatusFresh
		|| Source != Other.Source
	)
	{
		Result |= static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr);
	}
	if (
		UsPrivacy.Applicability != Other.UsPrivacy.Applicability
		|| UsPrivacy.Choice != Other.UsPrivacy.Choice
		|| UsPrivacy.DataProcessingMode != Other.UsPrivacy.DataProcessingMode
		|| bConsentStatusFresh != Other.bConsentStatusFresh
		|| Source != Other.Source
	)
	{
		Result |= static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy);
	}
	if (ChildDirectedTreatment != Other.ChildDirectedTreatment)
	{
		Result |= static_cast<int32>(EOpenMobileAdsConsentSignal::ChildDirected);
	}
	if (UnderAgeOfConsent != Other.UnderAgeOfConsent)
	{
		Result |= static_cast<int32>(
			EOpenMobileAdsConsentSignal::UnderAgeOfConsent
		);
	}
	return Result;
}

bool FOpenMobileAdsConsentSignals::operator==(
	const FOpenMobileAdsConsentSignals& Other
) const
{
	return GetChangedSignalMask(Other) == 0;
}

const FOpenMobileAdsConsentSignalDeliveryStatus*
FOpenMobileAdsConsentSignalDeliverySnapshot::Find(
	EOpenMobileAdsConsentSignalConsumerType Type,
	FName Name,
	FName Parent
) const
{
	return Consumers.FindByPredicate(
		[Type, Name, Parent](
			const FOpenMobileAdsConsentSignalDeliveryStatus& Candidate
		)
		{
			return Candidate.Type == Type
				&& Candidate.Name == Name
				&& Candidate.Parent == Parent;
		}
	);
}

FOpenMobileAdsConsentSignalApplyResult
FOpenMobileAdsConsentSignalApplyResult::Applied(
	int32 AppliedSignals,
	int32 ConfirmedSignals
)
{
	FOpenMobileAdsConsentSignalApplyResult Result;
	Result.AppliedSignals = AppliedSignals;
	Result.ConfirmedSignals = ConfirmedSignals;
	return Result;
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
