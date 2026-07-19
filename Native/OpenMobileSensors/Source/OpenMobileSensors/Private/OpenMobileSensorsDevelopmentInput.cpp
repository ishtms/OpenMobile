#include "OpenMobileSensorsDevelopmentInput.h"

#include "OpenMobileSensorsDevelopmentInputService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsSettings.h"

namespace OpenMobileSensorsDevelopmentInputPrivate
{
	IOpenMobileSensorsDevelopmentInputProvider* Provider = nullptr;

	FOpenMobileSensorOperationResult GetProviderResult(
		IOpenMobileSensorsDevelopmentInputProvider*& OutProvider
	)
	{
		OutProvider = nullptr;
#if UE_BUILD_SHIPPING
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::ConfigurationBlocked,
			TEXT("OpenMobileSensors.DevelopmentInput"),
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
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::ConfigurationBlocked,
				TEXT("OpenMobileSensors.DevelopmentInput"),
				TEXT("MockInputNotSelected")
			);
		}
		OutProvider = FOpenMobileSensorsDevelopmentInputService::GetProvider();
		if (!OutProvider || !OutProvider->IsActive())
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::TemporarilyUnavailable,
				TEXT("OpenMobileSensors.DevelopmentInput"),
				TEXT("MockInputProviderUnavailable")
			);
		}
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::Success;
		return Result;
#endif
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
