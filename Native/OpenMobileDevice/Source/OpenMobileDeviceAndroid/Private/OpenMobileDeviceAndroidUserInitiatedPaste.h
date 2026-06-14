#pragma once

#include "OpenMobileDeviceUserInitiatedPasteCallback.h"

bool BeginOpenMobileDeviceAndroidUserInitiatedPaste(
	const FOpenMobileUserInitiatedPasteRequest& Request,
	const FGuid& OperationId,
	FOpenMobileDeviceUserInitiatedPasteCompletion&& Completion,
	FOpenMobileError& OutError
);
void CancelOpenMobileDeviceAndroidUserInitiatedPaste(
	const FGuid& OperationId
);
