#include "OpenMobileDeviceCapabilities.h"

const FName FOpenMobileDeviceCapabilityNames::CapabilityReport(
	TEXT("OpenMobile.Device.Architecture.CapabilityReport")
);
const FName FOpenMobileDeviceCapabilityNames::PlatformInformation(
	TEXT("OpenMobile.Device.Identity.PlatformInformation")
);
const FName FOpenMobileDeviceCapabilityNames::ManufacturerBrandModel(
	TEXT("OpenMobile.Device.Identity.ManufacturerBrandModel")
);
const FName FOpenMobileDeviceCapabilityNames::HardwareModelIdentifier(
	TEXT("OpenMobile.Device.Identity.HardwareModelIdentifier")
);
const FName FOpenMobileDeviceCapabilityNames::FormFactor(
	TEXT("OpenMobile.Device.Identity.FormFactor")
);
const FName FOpenMobileDeviceCapabilityNames::CpuArchitecture(
	TEXT("OpenMobile.Device.Identity.CpuArchitecture")
);
const FName FOpenMobileDeviceCapabilityNames::LogicalProcessorCount(
	TEXT("OpenMobile.Device.Identity.LogicalProcessorCount")
);
const FName FOpenMobileDeviceCapabilityNames::PhysicalMemory(
	TEXT("OpenMobile.Device.Identity.PhysicalMemory")
);
const FName FOpenMobileDeviceCapabilityNames::ApplicationMetadata(
	TEXT("OpenMobile.Device.Environment.ApplicationMetadata")
);
const FName FOpenMobileDeviceCapabilityNames::EmulatorDetection(
	TEXT("OpenMobile.Device.Environment.EmulatorDetection")
);
const FName FOpenMobileDeviceCapabilityNames::PreferredLanguages(
	TEXT("OpenMobile.Device.Environment.PreferredLanguages")
);
const FName FOpenMobileDeviceCapabilityNames::Locale(
	TEXT("OpenMobile.Device.Environment.Locale")
);
const FName FOpenMobileDeviceCapabilityNames::TimeZone(
	TEXT("OpenMobile.Device.Environment.TimeZone")
);
const FName FOpenMobileDeviceCapabilityNames::RegionalFormatting(
	TEXT("OpenMobile.Device.Environment.RegionalFormatting")
);
const FName FOpenMobileDeviceCapabilityNames::LocaleChangeEvents(
	TEXT("OpenMobile.Device.Environment.LocaleChangeEvents")
);
const FName FOpenMobileDeviceCapabilityNames::BatteryLevel(
	TEXT("OpenMobile.Device.Power.BatteryLevel")
);
const FName FOpenMobileDeviceCapabilityNames::ChargingState(
	TEXT("OpenMobile.Device.Power.ChargingState")
);
const FName FOpenMobileDeviceCapabilityNames::ChargingSource(
	TEXT("OpenMobile.Device.Power.ChargingSource")
);
const FName FOpenMobileDeviceCapabilityNames::PowerSavingMode(
	TEXT("OpenMobile.Device.Power.PowerSavingMode")
);
const FName FOpenMobileDeviceCapabilityNames::ThermalState(
	TEXT("OpenMobile.Device.Power.ThermalState")
);
const FName FOpenMobileDeviceCapabilityNames::ThermalHeadroom(
	TEXT("OpenMobile.Device.Power.ThermalHeadroom")
);
const FName FOpenMobileDeviceCapabilityNames::BatteryEvents(
	TEXT("OpenMobile.Device.Power.BatteryEvents")
);
const FName FOpenMobileDeviceCapabilityNames::PowerSavingEvents(
	TEXT("OpenMobile.Device.Power.PowerSavingEvents")
);
const FName FOpenMobileDeviceCapabilityNames::ThermalEvents(
	TEXT("OpenMobile.Device.Power.ThermalEvents")
);
const FName FOpenMobileDeviceCapabilityNames::MemoryPressureEvents(
	TEXT("OpenMobile.Device.Memory.PressureEvents")
);
const FName FOpenMobileDeviceCapabilityNames::StorageSpace(
	TEXT("OpenMobile.Device.Storage.Space")
);
const FName FOpenMobileDeviceCapabilityNames::LowStorageEvents(
	TEXT("OpenMobile.Device.Storage.LowStorageEvents")
);
const FName FOpenMobileDeviceCapabilityNames::NetworkPath(
	TEXT("OpenMobile.Device.Connectivity.NetworkPath")
);
const FName FOpenMobileDeviceCapabilityNames::NetworkTransport(
	TEXT("OpenMobile.Device.Connectivity.NetworkTransport")
);
const FName FOpenMobileDeviceCapabilityNames::NetworkPolicy(
	TEXT("OpenMobile.Device.Connectivity.NetworkPolicy")
);
const FName FOpenMobileDeviceCapabilityNames::CaptivePortal(
	TEXT("OpenMobile.Device.Connectivity.CaptivePortal")
);
const FName FOpenMobileDeviceCapabilityNames::NetworkChangeEvents(
	TEXT("OpenMobile.Device.Connectivity.NetworkChangeEvents")
);
const FName FOpenMobileDeviceCapabilityNames::EndpointReachability(
	TEXT("OpenMobile.Device.Connectivity.EndpointReachability")
);
const FName FOpenMobileDeviceCapabilityNames::WindowMetrics(
	TEXT("OpenMobile.Device.Display.WindowMetrics")
);
const FName FOpenMobileDeviceCapabilityNames::RefreshRateInformation(
	TEXT("OpenMobile.Device.Display.RefreshRateInformation")
);
const FName FOpenMobileDeviceCapabilityNames::RefreshRateControl(
	TEXT("OpenMobile.Device.Display.RefreshRateControl")
);
const FName FOpenMobileDeviceCapabilityNames::SafeAreaInsets(
	TEXT("OpenMobile.Device.Display.SafeAreaInsets")
);
const FName FOpenMobileDeviceCapabilityNames::DisplayCutout(
	TEXT("OpenMobile.Device.Display.Cutout")
);
const FName FOpenMobileDeviceCapabilityNames::WindowOrientation(
	TEXT("OpenMobile.Device.Display.WindowOrientation")
);
const FName FOpenMobileDeviceCapabilityNames::WindowChangeEvents(
	TEXT("OpenMobile.Device.Display.WindowChangeEvents")
);
const FName FOpenMobileDeviceCapabilityNames::OrientationControl(
	TEXT("OpenMobile.Device.Display.OrientationControl")
);
const FName FOpenMobileDeviceCapabilityNames::MultiWindowEvents(
	TEXT("OpenMobile.Device.Display.MultiWindowEvents")
);
const FName FOpenMobileDeviceCapabilityNames::FoldablePosture(
	TEXT("OpenMobile.Device.Display.FoldablePosture")
);
const FName FOpenMobileDeviceCapabilityNames::HdrWideColor(
	TEXT("OpenMobile.Device.Display.HdrWideColor")
);
const FName FOpenMobileDeviceCapabilityNames::SystemAppearance(
	TEXT("OpenMobile.Device.Appearance.SystemAppearance")
);
const FName FOpenMobileDeviceCapabilityNames::AppearanceChangeEvents(
	TEXT("OpenMobile.Device.Appearance.ChangeEvents")
);
const FName FOpenMobileDeviceCapabilityNames::PreferredTextScale(
	TEXT("OpenMobile.Device.Accessibility.PreferredTextScale")
);
const FName FOpenMobileDeviceCapabilityNames::ReducedAnimation(
	TEXT("OpenMobile.Device.Accessibility.ReducedAnimation")
);
const FName FOpenMobileDeviceCapabilityNames::ScreenReader(
	TEXT("OpenMobile.Device.Accessibility.ScreenReader")
);
const FName FOpenMobileDeviceCapabilityNames::AccessibilityChangeEvents(
	TEXT("OpenMobile.Device.Accessibility.ChangeEvents")
);
const FName FOpenMobileDeviceCapabilityNames::Brightness(
	TEXT("OpenMobile.Device.Control.Brightness")
);
const FName FOpenMobileDeviceCapabilityNames::KeepScreenAwake(
	TEXT("OpenMobile.Device.Control.KeepScreenAwake")
);
const FName FOpenMobileDeviceCapabilityNames::ImmersiveMode(
	TEXT("OpenMobile.Device.Control.ImmersiveMode")
);
const FName FOpenMobileDeviceCapabilityNames::FlashlightAvailability(
	TEXT("OpenMobile.Device.Utility.FlashlightAvailability")
);
const FName FOpenMobileDeviceCapabilityNames::FlashlightControl(
	TEXT("OpenMobile.Device.Utility.FlashlightControl")
);
const FName FOpenMobileDeviceCapabilityNames::ClipboardWrite(
	TEXT("OpenMobile.Device.Clipboard.Write")
);
const FName FOpenMobileDeviceCapabilityNames::ClipboardRead(
	TEXT("OpenMobile.Device.Clipboard.Read")
);
const FName FOpenMobileDeviceCapabilityNames::ClipboardTypeCheck(
	TEXT("OpenMobile.Device.Clipboard.TypeCheck")
);
const FName FOpenMobileDeviceCapabilityNames::ClipboardClear(
	TEXT("OpenMobile.Device.Clipboard.Clear")
);
const FName FOpenMobileDeviceCapabilityNames::UrlHandlerCheck(
	TEXT("OpenMobile.Device.External.UrlHandlerCheck")
);
const FName FOpenMobileDeviceCapabilityNames::AndroidPackageCheck(
	TEXT("OpenMobile.Device.External.AndroidPackageCheck")
);
const FName FOpenMobileDeviceCapabilityNames::OpenApplicationSettings(
	TEXT("OpenMobile.Device.External.OpenApplicationSettings")
);
const FName FOpenMobileDeviceCapabilityNames::MediaVolume(
	TEXT("OpenMobile.Device.Utility.MediaVolume")
);
const FName FOpenMobileDeviceCapabilityNames::VolumeEvents(
	TEXT("OpenMobile.Device.Utility.VolumeEvents")
);

