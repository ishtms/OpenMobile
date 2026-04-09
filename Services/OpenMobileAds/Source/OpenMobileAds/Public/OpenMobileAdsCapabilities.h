#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsTypes.h"
#include "OpenMobileAdsCapabilities.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdFormatCapabilities
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanLoad = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanShow = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanDestroy = true;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsPreload = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsRefresh = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsCancellation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsImpression = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsClick = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsDismiss = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsReward = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsRevenue = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsServerVerification = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	int32 MaxCachedAdsPerPlacement = 1;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsProviderCapabilities
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString ProviderVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	TArray<FOpenMobileAdFormatCapabilities> Formats;

	const FOpenMobileAdFormatCapabilities* FindFormat(EOpenMobileAdFormat Format) const
	{
		return Formats.FindByPredicate(
			[Format](const FOpenMobileAdFormatCapabilities& Candidate)
			{
				return Candidate.Format == Format;
			}
		);
	}

	bool SupportsFormat(EOpenMobileAdFormat Format) const
	{
		const FOpenMobileAdFormatCapabilities* Capabilities = FindFormat(Format);
		return Capabilities && Capabilities->bCanLoad;
	}
};
