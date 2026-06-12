#pragma once

#include "OpenMobileDeviceClipboardTypes.h"

class FOpenMobileDeviceClipboardPolicy final
{
public:
	static constexpr int32 MaximumPayloadBytes = 256 * 1024;

	static bool ValidateWrite(
		const FOpenMobileClipboardWriteRequest& Request,
		FOpenMobileError& OutError
	);
	static bool ValidateReadType(
		EOpenMobileClipboardContentType ContentType,
		FOpenMobileError& OutError
	);
	static int32 GetPayloadSizeBytes(const FString& Value);
	static bool IsAbsoluteUrl(const FString& Value);
};
