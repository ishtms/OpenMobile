#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceCapabilities.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceMonitoring.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceResourceTypes.h"

enum class EOpenMobileDeviceMockEventDelivery : uint8
{
	Immediate,
	Delayed,
	Duplicate,
	OutOfOrder,
	OffThread,
	Stale
};

struct OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceMockState
{
	FOpenMobilePowerSnapshot Power;
	FOpenMobileMemorySnapshot Memory;
	FOpenMobileStorageSnapshot Storage;
	FOpenMobileNetworkPathSnapshot Network;
	FOpenMobileWindowDisplaySnapshot Window;
	FOpenMobileAppearanceSnapshot Appearance;
	FOpenMobileAccessibilitySnapshot Accessibility;
	TMap<FName, FOpenMobileError> Failures;
};

struct OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceMockScriptStep
{
	EOpenMobileDeviceMonitoringGroup Group =
		EOpenMobileDeviceMonitoringGroup::Power;
	EOpenMobileDeviceMockEventDelivery Delivery =
		EOpenMobileDeviceMockEventDelivery::Immediate;
	float DelaySeconds = 0.0f;
	FOpenMobileDeviceMockState State;
};

class OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceEditorMock final
{
public:
	static void Startup();
	static void Shutdown();
	static void ApplySettings();
	static void SetEnabled(bool bEnabled);
	static bool IsEnabled();
	static bool IsSelected();
	static void SetState(const FOpenMobileDeviceMockState& State);
	static const FOpenMobileDeviceMockState& GetState();
	static void QueueScriptStep(const FOpenMobileDeviceMockScriptStep& Step);
	static bool RunNextScriptStep();
	static int32 GetQueuedScriptStepCount();
	static void ResetOverrides();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
	static void TickForTests(float DeltaTime);
	static void FlushBackgroundEventsForTests();
#endif
};
