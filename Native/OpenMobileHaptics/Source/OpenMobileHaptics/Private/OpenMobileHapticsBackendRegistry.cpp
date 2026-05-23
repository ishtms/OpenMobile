#include "OpenMobileHapticsBackendRegistry.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileHapticsBackend.h"

namespace OpenMobileHapticsBackendRegistryPrivate
{
	TAtomic<uint64> Generation(1);
	TAtomic<bool> bShuttingDown(false);
	uint64 NextRequestId = 1;
	TSet<IOpenMobileHapticsBackend*> ShutdownBackends;

	void AdvanceGeneration()
	{
		Generation++;
		if (Generation.Load() == 0)
		{
			Generation++;
		}
	}

	uint64 AllocateRequestId()
	{
		const uint64 RequestId = NextRequestId++;
		if (NextRequestId == 0)
		{
			NextRequestId = 1;
		}
		return RequestId;
	}

	TArray<IOpenMobileHapticsBackend*> GetBackends()
	{
		return IModularFeatures::Get()
			.GetModularFeatureImplementations<IOpenMobileHapticsBackend>(
				IOpenMobileHapticsBackend::GetModularFeatureName()
			);
	}

	void StopBackend(IOpenMobileHapticsBackend& Backend)
	{
		if (!ShutdownBackends.Contains(&Backend))
		{
			ShutdownBackends.Add(&Backend);
			Backend.BeginShutdown();
		}
	}
}

void FOpenMobileHapticsBackendRegistry::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Exchange(false))
	{
		ShutdownBackends.Reset();
		AdvanceGeneration();
	}
}

bool FOpenMobileHapticsBackendRegistry::RegisterBackend(
	IOpenMobileHapticsBackend& Backend
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Load() || Backend.GetBackendName().IsNone())
	{
		return false;
	}

	for (IOpenMobileHapticsBackend* Registered : GetBackends())
	{
		if (Registered == &Backend
			|| (Registered
				&& Registered->GetBackendName() == Backend.GetBackendName()))
		{
			return false;
		}
	}

	ShutdownBackends.Remove(&Backend);
	IModularFeatures::Get().RegisterModularFeature(
		IOpenMobileHapticsBackend::GetModularFeatureName(),
		&Backend
	);
	AdvanceGeneration();
	return true;
}

bool FOpenMobileHapticsBackendRegistry::UnregisterBackend(
	IOpenMobileHapticsBackend& Backend
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (!GetBackends().Contains(&Backend))
	{
		return false;
	}

	StopBackend(Backend);
	IModularFeatures::Get().UnregisterModularFeature(
		IOpenMobileHapticsBackend::GetModularFeatureName(),
		&Backend
	);
	ShutdownBackends.Remove(&Backend);
	AdvanceGeneration();
	return true;
}

bool FOpenMobileHapticsBackendRegistry::IsBackendRegistered(
	const IOpenMobileHapticsBackend* Backend
)
{
	check(IsInGameThread());
	return Backend
		&& OpenMobileHapticsBackendRegistryPrivate::GetBackends().Contains(
			Backend
		);
}

IOpenMobileHapticsBackend* FOpenMobileHapticsBackendRegistry::FindBackend()
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Load())
	{
		return nullptr;
	}

	IOpenMobileHapticsBackend* Best = nullptr;
	for (IOpenMobileHapticsBackend* Candidate : GetBackends())
	{
		if (!Candidate || !Candidate->IsAvailable())
		{
			continue;
		}

		const bool bHigherPriority = !Best
			|| Candidate->GetPriority() > Best->GetPriority();
		const bool bStableTieBreak = Best
			&& Candidate->GetPriority() == Best->GetPriority()
			&& Candidate->GetBackendName().LexicalLess(
				Best->GetBackendName()
			);
		if (bHigherPriority || bStableTieBreak)
		{
			Best = Candidate;
		}
	}
	return Best;
}

FOpenMobileHapticsBackendRequestToken
FOpenMobileHapticsBackendRegistry::CreateRequestToken(
	IOpenMobileHapticsBackend& Backend,
	bool bCreatePlaybackHandle
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Load() || !GetBackends().Contains(&Backend))
	{
		return {};
	}

	FOpenMobileHapticsBackendRequestToken Token;
	Token.RegistryGeneration = Generation.Load();
	Token.RequestId = AllocateRequestId();
	Token.BackendName = Backend.GetBackendName();
	if (bCreatePlaybackHandle)
	{
		Token.PlaybackHandle.Id = FGuid::NewGuid();
	}
	return Token;
}

bool FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(
	const FOpenMobileHapticsBackendRequestToken& Token
)
{
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	return Token.IsValid()
		&& !bShuttingDown.Load()
		&& Token.RegistryGeneration == Generation.Load();
}

bool FOpenMobileHapticsBackendRegistry::IsShuttingDown()
{
	return OpenMobileHapticsBackendRegistryPrivate::bShuttingDown.Load();
}

void FOpenMobileHapticsBackendRegistry::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Exchange(true))
	{
		return;
	}

	AdvanceGeneration();
	for (IOpenMobileHapticsBackend* Backend : GetBackends())
	{
		if (Backend)
		{
			StopBackend(*Backend);
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileHapticsBackendRegistry::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	bShuttingDown.Store(false);
	ShutdownBackends.Reset();
	AdvanceGeneration();
}
#endif