namespace OpenMobileDeviceCapabilitiesPrivate
{
	int32 GetStatePrecedence(EOpenMobileCapabilityState State)
	{
		switch (FOpenMobileDeviceCapability::NormalizeState(State))
		{
		case EOpenMobileCapabilityState::NotSupported:
			return 80;
		case EOpenMobileCapabilityState::Restricted:
			return 70;
		case EOpenMobileCapabilityState::Denied:
			return 60;
		case EOpenMobileCapabilityState::PermissionRequired:
			return 50;
		case EOpenMobileCapabilityState::TemporarilyUnavailable:
			return 40;
		case EOpenMobileCapabilityState::NotConfigured:
			return 30;
		case EOpenMobileCapabilityState::Unavailable:
			return 20;
		case EOpenMobileCapabilityState::Available:
			return 10;
		}
		return 20;
	}
}

EOpenMobileCapabilityState FOpenMobileDeviceCapability::NormalizeState(
	EOpenMobileCapabilityState InState
)
{
	switch (InState)
	{
	case EOpenMobileCapabilityState::Available:
	case EOpenMobileCapabilityState::Unavailable:
	case EOpenMobileCapabilityState::NotSupported:
	case EOpenMobileCapabilityState::NotConfigured:
	case EOpenMobileCapabilityState::PermissionRequired:
	case EOpenMobileCapabilityState::Denied:
	case EOpenMobileCapabilityState::Restricted:
	case EOpenMobileCapabilityState::TemporarilyUnavailable:
		return InState;
	}
	return EOpenMobileCapabilityState::Unavailable;
}

