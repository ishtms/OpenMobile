#include "OpenMobileSensorsBackendRegistry.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileSensorsBackend.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMetadataService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsBackendRegistryPrivate
{
	TAtomic<uint64> Generation(1);
	TAtomic<bool> bShuttingDown(false);
	TSet<IOpenMobileSensorsBackend*> ShutdownBackends;

	void AdvanceGeneration()
	{
		Generation++;
		if (Generation.Load() == 0)
		{
			Generation++;
		}
		FOpenMobileSensorsSubscriptionService::
			HandleBackendGenerationChanged();
		FOpenMobileSensorsCapabilityService::
			HandleBackendGenerationChanged();
		FOpenMobileSensorsMetadataService::
			HandleBackendGenerationChanged();
	}

	TArray<IOpenMobileSensorsBackend*> GetBackends()
	{
		return IModularFeatures::Get()
			.GetModularFeatureImplementations<IOpenMobileSensorsBackend>(
				IOpenMobileSensorsBackend::GetModularFeatureName()
			);
	}

	void StopBackend(IOpenMobileSensorsBackend& Backend)
	{
		if (!ShutdownBackends.Contains(&Backend))
		{
			ShutdownBackends.Add(&Backend);
			Backend.BeginShutdown();
		}
	}
}

void FOpenMobileSensorsBackendRegistry::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsBackendRegistryPrivate;
	if (bShuttingDown.Exchange(false))
	{
		ShutdownBackends.Reset();
		AdvanceGeneration();
	}
}

bool FOpenMobileSensorsBackendRegistry::RegisterBackend(
	IOpenMobileSensorsBackend& Backend
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsBackendRegistryPrivate;
	if (bShuttingDown.Load() || Backend.GetBackendName().IsNone())
	{
		return false;
	}

	for (IOpenMobileSensorsBackend* Registered : GetBackends())
	{
		if (Registered == &Backend
			|| (Registered && Registered->GetBackendName() == Backend.GetBackendName()))
		{
			return false;
		}
	}

	ShutdownBackends.Remove(&Backend);
	IModularFeatures::Get().RegisterModularFeature(
		IOpenMobileSensorsBackend::GetModularFeatureName(),
		&Backend
	);
	AdvanceGeneration();
	return true;
}

bool FOpenMobileSensorsBackendRegistry::UnregisterBackend(
	IOpenMobileSensorsBackend& Backend
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsBackendRegistryPrivate;
	if (!GetBackends().Contains(&Backend))
	{
		return false;
	}

	StopBackend(Backend);
	IModularFeatures::Get().UnregisterModularFeature(
		IOpenMobileSensorsBackend::GetModularFeatureName(),
		&Backend
	);
	ShutdownBackends.Remove(&Backend);
	AdvanceGeneration();
	return true;
}

bool FOpenMobileSensorsBackendRegistry::IsBackendRegistered(
	const IOpenMobileSensorsBackend* Backend
)
{
	check(IsInGameThread());
	return Backend
		&& OpenMobileSensorsBackendRegistryPrivate::GetBackends().Contains(Backend);
}

IOpenMobileSensorsBackend* FOpenMobileSensorsBackendRegistry::FindBackend()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsBackendRegistryPrivate;
	if (bShuttingDown.Load())
	{
		return nullptr;
	}

	IOpenMobileSensorsBackend* Best = nullptr;
	for (IOpenMobileSensorsBackend* Candidate : GetBackends())
	{
		if (!Candidate || !Candidate->IsAvailable())
		{
			continue;
		}

		const bool bHigherPriority = !Best
			|| Candidate->GetPriority() > Best->GetPriority();
		const bool bStableTieBreak = Best
			&& Candidate->GetPriority() == Best->GetPriority()
			&& Candidate->GetBackendName().LexicalLess(Best->GetBackendName());
		if (bHigherPriority || bStableTieBreak)
		{
			Best = Candidate;
		}
	}
	return Best;
}

FOpenMobileSensorsBackendToken FOpenMobileSensorsBackendRegistry::CaptureToken()
{
	check(IsInGameThread());
	if (!FindBackend())
	{
		return {};
	}

	FOpenMobileSensorsBackendToken Token;
	Token.Generation = OpenMobileSensorsBackendRegistryPrivate::Generation.Load();
	return Token;
}

bool FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
	const FOpenMobileSensorsBackendToken& Token
)
{
	using namespace OpenMobileSensorsBackendRegistryPrivate;
	return Token.Generation != 0
		&& !bShuttingDown.Load()
		&& Token.Generation == Generation.Load();
}

bool FOpenMobileSensorsBackendRegistry::IsShuttingDown()
{
	return OpenMobileSensorsBackendRegistryPrivate::bShuttingDown.Load();
}

void FOpenMobileSensorsBackendRegistry::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsBackendRegistryPrivate;
	if (bShuttingDown.Exchange(true))
	{
		return;
	}

	AdvanceGeneration();
	for (IOpenMobileSensorsBackend* Backend : GetBackends())
	{
		if (Backend)
		{
			StopBackend(*Backend);
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileSensorsBackendRegistry::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsBackendRegistryPrivate;
	bShuttingDown.Store(false);
	ShutdownBackends.Reset();
	AdvanceGeneration();
}
#endif
