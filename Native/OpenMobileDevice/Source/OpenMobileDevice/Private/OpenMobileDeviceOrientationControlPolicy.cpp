#include "OpenMobileDeviceOrientationControlPolicy.h"

bool FOpenMobileDeviceOrientationControlPolicy::Validate(
	const FOpenMobileOrientationPolicyRequest& Request,
	FOpenMobileError& OutError
)
{
	OutError = {};
	switch (Request.Policy)
	{
	case EOpenMobileOrientationPolicy::Automatic:
	case EOpenMobileOrientationPolicy::Portrait:
	case EOpenMobileOrientationPolicy::Landscape:
	case EOpenMobileOrientationPolicy::PortraitOnly:
	case EOpenMobileOrientationPolicy::PortraitUpsideDownOnly:
	case EOpenMobileOrientationPolicy::LandscapeLeftOnly:
	case EOpenMobileOrientationPolicy::LandscapeRightOnly:
		return true;
	default:
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The requested orientation policy is unknown.")
		);
		return false;
	}
}

void FOpenMobileDeviceOrientationRequestStack::Add(
	uint64 RequestId,
	const FOpenMobileOrientationPolicyRequest& Request
)
{
	Remove(RequestId);
	FEntry Entry;
	Entry.RequestId = RequestId;
	Entry.Request = Request;
	Requests.Add(MoveTemp(Entry));
}

void FOpenMobileDeviceOrientationRequestStack::Remove(uint64 RequestId)
{
	Requests.RemoveAll([RequestId](const FEntry& Entry)
	{
		return Entry.RequestId == RequestId;
	});
}

TOptional<FOpenMobileOrientationPolicyRequest>
FOpenMobileDeviceOrientationRequestStack::GetEffectiveRequest() const
{
	return Requests.IsEmpty()
		? TOptional<FOpenMobileOrientationPolicyRequest>()
		: TOptional<FOpenMobileOrientationPolicyRequest>(
			Requests.Last().Request
		);
}
