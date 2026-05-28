#include "OpenMobileSensorsSubscriptionService.h"

#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsErrorMapper.h"

namespace OpenMobileSensorsSubscriptionServicePrivate
{
	struct FSubscriptionEntry
	{
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		FOpenMobileSensorSubscriptionRequest Request;
		FOpenMobileSensorStreamOptions AppliedOptions;
		FOpenMobileSensorsBackendToken BackendToken;
		EOpenMobileSensorSubscriptionState State =
			EOpenMobileSensorSubscriptionState::Accepted;
	};

	TMap<FGuid, FSubscriptionEntry> Subscriptions;
	uint32 NextHandleGeneration = 1;
	bool bShuttingDown = false;

	FOpenMobileSensorOperationResult MakeSuccess(
		EOpenMobileSensorResultCode ResultCode =
			EOpenMobileSensorResultCode::Success
	)
	{
		FOpenMobileSensorOperationResult Result;
		Result.Code = ResultCode;
		return Result;
	}

	FOpenMobileSensorOperationResult MakeHandleFailure(
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		if (!Handle.IsValid())
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidHandle
			);
		}
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::StaleHandle
		);
	}

	uint32 AllocateGeneration()
	{
		const uint32 Generation = NextHandleGeneration++;
		if (NextHandleGeneration == 0)
		{
			NextHandleGeneration = 1;
		}
		return Generation == 0 ? NextHandleGeneration++ : Generation;
	}

	FSubscriptionEntry* FindOwnedEntry(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		if (!OwnerIdentifier.IsValid() || !Handle.IsValid())
		{
			return nullptr;
		}
		FSubscriptionEntry* Entry = Subscriptions.Find(
			Handle.GetIdentifier()
		);
		if (!Entry
			|| Entry->OwnerIdentifier != OwnerIdentifier
			|| Entry->Handle != Handle
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				Entry->BackendToken
			))
		{
			return nullptr;
		}
		return Entry;
	}
}

void FOpenMobileSensorsSubscriptionService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	bShuttingDown = false;
	Subscriptions.Reset();
}

void FOpenMobileSensorsSubscriptionService::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	bShuttingDown = true;
	Subscriptions.Reset();
}

void FOpenMobileSensorsSubscriptionService::HandleBackendGenerationChanged()
{
	check(IsInGameThread());
	OpenMobileSensorsSubscriptionServicePrivate::Subscriptions.Reset();
}

FOpenMobileSensorSubscriptionResult
FOpenMobileSensorsSubscriptionService::StartSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionRequest& Request
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	FOpenMobileSensorSubscriptionResult Result;
	Result.RequestedOptions = Request.Options;
	Result.AppliedOptions = Request.Options;
	if (!OwnerIdentifier.IsValid())
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
		return Result;
	}
	if (!Request.Sensor.IsValid())
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
		return Result;
	}
	if (bShuttingDown)
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
		return Result;
	}
	const FOpenMobileSensorsBackendToken BackendToken =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	if (BackendToken.Generation == 0)
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedPlatform
		);
		return Result;
	}

	FOpenMobileSensorSubscriptionHandle Handle;
	do
	{
		Handle.Identifier = FGuid::NewGuid();
	}
	while (!Handle.Identifier.IsValid()
		|| Subscriptions.Contains(Handle.Identifier));
	Handle.Generation = AllocateGeneration();

	FSubscriptionEntry Entry;
	Entry.OwnerIdentifier = OwnerIdentifier;
	Entry.Handle = Handle;
	Entry.Request = Request;
	Entry.AppliedOptions = Request.Options;
	Entry.BackendToken = BackendToken;
	Subscriptions.Add(Handle.Identifier, MoveTemp(Entry));

	Result.Handle = Handle;
	Result.Operation = MakeSuccess(EOpenMobileSensorResultCode::Accepted);
	return Result;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::UpdateSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorStreamOptions& Options
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeHandleFailure(Handle);
	}
	Entry->Request.Options = Options;
	Entry->AppliedOptions = Options;
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::StopSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (!FindOwnedEntry(OwnerIdentifier, Handle))
	{
		return MakeHandleFailure(Handle);
	}
	Subscriptions.Remove(Handle.GetIdentifier());
	return MakeSuccess();
}

int32 FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(
	const FGuid& OwnerIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (!OwnerIdentifier.IsValid())
	{
		return 0;
	}
	int32 Removed = 0;
	for (auto It = Subscriptions.CreateIterator(); It; ++It)
	{
		if (It.Value().OwnerIdentifier == OwnerIdentifier)
		{
			It.RemoveCurrent();
			++Removed;
		}
	}
	return Removed;
}

bool FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	FOpenMobileSensorSubscriptionStateSnapshot& OutState
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	OutState = {};
	OutState.Handle = Handle;
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		OutState.Error = MakeHandleFailure(Handle).Error;
		return false;
	}
	OutState.Sensor = Entry->Request.Sensor;
	OutState.State = Entry->State;
	OutState.RequestedOptions = Entry->Request.Options;
	OutState.AppliedOptions = Entry->AppliedOptions;
	return true;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::GetHandleStatus(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	return FindOwnedEntry(OwnerIdentifier, Handle)
		? MakeSuccess()
		: MakeHandleFailure(Handle);
}

bool FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	return OpenMobileSensorsSubscriptionServicePrivate::FindOwnedEntry(
		OwnerIdentifier,
		Handle
	) != nullptr;
}

bool FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (!Handle.IsValid())
	{
		return false;
	}
	FSubscriptionEntry* Entry = Subscriptions.Find(Handle.GetIdentifier());
	return Entry
		&& Entry->Handle == Handle
		&& FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
			Entry->BackendToken
		);
}

void FOpenMobileSensorsSubscriptionService::
InvalidateForUnrecoverablePermissionLoss(EOpenMobileSensorType SensorType)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	for (auto It = Subscriptions.CreateIterator(); It; ++It)
	{
		if (It.Value().Request.Sensor.Type == SensorType)
		{
			It.RemoveCurrent();
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
int32 FOpenMobileSensorsSubscriptionService::
GetActiveSubscriptionCountForTests()
{
	check(IsInGameThread());
	return OpenMobileSensorsSubscriptionServicePrivate::Subscriptions.Num();
}

int32 FOpenMobileSensorsSubscriptionService::
GetActiveSubscriptionCountForTests(
	const FOpenMobileSensorIdentifier& Sensor
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	int32 Count = 0;
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		if (Pair.Value.Request.Sensor == Sensor)
		{
			++Count;
		}
	}
	return Count;
}

void FOpenMobileSensorsSubscriptionService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	Subscriptions.Reset();
	bShuttingDown = false;
}
#endif
