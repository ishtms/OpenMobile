#include "OpenMobileDeviceSubsystem.h"

#include "OpenMobileDeviceAsyncActionBase.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBlueprintLibrary.h"
#include "OpenMobileDeviceBrightnessControlService.h"
#include "OpenMobileDeviceKeepScreenAwakeControlService.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceOrientationControlService.h"
#include "OpenMobileDeviceRefreshRateControlService.h"
#include "OpenMobileDeviceSnapshotService.h"
#include "OpenMobileDeviceStorageQueryAsyncAction.h"
#include "OpenMobileDeviceSystemUiControlService.h"

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

	bool EquivalentBattery(
		const FOpenMobilePowerSnapshot& Left,
		const FOpenMobilePowerSnapshot& Right
	)
	{
		return EquivalentFloat(Left.BatteryPercent, Right.BatteryPercent, 0.5f)
			&& EquivalentFloat(Left.NativeBatteryLevel, Right.NativeBatteryLevel, 0.005f)
			&& Left.ChargingState == Right.ChargingState
			&& Left.NativeChargingState == Right.NativeChargingState
			&& Left.ChargingSource == Right.ChargingSource;
	}

	bool EquivalentPowerSavingMode(
		const FOpenMobilePowerSnapshot& Left,
		const FOpenMobilePowerSnapshot& Right
	)
	{
		return Left.bPowerSavingEnabled == Right.bPowerSavingEnabled
			&& Left.NativePowerSavingState == Right.NativePowerSavingState;
	}

	bool EquivalentThermal(
		const FOpenMobilePowerSnapshot& Left,
		const FOpenMobilePowerSnapshot& Right
	)
	{
		return Left.ThermalState == Right.ThermalState
			&& Left.NativeThermalState == Right.NativeThermalState
			&& EquivalentFloat(Left.ThermalHeadroom, Right.ThermalHeadroom, 0.01f)
			&& EquivalentFloat(
				Left.ThermalForecastSeconds,
				Right.ThermalForecastSeconds,
				0.1f
			)
			&& Left.ThermalTrend == Right.ThermalTrend;
	}

	bool EquivalentPower(
		const FOpenMobilePowerSnapshot& Left,
		const FOpenMobilePowerSnapshot& Right
	)
	{
		return EquivalentBattery(Left, Right)
			&& EquivalentPowerSavingMode(Left, Right)
			&& EquivalentThermal(Left, Right);
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
	TArray<TObjectPtr<UOpenMobileOrientationPolicyHandle>> OrientationHandles =
		OrientationPolicyHandles;
	for (UOpenMobileOrientationPolicyHandle* Handle : OrientationHandles)
	{
		if (Handle)
		{
			Handle->Release();
		}
	}
	OrientationPolicyHandles.Reset();

	TArray<TObjectPtr<UOpenMobilePreferredRefreshRateHandle>> RefreshRateHandles =
		PreferredRefreshRateHandles;
	for (UOpenMobilePreferredRefreshRateHandle* Handle : RefreshRateHandles)
	{
		if (Handle)
		{
			Handle->Release();
		}
	}
	PreferredRefreshRateHandles.Reset();

	TArray<TObjectPtr<UOpenMobileBrightnessHandle>> BrightnessOverrideHandles =
		BrightnessHandles;
	for (UOpenMobileBrightnessHandle* Handle : BrightnessOverrideHandles)
	{
		if (Handle)
		{
			Handle->Release();
		}
	}
	BrightnessHandles.Reset();

	TArray<TObjectPtr<UOpenMobileKeepScreenAwakeHandle>> KeepAwakeHandles =
		KeepScreenAwakeHandles;
	for (UOpenMobileKeepScreenAwakeHandle* Handle : KeepAwakeHandles)
	{
		if (Handle)
		{
			Handle->Release();
		}
	}
	KeepScreenAwakeHandles.Reset();

	TArray<TObjectPtr<UOpenMobileSystemUiHandle>> SystemUiModeHandles =
		SystemUiHandles;
	for (UOpenMobileSystemUiHandle* Handle : SystemUiModeHandles)
	{
		if (Handle)
		{
			Handle->Release();
		}
	}
	SystemUiHandles.Reset();

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
	ActiveStorageMonitoringQuery.Reset();
	bStorageMonitoringRefreshPending = false;

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
	if (bDeinitialized)
	{
		return {};
	}
	const IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	const uint64 CurrentBackendGeneration = Backend
		? FOpenMobileDeviceBackendRegistry::CaptureCallbackToken().Generation
		: 0;
	return !LastStorageSnapshot.IsSet()
		|| LastStorageBackendGeneration != CurrentBackendGeneration
		? FOpenMobileStorageSnapshot()
		: LastStorageSnapshot.GetValue();
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

FOpenMobileBrightnessSnapshot
UOpenMobileDeviceSubsystem::GetBrightnessSnapshot() const
{
	return bDeinitialized
		? FOpenMobileBrightnessSnapshot()
		: FOpenMobileDeviceSnapshotService::GetBrightnessSnapshot();
}

UOpenMobileBrightnessHandle*
UOpenMobileDeviceSubsystem::RequestBrightnessOverride(
	const FOpenMobileBrightnessRequest& Request
)
{
	UOpenMobileBrightnessHandle* Handle =
		NewObject<UOpenMobileBrightnessHandle>(this);
	Handle->Request = Request;
	if (bDeinitialized)
	{
		Handle->Result.Request = Request;
		Handle->Result.State = EOpenMobileBrightnessApplyState::Rejected;
		Handle->Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Device subsystem has been deinitialized.")
		);
		return Handle;
	}
	Handle->RequestId = FOpenMobileDeviceBrightnessControlService::AddRequest(
		Request,
		Handle->Result
	);
	Handle->bActive = Handle->RequestId.IsValid();
	if (Handle->bActive)
	{
		Handle->Subsystem = this;
		BrightnessHandles.Add(Handle);
	}
	return Handle;
}

