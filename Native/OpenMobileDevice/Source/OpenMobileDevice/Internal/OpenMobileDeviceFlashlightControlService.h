#pragma once

#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceFlashlightTypes.h"

class FOpenMobileDeviceFlashlightControlService final
{
public:
	static void Start();
	static void Shutdown();
	static FGuid BeginOperation(FOpenMobileError& OutError);
	static bool IsOperationCurrent(const FGuid& OperationId);
	static void CompleteOperation(
		const FGuid& OperationId,
		const FOpenMobileFlashlightOperationResult& Result
	);
	static void CancelOperation(const FGuid& OperationId);
	static void HandleGameInstanceTeardown();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
	static void SetApplicationActiveForTests(bool bActive);
	static bool IsTorchOnForTests();
#endif
};
