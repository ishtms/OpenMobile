#include "OpenMobileDeviceSystemUiControlPolicy.h"

bool FOpenMobileDeviceSystemUiControlPolicy::Validate(
	const FOpenMobileSystemUiRequest& Request,
	FOpenMobileError& OutError
)
{
	OutError = {};
	switch (Request.Mode)
	{
	case EOpenMobileSystemUiMode::Normal:
	case EOpenMobileSystemUiMode::EdgeToEdge:
	case EOpenMobileSystemUiMode::Immersive:
		return true;
	}
	OutError = FOpenMobileError::Make(
		EOpenMobileErrorCode::InvalidArgument,
		TEXT("The requested system UI mode is invalid.")
	);
	return false;
}

void FOpenMobileDeviceSystemUiRequestStack::Add(
	uint64 RequestId,
	const FOpenMobileSystemUiRequest& Request
)
{
	Remove(RequestId);
	FEntry Entry;
	Entry.RequestId = RequestId;
	Entry.Request = Request;
	Requests.Add(MoveTemp(Entry));
}

void FOpenMobileDeviceSystemUiRequestStack::Remove(uint64 RequestId)
{
	Requests.RemoveAll([RequestId](const FEntry& Entry)
	{
		return Entry.RequestId == RequestId;
	});
}

TOptional<FOpenMobileSystemUiRequest>
FOpenMobileDeviceSystemUiRequestStack::GetEffectiveRequest() const
{
	return Requests.IsEmpty()
		? TOptional<FOpenMobileSystemUiRequest>()
		: TOptional<FOpenMobileSystemUiRequest>(Requests.Last().Request);
}
