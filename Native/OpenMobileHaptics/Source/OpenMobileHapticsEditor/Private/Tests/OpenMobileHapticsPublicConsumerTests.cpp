#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenMobileHaptics.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBlueprintDocumentationRecipesTest,
	"OpenMobile.Haptics.API.BlueprintDocumentationRecipes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBlueprintDocumentationRecipesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const TSharedPtr<IPlugin> Plugin =
		IPluginManager::Get().FindPlugin(TEXT("OpenMobileHaptics"));
	TestTrue(TEXT("The Haptics plugin is discoverable"), Plugin.IsValid());
	if (!Plugin)
	{
		return false;
	}
	FString Readme;
	TestTrue(
		TEXT("The Haptics README can be loaded"),
		FFileHelper::LoadFileToString(
			Readme,
			*FPaths::Combine(Plugin->GetBaseDir(), TEXT("README.md"))
		)
	);
	for (const TCHAR* Recipe : {
		TEXT("### Simple UI feedback"),
		TEXT("### Game feedback and fallback"),
		TEXT("### Prepared library and asset playback"),
		TEXT("### Cancellable delayed playback"),
		TEXT("### Dynamic intensity and playback control"),
		TEXT("### Player policy settings"),
		TEXT("### Calibrated scheduling"),
		TEXT("### Suppression and failure display"),
		TEXT("### Editor and physical-device validation")
	})
	{
		TestTrue(
			*FString::Printf(TEXT("README contains %s"), Recipe),
			Readme.Contains(Recipe)
		);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPublicConsumerTest,
	"OpenMobile.Haptics.API.PublicConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPublicConsumerTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	FOpenMobileHapticCapabilities Capabilities;
	FOpenMobileHapticNamedSupport NamedSupport;
	FOpenMobileHapticIntegerLimit IntegerLimit;
	FOpenMobileHapticDurationLimit DurationLimit;
	FOpenMobileHapticSemanticRequest SemanticRequest;
	FOpenMobileHapticOneShotRequest OneShotRequest;
	FOpenMobileHapticNamedPatternRequest NamedRequest;
	FOpenMobileHapticPattern Pattern;
	FOpenMobileHapticPlaybackOptions Options;
	FOpenMobileHapticPlaybackHandle Handle;
	FOpenMobileHapticError Error;
	FOpenMobileHapticPlaybackResult PlaybackResult;
	FOpenMobileHapticPlaybackEvent PlaybackEvent;
	FOpenMobileHapticUserPolicy Policy;
	FOpenMobileHapticsDiagnostics Diagnostics;
	UOpenMobileHapticLibrary* Library = nullptr;
	UOpenMobileHapticsSettings* Settings = nullptr;
	static_cast<void>(Capabilities);
	static_cast<void>(NamedSupport);
	static_cast<void>(IntegerLimit);
	static_cast<void>(DurationLimit);
	static_cast<void>(SemanticRequest);
	static_cast<void>(OneShotRequest);
	static_cast<void>(NamedRequest);
	static_cast<void>(Pattern);
	static_cast<void>(Options);
	static_cast<void>(Handle);
	static_cast<void>(Error);
	static_cast<void>(PlaybackResult);
	static_cast<void>(PlaybackEvent);
	static_cast<void>(Policy);
	static_cast<void>(Diagnostics);
	static_cast<void>(Library);
	static_cast<void>(Settings);

	using FNativeSemanticSubmit = FOpenMobileHapticPlaybackResult (
		IOpenMobileHaptics::*
	)(const FOpenMobileHapticSemanticRequest&);
	FNativeSemanticSubmit NativeSemanticSubmit =
		&IOpenMobileHaptics::SubmitSemantic;
	TestTrue(
		TEXT("C++ semantic submission uses the native interface"),
		NativeSemanticSubmit != nullptr
	);

	const TArray<FName> BlueprintFunctions = {
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			GetHapticCapabilities
		),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			PlaySelectionFeedback
		),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			PlayImpactFeedback
		),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			PlayNotificationFeedback
		),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			PlayGameFeedback
		),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			PlaySemanticFeedback
		),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileHapticsSubsystem, Vibrate),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			PlayNamedPattern
		),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			StopPlayback
		),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			UpdatePlaybackParameters
		),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileHapticsSubsystem, StopChannel),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileHapticsSubsystem, StopAll),
		GET_FUNCTION_NAME_CHECKED(
			UOpenMobileHapticsSubsystem,
			GetPlaybackState
		),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileHapticsSubsystem, GetUserPolicy),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileHapticsSubsystem, SetUserPolicy),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileHapticsSubsystem, GetDiagnostics)
	};
	for (const FName FunctionName : BlueprintFunctions)
	{
		const UFunction* Function =
			UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
				FunctionName
			);
		TestNotNull(
			*FString::Printf(
				TEXT("%s is available to Blueprint"),
				*FunctionName.ToString()
			),
			Function
		);
		if (!Function)
		{
			continue;
		}
		TestEqual(
			*FString::Printf(
				TEXT("%s uses the advanced Haptics category"),
				*FunctionName.ToString()
			),
			Function->GetMetaData(TEXT("Category")),
			FString(TEXT("OpenMobile|Haptics|Advanced"))
		);
		TestFalse(
			*FString::Printf(
				TEXT("%s has a display name"),
				*FunctionName.ToString()
			),
			Function->GetMetaData(TEXT("DisplayName")).IsEmpty()
		);
		TestFalse(
			*FString::Printf(
				TEXT("%s has a tooltip"),
				*FunctionName.ToString()
			),
			Function->GetMetaData(TEXT("ToolTip")).IsEmpty()
		);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBlueprintFirstSurfaceTest,
	"OpenMobile.Haptics.API.BlueprintFirstSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBlueprintFirstSurfaceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	const UClass* BlueprintLibraryClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary")
	);
	TestNotNull(
		TEXT("The Blueprint-first Haptics library is reflected"),
		BlueprintLibraryClass
	);
	if (!BlueprintLibraryClass)
	{
		return false;
	}

	const TArray<FName> CommonPlayFunctions = {
		TEXT("PlaySelectionHaptic"),
		TEXT("PlayImpactHaptic"),
		TEXT("PlayNotificationHaptic"),
		TEXT("PlayGameHaptic"),
		TEXT("VibratePhone")
	};
	for (const FName FunctionName : CommonPlayFunctions)
	{
		const UFunction* Function =
			BlueprintLibraryClass->FindFunctionByName(FunctionName);
		TestNotNull(
			*FString::Printf(
				TEXT("%s is available without a subsystem target"),
				*FunctionName.ToString()
			),
			Function
		);
		if (!Function)
		{
			continue;
		}
		TestEqual(
			*FString::Printf(
				TEXT("%s uses the common play category"),
				*FunctionName.ToString()
			),
			Function->GetMetaData(TEXT("Category")),
			FString(TEXT("OpenMobile|Haptics|Play"))
		);
		TestEqual(
			*FString::Printf(
				TEXT("%s resolves its Game Instance from context"),
				*FunctionName.ToString()
			),
			Function->GetMetaData(TEXT("WorldContext")),
			FString(TEXT("WorldContextObject"))
		);
		TestEqual(
			*FString::Printf(
				TEXT("%s exposes honest request branches"),
				*FunctionName.ToString()
			),
			Function->GetMetaData(TEXT("ExpandEnumAsExecs")),
			FString(TEXT("Outcome"))
		);
		TestFalse(
			*FString::Printf(
				TEXT("%s has search keywords"),
				*FunctionName.ToString()
			),
			Function->GetMetaData(TEXT("Keywords")).IsEmpty()
		);
		const FFloatProperty* IntensityProperty =
			FindFProperty<FFloatProperty>(Function, TEXT("Intensity"));
		TestTrue(
			*FString::Printf(
				TEXT("%s constrains normalized intensity"),
				*FunctionName.ToString()
			),
			IntensityProperty
				&& IntensityProperty->GetMetaData(TEXT("ClampMin"))
					== TEXT("0.0")
				&& IntensityProperty->GetMetaData(TEXT("ClampMax"))
					== TEXT("1.0")
		);
	}

	for (const FName FunctionName : {
		FName(TEXT("IsHapticPlaybackHandleValid")),
		FName(TEXT("EqualHapticPlaybackHandles")),
		FName(TEXT("IsHapticPreloadHandleValid")),
		FName(TEXT("EqualHapticPreloadHandles")),
		FName(TEXT("IsHapticRequestAccepted")),
		FName(TEXT("DidHapticRequestProduceOutput")),
		FName(TEXT("HasHapticError")),
		FName(TEXT("GetHapticResultSummary")),
		FName(TEXT("FormatHapticError"))
	})
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is available to Blueprint"),
				*FunctionName.ToString()
			),
			BlueprintLibraryClass->FindFunctionByName(FunctionName)
		);
	}

	const UClass* PlaybackClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticPlayback")
	);
	TestNotNull(
		TEXT("Accepted common requests expose a playback object"),
		PlaybackClass
	);
	if (PlaybackClass)
	{
		for (const FName FunctionName : {
			FName(TEXT("IsValid")),
			FName(TEXT("IsActive")),
			FName(TEXT("IsPaused")),
			FName(TEXT("Stop")),
			FName(TEXT("Cancel")),
			FName(TEXT("Pause")),
			FName(TEXT("Resume")),
			FName(TEXT("Seek")),
			FName(TEXT("SetIntensity")),
			FName(TEXT("SetSharpness")),
			FName(TEXT("SetIntensityAndSharpness"))
		})
		{
			TestNotNull(
				*FString::Printf(
					TEXT("Playback exposes %s"),
					*FunctionName.ToString()
				),
				PlaybackClass->FindFunctionByName(FunctionName)
			);
		}
		for (const FName EventName : {
			FName(TEXT("OnAccepted")),
			FName(TEXT("OnStarted")),
			FName(TEXT("OnFinished"))
		})
		{
			const FMulticastDelegateProperty* Event =
				FindFProperty<FMulticastDelegateProperty>(
					PlaybackClass,
					EventName
				);
			TestTrue(
				*FString::Printf(
					TEXT("Playback %s is Blueprint assignable"),
					*EventName.ToString()
				),
				Event && Event->HasAnyPropertyFlags(
					CPF_BlueprintAssignable
				)
			);
		}
	}

	TestNotNull(
		TEXT("Suppressed async playback has its own terminal branch"),
		FindFProperty<FMulticastDelegateProperty>(
			UOpenMobileHapticPlaybackAsyncAction::StaticClass(),
			TEXT("Suppressed")
		)
	);
	TestEqual(
		TEXT("The subsystem has a friendly Blueprint display name"),
		UOpenMobileHapticsSubsystem::StaticClass()->GetMetaData(
			TEXT("DisplayName")
		),
		FString(TEXT("Open Mobile Haptics"))
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBlueprintAuthoredContentSurfaceTest,
	"OpenMobile.Haptics.API.BlueprintAuthoredContentSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBlueprintAuthoredContentSurfaceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	const UClass* PreparationActionClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticPreparationAsyncAction")
	);
	TestNotNull(
		TEXT("Owned preparation has a Blueprint async action"),
		PreparationActionClass
	);
	if (PreparationActionClass)
	{
		for (const FName FactoryName : {
			FName(TEXT("PrepareHapticsAsync")),
			FName(TEXT("PrepareHapticLibraryAsync")),
			FName(TEXT("PrepareHapticPatternAsync"))
		})
		{
			TestNotNull(
				*FString::Printf(
					TEXT("%s exposes owned async preparation"),
					*FactoryName.ToString()
				),
				PreparationActionClass->FindFunctionByName(FactoryName)
			);
		}
		for (const FName BranchName : {
			FName(TEXT("Ready")),
			FName(TEXT("Cancelled")),
			FName(TEXT("Failed"))
		})
		{
			const FMulticastDelegateProperty* Branch =
				FindFProperty<FMulticastDelegateProperty>(
					PreparationActionClass,
					BranchName
				);
			TestTrue(
				*FString::Printf(
					TEXT("Preparation %s is Blueprint assignable"),
					*BranchName.ToString()
				),
				Branch && Branch->HasAnyPropertyFlags(
					CPF_BlueprintAssignable
				)
			);
		}
	}

	const UClass* LeaseClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticPreparationLease")
	);
	TestNotNull(TEXT("Preparation returns an owned lease"), LeaseClass);
	if (LeaseClass)
	{
		TestNotNull(
			TEXT("A preparation lease can release only its own claim"),
			LeaseClass->FindFunctionByName(TEXT("Release"))
		);
	}

	const UClass* PatternActionClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticPatternPlaybackAsyncAction")
	);
	TestNotNull(
		TEXT("Pattern assets have a Blueprint playback task"),
		PatternActionClass
	);
	if (PatternActionClass)
	{
		TestNotNull(
			TEXT("Pattern asset playback exposes its async factory"),
			PatternActionClass->FindFunctionByName(
				TEXT("PlayHapticPatternAsset")
			)
		);
		for (const FName BranchName : {
			FName(TEXT("WaitingForPreparation")),
			FName(TEXT("Accepted")),
			FName(TEXT("Suppressed")),
			FName(TEXT("Rejected"))
		})
		{
			TestNotNull(
				*FString::Printf(
					TEXT("Pattern playback exposes %s"),
					*BranchName.ToString()
				),
				FindFProperty<FMulticastDelegateProperty>(
					PatternActionClass,
					BranchName
				)
			);
		}
	}

	const UClass* BlueprintLibraryClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary")
	);
	TestNotNull(TEXT("Blueprint Haptics library remains available"), BlueprintLibraryClass);
	if (BlueprintLibraryClass)
	{
		for (const FName FunctionName : {
			FName(TEXT("GetConfiguredHapticPatternNames")),
			FName(TEXT("GetPreparedHapticPatternNames")),
			FName(TEXT("IsHapticPatternReady"))
		})
		{
			TestNotNull(
				*FString::Printf(
					TEXT("%s supports authored-content discovery"),
					*FunctionName.ToString()
				),
				BlueprintLibraryClass->FindFunctionByName(FunctionName)
			);
		}
	}

	TestNotNull(
		TEXT("Preparation state changes can be observed without polling"),
		FindFProperty<FMulticastDelegateProperty>(
			UOpenMobileHapticsSubsystem::StaticClass(),
			TEXT("OnPreparationStateChanged")
		)
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBlueprintPreviewAndAuthoringSurfaceTest,
	"OpenMobile.Haptics.API.BlueprintPreviewAndAuthoringSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBlueprintPreviewAndAuthoringSurfaceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	const UScriptStruct* WaveformStep = FindObject<UScriptStruct>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticAndroidWaveformStep")
	);
	TestNotNull(TEXT("Android waveforms expose one row type"), WaveformStep);

	const UClass* AndroidAssetClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticAndroidPatternAsset")
	);
	TestNotNull(TEXT("Android pattern assets are reflected"), AndroidAssetClass);
	if (AndroidAssetClass)
	{
		TestFalse(
			TEXT("Platform authoring assets stay out of Blueprint type search"),
			AndroidAssetClass->HasMetaData(TEXT("BlueprintType"))
		);
		for (const TPair<FName, FString>& ConditionalField : {
			TPair<FName, FString>(
				TEXT("Primitives"),
				TEXT("Primitives")
			),
			TPair<FName, FString>(
				TEXT("EnvelopePoints"),
				TEXT("Envelope")
			)
		})
		{
			const FProperty* Property = FindFProperty<FProperty>(
				AndroidAssetClass,
				ConditionalField.Key
			);
			TestTrue(
				*FString::Printf(
					TEXT("%s is conditional and hidden when irrelevant"),
					*ConditionalField.Key.ToString()
				),
				Property
					&& Property->GetMetaData(TEXT("EditCondition")).Contains(
						ConditionalField.Value
					)
					&& Property->HasMetaData(TEXT("EditConditionHides"))
			);
		}
		const FProperty* WaveformSteps = FindFProperty<FProperty>(
			AndroidAssetClass,
			TEXT("WaveformSteps")
		);
		TestTrue(
			TEXT("Waveform rows are visible only for the waveform format"),
			WaveformSteps
				&& WaveformSteps->GetMetaData(TEXT("EditCondition")).Contains(
					TEXT("Waveform")
				)
				&& WaveformSteps->HasMetaData(TEXT("EditConditionHides"))
		);
		for (const FName LegacyName : {
			FName(TEXT("WaveformTimingsMilliseconds")),
			FName(TEXT("WaveformAmplitudes"))
		})
		{
			const FProperty* LegacyProperty = FindFProperty<FProperty>(
				AndroidAssetClass,
				LegacyName
			);
			TestTrue(
				*FString::Printf(
					TEXT("%s remains serialized only for migration"),
					*LegacyName.ToString()
				),
				LegacyProperty
					&& LegacyProperty->HasMetaData(TEXT("DeprecatedProperty"))
					&& !LegacyProperty->HasAnyPropertyFlags(
						CPF_Edit | CPF_BlueprintVisible
					)
			);
		}
		TestNotNull(
			TEXT("The resolved Android API requirement is visible"),
			FindFProperty<FIntProperty>(
				AndroidAssetClass,
				TEXT("ResolvedMinimumAndroidAPI")
			)
		);
	}
	const UClass* IOSAssetClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticIOSPatternAsset")
	);
	TestTrue(
		TEXT("Imported iOS audio bytes stay out of Blueprint"),
		IOSAssetClass
			&& !IOSAssetClass->HasMetaData(TEXT("BlueprintType"))
	);

	const UClass* ReceiverClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHapticsPreview.OpenMobileHapticsPreviewReceiverSubsystem")
	);
	TestNotNull(TEXT("Development receiver is reflected"), ReceiverClass);
	if (ReceiverClass)
	{
		for (const FName FunctionName : {
			FName(TEXT("EnableHapticPreviewReceiver")),
			FName(TEXT("ApproveHapticPreviewPairing")),
			FName(TEXT("RejectHapticPreviewPairing"))
		})
		{
			const UFunction* Function = ReceiverClass->FindFunctionByName(
				FunctionName
			);
			TestNotNull(
				*FString::Printf(
					TEXT("%s exposes a typed Development action"),
					*FunctionName.ToString()
				),
				Function
			);
			if (Function)
			{
				TestEqual(
					TEXT("Preview actions use compact outcome branches"),
					Function->GetMetaData(TEXT("ExpandEnumAsExecs")),
					FString(TEXT("Outcome"))
				);
				TestEqual(
					TEXT("Preview actions share one Blueprint category"),
					Function->GetMetaData(TEXT("Category")),
					FString(TEXT("OpenMobile|Haptics|Preview"))
				);
				TestTrue(
					TEXT("Preview actions identify Development-only use"),
					Function->GetMetaData(TEXT("DisplayName")).Contains(
						TEXT("Development Only")
					)
				);
				TestNotNull(
					TEXT("Preview action exposes an error output"),
					FindFProperty<FStrProperty>(Function, TEXT("Error"))
				);
				if (FunctionName == TEXT("EnableHapticPreviewReceiver"))
				{
					TestEqual(
						TEXT("Preview receiver has its documented default port"),
						Function->GetMetaData(TEXT("CPP_Default_Port")),
						FString(TEXT("41798"))
					);
				}
				else
				{
					const FStructProperty* PairingRequest =
						FindFProperty<FStructProperty>(
							Function,
							TEXT("PairingRequest")
						);
					TestTrue(
						TEXT("Pairing actions accept the typed request"),
						PairingRequest
							&& PairingRequest->Struct->GetName()
								== TEXT("OpenMobileHapticsPreviewPairingRequest")
					);
				}
			}
		}
		for (const FName LegacyName : {
			FName(TEXT("EnableReceiver")),
			FName(TEXT("ApprovePairing")),
			FName(TEXT("RejectPairing"))
		})
		{
			const UFunction* LegacyFunction =
				ReceiverClass->FindFunctionByName(LegacyName);
			TestTrue(
				*FString::Printf(
					TEXT("%s has Blueprint migration guidance"),
					*LegacyName.ToString()
				),
				LegacyFunction
					&& LegacyFunction->HasMetaData(
						TEXT("DeprecatedFunction")
					)
					&& !LegacyFunction->GetMetaData(
						TEXT("DeprecationMessage")
					).IsEmpty()
			);
		}
		const FMulticastDelegateProperty* ReceiverChanged =
			FindFProperty<FMulticastDelegateProperty>(
				ReceiverClass,
				TEXT("OnReceiverChanged")
			);
		TestTrue(
			TEXT("Receiver changes include status and pairing payloads"),
			ReceiverChanged
				&& ReceiverChanged->SignatureFunction
				&& ReceiverChanged->SignatureFunction->NumParms == 2
		);
	}

	const UClass* TesterClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHapticsPreview.OpenMobileHapticsCapabilityTesterLibrary")
	);
	TestNotNull(TEXT("Development capability tester is reflected"), TesterClass);
	if (TesterClass)
	{
		const UFunction* SnapshotFunction = TesterClass->FindFunctionByName(
			TEXT("CreateHapticCapabilitySnapshot")
		);
		TestTrue(
			TEXT("Capability snapshot uses a typed outcome branch"),
			SnapshotFunction
				&& SnapshotFunction->GetMetaData(TEXT("ExpandEnumAsExecs"))
					== TEXT("Outcome")
				&& SnapshotFunction->GetMetaData(TEXT("DisplayName")).Contains(
					TEXT("Development Only")
				)
		);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsBlueprintPolicyTimingAndIdentifiersSurfaceTest,
	"OpenMobile.Haptics.API.BlueprintPolicyTimingAndIdentifiersSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsBlueprintPolicyTimingAndIdentifiersSurfaceTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	const UClass* BlueprintLibraryClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary")
	);
	TestNotNull(TEXT("Blueprint Haptics library is reflected"), BlueprintLibraryClass);
	if (!BlueprintLibraryClass)
	{
		return false;
	}

	for (const TPair<FName, FString>& FunctionContract : {
		TPair<FName, FString>(TEXT("GetHapticsEnabled"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("SetHapticsEnabled"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("GetHapticsMasterIntensity"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("SetHapticsMasterIntensity"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("GetHapticCategoryIntensity"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("SetHapticCategoryIntensity"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("ResetHapticCategoryIntensity"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("GetHapticEffectIntensity"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("SetHapticEffectIntensity"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("ResetHapticEffectIntensity"), TEXT("Policy")),
		TPair<FName, FString>(TEXT("SupportsHapticFeature"), TEXT("Capabilities")),
		TPair<FName, FString>(TEXT("CalibrateHapticTiming"), TEXT("Timing")),
		TPair<FName, FString>(TEXT("IsHapticClockCalibrated"), TEXT("Timing")),
		TPair<FName, FString>(TEXT("GetHapticTimingAccuracy"), TEXT("Timing")),
		TPair<FName, FString>(TEXT("MakeHapticPlaybackOptions"), TEXT("Options")),
		TPair<FName, FString>(TEXT("MakeStandardHapticChannel"), TEXT("Options")),
		TPair<FName, FString>(TEXT("MakeHapticPatternIdentifier"), TEXT("Advanced")),
		TPair<FName, FString>(TEXT("MakeHapticLibraryIdentifier"), TEXT("Advanced")),
		TPair<FName, FString>(TEXT("GetConfiguredHapticPatternIdentifiers"), TEXT("Prepare")),
		TPair<FName, FString>(TEXT("GetConfiguredHapticLibraryIdentifiers"), TEXT("Prepare")),
		TPair<FName, FString>(TEXT("WaitForHapticPlayback"), TEXT("Control")),
		TPair<FName, FString>(TEXT("BreakHapticPlaybackResult"), TEXT("Diagnostics"))
	})
	{
		const UFunction* Function = BlueprintLibraryClass->FindFunctionByName(
			FunctionContract.Key
		);
		TestNotNull(
			*FString::Printf(
				TEXT("%s is reflected"),
				*FunctionContract.Key.ToString()
			),
			Function
		);
		if (!Function)
		{
			continue;
		}
		TestEqual(
			TEXT("Blueprint-first functions use the intended category"),
			Function->GetMetaData(TEXT("Category")),
			FString::Printf(
				TEXT("OpenMobile|Haptics|%s"),
				*FunctionContract.Value
			)
		);
		TestFalse(
			TEXT("Blueprint-first functions have search keywords"),
			Function->GetMetaData(TEXT("Keywords")).IsEmpty()
		);
		TestFalse(
			TEXT("Blueprint-first functions have authored tooltips"),
			Function->GetMetaData(TEXT("ToolTip")).IsEmpty()
		);
	}

	for (const TCHAR* StructName : {
		TEXT("OpenMobileHapticPatternIdentifier"),
		TEXT("OpenMobileHapticLibraryIdentifier"),
		TEXT("OpenMobileHapticChannelIdentifier"),
		TEXT("OpenMobileHapticCategoryIdentifier"),
		TEXT("OpenMobileHapticEffectIdentifier")
	})
	{
		const UScriptStruct* Identifier = FindObject<UScriptStruct>(
			nullptr,
			*FString::Printf(
				TEXT("/Script/OpenMobileHaptics.%s"),
				StructName
			)
		);
		TestTrue(
			*FString::Printf(TEXT("%s is a typed Blueprint identifier"), StructName),
			Identifier && Identifier->HasMetaData(TEXT("BlueprintType"))
		);
	}

	const UClass* NamedTaskClass = FindObject<UClass>(
		nullptr,
		TEXT("/Script/OpenMobileHaptics.OpenMobileHapticNamedPlaybackAsyncAction")
	);
	TestTrue(
		TEXT("Configured identifiers have prepare-if-needed playback"),
		NamedTaskClass
			&& NamedTaskClass->FindFunctionByName(TEXT("PlayNamedHaptic"))
	);

	for (const FName EventName : {
		FName(TEXT("OnPolicyChanged")),
		FName(TEXT("OnAvailabilityChanged")),
		FName(TEXT("OnCapabilitiesChanged")),
		FName(TEXT("OnMasterIntensityChanged"))
	})
	{
		const FMulticastDelegateProperty* Event =
			FindFProperty<FMulticastDelegateProperty>(
				UOpenMobileHapticsSubsystem::StaticClass(),
				EventName
			);
		TestTrue(
			*FString::Printf(TEXT("%s is Blueprint assignable"), *EventName.ToString()),
			Event && Event->SignatureFunction
				&& Event->SignatureFunction->NumParms == 2
		);
	}

	for (const FName LegacyName : {
		FName(TEXT("PlaySelectionFeedback")),
		FName(TEXT("PlayImpactFeedback")),
		FName(TEXT("PlayNotificationFeedback")),
		FName(TEXT("PlayGameFeedback")),
		FName(TEXT("Vibrate")),
		FName(TEXT("PlayNamedPattern"))
	})
	{
		const UFunction* Legacy =
			UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
				LegacyName
			);
		TestTrue(
			*FString::Printf(
				TEXT("%s is an advanced deprecated compatibility node"),
				*LegacyName.ToString()
			),
			Legacy
				&& Legacy->GetMetaData(TEXT("Category"))
					== TEXT("OpenMobile|Haptics|Advanced")
				&& Legacy->HasMetaData(TEXT("DeprecatedFunction"))
				&& !Legacy->GetMetaData(TEXT("DeprecationMessage")).IsEmpty()
		);
	}

	const UFunction* Diagnostics =
		UOpenMobileHapticsSubsystem::StaticClass()->FindFunctionByName(
			TEXT("GetDiagnostics")
		);
	TestTrue(
		TEXT("Legacy full diagnostics are impure and advanced"),
		Diagnostics
			&& !Diagnostics->HasAnyFunctionFlags(FUNC_BlueprintPure)
			&& Diagnostics->GetMetaData(TEXT("Category"))
				== TEXT("OpenMobile|Haptics|Advanced")
	);

	for (TObjectIterator<UEnum> Enum; Enum; ++Enum)
	{
		if (!Enum->GetOutermost()->GetName().StartsWith(
			TEXT("/Script/OpenMobileHaptics")
		) || !Enum->HasMetaData(TEXT("BlueprintType")))
		{
			continue;
		}
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			if (Enum->HasMetaData(TEXT("Hidden"), Index))
			{
				continue;
			}
			const FString ValueName = Enum->GetNameStringByIndex(Index);
			TestFalse(
				*FString::Printf(
					TEXT("%s.%s has authored display text"),
					*Enum->GetName(),
					*ValueName
				),
				Enum->GetMetaData(TEXT("DisplayName"), Index).IsEmpty()
			);
			TestFalse(
				*FString::Printf(
					TEXT("%s.%s has an authored tooltip"),
					*Enum->GetName(),
					*ValueName
				),
				Enum->GetMetaData(TEXT("ToolTip"), Index).IsEmpty()
			);
		}
	}
	return true;
}

#endif
