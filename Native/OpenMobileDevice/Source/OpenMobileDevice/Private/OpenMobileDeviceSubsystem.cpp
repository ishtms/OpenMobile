#include "OpenMobileDeviceSubsystem.h"

#include "OpenMobileDeviceAsyncActionBase.h"
#include "OpenMobileDeviceBlueprintLibrary.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceSnapshotService.h"

namespace OpenMobileDeviceSubsystemPrivate
{
	template <typename SnapshotType>
	bool EquivalentWithoutMetadata(SnapshotType Left, SnapshotType Right)
	{
		Left.Metadata = {};
		Right.Metadata = {};
		return Left == Right;
	}

	bool EquivalentFloat(
		const FOpenMobileDeviceOptionalFloat& Left,
		const FOpenMobileDeviceOptionalFloat& Right,
		float Tolerance
	)
	{
		return Left.bIsAvailable == Right.bIsAvailable
			&& (!Left.bIsAvailable
				|| FMath::Abs(Left.Value - Right.Value) <= Tolerance);
	}

	bool EquivalentPower(
		const FOpenMobilePowerSnapshot& Left,
		const FOpenMobilePowerSnapshot& Right
	)
	{
		return EquivalentFloat(Left.BatteryPercent, Right.BatteryPercent, 0.5f)
			&& EquivalentFloat(Left.NativeBatteryLevel, Right.NativeBatteryLevel, 0.005f)
			&& Left.ChargingState == Right.ChargingState
			&& Left.NativeChargingState == Right.NativeChargingState
			&& Left.ChargingSource == Right.ChargingSource
			&& Left.bPowerSavingEnabled == Right.bPowerSavingEnabled
			&& Left.ThermalState == Right.ThermalState
			&& Left.NativeThermalState == Right.NativeThermalState
			&& EquivalentFloat(Left.ThermalHeadroom, Right.ThermalHeadroom, 0.01f)
			&& EquivalentFloat(
				Left.ThermalForecastSeconds,
				Right.ThermalForecastSeconds,
				0.1f
			)
			&& Left.ThermalTrend == Right.ThermalTrend;
	}

	bool EquivalentAccessibility(
		FOpenMobileAccessibilitySnapshot Left,
		FOpenMobileAccessibilitySnapshot Right
	)
	{
		const bool bTextScaleEquivalent = EquivalentFloat(
			Left.PreferredTextScale,
			Right.PreferredTextScale,
			0.01f
		);
		Left.Metadata = {};
		Right.Metadata = {};
		Left.PreferredTextScale = {};
		Right.PreferredTextScale = {};
		return bTextScaleEquivalent && Left == Right;
	}
}

void UOpenMobileDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bDeinitialized = false;

	LatestStatus = UOpenMobileDeviceBlueprintLibrary::GetDeviceStatus();
}

void UOpenMobileDeviceSubsystem::Deinitialize()
{
	bDeinitialized = true;
	TArray<TObjectPtr<UOpenMobileDeviceMonitoringSubscription>> Subscriptions =
		MonitoringSubscriptions;
	for (UOpenMobileDeviceMonitoringSubscription* Subscription : Subscriptions)
	{
		if (Subscription)
		{
			Subscription->Stop();
		}
	}
	MonitoringSubscriptions.Reset();
	LocalMonitoringCounts.Reset();
	UnbindMonitoringService();

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

FOpenMobileMediaVolumeSnapshot
UOpenMobileDeviceSubsystem::GetMediaVolumeSnapshot() const
{
	return bDeinitialized
		? FOpenMobileMediaVolumeSnapshot()
		: FOpenMobileDeviceSnapshotService::GetMediaVolumeSnapshot();
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
		NativeDeviceStatusChanged.Broadcast(LatestStatus);
	}

	return LatestStatus;
}

