#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceIdentityTypes.h"
#include "OpenMobileDeviceLocaleTypes.h"
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

class UOpenMobileDeviceAsyncActionBase;

/** Polls portable UE device state and emits only meaningful status changes. */
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

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileDeviceStatusChangedEvent OnDeviceStatusChanged;

private:
	friend class UOpenMobileDeviceAsyncActionBase;
	friend class FOpenMobileDeviceAsyncContractTest;

	bool TickStatus(float DeltaTime);
	void RegisterAsyncAction(UOpenMobileDeviceAsyncActionBase* Action);
	void UnregisterAsyncAction(UOpenMobileDeviceAsyncActionBase* Action);

	UPROPERTY(Transient)
	FOpenMobileDeviceStatus LatestStatus;

	FTSTicker::FDelegateHandle TickerHandle;
	TSet<TWeakObjectPtr<UOpenMobileDeviceAsyncActionBase>> ActiveAsyncActions;
	bool bDeinitialized = false;
};
