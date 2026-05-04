#include "OpenMobileDeviceSubsystem.h"

#include "OpenMobileDeviceBlueprintLibrary.h"

void UOpenMobileDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LatestStatus = UOpenMobileDeviceBlueprintLibrary::GetDeviceStatus();
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UOpenMobileDeviceSubsystem::TickStatus),
		0.5f
	);
}

void UOpenMobileDeviceSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	Super::Deinitialize();
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
