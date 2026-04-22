#include "OpenMobileAdsPrivacy.h"

namespace
{
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
	bool bStatusFresh
)
{
	FOpenMobileAdsConsentStatusUpdate Update;
	Update.Type = EOpenMobileAdsConsentStatusUpdateType::Completed;
	Update.Status = Status;
	Update.Source = Source;
	Update.ProviderDetails = MoveTemp(ProviderDetails);
	Update.bStatusFresh = bStatusFresh;
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
