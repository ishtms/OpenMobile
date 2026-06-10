#include "OpenMobileDeviceFlashlightControlPolicy.h"

bool FOpenMobileDeviceFlashlightControlPolicy::Validate(
	const FOpenMobileFlashlightRequest& Request,
	FOpenMobileError& OutError
)
{
	OutError = {};
	switch (Request.Operation)
	{
	case EOpenMobileFlashlightOperation::Off:
	case EOpenMobileFlashlightOperation::On:
		return true;
	case EOpenMobileFlashlightOperation::SetIntensity:
		if (FMath::IsFinite(Request.Intensity))
		{
			return true;
		}
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Flashlight intensity must be finite.")
		);
		return false;
	}
	OutError = FOpenMobileError::Make(
		EOpenMobileErrorCode::InvalidArgument,
		TEXT("The flashlight operation is invalid.")
	);
	return false;
}
