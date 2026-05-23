#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceRefreshRateControl.generated.h"

class UOpenMobileDeviceSubsystem;

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobilePreferredRefreshRateRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	bool bUsePreferredMinimumHz = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	float PreferredMinimumHz = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	bool bUsePreferredMaximumHz = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	float PreferredMaximumHz = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	bool bUsePreferredTargetHz = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	float PreferredTargetHz = 60.0f;

	bool operator==(const FOpenMobilePreferredRefreshRateRequest& Other) const
	{
		return bUsePreferredMinimumHz == Other.bUsePreferredMinimumHz
			&& PreferredMinimumHz == Other.PreferredMinimumHz
			&& bUsePreferredMaximumHz == Other.bUsePreferredMaximumHz
			&& PreferredMaximumHz == Other.PreferredMaximumHz
			&& bUsePreferredTargetHz == Other.bUsePreferredTargetHz
			&& PreferredTargetHz == Other.PreferredTargetHz;
	}
};

UENUM(BlueprintType)
enum class EOpenMobilePreferredRefreshRateApplyState : uint8
{
	Unknown,
	Applied,
	Accepted,
	Rejected,
	Unsupported
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobilePreferredRefreshRateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobilePreferredRefreshRateApplyState State =
		EOpenMobilePreferredRefreshRateApplyState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobilePreferredRefreshRateRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat EffectiveRefreshRateHz;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;

	bool IsAccepted() const
	{
		return State == EOpenMobilePreferredRefreshRateApplyState::Applied
			|| State == EOpenMobilePreferredRefreshRateApplyState::Accepted;
	}
};

UCLASS(BlueprintType, Transient)
class OPENMOBILEDEVICE_API UOpenMobilePreferredRefreshRateHandle final
	: public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobilePreferredRefreshRateRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobilePreferredRefreshRateResult Result;

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