void UOpenMobileDeviceSubsystem::ReleaseBrightnessHandle(
	UOpenMobileBrightnessHandle* Handle
)
{
	if (!Handle || !Handle->bActive)
	{
		return;
	}
	FOpenMobileDeviceBrightnessControlService::RemoveRequest(Handle->RequestId);
	Handle->bActive = false;
	Handle->RequestId.Invalidate();
	Handle->Subsystem.Reset();
	BrightnessHandles.RemoveSingleSwap(Handle);
}

UOpenMobileKeepScreenAwakeHandle*
UOpenMobileDeviceSubsystem::RequestKeepScreenAwake()
{
	UOpenMobileKeepScreenAwakeHandle* Handle =
		NewObject<UOpenMobileKeepScreenAwakeHandle>(this);
	if (bDeinitialized)
	{
		Handle->Result.State =
			EOpenMobileKeepScreenAwakeApplyState::Rejected;
		Handle->Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Device subsystem has been deinitialized.")
		);
		return Handle;
	}
	Handle->RequestId =
		FOpenMobileDeviceKeepScreenAwakeControlService::AddRequest(
			Handle->Result
		);
	Handle->bActive = Handle->RequestId.IsValid();
	if (Handle->bActive)
	{
		Handle->Subsystem = this;
		KeepScreenAwakeHandles.Add(Handle);
	}
	return Handle;
}

void UOpenMobileDeviceSubsystem::ReleaseKeepScreenAwakeHandle(
	UOpenMobileKeepScreenAwakeHandle* Handle
)
{
	if (!Handle || !Handle->bActive)
	{
		return;
	}
	FOpenMobileDeviceKeepScreenAwakeControlService::RemoveRequest(
		Handle->RequestId
	);
	Handle->bActive = false;
	Handle->RequestId.Invalidate();
	Handle->Subsystem.Reset();
	KeepScreenAwakeHandles.RemoveSingleSwap(Handle);
}

UOpenMobileSystemUiHandle* UOpenMobileDeviceSubsystem::RequestSystemUiMode(
	const FOpenMobileSystemUiRequest& Request
)
{
	UOpenMobileSystemUiHandle* Handle =
		NewObject<UOpenMobileSystemUiHandle>(this);
	Handle->Request = Request;
	if (bDeinitialized)
	{
		Handle->Result.Request = Request;
		Handle->Result.State = EOpenMobileSystemUiApplyState::Rejected;
		Handle->Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Device subsystem has been deinitialized.")
		);
		return Handle;
	}
	Handle->RequestId = FOpenMobileDeviceSystemUiControlService::AddRequest(
		Request,
		Handle->Result
	);
	Handle->bActive = Handle->RequestId.IsValid();
	if (Handle->bActive)
	{
		Handle->Subsystem = this;
		SystemUiHandles.Add(Handle);
	}
	return Handle;
}

void UOpenMobileDeviceSubsystem::ReleaseSystemUiHandle(
	UOpenMobileSystemUiHandle* Handle
)
{
	if (!Handle || !Handle->bActive)
	{
		return;
	}
	FOpenMobileDeviceSystemUiControlService::RemoveRequest(Handle->RequestId);
	Handle->bActive = false;
	Handle->RequestId.Invalidate();
	Handle->Subsystem.Reset();
	SystemUiHandles.RemoveSingleSwap(Handle);
}

