#pragma once

#include "CoreMinimal.h"

enum class ENetworkConnectionType : uint8;

enum class EOpenMobileAdsNetworkWorkOrigin : uint8
{
	CallerInitiated,
	Automatic
};

enum class EOpenMobileAdsNetworkWorkDecision : uint8
{
	Start,
	RejectOffline,
	DeferUntilConnectionChange
};

class FOpenMobileAdsConnectivityPolicy
{
public:
	static bool IsDefinitelyOffline(ENetworkConnectionType ConnectionType);
	static EOpenMobileAdsNetworkWorkDecision Evaluate(
		ENetworkConnectionType ConnectionType,
		EOpenMobileAdsNetworkWorkOrigin Origin
	);
};
