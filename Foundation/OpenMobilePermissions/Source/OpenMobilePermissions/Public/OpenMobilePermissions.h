#pragma once

#include "CoreMinimal.h"
#include "OpenMobilePermissionTypes.h"

class IOpenMobilePermissionProvider;

class OPENMOBILEPERMISSIONS_API FOpenMobilePermissions final
{
public:
	static FOpenMobilePermissionResult GetStatus(FName Permission);

	static FOpenMobilePermissionRequestHandle RequestPermission(
		FName Permission,
		FOnOpenMobilePermissionRequestComplete&& Completion
	);

	static bool CancelRequest(
		const FOpenMobilePermissionRequestHandle& Handle
	);

private:
	friend class FOpenMobilePermissionProviderRegistry;
	friend class FOpenMobilePermissionsModule;

	static void Start();
	static void BeginShutdown();
	static void FailRequestsForProvider(
		IOpenMobilePermissionProvider& Provider,
		const FOpenMobileError& Error
	);

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};
