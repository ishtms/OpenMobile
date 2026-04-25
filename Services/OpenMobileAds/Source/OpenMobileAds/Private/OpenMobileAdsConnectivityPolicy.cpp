#include "OpenMobileAdsConnectivityPolicy.h"

#include "HAL/PlatformMisc.h"

bool FOpenMobileAdsConnectivityPolicy::IsDefinitelyOffline(
	const ENetworkConnectionType ConnectionType
)
{
	return ConnectionType == ENetworkConnectionType::None
		|| ConnectionType == ENetworkConnectionType::AirplaneMode;
}

EOpenMobileAdsNetworkWorkDecision FOpenMobileAdsConnectivityPolicy::Evaluate(
	const ENetworkConnectionType ConnectionType,
	const EOpenMobileAdsNetworkWorkOrigin Origin
)
{
	if (!IsDefinitelyOffline(ConnectionType))
	{
		return EOpenMobileAdsNetworkWorkDecision::Start;
	}
	return Origin == EOpenMobileAdsNetworkWorkOrigin::Automatic
		? EOpenMobileAdsNetworkWorkDecision::DeferUntilConnectionChange
		: EOpenMobileAdsNetworkWorkDecision::RejectOffline;
}
