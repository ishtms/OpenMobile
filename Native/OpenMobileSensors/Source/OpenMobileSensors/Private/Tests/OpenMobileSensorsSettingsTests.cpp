#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsSettings.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace OpenMobileSensorsSettingsTestsPrivate
{
	bool HasErrorContaining(
		const TArray<FString>& Errors,
		const TCHAR* Expected
	)
	{
		return Errors.ContainsByPredicate(
			[Expected](const FString& Error)
			{
				return Error.Contains(Expected);
			}
		);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSettingsMetadataTest,
	"OpenMobile.Sensors.Settings.Metadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSettingsMetadataTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileSensorsSettings* Settings =
		NewObject<UOpenMobileSensorsSettings>();
	TestEqual(TEXT("Sensors settings use the OpenMobile category"),
		Settings->GetCategoryName(), FName(TEXT("OpenMobile")));
	TestEqual(TEXT("Sensors settings keep their own section"),
		Settings->GetSectionName(), FName(TEXT("OpenMobile Sensors")));
#if WITH_METADATA
	TestEqual(TEXT("Sensors display name matches its section"),
		Settings->GetClass()->GetMetaData(TEXT("DisplayName")),
		FString(TEXT("OpenMobile Sensors")));
#endif
	TestTrue(TEXT("Sensors settings use default config"),
		Settings->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig));
	const FName EngineConfigName(NAME_Engine);
	TestEqual(TEXT("Sensors settings serialize to Engine config"),
		Settings->GetClass()->GetConfigName(), EngineConfigName.ToString());

	const TArray<FName> ConfigProperties = {
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, UIPreset),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, GamePreset),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, FastPreset),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, DefaultStreamOptions),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, bAllowHighSamplingRate),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, bAllowBackgroundSensorDelivery),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, MaximumRecordingDurationSeconds),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, MaximumRecordingBytes),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, MaximumRecordingBufferedBatches),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, bEnablePermissionSensitiveSensors),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, IOSMotionUsageDescription),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, AndroidActivityRecognitionRationale),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, DevelopmentInputMode),
		GET_MEMBER_NAME_CHECKED(UOpenMobileSensorsSettings, DevelopmentReplayFile)
	};
	for (FName PropertyName : ConfigProperties)
	{
		const FProperty* Property = FindFProperty<FProperty>(
			UOpenMobileSensorsSettings::StaticClass(),
			PropertyName
		);
		TestTrue(
			*FString::Printf(TEXT("%s is serialized to config"),
				*PropertyName.ToString()),
			Property && Property->HasAnyPropertyFlags(CPF_Config)
		);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSettingsDefaultsTest,
	"OpenMobile.Sensors.Settings.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSettingsDefaultsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	UOpenMobileSensorsSettings* Settings =
		NewObject<UOpenMobileSensorsSettings>();
	TestEqual(TEXT("UI preset requests 15 Hz"),
		Settings->UIPreset.RequestedFrequencyHz, 15.0);
	TestEqual(TEXT("Game preset requests 60 Hz"),
		Settings->GamePreset.RequestedFrequencyHz, 60.0);
	TestEqual(TEXT("Fast preset requests 200 Hz"),
		Settings->FastPreset.RequestedFrequencyHz, 200.0);
	TestEqual(TEXT("Fast preset caps callbacks separately"),
		Settings->FastPreset.MaximumCallbackFrequencyHz, 60.0);
	TestEqual(TEXT("Default rate preset is UI"),
		Settings->DefaultStreamOptions.RatePreset,
		EOpenMobileSensorRatePreset::UI);
	TestEqual(TEXT("Default native request is 15 Hz"),
		Settings->DefaultStreamOptions.CustomFrequencyHz, 15.0);
	TestEqual(TEXT("Default callback cap is 15 Hz"),
		Settings->DefaultStreamOptions.MaximumCallbackFrequencyHz, 15.0);
	TestEqual(TEXT("Default buffer is bounded"),
		Settings->DefaultStreamOptions.BufferCapacitySamples, 128);
	TestEqual(TEXT("Default lifecycle suspends in background"),
		Settings->DefaultStreamOptions.LifecyclePolicy,
		EOpenMobileSensorLifecyclePolicy::SuspendInBackground);
	TestFalse(TEXT("High-rate sampling is opt-in"),
		Settings->bAllowHighSamplingRate);
	TestFalse(TEXT("Background delivery is opt-in"),
		Settings->bAllowBackgroundSensorDelivery);
	TestEqual(TEXT("Recording duration is bounded"),
		Settings->MaximumRecordingDurationSeconds, 300.0);
	TestEqual(TEXT("Recording size is bounded"),
		Settings->MaximumRecordingBytes, 64ll * 1024 * 1024);
	TestEqual(TEXT("Development input defaults off"),
		Settings->DevelopmentInputMode,
		EOpenMobileSensorsDevelopmentInputMode::Disabled);
	TArray<FString> Errors;
	TestTrue(TEXT("Defaults validate in Development"),
		Settings->Validate(Errors, false));
	TestTrue(TEXT("Defaults validate in Shipping"),
		Settings->Validate(Errors, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSettingsValidationTest,
	"OpenMobile.Sensors.Settings.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSettingsValidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsSettingsTestsPrivate;
	TArray<FString> Errors;
	UOpenMobileSensorsSettings* Preset =
		NewObject<UOpenMobileSensorsSettings>();
	Preset->FastPreset.RequestedFrequencyHz = 0.0;
	TestFalse(TEXT("Unsafe preset frequency is rejected"),
		Preset->Validate(Errors, false));
	TestTrue(TEXT("Preset failure names its field"),
		HasErrorContaining(Errors, TEXT("FastPreset")));

	UOpenMobileSensorsSettings* Frequency =
		NewObject<UOpenMobileSensorsSettings>();
	Frequency->DefaultStreamOptions.CustomFrequencyHz =
		std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Nonfinite frequency is rejected"),
		Frequency->Validate(Errors, false));
	TestTrue(TEXT("Frequency failure names its field"),
		HasErrorContaining(Errors, TEXT("CustomFrequencyHz")));

	UOpenMobileSensorsSettings* Callback =
		NewObject<UOpenMobileSensorsSettings>();
	Callback->DefaultStreamOptions.MaximumCallbackFrequencyHz = 0.0;
	TestFalse(TEXT("Zero callback cap is rejected"),
		Callback->Validate(Errors, false));
	TestTrue(TEXT("Callback failure names its field"),
		HasErrorContaining(Errors, TEXT("MaximumCallbackFrequencyHz")));

	UOpenMobileSensorsSettings* Buffer =
		NewObject<UOpenMobileSensorsSettings>();
	Buffer->DefaultStreamOptions.BufferCapacitySamples = 0;
	TestFalse(TEXT("Unbounded buffer is rejected"),
		Buffer->Validate(Errors, false));
	TestTrue(TEXT("Buffer failure names its field"),
		HasErrorContaining(Errors, TEXT("BufferCapacitySamples")));

	UOpenMobileSensorsSettings* Latency =
		NewObject<UOpenMobileSensorsSettings>();
	Latency->DefaultStreamOptions.MaximumDeliveryLatencySeconds = 11.0;
	TestFalse(TEXT("Unsafe batching latency is rejected"),
		Latency->Validate(Errors, false));
	TestTrue(TEXT("Latency failure names its field"),
		HasErrorContaining(Errors, TEXT("MaximumDeliveryLatencySeconds")));

	UOpenMobileSensorsSettings* Filter =
		NewObject<UOpenMobileSensorsSettings>();
	Filter->DefaultStreamOptions.Filters.bEnableLowPass = true;
	Filter->DefaultStreamOptions.Filters.LowPassTimeConstantSeconds = 0.0;
	TestFalse(TEXT("Invalid filter settings are rejected"),
		Filter->Validate(Errors, false));

	UOpenMobileSensorsSettings* Recording =
		NewObject<UOpenMobileSensorsSettings>();
	Recording->MaximumRecordingDurationSeconds = 0.0;
	Recording->MaximumRecordingBytes = 0;
	TestFalse(TEXT("Unbounded recording settings are rejected"),
		Recording->Validate(Errors, false));
	TestTrue(TEXT("Recording duration failure is explicit"),
		HasErrorContaining(Errors, TEXT("MaximumRecordingDurationSeconds")));
	TestTrue(TEXT("Recording size failure is explicit"),
		HasErrorContaining(Errors, TEXT("MaximumRecordingBytes")));

	UOpenMobileSensorsSettings* Permissions =
		NewObject<UOpenMobileSensorsSettings>();
	Permissions->bEnablePermissionSensitiveSensors = true;
	TestFalse(TEXT("Missing permission usage text is rejected"),
		Permissions->Validate(Errors, false));
	TestTrue(TEXT("iOS usage failure is explicit"),
		HasErrorContaining(Errors, TEXT("IOSMotionUsageDescription")));
	TestTrue(TEXT("Android rationale failure is explicit"),
		HasErrorContaining(Errors, TEXT("AndroidActivityRecognitionRationale")));

	UOpenMobileSensorsSettings* Background =
		NewObject<UOpenMobileSensorsSettings>();
	Background->DefaultStreamOptions.LifecyclePolicy =
		EOpenMobileSensorLifecyclePolicy::ContinueWhenSupported;
	TestFalse(TEXT("Contradictory background policy is rejected"),
		Background->Validate(Errors, false));

	UOpenMobileSensorsSettings* HighRate =
		NewObject<UOpenMobileSensorsSettings>();
	HighRate->DefaultStreamOptions.RatePreset =
		EOpenMobileSensorRatePreset::Fast;
	TestFalse(TEXT("High-rate default requires project opt-in"),
		HighRate->Validate(Errors, false));

	UOpenMobileSensorsSettings* Replay =
		NewObject<UOpenMobileSensorsSettings>();
	Replay->DevelopmentInputMode =
		EOpenMobileSensorsDevelopmentInputMode::Replay;
	TestFalse(TEXT("Replay requires a file"),
		Replay->Validate(Errors, false));
	TestTrue(TEXT("Replay file failure is explicit"),
		HasErrorContaining(Errors, TEXT("DevelopmentReplayFile")));
	Replay->DevelopmentReplayFile = TEXT("Fixtures/sensors.omrecording");
	TestTrue(TEXT("Configured replay is allowed in Development"),
		Replay->Validate(Errors, false));
	TestFalse(TEXT("Configured replay is rejected in Shipping"),
		Replay->Validate(Errors, true));

	UOpenMobileSensorsSettings* ShippingMock =
		NewObject<UOpenMobileSensorsSettings>();
	ShippingMock->DevelopmentInputMode =
		EOpenMobileSensorsDevelopmentInputMode::Mock;
	TestTrue(TEXT("Explicit mock is allowed in Development"),
		ShippingMock->Validate(Errors, false));
	TestFalse(TEXT("Explicit mock is rejected in Shipping"),
		ShippingMock->Validate(Errors, true));
	TestTrue(TEXT("Shipping mock failure is explicit"),
		HasErrorContaining(Errors, TEXT("DevelopmentInputMode")));
	return true;
}

#endif
