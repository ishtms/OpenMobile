#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "OpenMobileDeviceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenMobileDeviceSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileDeviceStatusChangedEvent,
	const FOpenMobileDeviceStatus&,
	Status
);

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

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileDeviceStatusChangedEvent OnDeviceStatusChanged;

private:
	bool TickStatus(float DeltaTime);

	UPROPERTY(Transient)
	FOpenMobileDeviceStatus LatestStatus;

	FTSTicker::FDelegateHandle TickerHandle;
};