UOpenMobileDeviceMonitoringSubscription*
UOpenMobileDeviceSubsystem::StartMonitoring(
	UObject* Owner,
	const TArray<EOpenMobileDeviceMonitoringGroup>& Groups,
	float FallbackPollingIntervalSeconds
)
{
	if (bDeinitialized || !IsValid(Owner))
	{
		return nullptr;
	}

	TArray<EOpenMobileDeviceMonitoringGroup> UniqueGroups;
	for (EOpenMobileDeviceMonitoringGroup Group : Groups)
	{
		UniqueGroups.AddUnique(Group);
	}
	if (UniqueGroups.IsEmpty())
	{
		return nullptr;
	}

	if (MonitoringSubscriptions.IsEmpty())
	{
		BindMonitoringService();
	}
	for (EOpenMobileDeviceMonitoringGroup Group : UniqueGroups)
	{
		++LocalMonitoringCounts.FindOrAdd(Group);
	}

	const FGuid RequestId = FOpenMobileDeviceMonitoringService::AddSubscription(
		UniqueGroups,
		FallbackPollingIntervalSeconds
	);
	if (!RequestId.IsValid())
	{
		for (EOpenMobileDeviceMonitoringGroup Group : UniqueGroups)
		{
			if (int32* Count = LocalMonitoringCounts.Find(Group))
			{
				if (--*Count <= 0)
				{
					LocalMonitoringCounts.Remove(Group);
				}
			}
		}
		if (MonitoringSubscriptions.IsEmpty())
		{
			UnbindMonitoringService();
		}
		return nullptr;
	}

	UOpenMobileDeviceMonitoringSubscription* Subscription =
		NewObject<UOpenMobileDeviceMonitoringSubscription>(this);
	Subscription->Owner = Owner;
	Subscription->Subsystem = this;
	Subscription->Groups = UniqueGroups;
	Subscription->ServiceRequestId = RequestId;
	Subscription->bActive = true;
	MonitoringSubscriptions.Add(Subscription);

	for (EOpenMobileDeviceMonitoringGroup Group : UniqueGroups)
	{
		if (LocalMonitoringCounts.FindRef(Group) == 1)
		{
			PrimeMonitoringGroup(Group);
		}
	}
	return Subscription;
}

FOpenMobileDeviceMonitoringHandle
UOpenMobileDeviceSubsystem::StartMonitoringNative(
	const TArray<EOpenMobileDeviceMonitoringGroup>& Groups,
	float FallbackPollingIntervalSeconds
)
{
	return FOpenMobileDeviceMonitoringHandle(StartMonitoring(
		this,
		Groups,
		FallbackPollingIntervalSeconds
	));
}

void UOpenMobileDeviceSubsystem::StopMonitoringSubscription(
	UOpenMobileDeviceMonitoringSubscription* Subscription
)
{
	if (!Subscription)
	{
		return;
	}
	FOpenMobileDeviceMonitoringService::RemoveSubscription(
		Subscription->ServiceRequestId
	);
	for (EOpenMobileDeviceMonitoringGroup Group : Subscription->Groups)
	{
		if (int32* Count = LocalMonitoringCounts.Find(Group))
		{
			if (--*Count <= 0)
			{
				LocalMonitoringCounts.Remove(Group);
			}
		}
	}
	MonitoringSubscriptions.RemoveSingleSwap(Subscription);
	if (MonitoringSubscriptions.IsEmpty())
	{
		UnbindMonitoringService();
	}
}

void UOpenMobileDeviceSubsystem::BindMonitoringService()
{
	if (!MonitoringChangedHandle.IsValid())
	{
		MonitoringChangedHandle =
			FOpenMobileDeviceMonitoringService::OnGroupChanged().AddUObject(
				this,
				&UOpenMobileDeviceSubsystem::HandleMonitoringGroupChanged
			);
	}
	if (!MonitoringMaintenanceHandle.IsValid())
	{
		MonitoringMaintenanceHandle =
			FOpenMobileDeviceMonitoringService::OnMaintenance().AddUObject(
				this,
				&UOpenMobileDeviceSubsystem::HandleMonitoringMaintenance
			);
	}
}

void UOpenMobileDeviceSubsystem::UnbindMonitoringService()
{
	if (MonitoringChangedHandle.IsValid())
	{
		FOpenMobileDeviceMonitoringService::OnGroupChanged().Remove(
			MonitoringChangedHandle
		);
		MonitoringChangedHandle.Reset();
	}
	if (MonitoringMaintenanceHandle.IsValid())
	{
		FOpenMobileDeviceMonitoringService::OnMaintenance().Remove(
			MonitoringMaintenanceHandle
		);
		MonitoringMaintenanceHandle.Reset();
	}
}

void UOpenMobileDeviceSubsystem::HandleMonitoringMaintenance()
{
	TArray<TObjectPtr<UOpenMobileDeviceMonitoringSubscription>> Subscriptions =
		MonitoringSubscriptions;
	for (UOpenMobileDeviceMonitoringSubscription* Subscription : Subscriptions)
	{
		if (Subscription && Subscription->bActive && !Subscription->Owner.IsValid())
		{
			Subscription->Stop();
		}
	}
}

