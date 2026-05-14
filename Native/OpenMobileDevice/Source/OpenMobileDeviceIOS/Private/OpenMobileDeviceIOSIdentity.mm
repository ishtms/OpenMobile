#include "OpenMobileDeviceIOSIdentity.h"
#include "OpenMobileDeviceFormFactor.h"
#include "OpenMobileDeviceEmulatorDetection.h"

#include <sys/sysctl.h>

#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

FString GetOpenMobileDeviceIOSModel()
{
	@autoreleasepool
	{
		return FString([[UIDevice currentDevice] model]);
	}
}

FString GetOpenMobileDeviceIOSHardwareModel()
{
	@autoreleasepool
	{
#if TARGET_OS_SIMULATOR
		NSString* SimulatorModel =
			[[NSProcessInfo processInfo] environment][@"SIMULATOR_MODEL_IDENTIFIER"];
		if ([SimulatorModel length] > 0)
		{
			return FString(SimulatorModel);
		}
#endif
		size_t Length = 0;
		if (sysctlbyname("hw.machine", nullptr, &Length, nullptr, 0) != 0
			|| Length == 0)
		{
			return {};
		}

		TArray<ANSICHAR> Machine;
		Machine.SetNumZeroed(Length);
		if (sysctlbyname(
			"hw.machine",
			Machine.GetData(),
			&Length,
			nullptr,
			0
		) != 0)
		{
			return {};
		}
		return UTF8_TO_TCHAR(Machine.GetData());
	}
}

EOpenMobileDeviceFormFactor GetOpenMobileDeviceIOSFormFactor()
{
	@autoreleasepool
	{
		FOpenMobileDeviceFormFactorTraits Traits;
		const UIUserInterfaceIdiom Idiom = [[UIDevice currentDevice] userInterfaceIdiom];
		Traits.bPhoneIdiom = Idiom == UIUserInterfaceIdiomPhone;
		Traits.bTabletIdiom = Idiom == UIUserInterfaceIdiomPad;
#if TARGET_OS_SIMULATOR
		Traits.bSimulator = true;
#endif
		return FOpenMobileDeviceFormFactor::Classify(Traits);
	}
}

void ApplyOpenMobileDeviceIOSEmulatorDetection(
	FOpenMobileDeviceInformationSnapshot& Snapshot
)
{
#if TARGET_OS_SIMULATOR
	FOpenMobileDeviceEmulatorDetection::ApplyIOS(Snapshot, true);
#else
	FOpenMobileDeviceEmulatorDetection::ApplyIOS(Snapshot, false);
#endif
}
