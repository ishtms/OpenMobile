#include "OpenMobileDeviceEmulatorDetection.h"

namespace OpenMobileDeviceEmulatorDetectionPrivate
{
	bool ContainsAny(const FString& Value, TConstArrayView<const TCHAR*> Terms)
	{
		for (const TCHAR* Term : Terms)
		{
			if (Value.Contains(Term, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}

void FOpenMobileDeviceEmulatorDetection::ApplyAndroid(
	FOpenMobileDeviceInformationSnapshot& Snapshot,
	const FOpenMobileDeviceAndroidEmulatorEvidence& Evidence
)
{
	using namespace OpenMobileDeviceEmulatorDetectionPrivate;
	TArray<FString> Matches;
	if (ContainsAny(
		Evidence.Fingerprint,
		{TEXT("generic"), TEXT("unknown")}
	))
	{
		Matches.Add(TEXT("fingerprint"));
	}
	if (ContainsAny(
		Evidence.Model,
		{TEXT("google_sdk"), TEXT("emulator"), TEXT("android sdk built for")}
	))
	{
		Matches.Add(TEXT("model"));
	}
	if (ContainsAny(Evidence.Manufacturer, {TEXT("genymotion")}))
	{
		Matches.Add(TEXT("manufacturer"));
	}
	if (ContainsAny(
		Evidence.Hardware,
		{TEXT("goldfish"), TEXT("ranchu"), TEXT("vbox86")}
	))
	{
		Matches.Add(TEXT("hardware"));
	}
	if (ContainsAny(
		Evidence.Product,
		{TEXT("sdk_gphone"), TEXT("google_sdk"), TEXT("emulator"), TEXT("simulator"), TEXT("vbox86")}
	))
	{
		Matches.Add(TEXT("product"));
	}
	if (Evidence.Brand.StartsWith(TEXT("generic"), ESearchCase::IgnoreCase)
		&& Evidence.Device.StartsWith(
			TEXT("generic"),
			ESearchCase::IgnoreCase
		))
	{
		Matches.Add(TEXT("brand and device"));
	}

	const bool bProbablyEmulator = Matches.Num() >= 2;
	Snapshot.bProbablyEmulator =
		FOpenMobileDeviceOptionalBool::MakeAvailable(bProbablyEmulator);
	Snapshot.EmulatorConfidence = Matches.IsEmpty()
		? EOpenMobileDeviceEmulatorConfidence::NoEvidence
		: Matches.Num() == 1
			? EOpenMobileDeviceEmulatorConfidence::Possible
			: EOpenMobileDeviceEmulatorConfidence::Likely;
	Snapshot.EmulatorReason = Matches.IsEmpty()
		? FOpenMobileDeviceOptionalString()
		: FOpenMobileDeviceOptionalString::MakeAvailable(
			FString::Printf(
				TEXT("Matched Android emulator traits: %s."),
				*FString::Join(Matches, TEXT(", "))
			)
		);
}

void FOpenMobileDeviceEmulatorDetection::ApplyIOS(
	FOpenMobileDeviceInformationSnapshot& Snapshot,
	bool bIsSimulator
)
{
	Snapshot.bProbablyEmulator =
		FOpenMobileDeviceOptionalBool::MakeAvailable(bIsSimulator);
	Snapshot.EmulatorConfidence = bIsSimulator
		? EOpenMobileDeviceEmulatorConfidence::Confirmed
		: EOpenMobileDeviceEmulatorConfidence::NoEvidence;
	Snapshot.EmulatorReason = bIsSimulator
		? FOpenMobileDeviceOptionalString::MakeAvailable(
			TEXT("Compiled for iOS Simulator.")
		)
		: FOpenMobileDeviceOptionalString();
}
