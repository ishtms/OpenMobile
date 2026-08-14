#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileAdsPrivacy.h"

/** Hides platform tracking frameworks behind one discoverable authorization contract. */
class IOpenMobileAdsTrackingAuthorizationBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileAdsTrackingAuthorizationBackend() = default;

	/** Uses one modular feature key so the core service stays free of platform framework includes. */
	static FName GetModularFeatureName()
	{
		static const FName FeatureName(
			TEXT("OpenMobile.Ads.TrackingAuthorization.Backend")
		);
		return FeatureName;
	}

	/** Returns the stable backend name used to break equal-priority selection deterministically. */
	virtual FName GetBackendName() const = 0;
	/** Lets a project override choose among multiple available platform backends. */
	virtual int32 GetPriority() const { return 0; }
	/** Reports runtime framework availability without triggering the system prompt. */
	virtual bool IsAvailable() const = 0;
	/** Reads the current platform answer without requesting a new decision. */
	virtual EOpenMobileAdsTrackingAuthorizationStatus GetStatus() const = 0;
	/** Checks identifier bytes separately because authorization alone doesn't guarantee a usable value. */
	virtual bool HasNonZeroAdvertisingIdentifier() const = 0;
	/** Requests one platform decision and returns setup failure synchronously when no prompt can start. */
	virtual bool RequestAuthorization(
		TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>&& Completion,
		FString& OutError
	) = 0;
};
