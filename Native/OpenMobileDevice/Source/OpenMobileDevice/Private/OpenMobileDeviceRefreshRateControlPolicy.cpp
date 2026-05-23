#include "OpenMobileDeviceRefreshRateControlPolicy.h"

namespace OpenMobileDeviceRefreshRateControlPolicyPrivate
{
	bool IsValidRate(float Rate)
	{
		return FMath::IsFinite(Rate) && Rate >= 1.0f && Rate <= 1000.0f;
	}
}

bool FOpenMobileDeviceRefreshRateControlPolicy::Validate(
	const FOpenMobilePreferredRefreshRateRequest& Request,
	FOpenMobileError& OutError
)
{
	using namespace OpenMobileDeviceRefreshRateControlPolicyPrivate;
	OutError = {};
	if (!Request.bUsePreferredMinimumHz
		&& !Request.bUsePreferredMaximumHz
		&& !Request.bUsePreferredTargetHz)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("A preferred refresh-rate request must set at least one value.")
		);
		return false;
	}
	if ((Request.bUsePreferredMinimumHz
			&& !IsValidRate(Request.PreferredMinimumHz))
		|| (Request.bUsePreferredMaximumHz
			&& !IsValidRate(Request.PreferredMaximumHz))
		|| (Request.bUsePreferredTargetHz
			&& !IsValidRate(Request.PreferredTargetHz)))
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Preferred refresh rates must be finite values from 1 through 1000 Hz.")
		);
		return false;
	}
	if (Request.bUsePreferredMinimumHz
		&& Request.bUsePreferredMaximumHz
		&& Request.PreferredMinimumHz > Request.PreferredMaximumHz)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The preferred minimum refresh rate exceeds the maximum.")
		);
		return false;
	}
	if (Request.bUsePreferredTargetHz
		&& ((Request.bUsePreferredMinimumHz
				&& Request.PreferredTargetHz < Request.PreferredMinimumHz)
			|| (Request.bUsePreferredMaximumHz
				&& Request.PreferredTargetHz > Request.PreferredMaximumHz)))
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The preferred target refresh rate is outside the requested range.")
		);
		return false;
	}
	return true;
}

void FOpenMobileDeviceRefreshRateRequestStack::Add(
	uint64 RequestId,
	const FOpenMobilePreferredRefreshRateRequest& Request
)
{
	Remove(RequestId);
	FEntry Entry;
	Entry.RequestId = RequestId;
	Entry.Request = Request;
	Requests.Add(MoveTemp(Entry));
}

void FOpenMobileDeviceRefreshRateRequestStack::Remove(uint64 RequestId)
{
	Requests.RemoveAll([RequestId](const FEntry& Entry)
	{
		return Entry.RequestId == RequestId;
	});
}

TOptional<FOpenMobilePreferredRefreshRateRequest>
FOpenMobileDeviceRefreshRateRequestStack::GetEffectiveRequest() const
{
	return Requests.IsEmpty()
		? TOptional<FOpenMobilePreferredRefreshRateRequest>()
		: TOptional<FOpenMobilePreferredRefreshRateRequest>(
			Requests.Last().Request
		);
}
