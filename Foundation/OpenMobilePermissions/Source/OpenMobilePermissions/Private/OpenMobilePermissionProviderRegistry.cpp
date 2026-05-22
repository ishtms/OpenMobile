#include "IOpenMobilePermissionProvider.h"

#include "Features/IModularFeatures.h"
#include "OpenMobilePermissions.h"

namespace OpenMobilePermissionProviderRegistryPrivate
{
	TAtomic<bool> bShuttingDown(false);
	TSet<IOpenMobilePermissionProvider*> ShutdownProviders;

	TArray<IOpenMobilePermissionProvider*> GetProviders()
	{
		return IModularFeatures::Get()
			.GetModularFeatureImplementations<IOpenMobilePermissionProvider>(
				IOpenMobilePermissionProvider::GetModularFeatureName()
			);
	}

	void StopProvider(IOpenMobilePermissionProvider& Provider)
	{
		if (!ShutdownProviders.Contains(&Provider))
		{
			ShutdownProviders.Add(&Provider);
			Provider.BeginShutdown();
		}
	}
}

bool FOpenMobilePermissionProviderRegistry::RegisterProvider(
	IOpenMobilePermissionProvider& Provider
)
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionProviderRegistryPrivate;
	if (bShuttingDown.Load() || Provider.GetProviderName().IsNone())
	{
		return false;
	}

	for (IOpenMobilePermissionProvider* Registered : GetProviders())
	{
		if (Registered == &Provider
			|| (Registered
				&& Registered->GetProviderName() == Provider.GetProviderName()))
		{
			return false;
		}
	}

	ShutdownProviders.Remove(&Provider);
	IModularFeatures::Get().RegisterModularFeature(
		IOpenMobilePermissionProvider::GetModularFeatureName(),
		&Provider
	);
	return true;
}

bool FOpenMobilePermissionProviderRegistry::UnregisterProvider(
	IOpenMobilePermissionProvider& Provider
)
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionProviderRegistryPrivate;
	if (!GetProviders().Contains(&Provider))
	{
		return false;
	}

	FOpenMobilePermissions::FailRequestsForProvider(
		Provider,
		FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The permission provider became unavailable."),
			FString(),
			Provider.GetProviderName().ToString()
		)
	);
	StopProvider(Provider);
	IModularFeatures::Get().UnregisterModularFeature(
		IOpenMobilePermissionProvider::GetModularFeatureName(),
		&Provider
	);
	ShutdownProviders.Remove(&Provider);
	return true;
}

IOpenMobilePermissionProvider* FOpenMobilePermissionProviderRegistry::FindProvider(
	FName Permission
)
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionProviderRegistryPrivate;
	if (Permission.IsNone() || bShuttingDown.Load())
	{
		return nullptr;
	}

	IOpenMobilePermissionProvider* Best = nullptr;
	for (IOpenMobilePermissionProvider* Candidate : GetProviders())
	{
		if (!Candidate
			|| !Candidate->IsAvailable()
			|| !Candidate->SupportsPermission(Permission))
		{
			continue;
		}

		const bool bHigherPriority = !Best
			|| Candidate->GetPriority() > Best->GetPriority();
		const bool bStableTieBreak = Best
			&& Candidate->GetPriority() == Best->GetPriority()
			&& Candidate->GetProviderName().LexicalLess(Best->GetProviderName());
		if (bHigherPriority || bStableTieBreak)
		{
			Best = Candidate;
		}
	}
	return Best;
}

void FOpenMobilePermissionProviderRegistry::Start()
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionProviderRegistryPrivate;
	if (bShuttingDown.Exchange(false))
	{
		ShutdownProviders.Reset();
	}
}

void FOpenMobilePermissionProviderRegistry::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionProviderRegistryPrivate;
	if (bShuttingDown.Exchange(true))
	{
		return;
	}

	for (IOpenMobilePermissionProvider* Provider : GetProviders())
	{
		if (!Provider)
		{
			continue;
		}
		FOpenMobilePermissions::FailRequestsForProvider(
			*Provider,
			FOpenMobileError::Make(
				EOpenMobileErrorCode::Cancelled,
				TEXT("The permission service is shutting down."),
				FString(),
				Provider->GetProviderName().ToString()
			)
		);
		StopProvider(*Provider);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobilePermissionProviderRegistry::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionProviderRegistryPrivate;
	bShuttingDown.Store(false);
	ShutdownProviders.Reset();
}
#endif