void UOpenMobileDeviceSubsystem::PrimeMonitoringGroup(
	EOpenMobileDeviceMonitoringGroup Group
)
{
	switch (Group)
	{
	case EOpenMobileDeviceMonitoringGroup::Locale:
		LastLocaleSnapshot = GetLocaleSnapshot();
		break;
	case EOpenMobileDeviceMonitoringGroup::Power:
		LastPowerSnapshot = GetPowerSnapshot();
		break;
	case EOpenMobileDeviceMonitoringGroup::MemoryPressure:
		LastMemorySnapshot = GetMemorySnapshot();
		break;
	case EOpenMobileDeviceMonitoringGroup::Storage:
		LastStorageSnapshot = GetStorageSnapshot();
		break;
	case EOpenMobileDeviceMonitoringGroup::Network:
		LastNetworkSnapshot = GetNetworkPathSnapshot();
		break;
	case EOpenMobileDeviceMonitoringGroup::WindowDisplay:
		LastWindowSnapshot = GetWindowDisplaySnapshot();
		break;
	case EOpenMobileDeviceMonitoringGroup::Appearance:
		LastAppearanceSnapshot = GetAppearanceSnapshot();
		break;
	case EOpenMobileDeviceMonitoringGroup::Accessibility:
		LastAccessibilitySnapshot = GetAccessibilitySnapshot();
		break;
	case EOpenMobileDeviceMonitoringGroup::MediaVolume:
		LastMediaVolumeSnapshot = GetMediaVolumeSnapshot();
		break;
	}
}

void UOpenMobileDeviceSubsystem::HandleMonitoringGroupChanged(
	EOpenMobileDeviceMonitoringGroup Group
)
{
	using namespace OpenMobileDeviceSubsystemPrivate;
	if (bDeinitialized || LocalMonitoringCounts.FindRef(Group) <= 0)
	{
		return;
	}

	switch (Group)
	{
	case EOpenMobileDeviceMonitoringGroup::Locale:
	{
		const FOpenMobileLocaleSnapshot Snapshot = GetLocaleSnapshot();
		if (!LastLocaleSnapshot.IsSet()
			|| !EquivalentWithoutMetadata(LastLocaleSnapshot.GetValue(), Snapshot))
		{
			FOpenMobileLocaleSnapshotChange Change;
			if (LastLocaleSnapshot.IsSet())
			{
				Change.PreviousSnapshot = LastLocaleSnapshot.GetValue();
			}
			Change.CurrentSnapshot = Snapshot;
			LastLocaleSnapshot = Snapshot;
			OnLocaleSnapshotChanged.Broadcast(Change);
			NativeLocaleSnapshotChanged.Broadcast(Change);
		}
		break;
	}
	case EOpenMobileDeviceMonitoringGroup::Power:
	{
		const FOpenMobilePowerSnapshot Snapshot = GetPowerSnapshot();
		if (!LastPowerSnapshot.IsSet()
			|| !EquivalentPower(LastPowerSnapshot.GetValue(), Snapshot))
		{
			LastPowerSnapshot = Snapshot;
			OnPowerSnapshotChanged.Broadcast(Snapshot);
			NativePowerSnapshotChanged.Broadcast(Snapshot);
		}
		break;
	}
	case EOpenMobileDeviceMonitoringGroup::MemoryPressure:
	{
		const FOpenMobileMemorySnapshot Snapshot = GetMemorySnapshot();
		if (!LastMemorySnapshot.IsSet()
			|| !EquivalentWithoutMetadata(LastMemorySnapshot.GetValue(), Snapshot))
		{
			LastMemorySnapshot = Snapshot;
			OnMemorySnapshotChanged.Broadcast(Snapshot);
			NativeMemorySnapshotChanged.Broadcast(Snapshot);
		}
		break;
	}
	case EOpenMobileDeviceMonitoringGroup::Storage:
	{
		const FOpenMobileStorageSnapshot Snapshot = GetStorageSnapshot();
		if (!LastStorageSnapshot.IsSet()
			|| !EquivalentWithoutMetadata(LastStorageSnapshot.GetValue(), Snapshot))
		{
			LastStorageSnapshot = Snapshot;
			OnStorageSnapshotChanged.Broadcast(Snapshot);
			NativeStorageSnapshotChanged.Broadcast(Snapshot);
		}
		break;
	}
	case EOpenMobileDeviceMonitoringGroup::Network:
	{
		const FOpenMobileNetworkPathSnapshot Snapshot = GetNetworkPathSnapshot();
		if (!LastNetworkSnapshot.IsSet()
			|| !EquivalentWithoutMetadata(LastNetworkSnapshot.GetValue(), Snapshot))
		{
			LastNetworkSnapshot = Snapshot;
			OnNetworkPathSnapshotChanged.Broadcast(Snapshot);
			NativeNetworkPathSnapshotChanged.Broadcast(Snapshot);
		}
		break;
	}
	case EOpenMobileDeviceMonitoringGroup::WindowDisplay:
	{
		const FOpenMobileWindowDisplaySnapshot Snapshot = GetWindowDisplaySnapshot();
		if (!LastWindowSnapshot.IsSet()
			|| !EquivalentWithoutMetadata(LastWindowSnapshot.GetValue(), Snapshot))
		{
			LastWindowSnapshot = Snapshot;
			OnWindowDisplaySnapshotChanged.Broadcast(Snapshot);
			NativeWindowDisplaySnapshotChanged.Broadcast(Snapshot);
		}
		break;
	}
	case EOpenMobileDeviceMonitoringGroup::Appearance:
	{
		const FOpenMobileAppearanceSnapshot Snapshot = GetAppearanceSnapshot();
		if (!LastAppearanceSnapshot.IsSet()
			|| !EquivalentWithoutMetadata(LastAppearanceSnapshot.GetValue(), Snapshot))
		{
			LastAppearanceSnapshot = Snapshot;
			OnAppearanceSnapshotChanged.Broadcast(Snapshot);
			NativeAppearanceSnapshotChanged.Broadcast(Snapshot);
		}
		break;
	}
	case EOpenMobileDeviceMonitoringGroup::Accessibility:
	{
		const FOpenMobileAccessibilitySnapshot Snapshot = GetAccessibilitySnapshot();
		if (!LastAccessibilitySnapshot.IsSet()
			|| !EquivalentAccessibility(
				LastAccessibilitySnapshot.GetValue(),
				Snapshot
			))
		{
			LastAccessibilitySnapshot = Snapshot;
			OnAccessibilitySnapshotChanged.Broadcast(Snapshot);
			NativeAccessibilitySnapshotChanged.Broadcast(Snapshot);
		}
		break;
	}
	case EOpenMobileDeviceMonitoringGroup::MediaVolume:
	{
		const FOpenMobileMediaVolumeSnapshot Snapshot =
			GetMediaVolumeSnapshot();
		if (!LastMediaVolumeSnapshot.IsSet()
			|| !EquivalentWithoutMetadata(
				LastMediaVolumeSnapshot.GetValue(),
				Snapshot
			))
		{
			LastMediaVolumeSnapshot = Snapshot;
			OnMediaVolumeSnapshotChanged.Broadcast(Snapshot);
			NativeMediaVolumeSnapshotChanged.Broadcast(Snapshot);
		}
		break;
	}
	}
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

void UOpenMobileDeviceMonitoringSubscription::Stop()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	if (UOpenMobileDeviceSubsystem* DeviceSubsystem = Subsystem.Get())
	{
		DeviceSubsystem->StopMonitoringSubscription(this);
	}
	else
	{
		FOpenMobileDeviceMonitoringService::RemoveSubscription(ServiceRequestId);
	}
	Subsystem.Reset();
	Owner.Reset();
	ServiceRequestId.Invalidate();
}

