#pragma once

#include "OpenMobileDeviceUserInitiatedPasteCallback.h"

bool BeginOpenMobileDeviceIOSUserInitiatedPaste(
	const FOpenMobileUserInitiatedPasteRequest& Request,
	const FGuid& OperationId,
	FOpenMobileDeviceUserInitiatedPasteCompletion&& Completion,
	FOpenMobileError& OutError
);
void CancelOpenMobileDeviceIOSUserInitiatedPaste(
	const FGuid& OperationId
);
void ShutdownOpenMobileDeviceIOSUserInitiatedPaste();
