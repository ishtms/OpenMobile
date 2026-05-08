#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceIdentityTypes.h"
#include "OpenMobileDeviceLocaleTypes.h"
#include "OpenMobileDeviceMonitoring.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "OpenMobileDeviceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenMobileDeviceSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileDeviceStatusChangedEvent,
	const FOpenMobileDeviceStatus&,
	Status
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileLocaleSnapshotChangedEvent,
	const FOpenMobileLocaleSnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobilePowerSnapshotChangedEvent,
	const FOpenMobilePowerSnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileMemorySnapshotChangedEvent,
	const FOpenMobileMemorySnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileStorageSnapshotChangedEvent,
	const FOpenMobileStorageSnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileNetworkPathSnapshotChangedEvent,
	const FOpenMobileNetworkPathSnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileWindowDisplaySnapshotChangedEvent,
	const FOpenMobileWindowDisplaySnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAppearanceSnapshotChangedEvent,
	const FOpenMobileAppearanceSnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAccessibilitySnapshotChangedEvent,
	const FOpenMobileAccessibilitySnapshot&,
	Snapshot
);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobilePowerSnapshotChangedNativeEvent,
	const FOpenMobilePowerSnapshot&
);

class UOpenMobileDeviceAsyncActionBase;

UCLASS()
class OPENMOBILEDEVICE_API UOpenMobileDeviceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	const FOpenMobileDeviceStatus& GetLatestStatus() const { return LatestStatus; }

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device")
	FOpenMobileDeviceStatus RefreshNow();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobileDeviceInformationSnapshot GetDeviceInformationSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobileApplicationMetadataSnapshot GetApplicationMetadataSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobileLocaleSnapshot GetLocaleSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobilePowerSnapshot GetPowerSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobileMemorySnapshot GetMemorySnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobileStorageSnapshot GetStorageSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobileNetworkPathSnapshot GetNetworkPathSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobileWindowDisplaySnapshot GetWindowDisplaySnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobileAppearanceSnapshot GetAppearanceSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	FOpenMobileAccessibilitySnapshot GetAccessibilitySnapshot() const;

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Device",
		meta = (DefaultToSelf = "Owner", AdvancedDisplay = "FallbackPollingIntervalSeconds")
	)
	UOpenMobileDeviceMonitoringSubscription* StartMonitoring(
		UObject* Owner,
		const TArray<EOpenMobileDeviceMonitoringGroup>& Groups,
		float FallbackPollingIntervalSeconds = 1.0f
	);

	FOpenMobilePowerSnapshotChangedNativeEvent& OnNativePowerSnapshotChanged()
	{
		return NativePowerSnapshotChanged;
	}

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileDeviceStatusChangedEvent OnDeviceStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileLocaleSnapshotChangedEvent OnLocaleSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobilePowerSnapshotChangedEvent OnPowerSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileMemorySnapshotChangedEvent OnMemorySnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileStorageSnapshotChangedEvent OnStorageSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileNetworkPathSnapshotChangedEvent OnNetworkPathSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileWindowDisplaySnapshotChangedEvent OnWindowDisplaySnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileAppearanceSnapshotChangedEvent OnAppearanceSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileAccessibilitySnapshotChangedEvent OnAccessibilitySnapshotChanged;

private:
	friend class UOpenMobileDeviceAsyncActionBase;
	friend class UOpenMobileDeviceMonitoringSubscription;
	friend class FOpenMobileDeviceAsyncContractTest;

	void StopMonitoringSubscription(
		UOpenMobileDeviceMonitoringSubscription* Subscription
	);
	void BindMonitoringService();
	void UnbindMonitoringService();
	void HandleMonitoringGroupChanged(EOpenMobileDeviceMonitoringGroup Group);
	void HandleMonitoringMaintenance();
	void PrimeMonitoringGroup(EOpenMobileDeviceMonitoringGroup Group);
	void RegisterAsyncAction(UOpenMobileDeviceAsyncActionBase* Action);
	void UnregisterAsyncAction(UOpenMobileDeviceAsyncActionBase* Action);

	UPROPERTY(Transient)
	FOpenMobileDeviceStatus LatestStatus;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOpenMobileDeviceMonitoringSubscription>> MonitoringSubscriptions;

	TMap<EOpenMobileDeviceMonitoringGroup, int32> LocalMonitoringCounts;
	FDelegateHandle MonitoringChangedHandle;
	FDelegateHandle MonitoringMaintenanceHandle;
	TOptional<FOpenMobileLocaleSnapshot> LastLocaleSnapshot;
	TOptional<FOpenMobilePowerSnapshot> LastPowerSnapshot;
	TOptional<FOpenMobileMemorySnapshot> LastMemorySnapshot;
	TOptional<FOpenMobileStorageSnapshot> LastStorageSnapshot;
	TOptional<FOpenMobileNetworkPathSnapshot> LastNetworkSnapshot;
	TOptional<FOpenMobileWindowDisplaySnapshot> LastWindowSnapshot;
	TOptional<FOpenMobileAppearanceSnapshot> LastAppearanceSnapshot;
	TOptional<FOpenMobileAccessibilitySnapshot> LastAccessibilitySnapshot;
	FOpenMobilePowerSnapshotChangedNativeEvent NativePowerSnapshotChanged;
	TSet<TWeakObjectPtr<UOpenMobileDeviceAsyncActionBase>> ActiveAsyncActions;
	bool bDeinitialized = false;
};
