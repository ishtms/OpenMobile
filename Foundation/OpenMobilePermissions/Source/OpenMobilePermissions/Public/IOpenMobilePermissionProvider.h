#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobilePermissionTypes.h"

DECLARE_DELEGATE_TwoParams(
	FOpenMobileNativePermissionCompletion,
	EOpenMobilePermissionStatus,
	FOpenMobileError
);

class OPENMOBILEPERMISSIONS_API IOpenMobilePermissionProvider
	: public IModularFeature
{
public:
	virtual ~IOpenMobilePermissionProvider() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Permissions.Provider"));
		return FeatureName;
	}

	virtual FName GetProviderName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsAvailable() const { return true; }
	virtual bool SupportsPermission(FName Permission) const = 0;
	virtual FOpenMobilePermissionResult GetStatus(FName Permission) const = 0;

	virtual bool RequestPermission(
		FName Permission,
		const FGuid& RequestIdentifier,
		FOpenMobileNativePermissionCompletion&& Completion,
		FOpenMobileError& OutError
	) = 0;

	virtual void CancelRequest(const FGuid& RequestIdentifier)
	{
		static_cast<void>(RequestIdentifier);
	}

	virtual void BeginShutdown() {}
};

class OPENMOBILEPERMISSIONS_API FOpenMobilePermissionProviderRegistry final
{
public:
	static bool RegisterProvider(IOpenMobilePermissionProvider& Provider);
	static bool UnregisterProvider(IOpenMobilePermissionProvider& Provider);
	static IOpenMobilePermissionProvider* FindProvider(FName Permission);
	static void Start();
	static void BeginShutdown();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};
