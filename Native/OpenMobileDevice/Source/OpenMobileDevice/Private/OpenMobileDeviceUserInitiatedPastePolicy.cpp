#include "OpenMobileDeviceUserInitiatedPastePolicy.h"

#include "OpenMobileDeviceClipboardPolicy.h"

bool FOpenMobileDeviceUserInitiatedPastePolicy::Validate(
	const FOpenMobileUserInitiatedPasteRequest& Request,
	FOpenMobileError& OutError
)
{
	OutError = {};
	if (!Request.bCallerConfirmsUserInitiated)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("User-initiated paste requires explicit caller confirmation of a current user action.")
		);
		return false;
	}
	return FOpenMobileDeviceClipboardPolicy::ValidateReadType(
		Request.ContentType,
		OutError
	);
}
