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
	/** Treats only Unreal's explicit no-connection value as definitely offline. */
	static bool IsDefinitelyOffline(ENetworkConnectionType ConnectionType);
	/** Rejects caller work offline while automatic work waits for a connection change. */
	static EOpenMobileAdsNetworkWorkDecision Evaluate(
		ENetworkConnectionType ConnectionType,
		EOpenMobileAdsNetworkWorkOrigin Origin
	);
};
