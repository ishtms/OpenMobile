#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OpenMobileDeviceMonitoring.generated.h"

class UOpenMobileDeviceSubsystem;

UENUM(BlueprintType)
enum class EOpenMobileDeviceMonitoringGroup : uint8
{
	Locale,
	Power,
	MemoryPressure,
	Storage,
	Network,
	WindowDisplay,
	Appearance,
	Accessibility,
	MediaVolume
};

UCLASS(BlueprintType)
class OPENMOBILEDEVICE_API UOpenMobileDeviceMonitoringSubscription final
	: public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	TArray<EOpenMobileDeviceMonitoringGroup> GetGroups() const { return Groups; }

protected:
	virtual void BeginDestroy() override;

private:
	friend class UOpenMobileDeviceSubsystem;
	friend class FOpenMobileDeviceDemandDrivenMonitoringTest;

	TWeakObjectPtr<UObject> Owner;
	TWeakObjectPtr<UOpenMobileDeviceSubsystem> Subsystem;
	TArray<EOpenMobileDeviceMonitoringGroup> Groups;
	FGuid ServiceRequestId;
	bool bActive = false;
};
