#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceAndroidPackageTypes.h"
#include "OpenMobileDeviceApplicationSettingsTypes.h"
#include "OpenMobileDeviceBrightnessControl.h"
#include "OpenMobileDeviceCapabilities.h"
#include "OpenMobileDeviceClipboardTypes.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceFlashlightTypes.h"
#include "OpenMobileDeviceIdentityTypes.h"
#include "OpenMobileDeviceIntentHandlerTypes.h"
#include "OpenMobileDeviceKeepScreenAwakeControl.h"
#include "OpenMobileDeviceLocaleTypes.h"
#include "OpenMobileDeviceMonitoring.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceOrientationControl.h"
#include "OpenMobileDeviceRefreshRateControl.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "OpenMobileDeviceTypes.h"
#include "OpenMobileDeviceSystemUiControl.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenMobileDeviceSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileDeviceStatusChangedEvent,
	const FOpenMobileDeviceStatus&,
	Status
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileApplicationSettingsReturnedEvent,
	const FOpenMobileDeviceCapabilityReport&,
	CapabilityReport
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileLocaleSnapshotChangedEvent,
	const FOpenMobileLocaleSnapshotChange&,
	Change
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobilePowerSnapshotChangedEvent,
	const FOpenMobilePowerSnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileBatteryChangedEvent,
	const FOpenMobilePowerSnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobilePowerSavingModeChangedEvent,
	const FOpenMobilePowerSnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileThermalChangedEvent,
	const FOpenMobilePowerSnapshot&,
	Snapshot
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileMediaVolumeSnapshotChangedEvent,
	const FOpenMobileMediaVolumeSnapshot&,
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
	FOpenMobileWindowOrientationChangedEvent,
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileFlashlightSnapshotChangedEvent,
	const FOpenMobileFlashlightSnapshot&,
	Snapshot
);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileDeviceStatusChangedNativeEvent,
	const FOpenMobileDeviceStatus&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileApplicationSettingsReturnedNativeEvent,
	const FOpenMobileDeviceCapabilityReport&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileLocaleSnapshotChangedNativeEvent,
	const FOpenMobileLocaleSnapshotChange&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobilePowerSnapshotChangedNativeEvent,
	const FOpenMobilePowerSnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileBatteryChangedNativeEvent,
	const FOpenMobilePowerSnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobilePowerSavingModeChangedNativeEvent,
	const FOpenMobilePowerSnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileThermalChangedNativeEvent,
	const FOpenMobilePowerSnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileMediaVolumeSnapshotChangedNativeEvent,
	const FOpenMobileMediaVolumeSnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileMemorySnapshotChangedNativeEvent,
	const FOpenMobileMemorySnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileStorageSnapshotChangedNativeEvent,
	const FOpenMobileStorageSnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileNetworkPathSnapshotChangedNativeEvent,
	const FOpenMobileNetworkPathSnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileWindowDisplaySnapshotChangedNativeEvent,
	const FOpenMobileWindowDisplaySnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileWindowOrientationChangedNativeEvent,
	const FOpenMobileWindowDisplaySnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAppearanceSnapshotChangedNativeEvent,
	const FOpenMobileAppearanceSnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAccessibilitySnapshotChangedNativeEvent,
	const FOpenMobileAccessibilitySnapshot&
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileFlashlightSnapshotChangedNativeEvent,
	const FOpenMobileFlashlightSnapshot&
);

class UOpenMobileDeviceAsyncActionBase;
class UOpenMobileDeviceStorageQueryAsyncAction;
enum class EOpenMobileDeviceAsyncTerminalState : uint8;

