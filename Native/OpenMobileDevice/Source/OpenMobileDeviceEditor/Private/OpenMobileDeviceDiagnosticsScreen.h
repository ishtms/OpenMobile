#pragma once

#include "CoreMinimal.h"

class SDockTab;
class FSpawnTabArgs;

class FOpenMobileDeviceDiagnosticsScreen final
{
public:
	static void Register();
	static void Unregister();

private:
	static TSharedRef<SDockTab> Spawn(const FSpawnTabArgs& Args);
};
