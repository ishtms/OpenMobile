#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceBrightnessControl.generated.h"

class UOpenMobileDeviceSubsystem;

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileBrightnessSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat CurrentBrightness;

	bool operator==(const FOpenMobileBrightnessSnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& CurrentBrightness == Other.CurrentBrightness;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileBrightnessRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	float Brightness = 1.0f;

	bool operator==(const FOpenMobileBrightnessRequest& Other) const
	{
		return Brightness == Other.Brightness;
	}
};

UENUM(BlueprintType)
enum class EOpenMobileBrightnessApplyState : uint8
{
	Unknown,
	Applied,
	Accepted,
	Rejected,
	Unsupported
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileBrightnessResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileBrightnessApplyState State =
		EOpenMobileBrightnessApplyState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileBrightnessRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat EffectiveBrightness;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;

	bool IsAccepted() const
	{
		return State == EOpenMobileBrightnessApplyState::Applied
			|| State == EOpenMobileBrightnessApplyState::Accepted;
	}
};

UCLASS(BlueprintType, Transient)
class OPENMOBILEDEVICE_API UOpenMobileBrightnessHandle final : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileBrightnessRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileBrightnessResult Result;

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