UCLASS()
class OPENMOBILEDEVICE_API UOpenMobileDeviceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(
		BlueprintPure,
		Category = "Open Mobile|Device",
		meta = (DisplayName = "Get Latest Device Status", ToolTip = "Returns the latest legacy battery and media-volume status cached by this Game Instance.")
	)
	const FOpenMobileDeviceStatus& GetLatestStatus() const { return LatestStatus; }

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Device",
		meta = (DisplayName = "Refresh Device Status", ToolTip = "Refreshes the legacy battery and media-volume status immediately and broadcasts only if it changed.")
	)
	FOpenMobileDeviceStatus RefreshNow();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Device Information Snapshot", ToolTip = "Captures current normalized device and operating-system information without prompting."))
	FOpenMobileDeviceInformationSnapshot GetDeviceInformationSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Application Metadata Snapshot", ToolTip = "Captures current application identity and version metadata without prompting."))
	FOpenMobileApplicationMetadataSnapshot GetApplicationMetadataSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Locale Snapshot", ToolTip = "Captures current language, locale, region, time-zone, and formatting preferences."))
	FOpenMobileLocaleSnapshot GetLocaleSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Power Snapshot", ToolTip = "Captures current battery, charging, power-saving, and thermal state without prompting."))
	FOpenMobilePowerSnapshot GetPowerSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Media Volume Snapshot", ToolTip = "Captures the active output or media-volume level with explicit availability."))
	FOpenMobileMediaVolumeSnapshot GetMediaVolumeSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Memory Snapshot", ToolTip = "Captures current physical-memory and memory-pressure state."))
	FOpenMobileMemorySnapshot GetMemorySnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Storage Snapshot", ToolTip = "Returns the latest successful asynchronous application storage query without filesystem work."))
	FOpenMobileStorageSnapshot GetStorageSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Network Path Snapshot", ToolTip = "Captures the current network path and transport state without contacting an endpoint."))
	FOpenMobileNetworkPathSnapshot GetNetworkPathSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Window and Display Snapshot", ToolTip = "Captures the current game window, display, safe-area, orientation, and posture state."))
	FOpenMobileWindowDisplaySnapshot GetWindowDisplaySnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get App Screen Brightness", ToolTip = "Captures the normalized brightness of the active app screen without prompting."))
	FOpenMobileBrightnessSnapshot GetBrightnessSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Flashlight Snapshot", ToolTip = "Captures flashlight hardware, state, intensity support, permission, conflict, thermal, and ownership information without prompting or opening a camera capture session."))
	FOpenMobileFlashlightSnapshot GetFlashlightSnapshot() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Check Clipboard Content Types", ToolTip = "Checks portable clipboard types from metadata without reading clipboard values or triggering a paste notification."))
	FOpenMobileClipboardOperationResult CheckClipboardContentTypes() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Write Clipboard", ToolTip = "Writes bounded Text or Url content to the foreground system clipboard."))
	FOpenMobileClipboardOperationResult WriteClipboard(
		const FOpenMobileClipboardWriteRequest& Request
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Read Clipboard", ToolTip = "Directly reads one portable clipboard type while foregrounded. The OS may show a paste notification or permission prompt."))
	FOpenMobileClipboardOperationResult ReadClipboard(
		EOpenMobileClipboardContentType ContentType
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Clear Clipboard", ToolTip = "Removes all clipboard items when the platform provides a guaranteed clear operation."))
	FOpenMobileClipboardOperationResult ClearClipboard();

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Check Intent Handler", ToolTip = "Checks one declared URL or Android intent action without launching it or enumerating installed applications. A positive result does not guarantee a later launch succeeds."))
	FOpenMobileIntentHandlerCheckResult CheckIntentHandler(
		const FOpenMobileIntentHandlerCheckRequest& Request
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Check Android Package", ToolTip = "Checks one exact Android application ID declared for this build. It never enumerates installed applications and returns Unsupported on non-Android platforms."))
	FOpenMobileAndroidPackageCheckResult CheckAndroidPackage(
		const FOpenMobileAndroidPackageCheckRequest& Request
	) const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Open Application Settings", ToolTip = "Submits one request to open only this application's system-settings page. Accepted does not mean the user changed a setting."))
	FOpenMobileApplicationSettingsOpenResult OpenApplicationSettings();

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Request Brightness Override", ToolTip = "Overrides active app window or screen brightness until the returned handle is released."))
	UOpenMobileBrightnessHandle* RequestBrightnessOverride(
		const FOpenMobileBrightnessRequest& Request
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Keep Screen Awake", ToolTip = "Keeps the active app screen awake until the returned handle is released."))
	UOpenMobileKeepScreenAwakeHandle* RequestKeepScreenAwake();

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Request System UI Mode", ToolTip = "Applies Normal, Edge to Edge, or Immersive system UI until the returned handle is released."))
	UOpenMobileSystemUiHandle* RequestSystemUiMode(
		const FOpenMobileSystemUiRequest& Request
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Request Preferred Refresh Rate", ToolTip = "Requests preferred display refresh-rate bounds or a target until the returned handle is released."))
	UOpenMobilePreferredRefreshRateHandle* RequestPreferredRefreshRate(
		const FOpenMobilePreferredRefreshRateRequest& Request
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device", meta = (DisplayName = "Request Orientation Policy", ToolTip = "Applies an orientation policy until the returned handle is released."))
	UOpenMobileOrientationPolicyHandle* RequestOrientationPolicy(
		const FOpenMobileOrientationPolicyRequest& Request
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Appearance Snapshot", ToolTip = "Captures the current system appearance preference."))
	FOpenMobileAppearanceSnapshot GetAppearanceSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device", meta = (DisplayName = "Get Accessibility Snapshot", ToolTip = "Captures current accessibility preferences exposed by the platform."))
	FOpenMobileAccessibilitySnapshot GetAccessibilitySnapshot() const;

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Device",
		meta = (
			DefaultToSelf = "Owner",
			AdvancedDisplay = "FallbackPollingIntervalSeconds",
			DisplayName = "Start Device Monitoring",
			ToolTip = "Starts monitoring selected event-source groups. Stop the returned subscription when monitoring is no longer needed."
		)
	)
	UOpenMobileDeviceMonitoringSubscription* StartMonitoring(
		UObject* Owner,
		const TArray<EOpenMobileDeviceMonitoringGroup>& Groups,
		float FallbackPollingIntervalSeconds = 0.0f
	);

	FOpenMobileDeviceMonitoringHandle StartMonitoringNative(
		const TArray<EOpenMobileDeviceMonitoringGroup>& Groups,
		float FallbackPollingIntervalSeconds = 0.0f
	);

	FOpenMobileDeviceStatusChangedNativeEvent& OnNativeDeviceStatusChanged()
	{
		return NativeDeviceStatusChanged;
	}

	FOpenMobileApplicationSettingsReturnedNativeEvent&
	OnNativeApplicationSettingsReturned()
	{
		return NativeApplicationSettingsReturned;
	}

	FOpenMobileLocaleSnapshotChangedNativeEvent& OnNativeLocaleSnapshotChanged()
	{
		return NativeLocaleSnapshotChanged;
	}

	FOpenMobilePowerSnapshotChangedNativeEvent& OnNativePowerSnapshotChanged()
	{
		return NativePowerSnapshotChanged;
	}

	FOpenMobileBatteryChangedNativeEvent& OnNativeBatteryChanged()
	{
		return NativeBatteryChanged;
	}

	FOpenMobilePowerSavingModeChangedNativeEvent&
	OnNativePowerSavingModeChanged()
	{
		return NativePowerSavingModeChanged;
	}

	FOpenMobileThermalChangedNativeEvent& OnNativeThermalChanged()
	{
		return NativeThermalChanged;
	}

	FOpenMobileMediaVolumeSnapshotChangedNativeEvent&
	OnNativeMediaVolumeSnapshotChanged()
	{
		return NativeMediaVolumeSnapshotChanged;
	}

	FOpenMobileMemorySnapshotChangedNativeEvent& OnNativeMemorySnapshotChanged()
	{
		return NativeMemorySnapshotChanged;
	}

	FOpenMobileStorageSnapshotChangedNativeEvent& OnNativeStorageSnapshotChanged()
	{
		return NativeStorageSnapshotChanged;
	}

	FOpenMobileNetworkPathSnapshotChangedNativeEvent&
	OnNativeNetworkPathSnapshotChanged()
	{
		return NativeNetworkPathSnapshotChanged;
	}

	FOpenMobileWindowDisplaySnapshotChangedNativeEvent&
	OnNativeWindowDisplaySnapshotChanged()
	{
		return NativeWindowDisplaySnapshotChanged;
	}

	FOpenMobileWindowOrientationChangedNativeEvent&
	OnNativeWindowOrientationChanged()
	{
		return NativeWindowOrientationChanged;
	}

	FOpenMobileAppearanceSnapshotChangedNativeEvent&
	OnNativeAppearanceSnapshotChanged()
	{
		return NativeAppearanceSnapshotChanged;
	}

	FOpenMobileAccessibilitySnapshotChangedNativeEvent&
	OnNativeAccessibilitySnapshotChanged()
	{
		return NativeAccessibilitySnapshotChanged;
	}

	FOpenMobileFlashlightSnapshotChangedNativeEvent&
	OnNativeFlashlightSnapshotChanged()
	{
		return NativeFlashlightSnapshotChanged;
	}

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Device Status Changed", ToolTip = "Broadcasts when monitored legacy battery or media-volume status changes."))
	FOpenMobileDeviceStatusChangedEvent OnDeviceStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Application Settings Returned", ToolTip = "Broadcasts a freshly queried Device capability report after an accepted settings request returns to the foreground. Requery explicit permission statuses in this event."))
	FOpenMobileApplicationSettingsReturnedEvent OnApplicationSettingsReturned;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Locale Snapshot Changed", ToolTip = "Broadcasts the previous and current locale snapshots after a monitored value changes."))
	FOpenMobileLocaleSnapshotChangedEvent OnLocaleSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Power Snapshot Changed", ToolTip = "Broadcasts when a monitored power snapshot changes beyond its numeric tolerances."))
	FOpenMobilePowerSnapshotChangedEvent OnPowerSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Battery Changed", ToolTip = "Broadcasts when monitored battery level, charging state, or charging source changes beyond its numeric tolerances."))
	FOpenMobileBatteryChangedEvent OnBatteryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Power Saving Mode Changed", ToolTip = "Broadcasts when the monitored Battery Saver or Low Power Mode state changes."))
	FOpenMobilePowerSavingModeChangedEvent OnPowerSavingModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Thermal Changed", ToolTip = "Broadcasts when monitored thermal state, headroom, forecast, or trend changes beyond its numeric tolerances."))
	FOpenMobileThermalChangedEvent OnThermalChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Media Volume Snapshot Changed", ToolTip = "Broadcasts when the monitored active output or media-volume level changes."))
	FOpenMobileMediaVolumeSnapshotChangedEvent OnMediaVolumeSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Memory Snapshot Changed", ToolTip = "Broadcasts when a monitored memory or memory-pressure snapshot changes."))
	FOpenMobileMemorySnapshotChangedEvent OnMemorySnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Storage Snapshot Changed", ToolTip = "Broadcasts when a monitored storage snapshot changes."))
	FOpenMobileStorageSnapshotChangedEvent OnStorageSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Network Path Snapshot Changed", ToolTip = "Broadcasts when a monitored network-path snapshot changes."))
	FOpenMobileNetworkPathSnapshotChangedEvent OnNetworkPathSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Window and Display Snapshot Changed", ToolTip = "Broadcasts when a monitored window or display snapshot changes."))
	FOpenMobileWindowDisplaySnapshotChangedEvent OnWindowDisplaySnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Window Orientation Changed", ToolTip = "Broadcasts a complete Window and Display snapshot after the active UI orientation changes."))
	FOpenMobileWindowOrientationChangedEvent OnWindowOrientationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Appearance Snapshot Changed", ToolTip = "Broadcasts when the monitored system appearance changes."))
	FOpenMobileAppearanceSnapshotChangedEvent OnAppearanceSnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Accessibility Snapshot Changed", ToolTip = "Broadcasts when a monitored accessibility snapshot changes beyond its numeric tolerances."))
	FOpenMobileAccessibilitySnapshotChangedEvent OnAccessibilitySnapshotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "On Flashlight Snapshot Changed", ToolTip = "Broadcasts when monitored flashlight state, availability, intensity, conflict, or thermal restriction changes."))
	FOpenMobileFlashlightSnapshotChangedEvent OnFlashlightSnapshotChanged;

private:
	friend class UOpenMobileDeviceAsyncActionBase;
	friend class UOpenMobileDeviceStorageQueryAsyncAction;
	friend class UOpenMobileDeviceMonitoringSubscription;
	friend class UOpenMobilePreferredRefreshRateHandle;
	friend class UOpenMobileBrightnessHandle;
	friend class UOpenMobileKeepScreenAwakeHandle;
	friend class UOpenMobileSystemUiHandle;
	friend class UOpenMobileOrientationPolicyHandle;
	friend class FOpenMobileDeviceAsyncContractTest;
	friend class FOpenMobileDeviceStorageSpaceTest;
	friend class FOpenMobileDeviceLowStorageStateTest;

	void StopMonitoringSubscription(
		UOpenMobileDeviceMonitoringSubscription* Subscription
	);
	void ReleasePreferredRefreshRateHandle(
		UOpenMobilePreferredRefreshRateHandle* Handle
	);
	void ReleaseBrightnessHandle(UOpenMobileBrightnessHandle* Handle);
	void ReleaseKeepScreenAwakeHandle(
		UOpenMobileKeepScreenAwakeHandle* Handle
	);
	void ReleaseSystemUiHandle(UOpenMobileSystemUiHandle* Handle);
	void ReleaseOrientationPolicyHandle(
		UOpenMobileOrientationPolicyHandle* Handle
	);
	void BindMonitoringService();
	void UnbindMonitoringService();
	void HandleMonitoringGroupChanged(EOpenMobileDeviceMonitoringGroup Group);
	void HandleMonitoredNetworkPathChanged(
		const FOpenMobileNetworkPathSnapshot& Snapshot
	);
	void HandleMonitoringMaintenance();
	void HandleApplicationSettingsReturned();
	void PrimeMonitoringGroup(EOpenMobileDeviceMonitoringGroup Group);
	void RequestStorageRefreshForMonitoring();
	void HandleStorageMonitoringQueryTerminal(
		EOpenMobileDeviceAsyncTerminalState State,
		const FOpenMobileError& Error
	);
	void RegisterAsyncAction(UOpenMobileDeviceAsyncActionBase* Action);
	void UnregisterAsyncAction(UOpenMobileDeviceAsyncActionBase* Action);
	void CacheStorageSnapshot(
		const FOpenMobileStorageSnapshot& Snapshot,
		uint64 BackendGeneration
	);

	UPROPERTY(Transient)
	FOpenMobileDeviceStatus LatestStatus;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOpenMobileDeviceMonitoringSubscription>> MonitoringSubscriptions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOpenMobilePreferredRefreshRateHandle>>
		PreferredRefreshRateHandles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOpenMobileBrightnessHandle>> BrightnessHandles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOpenMobileKeepScreenAwakeHandle>>
		KeepScreenAwakeHandles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOpenMobileSystemUiHandle>> SystemUiHandles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UOpenMobileOrientationPolicyHandle>>
		OrientationPolicyHandles;

	TMap<EOpenMobileDeviceMonitoringGroup, int32> LocalMonitoringCounts;
	FDelegateHandle MonitoringChangedHandle;
	FDelegateHandle MonitoredNetworkChangedHandle;
	FDelegateHandle MonitoringMaintenanceHandle;
	FDelegateHandle ApplicationSettingsReturnedHandle;
	TOptional<FOpenMobileLocaleSnapshot> LastLocaleSnapshot;
	TOptional<FOpenMobilePowerSnapshot> LastPowerSnapshot;
	TOptional<FOpenMobileMediaVolumeSnapshot> LastMediaVolumeSnapshot;
	TOptional<FOpenMobileMemorySnapshot> LastMemorySnapshot;
	TOptional<FOpenMobileStorageSnapshot> LastStorageSnapshot;
	uint64 LastStorageBackendGeneration = 0;
	TWeakObjectPtr<UOpenMobileDeviceStorageQueryAsyncAction>
		ActiveStorageMonitoringQuery;
	bool bStorageMonitoringRefreshPending = false;
	TOptional<FOpenMobileNetworkPathSnapshot> LastNetworkSnapshot;
	TOptional<FOpenMobileWindowDisplaySnapshot> LastWindowSnapshot;
	TOptional<FOpenMobileAppearanceSnapshot> LastAppearanceSnapshot;
	TOptional<FOpenMobileAccessibilitySnapshot> LastAccessibilitySnapshot;
	TOptional<FOpenMobileFlashlightSnapshot> LastFlashlightSnapshot;
	FOpenMobileDeviceStatusChangedNativeEvent NativeDeviceStatusChanged;
	FOpenMobileApplicationSettingsReturnedNativeEvent
		NativeApplicationSettingsReturned;
	FOpenMobileLocaleSnapshotChangedNativeEvent NativeLocaleSnapshotChanged;
	FOpenMobilePowerSnapshotChangedNativeEvent NativePowerSnapshotChanged;
	FOpenMobileBatteryChangedNativeEvent NativeBatteryChanged;
	FOpenMobilePowerSavingModeChangedNativeEvent NativePowerSavingModeChanged;
	FOpenMobileThermalChangedNativeEvent NativeThermalChanged;
	FOpenMobileMediaVolumeSnapshotChangedNativeEvent
		NativeMediaVolumeSnapshotChanged;
	FOpenMobileMemorySnapshotChangedNativeEvent NativeMemorySnapshotChanged;
	FOpenMobileStorageSnapshotChangedNativeEvent NativeStorageSnapshotChanged;
	FOpenMobileNetworkPathSnapshotChangedNativeEvent NativeNetworkPathSnapshotChanged;
	FOpenMobileWindowDisplaySnapshotChangedNativeEvent NativeWindowDisplaySnapshotChanged;
	FOpenMobileWindowOrientationChangedNativeEvent NativeWindowOrientationChanged;
	FOpenMobileAppearanceSnapshotChangedNativeEvent NativeAppearanceSnapshotChanged;
	FOpenMobileAccessibilitySnapshotChangedNativeEvent
		NativeAccessibilitySnapshotChanged;
	FOpenMobileFlashlightSnapshotChangedNativeEvent
		NativeFlashlightSnapshotChanged;
	TSet<TWeakObjectPtr<UOpenMobileDeviceAsyncActionBase>> ActiveAsyncActions;
	bool bDeinitialized = false;
};
