#include "OpenMobileDeviceApplicationInfo.h"

namespace OpenMobileDeviceApplicationInfoPrivate
{
	FOpenMobileDeviceOptionalString MakeOptionalText(const FString& Value)
	{
		const FString Trimmed = Value.TrimStartAndEnd();
		return Trimmed.IsEmpty()
			? FOpenMobileDeviceOptionalString()
			: FOpenMobileDeviceOptionalString::MakeAvailable(Trimmed);
	}

	EOpenMobileBuildConfiguration NormalizeBuildConfiguration(
		EBuildConfiguration BuildConfiguration
	)
	{
		switch (BuildConfiguration)
		{
		case EBuildConfiguration::Debug:
		case EBuildConfiguration::DebugGame:
			return EOpenMobileBuildConfiguration::Debug;
		case EBuildConfiguration::Development:
			return EOpenMobileBuildConfiguration::Development;
		case EBuildConfiguration::Test:
			return EOpenMobileBuildConfiguration::Test;
		case EBuildConfiguration::Shipping:
			return EOpenMobileBuildConfiguration::Shipping;
		case EBuildConfiguration::Unknown:
			return EOpenMobileBuildConfiguration::Unknown;
		}
		return EOpenMobileBuildConfiguration::Unknown;
	}
}

FOpenMobileApplicationMetadataSnapshot FOpenMobileDeviceApplicationInfo::Build(
	const FString& DisplayName,
	const FString& PackageIdentifier,
	const FString& VersionName,
	const FString& BuildNumber,
	EBuildConfiguration BuildConfiguration
)
{
	using namespace OpenMobileDeviceApplicationInfoPrivate;
	FOpenMobileApplicationMetadataSnapshot Snapshot;
	Snapshot.DisplayName = MakeOptionalText(DisplayName);
	Snapshot.PackageIdentifier = MakeOptionalText(PackageIdentifier);
	Snapshot.VersionName = MakeOptionalText(VersionName);
	Snapshot.BuildNumber = MakeOptionalText(BuildNumber);
	Snapshot.BuildConfiguration = NormalizeBuildConfiguration(
		BuildConfiguration
	);
	return Snapshot;
}
