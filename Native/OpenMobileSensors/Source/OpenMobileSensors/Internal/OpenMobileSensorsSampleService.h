#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorSamples.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsSampleService final
{
public:
	static void Start();
	static void BeginShutdown();
	static void RegisterSubscription(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Options
	);
	static void SetSubscriptionState(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		EOpenMobileSensorSubscriptionState State
	);
	static void UpdateSubscriptionOptions(
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileSensorStreamOptions& Options
	);
	static void UnregisterSubscription(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);
	static void UnregisterAll();

	static void PublishVector(const FOpenMobileVectorSensorSample& Sample);
	static void PublishAttitude(const FOpenMobileAttitudeSensorSample& Sample);
	static void PublishScalar(const FOpenMobileScalarSensorSample& Sample);
	static void PublishHeading(const FOpenMobileHeadingSensorSample& Sample);
	static void PublishSteps(const FOpenMobileStepsSensorSample& Sample);
	static void PublishActivity(const FOpenMobileActivitySensorSample& Sample);
	static void PublishOrientation(
		const FOpenMobileOrientationSensorSample& Sample
	);
	static void PublishProximity(const FOpenMobileProximitySensorSample& Sample);

	static bool ReadLatestVector(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileVectorSensorSample& OutSample
	);
	static bool ReadLatestAttitude(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileAttitudeSensorSample& OutSample
	);
	static bool ReadLatestScalar(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileScalarSensorSample& OutSample
	);
	static bool ReadLatestHeading(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileHeadingSensorSample& OutSample
	);
	static bool ReadLatestSteps(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileStepsSensorSample& OutSample
	);
	static bool ReadLatestActivity(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileActivitySensorSample& OutSample
	);
	static bool ReadLatestOrientation(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileOrientationSensorSample& OutSample
	);
	static bool ReadLatestProximity(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		FOpenMobileProximitySensorSample& OutSample
	);

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};
