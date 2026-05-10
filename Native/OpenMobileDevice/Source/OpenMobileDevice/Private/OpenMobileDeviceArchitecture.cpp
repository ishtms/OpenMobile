#include "OpenMobileDeviceArchitecture.h"

namespace OpenMobileDeviceArchitecturePrivate
{
	FString Normalize(const FString& Value)
	{
		FString Normalized = Value.TrimStartAndEnd();
		Normalized.ToLowerInline();
		return Normalized;
	}
}

void FOpenMobileDeviceArchitecture::Apply(
	FOpenMobileDeviceInformationSnapshot& Snapshot,
	const FString& ProcessArchitecture,
	TConstArrayView<FString> SupportedAbis,
	bool bSupportedAbisAvailable
)
{
	using namespace OpenMobileDeviceArchitecturePrivate;
	const FString NormalizedProcess = Normalize(ProcessArchitecture);
	Snapshot.ProcessArchitecture = NormalizedProcess.IsEmpty()
		? FOpenMobileDeviceOptionalString()
		: FOpenMobileDeviceOptionalString::MakeAvailable(NormalizedProcess);
	Snapshot.bSupportedAbisAvailable = bSupportedAbisAvailable;
	Snapshot.SupportedAbis.Reset();
	if (!bSupportedAbisAvailable)
	{
		return;
	}

	TSet<FString> Seen;
	for (const FString& SupportedAbi : SupportedAbis)
	{
		FString Normalized = Normalize(SupportedAbi);
		if (!Normalized.IsEmpty() && !Seen.Contains(Normalized))
		{
			Seen.Add(Normalized);
			Snapshot.SupportedAbis.Add(MoveTemp(Normalized));
		}
	}
}
