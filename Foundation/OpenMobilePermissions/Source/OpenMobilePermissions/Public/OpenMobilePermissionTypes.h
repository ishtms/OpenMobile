#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobilePermissionTypes.generated.h"

class FOpenMobilePermissions;

UENUM(BlueprintType)
enum class EOpenMobilePermissionStatus : uint8
{
	NotDetermined,
	Granted,
	Denied,
	Restricted,
	PermanentlyDenied
};

USTRUCT(BlueprintType)
struct OPENMOBILEPERMISSIONS_API FOpenMobilePermissionRequestHandle
{
	GENERATED_BODY()

	bool IsValid() const
	{
		return Identifier.IsValid() && Generation != 0;
	}

	void Reset()
	{
		Identifier.Invalidate();
		Generation = 0;
	}

	bool operator==(const FOpenMobilePermissionRequestHandle& Other) const
	{
		return Identifier == Other.Identifier && Generation == Other.Generation;
	}

	bool operator!=(const FOpenMobilePermissionRequestHandle& Other) const
	{
		return !(*this == Other);
	}

	FGuid GetIdentifier() const
	{
		return Identifier;
	}

private:
	FGuid Identifier;
	uint32 Generation = 0;

	friend class FOpenMobilePermissions;
};

USTRUCT(BlueprintType)
struct OPENMOBILEPERMISSIONS_API FOpenMobilePermissionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Permissions")
	FName Permission;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Permissions")
	EOpenMobilePermissionStatus Status =
		EOpenMobilePermissionStatus::NotDetermined;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Permissions")
	FOpenMobileError Error;
};

DECLARE_DELEGATE_OneParam(
	FOnOpenMobilePermissionRequestComplete,
	const FOpenMobilePermissionResult&
);
