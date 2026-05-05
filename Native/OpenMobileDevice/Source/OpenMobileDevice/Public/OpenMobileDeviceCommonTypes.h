#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceCommonTypes.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceSnapshotMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FDateTime CapturedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	int64 Generation = 0;

	bool operator==(const FOpenMobileDeviceSnapshotMetadata& Other) const
	{
		return CapturedAtUtc == Other.CapturedAtUtc && Generation == Other.Generation;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceOptionalString
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FString Value;

	static FOpenMobileDeviceOptionalString MakeAvailable(FString InValue)
	{
		FOpenMobileDeviceOptionalString Result;
		Result.bIsAvailable = true;
		Result.Value = MoveTemp(InValue);
		return Result;
	}

	bool operator==(const FOpenMobileDeviceOptionalString& Other) const
	{
		return bIsAvailable == Other.bIsAvailable && Value == Other.Value;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceOptionalInt32
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	int32 Value = 0;

	static FOpenMobileDeviceOptionalInt32 MakeAvailable(int32 InValue)
	{
		FOpenMobileDeviceOptionalInt32 Result;
		Result.bIsAvailable = true;
		Result.Value = InValue;
		return Result;
	}

	bool operator==(const FOpenMobileDeviceOptionalInt32& Other) const
	{
		return bIsAvailable == Other.bIsAvailable && Value == Other.Value;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceOptionalInt64
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	int64 Value = 0;

	static FOpenMobileDeviceOptionalInt64 MakeAvailable(int64 InValue)
	{
		FOpenMobileDeviceOptionalInt64 Result;
		Result.bIsAvailable = true;
		Result.Value = InValue;
		return Result;
	}

	bool operator==(const FOpenMobileDeviceOptionalInt64& Other) const
	{
		return bIsAvailable == Other.bIsAvailable && Value == Other.Value;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceOptionalFloat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float Value = 0.0f;

	static FOpenMobileDeviceOptionalFloat MakeAvailable(float InValue)
	{
		FOpenMobileDeviceOptionalFloat Result;
		Result.bIsAvailable = true;
		Result.Value = InValue;
		return Result;
	}

	bool operator==(const FOpenMobileDeviceOptionalFloat& Other) const
	{
		return bIsAvailable == Other.bIsAvailable && Value == Other.Value;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceOptionalBool
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool Value = false;

	static FOpenMobileDeviceOptionalBool MakeAvailable(bool bInValue)
	{
		FOpenMobileDeviceOptionalBool Result;
		Result.bIsAvailable = true;
		Result.Value = bInValue;
		return Result;
	}

	bool operator==(const FOpenMobileDeviceOptionalBool& Other) const
	{
		return bIsAvailable == Other.bIsAvailable && Value == Other.Value;
	}
};

UENUM(BlueprintType)
enum class EOpenMobileDeviceControlResultCode : uint8
{
	Unknown,
	Succeeded,
	Cancelled,
	Failed,
	Unsupported,
	Restricted,
	PermissionRequired,
	Denied,
	Busy,
	InvalidArgument
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceControlResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FName ControlName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileDeviceControlResultCode Code = EOpenMobileDeviceControlResultCode::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;

	bool operator==(const FOpenMobileDeviceControlResult& Other) const
	{
		return ControlName == Other.ControlName
			&& Code == Other.Code
			&& Error.Code == Other.Error.Code
			&& Error.Message == Other.Error.Message
			&& Error.NativeCode == Other.Error.NativeCode
			&& Error.Provider == Other.Error.Provider;
	}
};
