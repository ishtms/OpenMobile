#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenMobileAdsSubsystem.generated.h"

class IOpenMobileAdsProvider;

UENUM(BlueprintType)
enum class EOpenMobileRewardedAdState : uint8
{
	Idle,
	Loading,
	Showing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOpenMobileAdSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileRewardEarnedEvent,
	int32,
	NetworkAmount,
	const FString&,
	NetworkRewardType
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdFailedEvent,
	const FOpenMobileError&,
	Error
);

/** Provider-neutral rewarded-ad API. Provider SDKs live in separate plugins. */
UCLASS()
class OPENMOBILEADS_API UOpenMobileAdsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Loads and presents one rewarded ad through the selected provider. */
	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads")
	bool RequestAndShowRewardedAd();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	bool IsSupported() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	bool IsBusy() const { return State != EOpenMobileRewardedAdState::Idle; }

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	EOpenMobileRewardedAdState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	FName GetActiveProviderName() const;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdSimpleEvent OnAdLoaded;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdSimpleEvent OnAdShown;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileRewardEarnedEvent OnRewardEarned;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdSimpleEvent OnAdClosed;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdFailedEvent OnAdFailed;

private:
	IOpenMobileAdsProvider* FindProvider() const;
	void HandleAdLoaded();
	void HandleAdShown();
	void HandleRewardEarned(int32 NetworkAmount, FString NetworkRewardType);
	void HandleAdClosed();
	void HandleAdFailed(FOpenMobileError Error);

	UPROPERTY(Transient)
	EOpenMobileRewardedAdState State = EOpenMobileRewardedAdState::Idle;
};
