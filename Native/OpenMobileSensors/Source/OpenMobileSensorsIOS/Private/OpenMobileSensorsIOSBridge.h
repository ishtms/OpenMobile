#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"
#include "OpenMobilePermissionTypes.h"
#include "OpenMobileSensorsBackendRegistry.h"

class FOpenMobileSensorsIOSBackend;

enum class EOpenMobileSensorsIOSBridgeFailure : uint8
{
	None,
	InvalidArgument,
	SensorUnavailable,
	ServiceTemporarilyUnavailable,
	ReferenceFrameUnavailable,
	PermissionDenied,
	PermissionRestricted,
	MissingUsageDescription,
	ManagerError,
	Paused,
	ShuttingDown,
	StreamMissing
};

struct FOpenMobileSensorsIOSAvailability
{
	bool bAccelerometer = false;
	bool bGyroscope = false;
	bool bMagnetometer = false;
	bool bDeviceMotion = false;
	bool bRelativeAltitude = false;
	bool bAbsoluteAltitudeApiSupported = false;
	bool bAbsoluteAltitude = false;
	bool bProximityApiSupported = false;
	bool bMagneticNorthReference = false;
	bool bPedometerApiSupported = false;
	bool bStepCounting = false;
	bool bMotionActivityApiSupported = false;
	bool bMotionActivity = false;
	EOpenMobilePermissionStatus PedometerAuthorizationStatus =
		EOpenMobilePermissionStatus::NotDetermined;
	EOpenMobilePermissionStatus MotionActivityAuthorizationStatus =
		EOpenMobilePermissionStatus::NotDetermined;
};

struct FOpenMobileSensorsIOSBridgeResult
{
	EOpenMobileSensorsIOSBridgeFailure Failure =
		EOpenMobileSensorsIOSBridgeFailure::None;
	double AppliedFrequencyHz = 0.0;
	FString NativeDomain;
	FString NativeCode;

	bool IsSuccess() const
	{
		return Failure == EOpenMobileSensorsIOSBridgeFailure::None;
	}
};

class FOpenMobileSensorsIOSBridge final
{
public:
	explicit FOpenMobileSensorsIOSBridge(
		FOpenMobileSensorsIOSBackend& InBackend
	);
	~FOpenMobileSensorsIOSBridge();

	static FOpenMobileSensorsIOSAvailability QuerySystemAvailability();
	FOpenMobileSensorsIOSAvailability QueryAvailability() const;
	FOpenMobileSensorsIOSBridgeResult StartStream(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorPhysicalStreamRequest& Request
	);
	FOpenMobileSensorsIOSBridgeResult ReconfigureStream(
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileSensorPhysicalStreamRequest& Request
	);
	FOpenMobileSensorsIOSBridgeResult StopStream(
		const FOpenMobileSensorBackendStreamHandle& Handle
	);
	FOpenMobileSensorsIOSBridgeResult QueryNativeStepCount(
		const FGuid& RequestId,
		const FOpenMobileNativeStepCountQuery& Query,
		FOnOpenMobileNativeStepCountBackendQueryComplete&& Completion
	);
	bool CancelNativeStepCountQuery(const FGuid& RequestId);
	void Shutdown();

private:
	class FImpl;
	TUniquePtr<FImpl> Impl;
};
