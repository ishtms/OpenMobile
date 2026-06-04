#include "OpenMobileDeviceHdrInfo.h"

namespace OpenMobileDeviceHdrInfoPrivate
{
	EOpenMobileHdrType NormalizeType(EOpenMobileHdrType Type)
	{
		switch (Type)
		{
		case EOpenMobileHdrType::HDR10:
		case EOpenMobileHdrType::HDR10Plus:
		case EOpenMobileHdrType::HLG:
		case EOpenMobileHdrType::DolbyVision:
		case EOpenMobileHdrType::Other:
			return Type;
		case EOpenMobileHdrType::Unknown:
		default:
			return EOpenMobileHdrType::Other;
		}
	}

	FOpenMobileDeviceOptionalBool MakeOptional(const TOptional<bool>& Value)
	{
		return Value.IsSet()
			? FOpenMobileDeviceOptionalBool::MakeAvailable(Value.GetValue())
			: FOpenMobileDeviceOptionalBool();
	}
}

EOpenMobileHdrType FOpenMobileDeviceHdrInfo::FromAndroidType(int32 NativeType)
{
	switch (NativeType)
	{
	case 1:
		return EOpenMobileHdrType::DolbyVision;
	case 2:
		return EOpenMobileHdrType::HDR10;
	case 3:
		return EOpenMobileHdrType::HLG;
	case 4:
		return EOpenMobileHdrType::HDR10Plus;
	default:
		return EOpenMobileHdrType::Other;
	}
}

void FOpenMobileDeviceHdrInfo::Apply(
	FOpenMobileWindowDisplaySnapshot& Snapshot,
	const FOpenMobileDeviceHdrEvidence& Evidence
)
{
	using namespace OpenMobileDeviceHdrInfoPrivate;
	Snapshot.bHdrAvailable = MakeOptional(Evidence.bHdrAvailable);
	Snapshot.bSupportedHdrTypesAvailable =
		Evidence.bSupportedHdrTypesAvailable;
	Snapshot.SupportedHdrTypes.Reset();
	if (Evidence.bSupportedHdrTypesAvailable)
	{
		for (const EOpenMobileHdrType Type : Evidence.SupportedHdrTypes)
		{
			Snapshot.SupportedHdrTypes.AddUnique(NormalizeType(Type));
		}
		Snapshot.SupportedHdrTypes.Sort(
			[](EOpenMobileHdrType Left, EOpenMobileHdrType Right)
			{
				return static_cast<uint8>(Left) < static_cast<uint8>(Right);
			}
		);
	}
	Snapshot.bWideColorAvailable = MakeOptional(
		Evidence.bWideColorAvailable
	);
	Snapshot.bHdrOutputActive = MakeOptional(Evidence.bHdrOutputActive);
}
