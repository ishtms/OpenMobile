#include "OpenMobileDeviceIOSIdentity.h"

#import <UIKit/UIKit.h>

FString GetOpenMobileDeviceIOSModel()
{
	@autoreleasepool
	{
		return FString([[UIDevice currentDevice] model]);
	}
}