void UOpenMobileDeviceMonitoringSubscription::BeginDestroy()
{
	Stop();
	Super::BeginDestroy();
}

FOpenMobileDeviceMonitoringHandle::FOpenMobileDeviceMonitoringHandle(
	UOpenMobileDeviceMonitoringSubscription* InSubscription
)
	: Subscription(InSubscription)
{
}

FOpenMobileDeviceMonitoringHandle::~FOpenMobileDeviceMonitoringHandle()
{
	Stop();
}

FOpenMobileDeviceMonitoringHandle::FOpenMobileDeviceMonitoringHandle(
	FOpenMobileDeviceMonitoringHandle&& Other
)
	: Subscription(Other.Subscription)
{
	Other.Subscription.Reset();
}

FOpenMobileDeviceMonitoringHandle&
FOpenMobileDeviceMonitoringHandle::operator=(
	FOpenMobileDeviceMonitoringHandle&& Other
)
{
	if (this != &Other)
	{
		Stop();
		Subscription = Other.Subscription;
		Other.Subscription.Reset();
	}
	return *this;
}

void FOpenMobileDeviceMonitoringHandle::Stop()
{
	if (UOpenMobileDeviceMonitoringSubscription* ActiveSubscription =
		Subscription.Get())
	{
		ActiveSubscription->Stop();
	}
	Subscription.Reset();
}

bool FOpenMobileDeviceMonitoringHandle::IsActive() const
{
	const UOpenMobileDeviceMonitoringSubscription* ActiveSubscription =
		Subscription.Get();
	return ActiveSubscription && ActiveSubscription->IsActive();
}
