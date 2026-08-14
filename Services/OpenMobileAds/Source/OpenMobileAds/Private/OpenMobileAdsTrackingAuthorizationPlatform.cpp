#include "OpenMobileAdsTrackingAuthorizationPlatform.h"

#include "Async/Async.h"
#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsTrackingAuthorizationBackend.h"

namespace OpenMobileAdsTrackingAuthorizationPlatformPrivate
{
	TArray<FOpenMobileAdsTrackingAuthorizationPlatform::FCompletion>
		PendingCompletions;
	bool bRequestInFlight = false;

	/** Converts unexpected backend statuses to Unsupported before they reach service policy. */
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

	/** Selects the highest-priority available backend and breaks ties by name. */
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

	/** Ends the single in-flight request and runs its completions after state is cleared. */
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

bool FOpenMobileAdsTrackingAuthorizationPlatform::
IsAdvertisingIdentifierAvailable()
{
	using namespace OpenMobileAdsTrackingAuthorizationPlatformPrivate;
	const IOpenMobileAdsTrackingAuthorizationBackend* Backend = FindBackend();
	if (
		!Backend
		|| NormalizeStatus(Backend->GetStatus())
			!= EOpenMobileAdsTrackingAuthorizationStatus::Authorized
	)
	{
		return false;
	}
	return Backend->HasNonZeroAdvertisingIdentifier();
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

bool OpenMobileAdsHasNonZeroAppleAdvertisingIdentifier(
	const uint8* IdentifierBytes,
	const int32 ByteCount
)
{
	if (!IdentifierBytes || ByteCount != 16)
	{
		return false;
	}
	uint8 CombinedValue = 0;
	for (int32 Index = 0; Index < ByteCount; ++Index)
	{
		CombinedValue |= IdentifierBytes[Index];
	}
	return CombinedValue != 0;
}
