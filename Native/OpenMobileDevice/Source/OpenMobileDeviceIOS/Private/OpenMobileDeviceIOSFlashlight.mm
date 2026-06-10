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
	if (Snapshot.bVariableIntensitySupported.Value)
	{
		Snapshot.MinimumIntensity =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(0.0f);
		Snapshot.MaximumIntensity =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(1.0f);
	}
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

FOpenMobileFlashlightOperationResult ApplyOpenMobileDeviceIOSFlashlight(
	const FOpenMobileFlashlightRequest& Request
)
{
	using namespace OpenMobileDeviceIOSFlashlightPrivate;
	FOpenMobileFlashlightOperationResult Result;
	Result.Request = Request;
#if TARGET_OS_SIMULATOR
	Result.State = EOpenMobileFlashlightOperationState::Unsupported;
	Result.Error = FOpenMobileError::Make(
		EOpenMobileErrorCode::NotSupported,
		TEXT("iOS Simulator does not support flashlight control."),
		FString(),
		TEXT("IOS")
	);
	return Result;
#else
	Result.PermissionState = GetPermissionState();
	AVCaptureDevice* Device = GetTorchDevice();
	if (!Device || !Device.hasTorch)
	{
		Result.State = EOpenMobileFlashlightOperationState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("This iOS device has no supported torch."),
			FString(),
			TEXT("IOS")
		);
		return Result;
	}
	if (Request.Operation == EOpenMobileFlashlightOperation::Off
		&& !Device.isTorchActive)
	{
		Result.State = EOpenMobileFlashlightOperationState::Applied;
		Result.EffectiveTorchState = EOpenMobileFlashlightTorchState::Off;
		Result.EffectiveIntensity =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(0.0f);
		return Result;
	}
	if (Request.Operation != EOpenMobileFlashlightOperation::Off
		&& !Device.isTorchAvailable)
	{
		const bool bThermal = HasThermalPressure(Device);
		Result.State = bThermal
			? EOpenMobileFlashlightOperationState::Restricted
			: EOpenMobileFlashlightOperationState::Busy;
		Result.Error = FOpenMobileError::Make(
			bThermal ? EOpenMobileErrorCode::Unavailable
				: EOpenMobileErrorCode::Busy,
			bThermal
				? TEXT("The iOS torch is thermally restricted.")
				: TEXT("The iOS torch or camera resource is busy."),
			bThermal ? TEXT("thermal") : TEXT("torch_unavailable"),
			TEXT("IOS")
		);
		return Result;
	}

	NSError* Error = nil;
	if (![Device lockForConfiguration:&Error])
	{
		const NSInteger NativeCode = Error ? Error.code : 0;
		if (NativeCode == AVErrorDeviceInUseByAnotherApplication)
		{
			Result.State = EOpenMobileFlashlightOperationState::Busy;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::Busy,
				TEXT("The iOS camera resource is busy."),
				FString::Printf(TEXT("%lld"), static_cast<int64>(NativeCode)),
				TEXT("IOS")
			);
		}
		else if (NativeCode
			== AVErrorApplicationIsNotAuthorizedToUseDevice)
		{
			Result.State =
				EOpenMobileFlashlightOperationState::PermissionDenied;
			Result.PermissionState =
				EOpenMobileFlashlightPermissionState::Denied;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::Unavailable,
				TEXT("iOS denied camera-device configuration access."),
				FString::Printf(TEXT("%lld"), static_cast<int64>(NativeCode)),
				TEXT("IOS")
			);
		}
		else
		{
			Result.State = EOpenMobileFlashlightOperationState::Rejected;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("iOS could not lock the torch for configuration."),
				FString::Printf(TEXT("%lld"), static_cast<int64>(NativeCode)),
				TEXT("IOS")
			);
		}
		return Result;
	}

	bool bApplied = true;
	if (Request.Operation == EOpenMobileFlashlightOperation::Off)
	{
		Device.torchMode = AVCaptureTorchModeOff;
	}
	else if (Request.Operation == EOpenMobileFlashlightOperation::On)
	{
		if ([Device isTorchModeSupported:AVCaptureTorchModeOn])
		{
			Device.torchMode = AVCaptureTorchModeOn;
		}
		else
		{
			bApplied = false;
			Result.State = EOpenMobileFlashlightOperationState::Unsupported;
		}
	}
	else
	{
		const float Intensity = FMath::Clamp(Request.Intensity, 0.0f, 1.0f);
		bApplied = [Device setTorchModeOnWithLevel:Intensity error:&Error];
		if (!bApplied)
		{
			Result.State = Error.code == AVErrorTorchLevelUnavailable
				? EOpenMobileFlashlightOperationState::Restricted
				: EOpenMobileFlashlightOperationState::Rejected;
		}
	}
	[Device unlockForConfiguration];

	if (!bApplied)
	{
		const NSInteger NativeCode = Error ? Error.code : 0;
		Result.Error = FOpenMobileError::Make(
			Result.State == EOpenMobileFlashlightOperationState::Restricted
				? EOpenMobileErrorCode::Unavailable
				: Result.State
					== EOpenMobileFlashlightOperationState::Unsupported
						? EOpenMobileErrorCode::NotSupported
						: EOpenMobileErrorCode::NativeFailure,
			Result.State == EOpenMobileFlashlightOperationState::Restricted
				? TEXT("The requested iOS torch intensity is thermally unavailable.")
				: TEXT("iOS rejected the flashlight operation."),
			FString::Printf(TEXT("%lld"), static_cast<int64>(NativeCode)),
			TEXT("IOS")
		);
		return Result;
	}

	Result.State = EOpenMobileFlashlightOperationState::Applied;
	Result.EffectiveTorchState = Device.isTorchActive
		? EOpenMobileFlashlightTorchState::On
		: EOpenMobileFlashlightTorchState::Off;
	Result.EffectiveIntensity = FOpenMobileDeviceOptionalFloat::MakeAvailable(
		FMath::Clamp(static_cast<float>(Device.torchLevel), 0.0f, 1.0f)
	);
	return Result;
#endif
}

void ClearOpenMobileDeviceIOSFlashlight()
{
	FOpenMobileFlashlightRequest Request;
	Request.Operation = EOpenMobileFlashlightOperation::Off;
	ApplyOpenMobileDeviceIOSFlashlight(Request);
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
