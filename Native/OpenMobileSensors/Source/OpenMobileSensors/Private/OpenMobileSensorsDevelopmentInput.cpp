#include "OpenMobileSensorsDevelopmentInput.h"

#include "OpenMobileSensorsDevelopmentInputService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsSettings.h"

namespace OpenMobileSensorsDevelopmentInputPrivate
{
	IOpenMobileSensorsDevelopmentInputProvider* Provider = nullptr;
	constexpr const TCHAR* DevelopmentInputDomain =
		TEXT("OpenMobileSensors.DevelopmentInput");

	FOpenMobileSensorOperationResult MakeMocksInactiveResult(
		const TCHAR* NativeCode
	)
	{
		FOpenMobileSensorOperationResult Result =
			FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::ConfigurationBlocked,
				DevelopmentInputDomain,
				NativeCode
			);
		Result.Error.Message = TEXT("Mocks are inactive.");
		Result.Failure.Correction = TEXT("Set Development Input Mode to Mock in Project Settings > OpenMobile > OpenMobile Sensors, then run a non-Shipping build.");
		return Result;
	}

	FOpenMobileSensorOperationResult GetProviderResult(
		IOpenMobileSensorsDevelopmentInputProvider*& OutProvider
	)
	{
		OutProvider = nullptr;
#if UE_BUILD_SHIPPING
		return MakeMocksInactiveResult(
			TEXT("DevelopmentInputDisabledInShipping")
		);
#else
		if (!IsInGameThread())
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidRequest,
				TEXT("OpenMobileSensors.DevelopmentInput"),
				TEXT("GameThreadRequired")
			);
		}
		const UOpenMobileSensorsSettings* Settings =
			GetDefault<UOpenMobileSensorsSettings>();
		if (!Settings
			|| Settings->DevelopmentInputMode !=
				EOpenMobileSensorsDevelopmentInputMode::Mock)
		{
			return MakeMocksInactiveResult(
				TEXT("MockInputNotSelected")
			);
		}
		OutProvider = FOpenMobileSensorsDevelopmentInputService::GetProvider();
		if (!OutProvider || !OutProvider->IsActive())
		{
			return MakeMocksInactiveResult(
				TEXT("MockInputProviderUnavailable")
			);
		}
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::Success;
		return Result;
#endif
	}

	void ResolveOutcome(
		const FOpenMobileSensorOperationResult& Result,
		EOpenMobileSensorMockActionOutcome& Outcome,
		FText& Message,
		FText& Correction,
		FOpenMobileSensorOperationResult& Details
	)
	{
		Details = Result;
		Message = FText::FromString(Result.Error.Message);
		Correction = FText::FromString(Result.Failure.Correction);
		if (Result.IsSuccess())
		{
			Outcome = EOpenMobileSensorMockActionOutcome::Applied;
			Message = NSLOCTEXT(
				"OpenMobileSensorsDevelopment",
				"MockActionApplied",
				"The sensor mock action was applied."
			);
			Correction = FText::GetEmpty();
		}
		else if (Result.Failure.NativeDomain == DevelopmentInputDomain)
		{
			Outcome = EOpenMobileSensorMockActionOutcome::MocksInactive;
		}
		else
		{
			Outcome = EOpenMobileSensorMockActionOutcome::Failed;
		}
	}
}

bool FOpenMobileSensorsDevelopmentInputService::RegisterProvider(
	IOpenMobileSensorsDevelopmentInputProvider& InProvider
)
{
#if UE_BUILD_SHIPPING
	static_cast<void>(InProvider);
	return false;
#else
	using namespace OpenMobileSensorsDevelopmentInputPrivate;
	if (!IsInGameThread() || Provider)
	{
		return false;
	}
	Provider = &InProvider;
	return true;
#endif
}

bool FOpenMobileSensorsDevelopmentInputService::UnregisterProvider(
	IOpenMobileSensorsDevelopmentInputProvider& InProvider
)
{
#if UE_BUILD_SHIPPING
	static_cast<void>(InProvider);
	return false;
#else
	using namespace OpenMobileSensorsDevelopmentInputPrivate;
	if (!IsInGameThread() || Provider != &InProvider)
	{
		return false;
	}
	Provider = nullptr;
	return true;
#endif
}

IOpenMobileSensorsDevelopmentInputProvider*
FOpenMobileSensorsDevelopmentInputService::GetProvider()
{
#if UE_BUILD_SHIPPING
	return nullptr;
#else
	return OpenMobileSensorsDevelopmentInputPrivate::Provider;
#endif
}

void UOpenMobileSensorsDevelopmentLibrary::ApplyMockInputWithOutcome(
	const FOpenMobileSensorsMockInput& Input,
	EOpenMobileSensorMockActionOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details
)
{
	OpenMobileSensorsDevelopmentInputPrivate::ResolveOutcome(
		ApplyMockInput(Input), Outcome, Message, Correction, Details);
}

void UOpenMobileSensorsDevelopmentLibrary::ApplyMockPresetWithOutcome(
	EOpenMobileSensorsMockPreset Preset,
	EOpenMobileSensorMockActionOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details
)
{
	OpenMobileSensorsDevelopmentInputPrivate::ResolveOutcome(
		ApplyMockPreset(Preset), Outcome, Message, Correction, Details);
}

