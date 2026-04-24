#include "OpenMobileAdsTrackingAuthorizationPlatform.h"

#include "Async/Async.h"
#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsTrackingAuthorizationBackend.h"

namespace OpenMobileAdsTrackingAuthorizationPlatformPrivate
{
	TArray<FOpenMobileAdsTrackingAuthorizationPlatform::FCompletion>
		PendingCompletions;
	bool bRequestInFlight = false;

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

	void CompleteRequest(EOpenMobileAdsTrackingAuthorizationStatus Status)
	{
		check(IsInGameThread());
		Status = NormalizeStatus(Status);
		TArray<FOpenMobileAdsTrackingAuthorizationPlatform::FCompletion>
			Completions = MoveTemp(PendingCompletions);
		PendingCompletions.Reset();
		bRequestInFlight = false;
		for (FOpenMobileAdsTrackingAuthorizationPlatform::FCompletion& Completion
			: Completions)
		{
			Completion(Status);
		}
	}
}

bool FOpenMobileAdsTrackingAuthorizationPlatform::IsAvailable()
{
	return OpenMobileAdsTrackingAuthorizationPlatformPrivate::FindBackend()
		!= nullptr;
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

bool FOpenMobileAdsTrackingAuthorizationPlatform::RequestAuthorization(
	FCompletion&& Completion,
	FString& OutError
)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsTrackingAuthorizationPlatformPrivate;
	IOpenMobileAdsTrackingAuthorizationBackend* Backend = FindBackend();
	if (!Backend)
	{
		OutError = TEXT("No tracking authorization backend is available.");
		return false;
	}
	PendingCompletions.Add(MoveTemp(Completion));
	if (bRequestInFlight)
	{
		return true;
	}
	bRequestInFlight = true;
	const bool bStarted = Backend->RequestAuthorization(
		[](EOpenMobileAdsTrackingAuthorizationStatus Status)
		{
			auto CompleteOnGameThread = [Status]()
			{
				OpenMobileAdsTrackingAuthorizationPlatformPrivate::
					CompleteRequest(Status);
			};
			if (IsInGameThread())
			{
				CompleteOnGameThread();
			}
			else
			{
				AsyncTask(
					ENamedThreads::GameThread,
					MoveTemp(CompleteOnGameThread)
				);
			}
		},
		OutError
	);
	if (!bStarted)
	{
		PendingCompletions.Reset();
		bRequestInFlight = false;
	}
	return bStarted;
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
