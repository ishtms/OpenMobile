#include "OpenMobileSensorsIOSBackend.h"

#import <Foundation/Foundation.h>

#include "Misc/ScopeLock.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsSampleService.h"

FOpenMobileSensorsIOSBackend::~FOpenMobileSensorsIOSBackend()
{
	StopMotionQueue();
}

FName FOpenMobileSensorsIOSBackend::GetBackendName() const
{
	return TEXT("IOS");
}

FOpenMobileCapability FOpenMobileSensorsIOSBackend::GetBackendCapability() const
{
	FOpenMobileCapability Capability;
	Capability.Name = GetModularFeatureName();
	Capability.State = EOpenMobileCapabilityState::Available;
	Capability.Detail = TEXT("The iOS Sensors backend is registered.");
	return Capability;
}

bool FOpenMobileSensorsIOSBackend::PublishVectorBatchFromMotionQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileVectorSensorBatch& Batch
)
{
	if (!EnsureMotionQueue())
	{
		return false;
	}
	return FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		Handle,
		Batch
	);
}

void FOpenMobileSensorsIOSBackend::BeginShutdown()
{
	bShuttingDown.Store(true);
	StopMotionQueue();
}

bool FOpenMobileSensorsIOSBackend::EnsureMotionQueue()
{
	FScopeLock Lock(&MotionQueueMutex);
	if (bShuttingDown.Load())
	{
		return false;
	}
	if (MotionQueue)
	{
		return true;
	}
	MotionQueue = [[NSOperationQueue alloc] init];
	MotionQueue.name = @"OpenMobileSensorsMotionQueue";
	MotionQueue.maxConcurrentOperationCount = 1;
	MotionQueue.qualityOfService = NSQualityOfServiceUserInitiated;
	return MotionQueue != nil;
}

void FOpenMobileSensorsIOSBackend::StopMotionQueue()
{
	NSOperationQueue* Queue = nil;
	{
		FScopeLock Lock(&MotionQueueMutex);
		Queue = MotionQueue;
		MotionQueue = nil;
	}
	if (!Queue)
	{
		return;
	}
	[Queue cancelAllOperations];
	[Queue waitUntilAllOperationsAreFinished];
#if !__has_feature(objc_arc)
	[Queue release];
#endif
}
