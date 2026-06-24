#pragma once

#include "OpenMobileDeviceDiagnostics.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceDiagnosticsSource final
{
public:
	static FOpenMobileDeviceDiagnosticsSnapshot Capture();
	static void RecordError(FName Operation, const FOpenMobileError& Error);
	static int32 GetMaximumCapabilityCount();
	static int32 GetMaximumRecentErrorCount();
	static int32 GetMaximumConfigurationIssueCount();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
#endif
};
