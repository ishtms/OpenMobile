#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceOrientationControl.generated.h"

class UOpenMobileDeviceSubsystem;

UENUM(BlueprintType)
enum class EOpenMobileOrientationPolicy : uint8
{
	Automatic,
	Portrait,
	Landscape,
	PortraitOnly,
	PortraitUpsideDownOnly,
	LandscapeLeftOnly,
	LandscapeRightOnly
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileOrientationPolicyRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	EOpenMobileOrientationPolicy Policy =
		EOpenMobileOrientationPolicy::Automatic;

	bool operator==(const FOpenMobileOrientationPolicyRequest& Other) const
	{
		return Policy == Other.Policy;
	}
};

UENUM(BlueprintType)
enum class EOpenMobileOrientationPolicyApplyState : uint8
{
	Unknown,
	Applied,
	Accepted,
	Restricted,
	Rejected,
	Unsupported
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileOrientationPolicyResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileOrientationPolicyApplyState State =
		EOpenMobileOrientationPolicyApplyState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileOrientationPolicyRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;

	bool IsAccepted() const
	{
		return State == EOpenMobileOrientationPolicyApplyState::Applied
			|| State == EOpenMobileOrientationPolicyApplyState::Accepted;
	}
};

UCLASS(BlueprintType, Transient)
class OPENMOBILEDEVICE_API UOpenMobileOrientationPolicyHandle final
	: public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileOrientationPolicyRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileOrientationPolicyResult Result;

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
