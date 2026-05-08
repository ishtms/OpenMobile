#include "OpenMobileDeviceBackendRegistry.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileDeviceBackend.h"

namespace OpenMobileDeviceBackendRegistryPrivate
{
	TAtomic<uint64> Generation(1);
	TAtomic<bool> bShuttingDown(false);
	TSet<IOpenMobileDeviceBackend*> ShutdownBackends;

	void AdvanceGeneration()
	{
		Generation++;
		if (Generation.Load() == 0)
		{
			Generation++;
		}
	}

	TArray<IOpenMobileDeviceBackend*> GetBackends()
	{
		return IModularFeatures::Get()
			.GetModularFeatureImplementations<IOpenMobileDeviceBackend>(
				IOpenMobileDeviceBackend::GetModularFeatureName()
			);
	}

	void StopBackend(IOpenMobileDeviceBackend& Backend)
	{
		if (!ShutdownBackends.Contains(&Backend))
		{
			ShutdownBackends.Add(&Backend);
			Backend.BeginShutdown();
		}
	}
}

void FOpenMobileDeviceBackendRegistry::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBackendRegistryPrivate;
	if (bShuttingDown.Exchange(false))
	{
		ShutdownBackends.Reset();
		AdvanceGeneration();
	}
}

bool FOpenMobileDeviceBackendRegistry::RegisterBackend(IOpenMobileDeviceBackend& Backend)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBackendRegistryPrivate;
	if (bShuttingDown.Load() || Backend.GetBackendName().IsNone())
	{
		return false;
	}

	for (IOpenMobileDeviceBackend* Registered : GetBackends())
	{
		if (Registered == &Backend
			|| (Registered && Registered->GetBackendName() == Backend.GetBackendName()))
		{
			return false;
		}
	}

	ShutdownBackends.Remove(&Backend);
	IModularFeatures::Get().RegisterModularFeature(
		IOpenMobileDeviceBackend::GetModularFeatureName(),
		&Backend
	);
	AdvanceGeneration();
	return true;
}

bool FOpenMobileDeviceBackendRegistry::UnregisterBackend(IOpenMobileDeviceBackend& Backend)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBackendRegistryPrivate;
	if (!GetBackends().Contains(&Backend))
	{
		return false;
	}

	StopBackend(Backend);
	IModularFeatures::Get().UnregisterModularFeature(
		IOpenMobileDeviceBackend::GetModularFeatureName(),
		&Backend
	);
	ShutdownBackends.Remove(&Backend);
	AdvanceGeneration();
	return true;
}

bool FOpenMobileDeviceBackendRegistry::IsBackendRegistered(
	const IOpenMobileDeviceBackend* Backend
)
{
	check(IsInGameThread());
	return Backend
		&& OpenMobileDeviceBackendRegistryPrivate::GetBackends().Contains(Backend);
}

IOpenMobileDeviceBackend* FOpenMobileDeviceBackendRegistry::FindBackend()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBackendRegistryPrivate;
	if (bShuttingDown.Load())
	{
		return nullptr;
	}

	IOpenMobileDeviceBackend* Best = nullptr;
	for (IOpenMobileDeviceBackend* Candidate : GetBackends())
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

FOpenMobileDeviceCallbackToken FOpenMobileDeviceBackendRegistry::CaptureCallbackToken()
{
	check(IsInGameThread());
	if (!FindBackend())
	{
		return {};
	}

	FOpenMobileDeviceCallbackToken Token;
	Token.Generation = OpenMobileDeviceBackendRegistryPrivate::Generation.Load();
	return Token;
}

bool FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(
	const FOpenMobileDeviceCallbackToken& Token
)
{
	using namespace OpenMobileDeviceBackendRegistryPrivate;
	return Token.Generation != 0
		&& !bShuttingDown.Load()
		&& Token.Generation == Generation.Load();
}

bool FOpenMobileDeviceBackendRegistry::IsShuttingDown()
{
	return OpenMobileDeviceBackendRegistryPrivate::bShuttingDown.Load();
}

void FOpenMobileDeviceBackendRegistry::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBackendRegistryPrivate;
	if (bShuttingDown.Exchange(true))
	{
		return;
	}

	AdvanceGeneration();
	for (IOpenMobileDeviceBackend* Backend : GetBackends())
	{
		if (Backend)
		{
			StopBackend(*Backend);
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceBackendRegistry::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBackendRegistryPrivate;
	bShuttingDown.Store(false);
	ShutdownBackends.Reset();
	AdvanceGeneration();
}
#endif
