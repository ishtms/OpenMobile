#include "OpenMobileDevicePlatformInfo.h"

namespace OpenMobileDevicePlatformInfoPrivate
{
	void ParseVersionComponent(
		const FString& Version,
		int32& Offset,
		FOpenMobileDeviceOptionalInt32& OutComponent
	)
	{
		const int32 Start = Offset;
		while (Offset < Version.Len() && FChar::IsDigit(Version[Offset]))
		{
			++Offset;
		}
		if (Offset == Start)
		{
			return;
		}

		const int64 Value = FCString::Atoi64(
			*Version.Mid(Start, Offset - Start)
		);
		if (Value <= MAX_int32)
		{
			OutComponent = FOpenMobileDeviceOptionalInt32::MakeAvailable(
				static_cast<int32>(Value)
			);
		}
	}

	FString GetPlatformLabel(EOpenMobileDevicePlatform Platform)
	{
		switch (Platform)
		{
		case EOpenMobileDevicePlatform::Android:
			return TEXT("Android");
		case EOpenMobileDevicePlatform::IOS:
			return TEXT("iOS");
		case EOpenMobileDevicePlatform::Unknown:
			return {};
		}
		return {};
	}

	FOpenMobileDeviceOptionalString MakeOptionalText(const FString& Value)
	{
		const FString Trimmed = Value.TrimStartAndEnd();
		return Trimmed.IsEmpty()
			? FOpenMobileDeviceOptionalString()
			: FOpenMobileDeviceOptionalString::MakeAvailable(Trimmed);
	}
}

FOpenMobileDeviceInformationSnapshot
FOpenMobileDevicePlatformInfo::BuildSnapshot(
	EOpenMobileDevicePlatform Platform,
	const FString& RawOsVersion,
	int32 AndroidApiLevel,
	const FString& Manufacturer,
	const FString& Brand,
	const FString& Model
)
{
	using namespace OpenMobileDevicePlatformInfoPrivate;
	FOpenMobileDeviceInformationSnapshot Snapshot;
	Snapshot.Platform = Platform;
	const FString TrimmedVersion = RawOsVersion.TrimStartAndEnd();
	if (!TrimmedVersion.IsEmpty())
	{
		Snapshot.RawOsVersion =
			FOpenMobileDeviceOptionalString::MakeAvailable(RawOsVersion);
		const FString PlatformLabel = GetPlatformLabel(Platform);
		Snapshot.ReadableOsVersion = FOpenMobileDeviceOptionalString::MakeAvailable(
			PlatformLabel.IsEmpty()
				? TrimmedVersion
				: FString::Printf(TEXT("%s %s"), *PlatformLabel, *TrimmedVersion)
		);

		int32 Offset = 0;
		ParseVersionComponent(
			TrimmedVersion,
			Offset,
			Snapshot.OsVersionMajor
		);
		if (Snapshot.OsVersionMajor.bIsAvailable
			&& Offset < TrimmedVersion.Len()
			&& TrimmedVersion[Offset] == TEXT('.'))
		{
			++Offset;
			ParseVersionComponent(
				TrimmedVersion,
				Offset,
				Snapshot.OsVersionMinor
			);
		}
		if (Snapshot.OsVersionMinor.bIsAvailable
			&& Offset < TrimmedVersion.Len()
			&& TrimmedVersion[Offset] == TEXT('.'))
		{
			++Offset;
			ParseVersionComponent(
				TrimmedVersion,
				Offset,
				Snapshot.OsVersionPatch
			);
		}
	}

	if (Platform == EOpenMobileDevicePlatform::Android && AndroidApiLevel > 0)
	{
		Snapshot.AndroidApiLevel =
			FOpenMobileDeviceOptionalInt32::MakeAvailable(AndroidApiLevel);
	}
	Snapshot.Manufacturer = MakeOptionalText(Manufacturer);
	Snapshot.Brand = MakeOptionalText(Brand);
	Snapshot.Model = MakeOptionalText(Model);
	return Snapshot;
}
