#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"
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
	void Shutdown();

private:
	class FImpl;
	TUniquePtr<FImpl> Impl;
};
