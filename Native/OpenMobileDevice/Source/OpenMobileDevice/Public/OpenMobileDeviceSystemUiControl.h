#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceSystemUiControl.generated.h"

class UOpenMobileDeviceSubsystem;

UENUM(BlueprintType)
enum class EOpenMobileSystemUiMode : uint8
{
	Normal,
	EdgeToEdge,
	Immersive
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileSystemUiRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	EOpenMobileSystemUiMode Mode = EOpenMobileSystemUiMode::Normal;

	bool operator==(const FOpenMobileSystemUiRequest& Other) const
	{
		return Mode == Other.Mode;
	}
};

UENUM(BlueprintType)
enum class EOpenMobileSystemUiApplyState : uint8
{
	Unknown,
	Applied,
	Accepted,
	Restricted,
	Rejected,
	Unsupported
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileSystemUiResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileSystemUiApplyState State = EOpenMobileSystemUiApplyState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileSystemUiRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bEffectiveModeAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileSystemUiMode EffectiveMode = EOpenMobileSystemUiMode::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;

	bool IsAccepted() const
	{
		return State == EOpenMobileSystemUiApplyState::Applied
			|| State == EOpenMobileSystemUiApplyState::Accepted;
	}
};

UCLASS(BlueprintType, Transient)
class OPENMOBILEDEVICE_API UOpenMobileSystemUiHandle final : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileSystemUiRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileSystemUiResult Result;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Device")
	void Release();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Device")
	bool IsActive() const { return bActive; }

	virtual void BeginDestroy() override;

private:
	friend class UOpenMobileDeviceSubsystem;

	TWeakObjectPtr<UOpenMobileDeviceSubsystem> Subsystem;
	FGuid RequestId;
	bool bActive = false;
};
