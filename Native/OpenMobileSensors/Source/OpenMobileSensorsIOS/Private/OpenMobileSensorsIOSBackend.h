#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"

struct FOpenMobileSensorsBackendToken;

#if __OBJC__
@class NSOperationQueue;
#else
class NSOperationQueue;
#endif

class FOpenMobileSensorsIOSBackend final : public IOpenMobileSensorsBackend
{
public:
	virtual ~FOpenMobileSensorsIOSBackend() override;
	virtual FName GetBackendName() const override;
	virtual FOpenMobileCapability GetBackendCapability() const override;
	virtual void BeginShutdown() override;
	static double ConvertCoreMotionTimestampSeconds(double TimestampSeconds);
	static bool CaptureApplicationWindowRotationFromMainThread(
		const FGuid& OwnerIdentifier,
		EOpenMobileSensorScreenRotation Rotation,
		double TimestampSeconds,
		bool bNaturalOrientationLandscape
	);

	bool PublishVectorBatchFromMotionQueue(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileVectorSensorBatch& Batch
	);

private:
	bool EnsureMotionQueue();
	void StopMotionQueue();

	FCriticalSection MotionQueueMutex;
	NSOperationQueue* MotionQueue = nullptr;
	TAtomic<bool> bShuttingDown = false;
};
