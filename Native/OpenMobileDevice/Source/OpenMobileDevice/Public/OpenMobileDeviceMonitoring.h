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
	MediaVolume,
	Flashlight
};

UCLASS(BlueprintType)
class OPENMOBILEDEVICE_API UOpenMobileDeviceMonitoringSubscription final
	: public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Device",
		meta = (
			DisplayName = "Stop Device Monitoring",
			ToolTip = "Stops this monitoring subscription. Calling it more than once has no effect."
		)
	)
	void Stop();

	UFUNCTION(
		BlueprintPure,
		Category = "Open Mobile|Device",
		meta = (DisplayName = "Is Device Monitoring Active", ToolTip = "Returns whether this subscription is still active.")
	)
	bool IsActive() const { return bActive; }

	UFUNCTION(
		BlueprintPure,
		Category = "Open Mobile|Device",
		meta = (DisplayName = "Get Device Monitoring Groups", ToolTip = "Returns the event-source groups owned by this subscription.")
	)
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

class OPENMOBILEDEVICE_API FOpenMobileDeviceMonitoringHandle final
{
public:
	FOpenMobileDeviceMonitoringHandle() = default;
	~FOpenMobileDeviceMonitoringHandle();
	FOpenMobileDeviceMonitoringHandle(FOpenMobileDeviceMonitoringHandle&& Other);
	FOpenMobileDeviceMonitoringHandle& operator=(
		FOpenMobileDeviceMonitoringHandle&& Other
	);
	FOpenMobileDeviceMonitoringHandle(
		const FOpenMobileDeviceMonitoringHandle&
	) = delete;
	FOpenMobileDeviceMonitoringHandle& operator=(
		const FOpenMobileDeviceMonitoringHandle&
	) = delete;

	void Stop();
	bool IsActive() const;

private:
	friend class UOpenMobileDeviceSubsystem;

	explicit FOpenMobileDeviceMonitoringHandle(
		UOpenMobileDeviceMonitoringSubscription* InSubscription
	);

	TWeakObjectPtr<UOpenMobileDeviceMonitoringSubscription> Subscription;
};
