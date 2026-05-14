#include "OpenMobileDeviceIOSApplication.h"

#include "Misc/App.h"
#include "OpenMobileDeviceApplicationInfo.h"

#import <Foundation/Foundation.h>

FOpenMobileApplicationMetadataSnapshot
GetOpenMobileDeviceIOSApplicationMetadata()
{
	@autoreleasepool
	{
		NSBundle* Bundle = [NSBundle mainBundle];
		NSDictionary* Info = [Bundle infoDictionary];
		NSDictionary* LocalizedInfo = [Bundle localizedInfoDictionary];
		NSString* DisplayName = LocalizedInfo[@"CFBundleDisplayName"];
		if ([DisplayName length] == 0)
		{
			DisplayName = Info[@"CFBundleDisplayName"];
		}
		return FOpenMobileDeviceApplicationInfo::Build(
			FString(DisplayName),
			FString([Bundle bundleIdentifier]),
			FString(Info[@"CFBundleShortVersionString"]),
			FString(Info[@"CFBundleVersion"]),
			FApp::GetBuildConfiguration()
		);
	}
}