void UOpenMobileSensorsDevelopmentLibrary::PlayMockTimelineWithOutcome(
	const FOpenMobileSensorsMockTimeline& Timeline,
	EOpenMobileSensorMockActionOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details
)
{
	OpenMobileSensorsDevelopmentInputPrivate::ResolveOutcome(
		PlayMockTimeline(Timeline), Outcome, Message, Correction, Details);
}

void UOpenMobileSensorsDevelopmentLibrary::StopMockTimelineWithOutcome(
	EOpenMobileSensorMockActionOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details
)
{
	OpenMobileSensorsDevelopmentInputPrivate::ResolveOutcome(
		StopMockTimeline(), Outcome, Message, Correction, Details);
}

void UOpenMobileSensorsDevelopmentLibrary::AdvanceMockTimelineWithOutcome(
	double DeltaSeconds,
	EOpenMobileSensorMockActionOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details
)
{
	OpenMobileSensorsDevelopmentInputPrivate::ResolveOutcome(
		AdvanceMockTimeline(DeltaSeconds),
		Outcome,
		Message,
		Correction,
		Details
	);
}

void UOpenMobileSensorsDevelopmentLibrary::InjectMockErrorWithOutcome(
	EOpenMobileSensorType Sensor,
	EOpenMobileSensorFailureReason FailureReason,
	FString NativeCode,
	EOpenMobileSensorMockActionOutcome& Outcome,
	FText& Message,
	FText& Correction,
	FOpenMobileSensorOperationResult& Details
)
{
	OpenMobileSensorsDevelopmentInputPrivate::ResolveOutcome(
		InjectMockError(Sensor, FailureReason, MoveTemp(NativeCode)),
		Outcome,
		Message,
		Correction,
		Details
	);
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsDevelopmentLibrary::ApplyMockInput(
	const FOpenMobileSensorsMockInput& Input
)
{
	IOpenMobileSensorsDevelopmentInputProvider* Provider = nullptr;
	FOpenMobileSensorOperationResult Result =
		OpenMobileSensorsDevelopmentInputPrivate::GetProviderResult(Provider);
	return Result.IsSuccess() ? Provider->ApplyInput(Input) : Result;
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsDevelopmentLibrary::ApplyMockPreset(
	EOpenMobileSensorsMockPreset Preset
)
{
	IOpenMobileSensorsDevelopmentInputProvider* Provider = nullptr;
	FOpenMobileSensorOperationResult Result =
		OpenMobileSensorsDevelopmentInputPrivate::GetProviderResult(Provider);
	return Result.IsSuccess() ? Provider->ApplyPreset(Preset) : Result;
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsDevelopmentLibrary::PlayMockTimeline(
	const FOpenMobileSensorsMockTimeline& Timeline
)
{
	IOpenMobileSensorsDevelopmentInputProvider* Provider = nullptr;
	FOpenMobileSensorOperationResult Result =
		OpenMobileSensorsDevelopmentInputPrivate::GetProviderResult(Provider);
	return Result.IsSuccess() ? Provider->PlayTimeline(Timeline) : Result;
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsDevelopmentLibrary::StopMockTimeline()
{
	IOpenMobileSensorsDevelopmentInputProvider* Provider = nullptr;
	FOpenMobileSensorOperationResult Result =
		OpenMobileSensorsDevelopmentInputPrivate::GetProviderResult(Provider);
	return Result.IsSuccess() ? Provider->StopTimeline() : Result;
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsDevelopmentLibrary::AdvanceMockTimeline(
	double DeltaSeconds
)
{
	IOpenMobileSensorsDevelopmentInputProvider* Provider = nullptr;
	FOpenMobileSensorOperationResult Result =
		OpenMobileSensorsDevelopmentInputPrivate::GetProviderResult(Provider);
	return Result.IsSuccess()
		? Provider->AdvanceTimeline(DeltaSeconds)
		: Result;
}

FOpenMobileSensorOperationResult
UOpenMobileSensorsDevelopmentLibrary::InjectMockError(
	EOpenMobileSensorType Sensor,
	EOpenMobileSensorFailureReason FailureReason,
	FString NativeCode
)
{
	IOpenMobileSensorsDevelopmentInputProvider* Provider = nullptr;
	FOpenMobileSensorOperationResult Result =
		OpenMobileSensorsDevelopmentInputPrivate::GetProviderResult(Provider);
	return Result.IsSuccess()
		? Provider->InjectError(Sensor, FailureReason, NativeCode)
		: Result;
}

bool UOpenMobileSensorsDevelopmentLibrary::IsMockInputActive()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!IsInGameThread())
	{
		return false;
	}
	const UOpenMobileSensorsSettings* Settings =
		GetDefault<UOpenMobileSensorsSettings>();
	IOpenMobileSensorsDevelopmentInputProvider* Provider =
		FOpenMobileSensorsDevelopmentInputService::GetProvider();
	return Settings
		&& Settings->DevelopmentInputMode ==
			EOpenMobileSensorsDevelopmentInputMode::Mock
		&& Provider
		&& Provider->IsActive();
#endif
}
