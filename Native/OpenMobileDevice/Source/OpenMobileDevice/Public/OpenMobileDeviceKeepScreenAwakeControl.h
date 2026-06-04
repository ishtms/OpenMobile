#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceKeepScreenAwakeControl.generated.h"

class UOpenMobileDeviceSubsystem;

UENUM(BlueprintType)
enum class EOpenMobileKeepScreenAwakeApplyState : uint8
{
	Unknown,
	Applied,
	Accepted,
	Rejected,
	Unsupported
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileKeepScreenAwakeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileKeepScreenAwakeApplyState State =
		EOpenMobileKeepScreenAwakeApplyState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bEffectiveKeepScreenAwake;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;

	bool IsAccepted() const
	{
		return State == EOpenMobileKeepScreenAwakeApplyState::Applied
			|| State == EOpenMobileKeepScreenAwakeApplyState::Accepted;
	}
};

UCLASS(BlueprintType, Transient)
class OPENMOBILEDEVICE_API UOpenMobileKeepScreenAwakeHandle final
	: public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileKeepScreenAwakeResult Result;

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
