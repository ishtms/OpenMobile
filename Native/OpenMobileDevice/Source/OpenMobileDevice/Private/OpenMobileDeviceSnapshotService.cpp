#include "OpenMobileDeviceSnapshotService.h"

#include "IOpenMobileDeviceBackend.h"
#include "OpenMobileDeviceBackendRegistry.h"

namespace OpenMobileDeviceSnapshotServicePrivate
{
	int64 SnapshotGeneration = 0;
	uint64 CachedDeviceInformationBackendGeneration = 0;
	TOptional<FOpenMobileDeviceInformationSnapshot> CachedDeviceInformation;

	int64 NextGeneration()
	{
		if (SnapshotGeneration == MAX_int64)
		{
			SnapshotGeneration = 1;
		}
		else
		{
			++SnapshotGeneration;
		}
		return SnapshotGeneration;
	}

	template <typename SnapshotType, typename QueryType>
	SnapshotType Capture(QueryType&& Query)
	{
		check(IsInGameThread());
		SnapshotType Snapshot;
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Snapshot = Query(*Backend);
		}
		Snapshot.Metadata.CapturedAtUtc = FDateTime::UtcNow();
		Snapshot.Metadata.Generation = NextGeneration();
		return Snapshot;
	}

	FOpenMobileDeviceInformationSnapshot CaptureDeviceInformation()
	{
		check(IsInGameThread());
		IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend();
		const FOpenMobileDeviceCallbackToken Token = Backend
			? FOpenMobileDeviceBackendRegistry::CaptureCallbackToken()
			: FOpenMobileDeviceCallbackToken();
		if (!CachedDeviceInformation.IsSet()
			|| CachedDeviceInformationBackendGeneration != Token.Generation)
		{
			CachedDeviceInformation = Backend
				? Backend->GetDeviceInformationSnapshot()
				: FOpenMobileDeviceInformationSnapshot();
			CachedDeviceInformation->Metadata = {};
			CachedDeviceInformationBackendGeneration = Token.Generation;
		}

		FOpenMobileDeviceInformationSnapshot Snapshot =
			CachedDeviceInformation.GetValue();
		Snapshot.FormFactor = Backend
			? Backend->GetDeviceFormFactor()
			: EOpenMobileDeviceFormFactor::Unknown;
		Snapshot.Metadata.CapturedAtUtc = FDateTime::UtcNow();
		Snapshot.Metadata.Generation = NextGeneration();
		return Snapshot;
	}
}

FOpenMobileDeviceInformationSnapshot
FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::CaptureDeviceInformation();
}

FOpenMobileApplicationMetadataSnapshot
FOpenMobileDeviceSnapshotService::GetApplicationMetadataSnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<
		FOpenMobileApplicationMetadataSnapshot
	>([](const IOpenMobileDeviceBackend& Backend)
	{
		return Backend.GetApplicationMetadataSnapshot();
	});
}

FOpenMobileLocaleSnapshot FOpenMobileDeviceSnapshotService::GetLocaleSnapshot()
{
	return GetLocaleSnapshotAtUtc(FDateTime::UtcNow());
}

FOpenMobileLocaleSnapshot FOpenMobileDeviceSnapshotService::GetLocaleSnapshotAtUtc(
	const FDateTime& UtcInstant
)
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<FOpenMobileLocaleSnapshot>(
		[&UtcInstant](const IOpenMobileDeviceBackend& Backend)
		{
			return Backend.GetLocaleSnapshotAtUtc(UtcInstant);
		}
	);
}

FOpenMobilePowerSnapshot FOpenMobileDeviceSnapshotService::GetPowerSnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<FOpenMobilePowerSnapshot>(
		[](const IOpenMobileDeviceBackend& Backend)
		{
			return Backend.GetPowerSnapshot();
		}
	);
}

FOpenMobileMediaVolumeSnapshot
FOpenMobileDeviceSnapshotService::GetMediaVolumeSnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<
		FOpenMobileMediaVolumeSnapshot
	>([](const IOpenMobileDeviceBackend& Backend)
	{
		return Backend.GetMediaVolumeSnapshot();
	});
}

FOpenMobileMemorySnapshot FOpenMobileDeviceSnapshotService::GetMemorySnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<FOpenMobileMemorySnapshot>(
		[](const IOpenMobileDeviceBackend& Backend)
		{
			return Backend.GetMemorySnapshot();
		}
	);
}

FOpenMobileStorageSnapshot FOpenMobileDeviceSnapshotService::GetStorageSnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<FOpenMobileStorageSnapshot>(
		[](const IOpenMobileDeviceBackend& Backend)
		{
			return Backend.GetStorageSnapshot();
		}
	);
}

FOpenMobileNetworkPathSnapshot
FOpenMobileDeviceSnapshotService::GetNetworkPathSnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<
		FOpenMobileNetworkPathSnapshot
	>([](const IOpenMobileDeviceBackend& Backend)
	{
		return Backend.GetNetworkPathSnapshot();
	});
}

FOpenMobileWindowDisplaySnapshot
FOpenMobileDeviceSnapshotService::GetWindowDisplaySnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<
		FOpenMobileWindowDisplaySnapshot
	>([](const IOpenMobileDeviceBackend& Backend)
	{
		return Backend.GetWindowDisplaySnapshot();
	});
}

FOpenMobileAppearanceSnapshot FOpenMobileDeviceSnapshotService::GetAppearanceSnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<FOpenMobileAppearanceSnapshot>(
		[](const IOpenMobileDeviceBackend& Backend)
		{
			return Backend.GetAppearanceSnapshot();
		}
	);
}

FOpenMobileAccessibilitySnapshot
FOpenMobileDeviceSnapshotService::GetAccessibilitySnapshot()
{
	return OpenMobileDeviceSnapshotServicePrivate::Capture<
		FOpenMobileAccessibilitySnapshot
	>([](const IOpenMobileDeviceBackend& Backend)
	{
		return Backend.GetAccessibilitySnapshot();
	});
}
