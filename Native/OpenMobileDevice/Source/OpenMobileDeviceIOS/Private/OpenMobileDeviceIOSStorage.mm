#include "OpenMobileDeviceIOSStorage.h"

#include "OpenMobileDeviceStorageInfo.h"

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>

bool QueryOpenMobileDeviceIOSStorage(
	FOpenMobileStorageSnapshot& OutSnapshot,
	FOpenMobileError& OutError
)
{
	OutSnapshot = {};
	OutError = {};
#if TARGET_OS_SIMULATOR
	OutError = FOpenMobileError::Make(
		EOpenMobileErrorCode::NotSupported,
		TEXT("iOS Simulator storage would describe the host Mac volume."),
		FString(),
		TEXT("IOS")
	);
	return false;
#else
	@autoreleasepool
	{
		NSURL* URL = [NSURL fileURLWithPath:NSHomeDirectory() isDirectory:YES];
		NSNumber* TotalValue = nil;
		NSNumber* AvailableValue = nil;
		NSError* RequiredError = nil;
		const bool bTotalAvailable = [URL
			getResourceValue:&TotalValue
			forKey:NSURLVolumeTotalCapacityKey
			error:&RequiredError];
		const bool bAvailableAvailable = [URL
			getResourceValue:&AvailableValue
			forKey:NSURLVolumeAvailableCapacityKey
			error:&RequiredError];
		const int64 TotalSigned = TotalValue ? TotalValue.longLongValue : -1;
		const int64 AvailableSigned = AvailableValue
			? AvailableValue.longLongValue
			: -1;
		if (!bTotalAvailable
			|| !bAvailableAvailable
			|| TotalSigned <= 0
			|| AvailableSigned < 0)
		{
			OutError = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("iOS could not read the application data volume."),
				RequiredError
					? FString::FromInt(static_cast<int32>(RequiredError.code))
					: FString(),
				TEXT("IOS")
			);
			return false;
		}

		NSNumber* ImportantValue = nil;
		const bool bImportantAvailable = [URL
			getResourceValue:&ImportantValue
			forKey:NSURLVolumeAvailableCapacityForImportantUsageKey
			error:nil]
			&& ImportantValue
			&& ImportantValue.longLongValue >= 0;
		OutSnapshot = FOpenMobileDeviceStorageInfo::Build(
			EOpenMobileStorageScope::ApplicationDataVolume,
			static_cast<uint64>(TotalSigned),
			static_cast<uint64>(AvailableSigned),
			bImportantAvailable,
			bImportantAvailable
				? static_cast<uint64>(ImportantValue.longLongValue)
				: 0,
			true
		);
		if (!OutSnapshot.TotalBytes.bIsAvailable
			|| !OutSnapshot.AvailableBytes.bIsAvailable)
		{
			OutError = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("iOS returned unusable application volume values."),
				FString(),
				TEXT("IOS")
			);
			return false;
		}
	}
	return true;
#endif
}