UOpenMobilePreferredRefreshRateHandle*
UOpenMobileDeviceSubsystem::RequestPreferredRefreshRate(
	const FOpenMobilePreferredRefreshRateRequest& Request
)
{
	UOpenMobilePreferredRefreshRateHandle* Handle =
		NewObject<UOpenMobilePreferredRefreshRateHandle>(this);
	Handle->Request = Request;
	if (bDeinitialized)
	{
		Handle->Result.Request = Request;
		Handle->Result.State =
			EOpenMobilePreferredRefreshRateApplyState::Rejected;
		Handle->Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Device subsystem has been deinitialized.")
		);
		return Handle;
	}
	Handle->RequestId = FOpenMobileDeviceRefreshRateControlService::AddRequest(
		Request,
		Handle->Result
	);
	Handle->bActive = Handle->RequestId.IsValid();
	if (Handle->bActive)
	{
		Handle->Subsystem = this;
		PreferredRefreshRateHandles.Add(Handle);
	}
	return Handle;
}

void UOpenMobileDeviceSubsystem::ReleasePreferredRefreshRateHandle(
	UOpenMobilePreferredRefreshRateHandle* Handle
)
{
	if (!Handle || !Handle->bActive)
	{
		return;
	}
	FOpenMobileDeviceRefreshRateControlService::RemoveRequest(Handle->RequestId);
	Handle->bActive = false;
	Handle->RequestId.Invalidate();
	Handle->Subsystem.Reset();
	PreferredRefreshRateHandles.RemoveSingleSwap(Handle);
}

UOpenMobileOrientationPolicyHandle*
UOpenMobileDeviceSubsystem::RequestOrientationPolicy(
	const FOpenMobileOrientationPolicyRequest& Request
)
{
	UOpenMobileOrientationPolicyHandle* Handle =
		NewObject<UOpenMobileOrientationPolicyHandle>(this);
	Handle->Request = Request;
	if (bDeinitialized)
	{
		Handle->Result.Request = Request;
		Handle->Result.State =
			EOpenMobileOrientationPolicyApplyState::Rejected;
		Handle->Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Device subsystem has been deinitialized.")
		);
		return Handle;
	}
	Handle->RequestId = FOpenMobileDeviceOrientationControlService::AddRequest(
		Request,
		Handle->Result
	);
	Handle->bActive = Handle->RequestId.IsValid();
	if (Handle->bActive)
	{
		Handle->Subsystem = this;
		OrientationPolicyHandles.Add(Handle);
	}
	return Handle;
}

