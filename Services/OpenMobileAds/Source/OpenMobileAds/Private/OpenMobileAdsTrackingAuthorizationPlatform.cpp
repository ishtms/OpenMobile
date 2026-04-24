#include "OpenMobileAdsTrackingAuthorizationPlatform.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsTrackingAuthorizationBackend.h"

namespace OpenMobileAdsTrackingAuthorizationPlatformPrivate
{
	EOpenMobileAdsTrackingAuthorizationStatus NormalizeStatus(
		EOpenMobileAdsTrackingAuthorizationStatus Status
	)
	{
		switch (Status)
		{
		case EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined:
		case EOpenMobileAdsTrackingAuthorizationStatus::Restricted:
		case EOpenMobileAdsTrackingAuthorizationStatus::Denied:
		case EOpenMobileAdsTrackingAuthorizationStatus::Authorized:
		case EOpenMobileAdsTrackingAuthorizationStatus::Unsupported:
			return Status;
		default:
			return EOpenMobileAdsTrackingAuthorizationStatus::Unsupported;
		}
	}

	IOpenMobileAdsTrackingAuthorizationBackend* FindBackend()
	{
		const TArray<IOpenMobileAdsTrackingAuthorizationBackend*> Backends =
			IModularFeatures::Get().GetModularFeatureImplementations<
				IOpenMobileAdsTrackingAuthorizationBackend
			>(IOpenMobileAdsTrackingAuthorizationBackend::GetModularFeatureName());
		IOpenMobileAdsTrackingAuthorizationBackend* Best = nullptr;
		for (IOpenMobileAdsTrackingAuthorizationBackend* Candidate : Backends)
		{
			if (!Candidate || !Candidate->IsAvailable())
			{
				continue;
			}
			const bool bHigherPriority =
				!Best || Candidate->GetPriority() > Best->GetPriority();
			const bool bStableTieBreak = Best
				&& Candidate->GetPriority() == Best->GetPriority()
				&& Candidate->GetBackendName().LexicalLess(
					Best->GetBackendName()
				);
			if (bHigherPriority || bStableTieBreak)
			{
				Best = Candidate;
			}
		}
		return Best;
	}
}

EOpenMobileAdsTrackingAuthorizationStatus
FOpenMobileAdsTrackingAuthorizationPlatform::GetStatus()
{
	using namespace OpenMobileAdsTrackingAuthorizationPlatformPrivate;
	const IOpenMobileAdsTrackingAuthorizationBackend* Backend = FindBackend();
	return Backend
		? NormalizeStatus(Backend->GetStatus())
		: EOpenMobileAdsTrackingAuthorizationStatus::Unsupported;
}

EOpenMobileAdsTrackingAuthorizationStatus
OpenMobileAdsMapAppleTrackingAuthorizationStatus(const int64 RawStatus)
{
	switch (RawStatus)
	{
	case 0:
		return EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined;
	case 1:
		return EOpenMobileAdsTrackingAuthorizationStatus::Restricted;
	case 2:
		return EOpenMobileAdsTrackingAuthorizationStatus::Denied;
	case 3:
		return EOpenMobileAdsTrackingAuthorizationStatus::Authorized;
	default:
		return EOpenMobileAdsTrackingAuthorizationStatus::Unsupported;
	}
}
