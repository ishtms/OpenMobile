#include "OpenMobileCoreModule.h"

#include "Modules/ModuleManager.h"
#include "OpenMobileCoreLog.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogOpenMobile);

namespace OpenMobileCoreLogPrivate
{
#if UE_BUILD_SHIPPING
	constexpr int32 DefaultGlobalLevel = 0;
#else
	constexpr int32 DefaultGlobalLevel = 2;
#endif

	TAutoConsoleVariable<int32> CVarGlobalLogLevel(
		TEXT("OpenMobile.LogLevel"),
		DefaultGlobalLevel,
		TEXT("OpenMobile log level: -1 off, 0 error, 1 warning, 2 info, 3 verbose, 4 very verbose."),
		ECVF_Default
	);
}

int32 FOpenMobileLogFilter::GetGlobalLevel()
{
	return FMath::Clamp(
		OpenMobileCoreLogPrivate::CVarGlobalLogLevel.GetValueOnAnyThread(),
		-1,
		4
	);
}

int32 FOpenMobileLogFilter::GetDefaultGlobalLevel()
{
	return OpenMobileCoreLogPrivate::DefaultGlobalLevel;
}

void FOpenMobileCoreModule::StartupModule()
{
}

void FOpenMobileCoreModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FOpenMobileCoreModule, OpenMobileCore)
