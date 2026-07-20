#pragma once

#include "CoreMinimal.h"

class SDockTab;
class FSpawnTabArgs;

class FOpenMobileSensorsDiagnosticsScreen final
{
public:
	static void Register();
	static void Unregister();
	static TSharedRef<SDockTab> Spawn(const FSpawnTabArgs& Args);
};
