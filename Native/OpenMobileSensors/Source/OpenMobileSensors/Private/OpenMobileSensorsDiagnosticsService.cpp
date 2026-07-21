#include "OpenMobileSensorsDiagnosticsService.h"

#include "OpenMobilePermissions.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMetadataService.h"
#include "OpenMobileSensorsRecordingService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsDiagnosticsServicePrivate
{
	bool SensorLess(
		const FOpenMobileSensorIdentifier& Left,
		const FOpenMobileSensorIdentifier& Right
	)
	{
		if (Left.Type != Right.Type)
		{
			return static_cast<uint8>(Left.Type)
				< static_cast<uint8>(Right.Type);
		}
		return Left.InstanceId.LexicalLess(Right.InstanceId);
	}
}

FOpenMobileSensorDiagnosticsSnapshot
FOpenMobileSensorsDiagnosticsService::Capture(const FGuid* OwnerIdentifier)
{
	check(IsInGameThread());
	FOpenMobileSensorDiagnosticsSnapshot Snapshot;
	Snapshot.CapturedAtUtc = FDateTime::UtcNow().ToIso8601();
	const FOpenMobileSensorCapabilitySnapshot Capabilities =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	Snapshot.BackendName = Capabilities.BackendName;
	Snapshot.BackendAvailability = Capabilities.BackendAvailability;
	Snapshot.BackendGeneration = Capabilities.BackendGeneration;
	Snapshot.Capabilities = Capabilities.Sensors;
	Snapshot.Metadata = FOpenMobileSensorsMetadataService::GetMetadata();
	for (const EOpenMobileSensorPermission Permission : {
		EOpenMobileSensorPermission::MotionActivity,
		EOpenMobileSensorPermission::ActivityRecognition,
		EOpenMobileSensorPermission::TrueHeadingLocation
	})
	{
		const FName PermissionName =
			FOpenMobileSensorPermissions::GetPermissionName(Permission);
		Snapshot.Permissions.Add(
			FOpenMobileSensorPermissions::Describe(
				Permission,
				FOpenMobilePermissions::GetStatus(PermissionName).Status
			)
		);
	}
	Snapshot.Streams = OwnerIdentifier
		? FOpenMobileSensorsSubscriptionService::GetStreamDiagnostics(
			*OwnerIdentifier
		)
		: FOpenMobileSensorsSubscriptionService::GetAllStreamDiagnostics();
	Snapshot.PhysicalStreams =
		FOpenMobileSensorsSubscriptionService::GetPhysicalStreamDiagnostics(
			OwnerIdentifier
		);
	Snapshot.RecentErrors =
		FOpenMobileSensorsSubscriptionService::GetRecentErrors(OwnerIdentifier);
	Snapshot.RecentErrorReports =
		FOpenMobileSensorsSubscriptionService::GetRecentErrorReports(
			OwnerIdentifier
		);
	FOpenMobileSensorsRecordingService::GetActiveOperationCounts(
		OwnerIdentifier,
		Snapshot.ActiveRecordingCount,
		Snapshot.ActiveReplayCount
	);
	Snapshot.Capabilities.Sort([](
		const FOpenMobileSensorCapability& Left,
		const FOpenMobileSensorCapability& Right
	)
	{
		return OpenMobileSensorsDiagnosticsServicePrivate::SensorLess(
			Left.Sensor,
			Right.Sensor
		);
	});
	Snapshot.Metadata.Sort([](
		const FOpenMobileSensorMetadata& Left,
		const FOpenMobileSensorMetadata& Right
	)
	{
		return OpenMobileSensorsDiagnosticsServicePrivate::SensorLess(
			Left.Sensor,
			Right.Sensor
		);
	});
	Snapshot.Streams.Sort([](
		const FOpenMobileSensorStreamDiagnostics& Left,
		const FOpenMobileSensorStreamDiagnostics& Right
	)
	{
		if (Left.Subscription.Sensor != Right.Subscription.Sensor)
		{
			return OpenMobileSensorsDiagnosticsServicePrivate::SensorLess(
				Left.Subscription.Sensor,
				Right.Subscription.Sensor
			);
		}
		return Left.Subscription.Handle.GetIdentifier().ToString()
			< Right.Subscription.Handle.GetIdentifier().ToString();
	});
	Snapshot.PhysicalStreams.Sort([](
		const FOpenMobileSensorPhysicalStreamDiagnostics& Left,
		const FOpenMobileSensorPhysicalStreamDiagnostics& Right
	)
	{
		if (Left.Sensor != Right.Sensor)
		{
			return OpenMobileSensorsDiagnosticsServicePrivate::SensorLess(
				Left.Sensor,
				Right.Sensor
			);
		}
		return static_cast<uint8>(Left.AttitudeReferenceFrame)
			< static_cast<uint8>(Right.AttitudeReferenceFrame);
	});
	return Snapshot;
}
