#pragma once

#include "OpenMobileDeviceIntentHandlerTypes.h"

class FOpenMobileDeviceIntentHandlerPolicy final
{
public:
	static constexpr int32 MaximumUrlBytes = 4 * 1024;
	static constexpr int32 MaximumDeclaredUrlSchemes = 50;
	static constexpr int32 MaximumDeclaredIntentActions = 50;
	static constexpr int32 MaximumIntentActionCharacters = 255;

	static bool Validate(
		const FOpenMobileIntentHandlerCheckRequest& Request,
		const TArray<FString>& DeclaredUrlSchemes,
		const TArray<FString>& DeclaredIntentActions,
		FName& OutScheme,
		FOpenMobileIntentHandlerCheckResult& OutFailure
	);
};
