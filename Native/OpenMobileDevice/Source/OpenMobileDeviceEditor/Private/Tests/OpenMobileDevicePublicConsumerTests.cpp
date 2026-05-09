#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileDevice.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDevicePublicConsumerTest,
	"OpenMobile.Device.API.PublicConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDevicePublicConsumerTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	FOpenMobileDeviceInformationSnapshot DeviceInformation;
	FOpenMobileApplicationMetadataSnapshot ApplicationMetadata;
	FOpenMobileLocaleSnapshot Locale;
	FOpenMobilePowerSnapshot Power;
	FOpenMobileMediaVolumeSnapshot MediaVolume;
	FOpenMobileMemorySnapshot Memory;
	FOpenMobileStorageSnapshot Storage;
	FOpenMobileNetworkPathSnapshot Network;
	FOpenMobileWindowDisplaySnapshot Window;
	FOpenMobileAppearanceSnapshot Appearance;
	FOpenMobileAccessibilitySnapshot Accessibility;
	FOpenMobileDeviceControlResult ControlResult;
	static_cast<void>(DeviceInformation);
	static_cast<void>(ApplicationMetadata);
	static_cast<void>(Locale);
	static_cast<void>(Power);
	static_cast<void>(MediaVolume);
	static_cast<void>(Memory);
	static_cast<void>(Storage);
	static_cast<void>(Network);
	static_cast<void>(Window);
	static_cast<void>(Appearance);
	static_cast<void>(Accessibility);
	static_cast<void>(ControlResult);

	using FNativeMonitoringStart = FOpenMobileDeviceMonitoringHandle (
		UOpenMobileDeviceSubsystem::*
	)(const TArray<EOpenMobileDeviceMonitoringGroup>&, float);
	FNativeMonitoringStart NativeMonitoringStart =
		&UOpenMobileDeviceSubsystem::StartMonitoringNative;
	TestTrue(
		TEXT("C++ monitoring does not require a UObject proxy"),
		NativeMonitoringStart != nullptr
	);

	const TArray<TPair<UClass*, FName>> BlueprintFunctions = {
		{UOpenMobileDeviceBlueprintLibrary::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceBlueprintLibrary, GetBatteryPercent)},
		{UOpenMobileDeviceBlueprintLibrary::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceBlueprintLibrary, GetVolumePercent)},
		{UOpenMobileDeviceBlueprintLibrary::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceBlueprintLibrary, GetDeviceStatus)},
		{UOpenMobileDeviceBlueprintLibrary::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceBlueprintLibrary, GetDeviceCapability)},
		{UOpenMobileDeviceBlueprintLibrary::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceBlueprintLibrary, GetDeviceCapabilityReport)},
		{UOpenMobileDeviceSubsystem::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceSubsystem, StartMonitoring)}
	};
	for (const TPair<UClass*, FName>& Entry : BlueprintFunctions)
	{
		const UFunction* Function = Entry.Key->FindFunctionByName(Entry.Value);
		TestNotNull(*FString::Printf(TEXT("%s is discoverable"), *Entry.Value.ToString()), Function);
		if (!Function)
		{
			continue;
		}
		TestEqual(
			*FString::Printf(TEXT("%s uses the Device category"), *Entry.Value.ToString()),
			Function->GetMetaData(TEXT("Category")),
			FString(TEXT("Open Mobile|Device"))
		);
		TestFalse(
			*FString::Printf(TEXT("%s has a display name"), *Entry.Value.ToString()),
			Function->GetMetaData(TEXT("DisplayName")).IsEmpty()
		);
		TestFalse(
			*FString::Printf(TEXT("%s has a tooltip"), *Entry.Value.ToString()),
			Function->GetMetaData(TEXT("ToolTip")).IsEmpty()
		);
	}
	for (UClass* Class : {
		UOpenMobileDeviceBlueprintLibrary::StaticClass(),
		UOpenMobileDeviceSubsystem::StaticClass(),
		UOpenMobileDeviceMonitoringSubscription::StaticClass()
	})
	{
		for (TFieldIterator<UFunction> Function(Class, EFieldIterationFlags::None);
			Function;
			++Function)
		{
			if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
			{
				continue;
			}
			TestEqual(
				*FString::Printf(TEXT("%s uses the Device category"),
					*Function->GetName()),
				Function->GetMetaData(TEXT("Category")),
				FString(TEXT("Open Mobile|Device"))
			);
			TestFalse(
				*FString::Printf(TEXT("%s has a display name"), *Function->GetName()),
				Function->GetMetaData(TEXT("DisplayName")).IsEmpty()
			);
			TestFalse(
				*FString::Printf(TEXT("%s has a tooltip"), *Function->GetName()),
				Function->GetMetaData(TEXT("ToolTip")).IsEmpty()
			);
		}
	}

	const UFunction* StartMonitoring = UOpenMobileDeviceSubsystem::StaticClass()
		->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceSubsystem, StartMonitoring)
		);
	TestTrue(
		TEXT("Fallback polling is an advanced pin"),
		StartMonitoring
			&& StartMonitoring->GetMetaData(TEXT("AdvancedDisplay"))
				.Contains(TEXT("FallbackPollingIntervalSeconds"))
	);
	TestEqual(
		TEXT("Fallback polling defaults to Project Settings"),
		FCString::Atof(*StartMonitoring->GetMetaData(
			TEXT("CPP_Default_FallbackPollingIntervalSeconds")
		)),
		0.0f
	);

	for (const FName BranchName : {FName(TEXT("Success")), FName(TEXT("Cancelled")),
		FName(TEXT("Failed"))})
	{
		const FMulticastDelegateProperty* Branch =
			FindFProperty<FMulticastDelegateProperty>(
				UOpenMobileDeviceAsyncActionBase::StaticClass(),
				BranchName
			);
		TestTrue(
			*FString::Printf(TEXT("%s terminal branch is Blueprint assignable"),
				*BranchName.ToString()),
			Branch && Branch->HasAnyPropertyFlags(CPF_BlueprintAssignable)
		);
	}
	for (UClass* Class : {
		UOpenMobileDeviceAsyncActionBase::StaticClass(),
		UOpenMobileDeviceSubsystem::StaticClass()
	})
	{
		for (TFieldIterator<FMulticastDelegateProperty> Event(
			Class,
			EFieldIterationFlags::None
		); Event; ++Event)
		{
			TestEqual(
				*FString::Printf(TEXT("%s uses the Device category"),
					*Event->GetName()),
				Event->GetMetaData(TEXT("Category")),
				FString(TEXT("Open Mobile|Device"))
			);
			TestFalse(
				*FString::Printf(TEXT("%s has a display name"), *Event->GetName()),
				Event->GetMetaData(TEXT("DisplayName")).IsEmpty()
			);
			TestFalse(
				*FString::Printf(TEXT("%s has a tooltip"), *Event->GetName()),
				Event->GetMetaData(TEXT("ToolTip")).IsEmpty()
			);
		}
	}
	return true;
}

#endif
