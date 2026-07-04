#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/CoreRedirects.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceSampleLegacyRedirectTest,
	"OpenMobile.Device.Sample.LegacyRedirects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceSampleLegacyRedirectTest::RunTest(const FString& Parameters)
{
	struct FRedirectCase
	{
		ECoreRedirectFlags Type;
		const TCHAR* OldName;
		const TCHAR* NewName;
	};

	const FRedirectCase Cases[] = {
		{
			ECoreRedirectFlags::Type_Class,
			TEXT("/Script/MobilePhotoPicker.MobileDeviceStatusSubsystem"),
			TEXT("/Script/OpenMobileDevice.OpenMobileDeviceSubsystem")
		},
		{
			ECoreRedirectFlags::Type_Struct,
			TEXT("/Script/MobilePhotoPicker.MobileDeviceStatus"),
			TEXT("/Script/OpenMobileDevice.OpenMobileDeviceStatus")
		},
		{
			ECoreRedirectFlags::Type_Function,
			TEXT("/Script/MobilePhotoPicker.MobilePhotoPickerBlueprintLibrary.GetBatteryPercent"),
			TEXT("/Script/OpenMobileDevice.OpenMobileDeviceBlueprintLibrary.GetBatteryPercent")
		},
		{
			ECoreRedirectFlags::Type_Function,
			TEXT("/Script/MobilePhotoPicker.MobilePhotoPickerBlueprintLibrary.GetVolumePercent"),
			TEXT("/Script/OpenMobileDevice.OpenMobileDeviceBlueprintLibrary.GetVolumePercent")
		},
		{
			ECoreRedirectFlags::Type_Function,
			TEXT("/Script/MobilePhotoPicker.MobilePhotoPickerBlueprintLibrary.GetDeviceStatus"),
			TEXT("/Script/OpenMobileDevice.OpenMobileDeviceBlueprintLibrary.GetDeviceStatus")
		}
	};

	for (const FRedirectCase& Case : Cases)
	{
		const FString Redirected = FCoreRedirects::GetRedirectedName(
			Case.Type,
			FCoreRedirectObjectName(Case.OldName)
		).ToString();
		const FString Expected = FCoreRedirectObjectName(Case.NewName).ToString();
		TestEqual(Case.OldName, Redirected, Expected);
	}
	return true;
}

#endif
