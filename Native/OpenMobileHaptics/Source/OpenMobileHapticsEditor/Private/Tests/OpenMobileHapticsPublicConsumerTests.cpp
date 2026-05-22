#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileHaptics.h"
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
	FOpenMobileHapticSemanticRequest SemanticRequest;
	FOpenMobileHapticOneShotRequest OneShotRequest;
	FOpenMobileHapticNamedPatternRequest NamedRequest;
	FOpenMobileHapticPattern Pattern;
	FOpenMobileHapticPlaybackOptions Options;
	FOpenMobileHapticPlaybackHandle Handle;
	FOpenMobileHapticPlaybackResult PlaybackResult;
	FOpenMobileHapticPlaybackEvent PlaybackEvent;
	FOpenMobileHapticUserPolicy Policy;
	FOpenMobileHapticsDiagnostics Diagnostics;
	static_cast<void>(Capabilities);
	static_cast<void>(SemanticRequest);
	static_cast<void>(OneShotRequest);
	static_cast<void>(NamedRequest);
	static_cast<void>(Pattern);
	static_cast<void>(Options);
	static_cast<void>(Handle);
	static_cast<void>(PlaybackResult);
	static_cast<void>(PlaybackEvent);
	static_cast<void>(Policy);
	static_cast<void>(Diagnostics);

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

#endif
