#include "OpenMobileDeviceIOSFlashlight.h"

#include "Misc/ScopeLock.h"
#include "OpenMobileDeviceMonitoringService.h"

#import <AVFoundation/AVFoundation.h>
#include <TargetConditionals.h>

namespace OpenMobileDeviceIOSFlashlightPrivate
{
	FCriticalSection StateMutex;
	FOpenMobileDeviceMonitoringCallbackToken ActiveToken;
	uint64 SourceSequence = 0;

	AVCaptureDevice* GetTorchDevice()
	{
		AVCaptureDevice* Device = nil;
		if (@available(iOS 10.0, *))
		{
			Device = [AVCaptureDevice
				defaultDeviceWithDeviceType:AVCaptureDeviceTypeBuiltInWideAngleCamera
				mediaType:AVMediaTypeVideo
				position:AVCaptureDevicePositionBack];
		}
		if (!Device)
		{
			Device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
		}
		return Device;
	}

	void NotifyChange()
	{
		FOpenMobileDeviceMonitoringCallbackToken CallbackToken;
		uint64 Sequence = 0;
		{
			FScopeLock Lock(&StateMutex);
			if (!ActiveToken.IsValid())
			{
				return;
			}
			SourceSequence = SourceSequence == MAX_uint64
				? 1
				: SourceSequence + 1;
			CallbackToken = ActiveToken;
			Sequence = SourceSequence;
		}
		FOpenMobileDeviceMonitoringService::NotifyNativeChange(
			CallbackToken,
			Sequence
		);
	}
}

@interface OpenMobileDeviceFlashlightObserver : NSObject
@property(nonatomic, strong) AVCaptureDevice* device;
- (BOOL)startWithDevice:(AVCaptureDevice*)device;
- (void)stop;
@end

@implementation OpenMobileDeviceFlashlightObserver

- (BOOL)startWithDevice:(AVCaptureDevice*)device
{
	if (!device)
	{
		return NO;
	}
	self.device = device;
	[device addObserver:self forKeyPath:@"torchAvailable" options:0 context:nil];
	[device addObserver:self forKeyPath:@"torchActive" options:0 context:nil];
	[device addObserver:self forKeyPath:@"torchLevel" options:0 context:nil];
	if (@available(iOS 11.1, *))
	{
		[device addObserver:self forKeyPath:@"systemPressureState" options:0 context:nil];
	}
	return YES;
}

- (void)stop
{
	AVCaptureDevice* Device = self.device;
	if (!Device)
	{
		return;
	}
	[Device removeObserver:self forKeyPath:@"torchAvailable"];
	[Device removeObserver:self forKeyPath:@"torchActive"];
	[Device removeObserver:self forKeyPath:@"torchLevel"];
	if (@available(iOS 11.1, *))
	{
		[Device removeObserver:self forKeyPath:@"systemPressureState"];
	}
	self.device = nil;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
	ofObject:(id)object
	change:(NSDictionary<NSKeyValueChangeKey, id>*)change
	context:(void*)context
{
	static_cast<void>(keyPath);
	static_cast<void>(object);
	static_cast<void>(change);
	static_cast<void>(context);
	OpenMobileDeviceIOSFlashlightPrivate::NotifyChange();
}

@end

namespace OpenMobileDeviceIOSFlashlightPrivate
{
	OpenMobileDeviceFlashlightObserver* ActiveObserver = nil;

	EOpenMobileFlashlightPermissionState GetPermissionState()
	{
		switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo])
		{
		case AVAuthorizationStatusNotDetermined:
			return EOpenMobileFlashlightPermissionState::NotDetermined;
		case AVAuthorizationStatusRestricted:
			return EOpenMobileFlashlightPermissionState::Restricted;
		case AVAuthorizationStatusDenied:
			return EOpenMobileFlashlightPermissionState::Denied;
		case AVAuthorizationStatusAuthorized:
			return EOpenMobileFlashlightPermissionState::Granted;
		}
		return EOpenMobileFlashlightPermissionState::Unknown;
	}

	bool HasThermalPressure(AVCaptureDevice* Device)
	{
		if (@available(iOS 11.1, *))
		{
			const AVCaptureSystemPressureFactors Factors =
				Device.systemPressureState.factors;
			bool bThermal =
				(Factors & AVCaptureSystemPressureFactorSystemTemperature) != 0
				|| (Factors & AVCaptureSystemPressureFactorDepthModuleTemperature) != 0
				;
			if (@available(iOS 17.0, *))
			{
				bThermal = bThermal
					|| (Factors & AVCaptureSystemPressureFactorCameraTemperature) != 0;
			}
			return bThermal;
		}
		return false;
	}
}

