#include "OpenMobileSensorsIOSBackend.h"

#import <Foundation/Foundation.h>

#include "Misc/ScopeLock.h"
#include "OpenMobileSensorAccuracyMapper.h"
#include "OpenMobileSensorCoordinates.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorTimestamp.h"
#include "OpenMobileSensorUnits.h"
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

double FOpenMobileSensorsIOSBackend::ConvertCoreMotionTimestampSeconds(
	double TimestampSeconds
)
{
	return FOpenMobileSensorTimestampConverter::FromIOSCoreMotionSeconds(
		TimestampSeconds
	);
}

bool FOpenMobileSensorsIOSBackend::
CaptureApplicationWindowRotationFromMainThread(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorScreenRotation Rotation,
	double TimestampSeconds,
	bool bNaturalOrientationLandscape
)
{
	return FOpenMobileSensorsScreenRotationService::
		CaptureApplicationWindowRotation(
			OwnerIdentifier,
			Rotation,
			TimestampSeconds,
			bNaturalOrientationLandscape
		);
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
	FOpenMobileVectorSensorBatch NormalizedBatch = Batch;
	for (FOpenMobileVectorSensorSample& Sample : NormalizedBatch.Samples)
	{
		if (Sample.Header.SourceFlags == 0)
		{
			Sample.Header.SourceFlags =
				FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
					Sample.Header.Sensor.Type
				);
		}
		FOpenMobileSensorUnitConverter::NormalizeVectorSample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample
		);
		FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample
		);
	}
	return FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		Handle,
		NormalizedBatch
	);
}

bool FOpenMobileSensorsIOSBackend::
PublishMagneticFieldAccuracyFromMotionQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorIdentifier& Sensor,
	int32 NativeAccuracy,
	double TimestampSeconds
)
{
	if (!EnsureMotionQueue())
	{
		return false;
	}
	return FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
		Token,
		Handle,
		FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
			Sensor,
			NativeAccuracy,
			TimestampSeconds
		)
	);
}

bool FOpenMobileSensorsIOSBackend::
PublishHeadingAccuracyFromLocationCallback(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorIdentifier& Sensor,
	double AccuracyDegrees,
	bool bCalibrationRequired,
	double TimestampSeconds
)
{
	return FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
		Token,
		Handle,
		FOpenMobileSensorAccuracyMapper::FromIOSHeadingAccuracy(
			Sensor,
			AccuracyDegrees,
			bCalibrationRequired,
			TimestampSeconds
		)
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
