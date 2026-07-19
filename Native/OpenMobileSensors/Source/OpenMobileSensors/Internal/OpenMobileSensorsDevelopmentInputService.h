#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorsDevelopmentInput.h"

class IOpenMobileSensorsDevelopmentInputProvider
{
public:
	virtual ~IOpenMobileSensorsDevelopmentInputProvider() = default;

	virtual FOpenMobileSensorOperationResult ApplyInput(
		const FOpenMobileSensorsMockInput& Input
	) = 0;
	virtual FOpenMobileSensorOperationResult ApplyPreset(
		EOpenMobileSensorsMockPreset Preset
	) = 0;
	virtual FOpenMobileSensorOperationResult PlayTimeline(
		const FOpenMobileSensorsMockTimeline& Timeline
	) = 0;
	virtual FOpenMobileSensorOperationResult StopTimeline() = 0;
	virtual FOpenMobileSensorOperationResult AdvanceTimeline(
		double DeltaSeconds
	) = 0;
	virtual FOpenMobileSensorOperationResult InjectError(
		EOpenMobileSensorType Sensor,
		EOpenMobileSensorFailureReason FailureReason,
		const FString& NativeCode
	) = 0;
	virtual bool IsActive() const = 0;
};

class OPENMOBILESENSORS_API FOpenMobileSensorsDevelopmentInputService final
{
public:
	static bool RegisterProvider(
		IOpenMobileSensorsDevelopmentInputProvider& Provider
	);
	static bool UnregisterProvider(
		IOpenMobileSensorsDevelopmentInputProvider& Provider
	);
	static IOpenMobileSensorsDevelopmentInputProvider* GetProvider();
};
