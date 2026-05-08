#include "OpenMobileDeviceSubsystem.h"

#include "OpenMobileDeviceAsyncActionBase.h"
#include "OpenMobileDeviceBlueprintLibrary.h"
#include "OpenMobileDeviceSnapshotService.h"

void UOpenMobileDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bDeinitialized = false;

	LatestStatus = UOpenMobileDeviceBlueprintLibrary::GetDeviceStatus();
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UOpenMobileDeviceSubsystem::TickStatus),
		0.5f
	);
}

void UOpenMobileDeviceSubsystem::Deinitialize()
{
	bDeinitialized = true;
	TArray<TWeakObjectPtr<UOpenMobileDeviceAsyncActionBase>> Actions;
	Actions.Reserve(ActiveAsyncActions.Num());
	for (const TWeakObjectPtr<UOpenMobileDeviceAsyncActionBase>& Action : ActiveAsyncActions)
	{
		Actions.Add(Action);
	}
	ActiveAsyncActions.Reset();
	for (const TWeakObjectPtr<UOpenMobileDeviceAsyncActionBase>& Action : Actions)
	{
		if (Action.IsValid())
		{
			Action->HandleGameInstanceTeardown();
		}
	}

	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	Super::Deinitialize();
}

FOpenMobileDeviceInformationSnapshot
UOpenMobileDeviceSubsystem::GetDeviceInformationSnapshot() const
{
	return bDeinitialized
		? FOpenMobileDeviceInformationSnapshot()
		: FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
}

FOpenMobileApplicationMetadataSnapshot
UOpenMobileDeviceSubsystem::GetApplicationMetadataSnapshot() const
{
	return bDeinitialized
		? FOpenMobileApplicationMetadataSnapshot()
		: FOpenMobileDeviceSnapshotService::GetApplicationMetadataSnapshot();
}

FOpenMobileLocaleSnapshot UOpenMobileDeviceSubsystem::GetLocaleSnapshot() const
{
	return bDeinitialized
		? FOpenMobileLocaleSnapshot()
		: FOpenMobileDeviceSnapshotService::GetLocaleSnapshot();
}

FOpenMobilePowerSnapshot UOpenMobileDeviceSubsystem::GetPowerSnapshot() const
{
	return bDeinitialized
		? FOpenMobilePowerSnapshot()
		: FOpenMobileDeviceSnapshotService::GetPowerSnapshot();
}

FOpenMobileMemorySnapshot UOpenMobileDeviceSubsystem::GetMemorySnapshot() const
{
	return bDeinitialized
		? FOpenMobileMemorySnapshot()
		: FOpenMobileDeviceSnapshotService::GetMemorySnapshot();
}

FOpenMobileStorageSnapshot UOpenMobileDeviceSubsystem::GetStorageSnapshot() const
{
	return bDeinitialized
		? FOpenMobileStorageSnapshot()
		: FOpenMobileDeviceSnapshotService::GetStorageSnapshot();
}

FOpenMobileNetworkPathSnapshot
UOpenMobileDeviceSubsystem::GetNetworkPathSnapshot() const
{
	return bDeinitialized
		? FOpenMobileNetworkPathSnapshot()
		: FOpenMobileDeviceSnapshotService::GetNetworkPathSnapshot();
}

FOpenMobileWindowDisplaySnapshot
UOpenMobileDeviceSubsystem::GetWindowDisplaySnapshot() const
{
	return bDeinitialized
		? FOpenMobileWindowDisplaySnapshot()
		: FOpenMobileDeviceSnapshotService::GetWindowDisplaySnapshot();
}

FOpenMobileAppearanceSnapshot UOpenMobileDeviceSubsystem::GetAppearanceSnapshot() const
{
	return bDeinitialized
		? FOpenMobileAppearanceSnapshot()
		: FOpenMobileDeviceSnapshotService::GetAppearanceSnapshot();
}

FOpenMobileAccessibilitySnapshot
UOpenMobileDeviceSubsystem::GetAccessibilitySnapshot() const
{
	return bDeinitialized
		? FOpenMobileAccessibilitySnapshot()
		: FOpenMobileDeviceSnapshotService::GetAccessibilitySnapshot();
}

FOpenMobileDeviceStatus UOpenMobileDeviceSubsystem::RefreshNow()
{
	const FOpenMobileDeviceStatus NewStatus = UOpenMobileDeviceBlueprintLibrary::GetDeviceStatus();
	if (NewStatus != LatestStatus)
	{
		LatestStatus = NewStatus;
		OnDeviceStatusChanged.Broadcast(LatestStatus);
	}

	return LatestStatus;
}

bool UOpenMobileDeviceSubsystem::TickStatus(float DeltaTime)
{
	static_cast<void>(DeltaTime);
	RefreshNow();
	return true;
}

void UOpenMobileDeviceSubsystem::RegisterAsyncAction(
	UOpenMobileDeviceAsyncActionBase* Action
)
{
	if (!Action)
	{
		return;
	}
	if (bDeinitialized)
	{
		Action->HandleGameInstanceTeardown();
		return;
	}
	ActiveAsyncActions.Add(Action);
}

void UOpenMobileDeviceSubsystem::UnregisterAsyncAction(
	UOpenMobileDeviceAsyncActionBase* Action
)
{
	ActiveAsyncActions.Remove(Action);
}