FOpenMobileFlashlightSnapshot GetOpenMobileDeviceIOSFlashlightSnapshot()
{
	using namespace OpenMobileDeviceIOSFlashlightPrivate;
	FOpenMobileFlashlightSnapshot Snapshot;
#if TARGET_OS_SIMULATOR
	Snapshot.HardwareState = EOpenMobileFlashlightHardwareState::Unavailable;
	Snapshot.bVariableIntensitySupported =
		FOpenMobileDeviceOptionalBool::MakeAvailable(false);
	return Snapshot;
#else
	Snapshot.PermissionState = GetPermissionState();
	AVCaptureDevice* Device = GetTorchDevice();
	if (!Device || !Device.hasTorch)
	{
		Snapshot.HardwareState = EOpenMobileFlashlightHardwareState::Unavailable;
		Snapshot.bVariableIntensitySupported =
			FOpenMobileDeviceOptionalBool::MakeAvailable(false);
		Snapshot.ConflictState = EOpenMobileFlashlightConflictState::None;
		return Snapshot;
	}

	Snapshot.HardwareState = EOpenMobileFlashlightHardwareState::Available;
	Snapshot.TorchState = Device.isTorchActive
		? EOpenMobileFlashlightTorchState::On
		: EOpenMobileFlashlightTorchState::Off;
	Snapshot.CurrentIntensity = FOpenMobileDeviceOptionalFloat::MakeAvailable(
		FMath::Clamp(static_cast<float>(Device.torchLevel), 0.0f, 1.0f)
	);
	Snapshot.bVariableIntensitySupported =
		FOpenMobileDeviceOptionalBool::MakeAvailable(
			[Device isTorchModeSupported:AVCaptureTorchModeOn]
		);
	Snapshot.Ownership = EOpenMobileFlashlightOwnership::Unknown;
	const bool bThermalPressure = HasThermalPressure(Device);
	Snapshot.ThermalState = bThermalPressure
		? EOpenMobileFlashlightThermalState::Restricted
		: Device.isTorchAvailable
			? EOpenMobileFlashlightThermalState::NotRestricted
			: EOpenMobileFlashlightThermalState::Unknown;
	Snapshot.ConflictState = Device.isTorchAvailable
		? EOpenMobileFlashlightConflictState::None
		: EOpenMobileFlashlightConflictState::Unknown;
	return Snapshot;
#endif
}

bool StartOpenMobileDeviceIOSFlashlightMonitoring(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
)
{
	using namespace OpenMobileDeviceIOSFlashlightPrivate;
	check(IsInGameThread());
#if TARGET_OS_SIMULATOR
	static_cast<void>(CallbackToken);
	return false;
#else
	if (!CallbackToken.IsValid() || ActiveObserver)
	{
		return false;
	}
	AVCaptureDevice* Device = GetTorchDevice();
	if (!Device || !Device.hasTorch)
	{
		return false;
	}
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = CallbackToken;
		SourceSequence = 0;
	}
	ActiveObserver = [[OpenMobileDeviceFlashlightObserver alloc] init];
	if (![ActiveObserver startWithDevice:Device])
	{
		[ActiveObserver release];
		ActiveObserver = nil;
		FScopeLock Lock(&StateMutex);
		ActiveToken = {};
		return false;
	}
	return true;
#endif
}

void StopOpenMobileDeviceIOSFlashlightMonitoring()
{
	using namespace OpenMobileDeviceIOSFlashlightPrivate;
	check(IsInGameThread());
	{
		FScopeLock Lock(&StateMutex);
		ActiveToken = {};
		SourceSequence = 0;
	}
	[ActiveObserver stop];
	[ActiveObserver release];
	ActiveObserver = nil;
}
