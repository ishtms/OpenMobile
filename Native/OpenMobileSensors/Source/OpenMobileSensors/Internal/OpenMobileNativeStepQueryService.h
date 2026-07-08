#pragma once

#include "CoreMinimal.h"
#include "OpenMobileNativeStepCount.h"

class OPENMOBILESENSORS_API FOpenMobileNativeStepQueryService final
{
public:
	static void Start();
	static void BeginShutdown();
	static FGuid Query(
		const FGuid& OwnerIdentifier,
		const FOpenMobileNativeStepCountQuery& Query,
		TFunction<void(const FOpenMobileNativeStepCountQueryResult&)>&& Completion
	);
	static bool Cancel(
		const FGuid& OwnerIdentifier,
		const FGuid& RequestId
	);
	static void CancelOwner(const FGuid& OwnerIdentifier);

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};