FOpenMobileDeviceCapability FOpenMobileDeviceCapability::Resolve(
	FName CapabilityName,
	TConstArrayView<FOpenMobileDeviceCapability> Candidates
)
{
	FOpenMobileDeviceCapability Result;
	Result.Name = CapabilityName;
	int32 BestPrecedence = INDEX_NONE;
	for (const FOpenMobileDeviceCapability& Candidate : Candidates)
	{
		const int32 Precedence =
			OpenMobileDeviceCapabilitiesPrivate::GetStatePrecedence(Candidate.State);
		if (Precedence > BestPrecedence)
		{
			Result = Candidate;
			Result.Name = CapabilityName;
			Result.State = NormalizeState(Candidate.State);
			BestPrecedence = Precedence;
		}
	}
	return Result;
}

const FOpenMobileDeviceCapability* FOpenMobileDeviceCapabilityReport::Find(
	FName CapabilityName
) const
{
	return Capabilities.FindByPredicate(
		[CapabilityName](const FOpenMobileDeviceCapability& Capability)
		{
			return Capability.Name == CapabilityName;
		}
	);
}

const TArray<FName>& FOpenMobileDeviceCapabilityNames::GetAll()
{
	static const TArray<FName> Names = {
		CapabilityReport,
		PlatformInformation,
		ManufacturerBrandModel,
		HardwareModelIdentifier,
		FormFactor,
		CpuArchitecture,
		LogicalProcessorCount,
		PhysicalMemory,
		ApplicationMetadata,
		EmulatorDetection,
		PreferredLanguages,
		Locale,
		TimeZone,
		RegionalFormatting,
		LocaleChangeEvents,
		BatteryLevel,
		ChargingState,
		ChargingSource,
		PowerSavingMode,
		ThermalState,
		ThermalHeadroom,
		BatteryEvents,
		PowerSavingEvents,
		ThermalEvents,
		MemoryPressureEvents,
		StorageSpace,
		LowStorageEvents,
		NetworkPath,
		NetworkTransport,
		NetworkPolicy,
		CaptivePortal,
		NetworkChangeEvents,
		EndpointReachability,
		WindowMetrics,
		RefreshRateInformation,
		RefreshRateControl,
		SafeAreaInsets,
		DisplayCutout,
		WindowOrientation,
		WindowChangeEvents,
		OrientationControl,
		MultiWindowEvents,
		FoldablePosture,
		HdrWideColor,
		SystemAppearance,
		AppearanceChangeEvents,
		PreferredTextScale,
		ReducedAnimation,
		ScreenReader,
		AccessibilityChangeEvents,
		Brightness,
		KeepScreenAwake,
		ImmersiveMode,
		FlashlightAvailability,
		FlashlightControl,
		ClipboardWrite,
		ClipboardRead,
		ClipboardTypeCheck,
		ClipboardClear,
		UrlHandlerCheck,
		AndroidPackageCheck,
		OpenApplicationSettings,
		MediaVolume,
		VolumeEvents
	};
	return Names;
}

bool FOpenMobileDeviceCapabilityNames::IsKnown(FName CapabilityName)
{
	static const TSet<FName> KnownNames = []
	{
		TSet<FName> Result;
		Result.Reserve(GetAll().Num());
		for (const FName Name : GetAll())
		{
			Result.Add(Name);
		}
		return Result;
	}();
	return KnownNames.Contains(CapabilityName);
}
