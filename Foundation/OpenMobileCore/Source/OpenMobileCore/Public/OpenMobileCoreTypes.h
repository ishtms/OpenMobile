#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.generated.h"

/** Stable, provider-independent error categories shared by every OpenMobile plugin. */
UENUM(BlueprintType)
enum class EOpenMobileErrorCode : uint8
{
	None,
	NotSupported,
	NotConfigured,
	Unavailable,
	Busy,
	Cancelled,
	InvalidArgument,
	NativeFailure,
	Internal
};

/** Public error value. Native/provider details stay data, never public SDK types. */
USTRUCT(BlueprintType)
struct OPENMOBILECORE_API FOpenMobileError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Error")
	EOpenMobileErrorCode Code = EOpenMobileErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Error")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Error")
	FString NativeCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Error")
	FString Provider;

	bool IsSet() const
	{
		return Code != EOpenMobileErrorCode::None;
	}

	static FOpenMobileError Make(
		EOpenMobileErrorCode InCode,
		FString InMessage,
		FString InNativeCode = FString(),
		FString InProvider = FString()
	)
	{
		FOpenMobileError Error;
		Error.Code = InCode;
		Error.Message = MoveTemp(InMessage);
		Error.NativeCode = MoveTemp(InNativeCode);
		Error.Provider = MoveTemp(InProvider);
		return Error;
	}
};

UENUM(BlueprintType)
enum class EOpenMobileCapabilityState : uint8
{
	Available,
	Unavailable,
	NotSupported,
	NotConfigured,
	PermissionRequired,
	Denied,
	Restricted,
	TemporarilyUnavailable
};

USTRUCT(BlueprintType)
struct OPENMOBILECORE_API FOpenMobileCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Capability")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Capability")
	EOpenMobileCapabilityState State = EOpenMobileCapabilityState::Unavailable;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Capability")
	FString Detail;

	bool IsAvailable() const
	{
		return State == EOpenMobileCapabilityState::Available;
	}
};
