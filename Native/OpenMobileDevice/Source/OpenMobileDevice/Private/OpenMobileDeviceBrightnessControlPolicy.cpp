#include "OpenMobileDeviceBrightnessControlPolicy.h"

bool FOpenMobileDeviceBrightnessControlPolicy::Validate(
	const FOpenMobileBrightnessRequest& Request,
	FOpenMobileError& OutError
)
{
	OutError = {};
	if (FMath::IsFinite(Request.Brightness)
		&& Request.Brightness >= 0.0f
		&& Request.Brightness <= 1.0f)
	{
		return true;
	}
	OutError = FOpenMobileError::Make(
		EOpenMobileErrorCode::InvalidArgument,
		TEXT("Brightness must be a finite normalized value from 0 through 1.")
	);
	return false;
}

void FOpenMobileDeviceBrightnessRequestStack::Add(
	uint64 RequestId,
	const FOpenMobileBrightnessRequest& Request
)
{
	Remove(RequestId);
	FEntry Entry;
	Entry.RequestId = RequestId;
	Entry.Request = Request;
	Requests.Add(MoveTemp(Entry));
}

void FOpenMobileDeviceBrightnessRequestStack::Remove(uint64 RequestId)
{
	Requests.RemoveAll([RequestId](const FEntry& Entry)
	{
		return Entry.RequestId == RequestId;
	});
}

TOptional<FOpenMobileBrightnessRequest>
FOpenMobileDeviceBrightnessRequestStack::GetEffectiveRequest() const
{
	return Requests.IsEmpty()
		? TOptional<FOpenMobileBrightnessRequest>()
		: TOptional<FOpenMobileBrightnessRequest>(Requests.Last().Request);
}