void UOpenMobileDeviceSubsystem::ReleaseOrientationPolicyHandle(
	UOpenMobileOrientationPolicyHandle* Handle
)
{
	if (!Handle || !Handle->bActive)
	{
		return;
	}
	FOpenMobileDeviceOrientationControlService::RemoveRequest(
		Handle->RequestId
	);
	Handle->bActive = false;
	Handle->RequestId.Invalidate();
	Handle->Subsystem.Reset();
	OrientationPolicyHandles.RemoveSingleSwap(Handle);
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
	if (!MonitoredNetworkChangedHandle.IsValid())
	{
		MonitoredNetworkChangedHandle =
			FOpenMobileDeviceMonitoringService::OnNetworkPathChanged().AddUObject(
				this,
				&UOpenMobileDeviceSubsystem::HandleMonitoredNetworkPathChanged
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
	if (MonitoredNetworkChangedHandle.IsValid())
	{
		FOpenMobileDeviceMonitoringService::OnNetworkPathChanged().Remove(
			MonitoredNetworkChangedHandle
		);
		MonitoredNetworkChangedHandle.Reset();
	}
	if (MonitoringMaintenanceHandle.IsValid())
	{
		FOpenMobileDeviceMonitoringService::OnMaintenance().Remove(
			MonitoringMaintenanceHandle
		);
		MonitoringMaintenanceHandle.Reset();
	}
}

void UOpenMobileDeviceSubsystem::HandleMonitoredNetworkPathChanged(
	const FOpenMobileNetworkPathSnapshot& Snapshot
)
{
	if (bDeinitialized
		|| LocalMonitoringCounts.FindRef(
			EOpenMobileDeviceMonitoringGroup::Network
		) <= 0)
	{
		return;
	}
	if (!LastNetworkSnapshot.IsSet()
		|| !OpenMobileDeviceSubsystemPrivate::EquivalentWithoutMetadata(
			LastNetworkSnapshot.GetValue(),
			Snapshot
		))
	{
		LastNetworkSnapshot = Snapshot;
		OnNetworkPathSnapshotChanged.Broadcast(Snapshot);
		NativeNetworkPathSnapshotChanged.Broadcast(Snapshot);
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
		RequestStorageRefreshForMonitoring();
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
		const bool bHadPrevious = LastPowerSnapshot.IsSet();
		const bool bBatteryChanged = !bHadPrevious
			|| !EquivalentBattery(LastPowerSnapshot.GetValue(), Snapshot);
		const bool bPowerSavingModeChanged = !bHadPrevious
			|| !EquivalentPowerSavingMode(
				LastPowerSnapshot.GetValue(),
				Snapshot
			);
		const bool bThermalChanged = !bHadPrevious
			|| !EquivalentThermal(LastPowerSnapshot.GetValue(), Snapshot);
		if (!bHadPrevious
			|| !EquivalentPower(LastPowerSnapshot.GetValue(), Snapshot))
		{
			LastPowerSnapshot = Snapshot;
			if (bBatteryChanged)
			{
				OnBatteryChanged.Broadcast(Snapshot);
				NativeBatteryChanged.Broadcast(Snapshot);
			}
			if (bPowerSavingModeChanged)
			{
				OnPowerSavingModeChanged.Broadcast(Snapshot);
				NativePowerSavingModeChanged.Broadcast(Snapshot);
			}
			if (bThermalChanged)
			{
				OnThermalChanged.Broadcast(Snapshot);
				NativeThermalChanged.Broadcast(Snapshot);
			}
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
		RequestStorageRefreshForMonitoring();
		break;
	case EOpenMobileDeviceMonitoringGroup::Network:
		break;
	case EOpenMobileDeviceMonitoringGroup::WindowDisplay:
	{
		const FOpenMobileWindowDisplaySnapshot Snapshot = GetWindowDisplaySnapshot();
		const bool bOrientationChanged = !LastWindowSnapshot.IsSet()
			|| LastWindowSnapshot->Orientation != Snapshot.Orientation;
		if (!LastWindowSnapshot.IsSet()
			|| !EquivalentWithoutMetadata(LastWindowSnapshot.GetValue(), Snapshot))
		{
			LastWindowSnapshot = Snapshot;
			if (bOrientationChanged)
			{
				OnWindowOrientationChanged.Broadcast(Snapshot);
				NativeWindowOrientationChanged.Broadcast(Snapshot);
			}
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

void UOpenMobileDeviceSubsystem::CacheStorageSnapshot(
	const FOpenMobileStorageSnapshot& Snapshot,
	uint64 BackendGeneration
)
{
	if (bDeinitialized)
	{
		return;
	}
	const bool bChanged = LastStorageSnapshot.IsSet()
		&& LastStorageBackendGeneration == BackendGeneration
		&& !OpenMobileDeviceSubsystemPrivate::EquivalentWithoutMetadata(
			LastStorageSnapshot.GetValue(),
			Snapshot
		);
	LastStorageSnapshot = Snapshot;
	LastStorageBackendGeneration = BackendGeneration;
	if (bChanged
		&& LocalMonitoringCounts.FindRef(
			EOpenMobileDeviceMonitoringGroup::Storage
		) > 0)
	{
		OnStorageSnapshotChanged.Broadcast(Snapshot);
		NativeStorageSnapshotChanged.Broadcast(Snapshot);
	}
}

void UOpenMobileDeviceSubsystem::RequestStorageRefreshForMonitoring()
{
	if (bDeinitialized
		|| LocalMonitoringCounts.FindRef(
			EOpenMobileDeviceMonitoringGroup::Storage
		) <= 0)
	{
		return;
	}
	if (ActiveStorageMonitoringQuery.IsValid()
		&& !ActiveStorageMonitoringQuery->IsFinished())
	{
		bStorageMonitoringRefreshPending = true;
		return;
	}
	bStorageMonitoringRefreshPending = false;
	UOpenMobileDeviceStorageQueryAsyncAction* Action =
		UOpenMobileDeviceStorageQueryAsyncAction::QueryStorage(GetGameInstance());
	ActiveStorageMonitoringQuery = Action;
	Action->OnNativeTerminal().AddUObject(
		this,
		&UOpenMobileDeviceSubsystem::HandleStorageMonitoringQueryTerminal
	);
	Action->Activate();
}

void UOpenMobileDeviceSubsystem::HandleStorageMonitoringQueryTerminal(
	EOpenMobileDeviceAsyncTerminalState State,
	const FOpenMobileError& Error
)
{
	static_cast<void>(State);
	static_cast<void>(Error);
	ActiveStorageMonitoringQuery.Reset();
	if (bStorageMonitoringRefreshPending)
	{
		bStorageMonitoringRefreshPending = false;
		RequestStorageRefreshForMonitoring();
	}
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
