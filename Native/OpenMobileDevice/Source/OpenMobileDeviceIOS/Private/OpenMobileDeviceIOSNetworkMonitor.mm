#include "OpenMobileDeviceIOSNetworkMonitor.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMonitoringService.h"

namespace OpenMobileDeviceIOSNetworkMonitorPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;
	FDelegateHandle ConnectionHandle;
	FDelegateHandle CharacteristicsHandle;

	void NotifyChange()
	{
		FOpenMobileDeviceMonitoringCallbackToken CallbackToken;
		uint64 Sequence = 0;
		{
			FScopeLock Lock(&StateMutex);
			if (!ActiveToken.IsValid())
			{
				return;
			}
			SourceSequence = SourceSequence == MAX_uint64
				? 1
				: SourceSequence + 1;
			CallbackToken = ActiveToken;
			Sequence = SourceSequence;
		}
		FOpenMobileDeviceMonitoringService::NotifyNativeChange(
			CallbackToken,
			Sequence
		);
	}

	void HandleConnectionChange(ENetworkConnectionType ConnectionType)
	{
		static_cast<void>(ConnectionType);
		NotifyChange();
	}

	void HandleCharacteristicsChange(
		FPlatformMisc::FNetworkConnectionCharacteristics Characteristics
	)
	{
		static_cast<void>(Characteristics);
		NotifyChange();
	}
}

bool StartOpenMobileDeviceIOSNetworkMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSNetworkMonitorPrivate;
	if (!CallbackToken.IsValid()
		|| ConnectionHandle.IsValid()
		|| CharacteristicsHandle.IsValid())
	{
		return false;
	}
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = CallbackToken;
		SourceSequence = 0;
	}
	ConnectionHandle = FCoreDelegates::OnNetworkConnectionChanged.AddStatic(
		&HandleConnectionChange
	);
	CharacteristicsHandle =
		FPlatformMisc::OnNetworkConnectionCharacteristicsChanged().AddStatic(
			&HandleCharacteristicsChange
		);
	return ConnectionHandle.IsValid() && CharacteristicsHandle.IsValid();
}

void StopOpenMobileDeviceIOSNetworkMonitoring()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSNetworkMonitorPrivate;
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = {};
		SourceSequence = 0;
	}
	if (ConnectionHandle.IsValid())
	{
		FCoreDelegates::OnNetworkConnectionChanged.Remove(ConnectionHandle);
		ConnectionHandle.Reset();
	}
	if (CharacteristicsHandle.IsValid())
	{
		FPlatformMisc::OnNetworkConnectionCharacteristicsChanged().Remove(
			CharacteristicsHandle
		);
		CharacteristicsHandle.Reset();
	}
}
