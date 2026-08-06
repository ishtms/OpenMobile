#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileHaptics.h"
#include "OpenMobileHapticsAsyncAction.h"
#include "UObject/UnrealType.h"

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
				TEXT("%s uses the Haptics category"),
				*FunctionName.ToString()
			),
			Function->GetMetaData(TEXT("Category")),
			FString(TEXT("Open Mobile|Haptics"))
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
		TestNotNull(
			TEXT("Preparation exposes its async factory"),
			PreparationActionClass->FindFunctionByName(TEXT("PrepareHapticsAsync"))
		);
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

#endif
