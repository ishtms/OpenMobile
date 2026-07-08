#include "OpenMobileNativeStepQueryService.h"

#include "IOpenMobileSensorsBackend.h"
#include "OpenMobileAsync.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsErrorMapper.h"

namespace OpenMobileNativeStepQueryServicePrivate
{
	constexpr int32 MaximumConcurrentQueries = 32;
	constexpr double MaximumFutureSkewSeconds = 300.0;

	struct FQueryEntry
	{
		FGuid OwnerIdentifier;
		FGuid RequestId;
		FOpenMobileSensorsBackendToken BackendToken;
		FOpenMobileNativeStepCountQuery Query;
		TFunction<void(const FOpenMobileNativeStepCountQueryResult&)> Completion;
	};

	TMap<FGuid, TSharedPtr<FQueryEntry, ESPMode::ThreadSafe>> Queries;
	bool bShuttingDown = true;

	FOpenMobileSensorOperationResult Failure(
		EOpenMobileSensorFailureReason Reason,
		const TCHAR* Code
	)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			Reason,
			TEXT("OpenMobileSensors.NativeStepQuery"),
			Code
		);
	}

	void Deliver(
		TFunction<void(const FOpenMobileNativeStepCountQueryResult&)>&& Completion,
		FOpenMobileNativeStepCountQueryResult Result
	)
	{
		OpenMobile::DispatchToGameThread(
			[Completion = MoveTemp(Completion), Result = MoveTemp(Result)]()
			mutable
			{
				Completion(Result);
			}
		);
	}

	TOptional<FOpenMobileSensorOperationResult> CapabilityFailure()
	{
		const FOpenMobileSensorCapabilitySnapshot Snapshot =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		const FOpenMobileSensorCapability* Capability =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Candidate)
				{
					return Candidate.Sensor.Type ==
						EOpenMobileSensorType::StepCounter;
				}
			);
		if (!Capability)
		{
			return Failure(
				EOpenMobileSensorFailureReason::MissingHardware,
				TEXT("StepCounterCapabilityMissing")
			);
		}
		switch (Capability->Availability.State)
		{
		case EOpenMobileCapabilityState::Available:
			return {};
		case EOpenMobileCapabilityState::NotSupported:
			return Failure(
				EOpenMobileSensorFailureReason::UnsupportedOperation,
				TEXT("HistoricalQueryUnsupported")
			);
		case EOpenMobileCapabilityState::NotConfigured:
			return Failure(
				EOpenMobileSensorFailureReason::ConfigurationBlocked,
				TEXT("StepCounterNotConfigured")
			);
		case EOpenMobileCapabilityState::PermissionRequired:
			return Failure(
				EOpenMobileSensorFailureReason::PermissionRequired,
				TEXT("MotionPermissionRequired")
			);
		case EOpenMobileCapabilityState::Denied:
			return Failure(
				EOpenMobileSensorFailureReason::PermissionDenied,
				TEXT("MotionPermissionDenied")
			);
		case EOpenMobileCapabilityState::Restricted:
			return Failure(
				EOpenMobileSensorFailureReason::PermissionRestricted,
				TEXT("MotionPermissionRestricted")
			);
		case EOpenMobileCapabilityState::TemporarilyUnavailable:
			return Failure(
				EOpenMobileSensorFailureReason::TemporarilyUnavailable,
				TEXT("StepCounterTemporarilyUnavailable")
			);
		case EOpenMobileCapabilityState::Unavailable:
		default:
			return Failure(
				Capability->ActiveRestriction ==
					EOpenMobileSensorRestriction::MissingHardware
					? EOpenMobileSensorFailureReason::MissingHardware
					: EOpenMobileSensorFailureReason::TemporarilyUnavailable,
				TEXT("StepCounterUnavailable")
			);
		}
	}

	void CompleteFromBackend(
		const FGuid& RequestId,
		const FOpenMobileSensorsBackendToken& Token,
		FOpenMobileSensorOperationResult Operation,
		FOpenMobileStepsSensorSample Sample
	)
	{
		OpenMobile::DispatchToGameThread(
			[RequestId,
			 Token,
			 Operation = MoveTemp(Operation),
			 Sample = MoveTemp(Sample)]() mutable
			{
				TSharedPtr<FQueryEntry, ESPMode::ThreadSafe> Entry;
				if (!Queries.RemoveAndCopyValue(RequestId, Entry) || !Entry)
				{
					return;
				}
				FOpenMobileNativeStepCountQueryResult Result;
				Result.RequestId = RequestId;
				Result.Query = Entry->Query;
				if (!FOpenMobileSensorsBackendRegistry::IsTokenCurrent(Token))
				{
					Result.Operation = Failure(
						EOpenMobileSensorFailureReason::TemporarilyUnavailable,
						TEXT("BackendChanged")
					);
				}
				else
				{
					Result.Operation = MoveTemp(Operation);
					if (Result.Operation.IsSuccess())
					{
						const bool bValid =
							FOpenMobileSensorUnitConverter::NormalizeStepsSample(
								EOpenMobileSensorNativePlatform::IOS,
								Sample
							);
						if (!bValid
							|| Sample.Origin !=
								EOpenMobileStepCountOrigin::QueryInterval)
						{
							Result.Operation = Failure(
								EOpenMobileSensorFailureReason::OperationalFailure,
								TEXT("InvalidNativeQueryResult")
							);
						}
						else
						{
							Result.Sample = MoveTemp(Sample);
						}
					}
				}
				Deliver(MoveTemp(Entry->Completion), MoveTemp(Result));
			}
		);
	}

	bool CancelEntry(
		const TSharedPtr<FQueryEntry, ESPMode::ThreadSafe>& Entry
	)
	{
		if (!Entry || !Queries.Remove(Entry->RequestId))
		{
			return false;
		}
		if (FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
			Entry->BackendToken))
		{
			if (IOpenMobileSensorsBackend* Backend =
				FOpenMobileSensorsBackendRegistry::FindBackend())
			{
				Backend->CancelNativeStepCountQuery(Entry->RequestId);
			}
		}
		FOpenMobileNativeStepCountQueryResult Result;
		Result.RequestId = Entry->RequestId;
		Result.Query = Entry->Query;
		Result.Operation = Failure(
			EOpenMobileSensorFailureReason::Cancelled,
			TEXT("QueryCancelled")
		);
		Deliver(MoveTemp(Entry->Completion), MoveTemp(Result));
		return true;
	}
}

void FOpenMobileNativeStepQueryService::Start()
{
	check(IsInGameThread());
	OpenMobileNativeStepQueryServicePrivate::bShuttingDown = false;
}

void FOpenMobileNativeStepQueryService::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileNativeStepQueryServicePrivate;
	if (bShuttingDown)
	{
		return;
	}
	bShuttingDown = true;
	TArray<TSharedPtr<FQueryEntry, ESPMode::ThreadSafe>> Pending;
	Queries.GenerateValueArray(Pending);
	for (const TSharedPtr<FQueryEntry, ESPMode::ThreadSafe>& Entry : Pending)
	{
		CancelEntry(Entry);
	}
}

FGuid FOpenMobileNativeStepQueryService::Query(
	const FGuid& OwnerIdentifier,
	const FOpenMobileNativeStepCountQuery& Query,
	TFunction<void(const FOpenMobileNativeStepCountQueryResult&)>&& Completion
)
{
	check(IsInGameThread());
	using namespace OpenMobileNativeStepQueryServicePrivate;
	if (!Completion)
	{
		return {};
	}
	const FGuid RequestId = FGuid::NewGuid();
	auto CompleteFailure = [RequestId, Query, &Completion](
		FOpenMobileSensorOperationResult Operation)
	{
		FOpenMobileNativeStepCountQueryResult Result;
		Result.RequestId = RequestId;
		Result.Query = Query;
		Result.Operation = MoveTemp(Operation);
		Deliver(MoveTemp(Completion), MoveTemp(Result));
	};
	const double CurrentUnixTimeSeconds = static_cast<double>(
		FDateTime::UtcNow().ToUnixTimestamp());
	if (!OwnerIdentifier.IsValid()
		|| !FMath::IsFinite(Query.StartUnixTimeSeconds)
		|| !FMath::IsFinite(Query.EndUnixTimeSeconds)
		|| Query.StartUnixTimeSeconds < 0.0
		|| Query.EndUnixTimeSeconds <= Query.StartUnixTimeSeconds
		|| Query.EndUnixTimeSeconds >
			CurrentUnixTimeSeconds + MaximumFutureSkewSeconds)
	{
		CompleteFailure(Failure(
			EOpenMobileSensorFailureReason::InvalidRequest,
			TEXT("InvalidQueryRange")));
		return RequestId;
	}
	if (bShuttingDown)
	{
		CompleteFailure(Failure(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable,
			TEXT("ServiceShuttingDown")));
		return RequestId;
	}
	if (Queries.Num() >= MaximumConcurrentQueries)
	{
		CompleteFailure(Failure(
			EOpenMobileSensorFailureReason::RateLimited,
			TEXT("TooManyQueries")));
		return RequestId;
	}
	IOpenMobileSensorsBackend* Backend =
		FOpenMobileSensorsBackendRegistry::FindBackend();
	if (!Backend)
	{
		CompleteFailure(Failure(
			EOpenMobileSensorFailureReason::UnsupportedPlatform,
			TEXT("BackendMissing")));
		return RequestId;
	}
	if (TOptional<FOpenMobileSensorOperationResult> Blocked =
		CapabilityFailure())
	{
		CompleteFailure(MoveTemp(Blocked.GetValue()));
		return RequestId;
	}

	TSharedPtr<FQueryEntry, ESPMode::ThreadSafe> Entry =
		MakeShared<FQueryEntry, ESPMode::ThreadSafe>();
	Entry->OwnerIdentifier = OwnerIdentifier;
	Entry->RequestId = RequestId;
	Entry->BackendToken =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	Entry->Query = Query;
	Entry->Completion = MoveTemp(Completion);
	Queries.Add(RequestId, Entry);
	const FOpenMobileSensorOperationResult StartResult =
		Backend->QueryNativeStepCount(
			RequestId,
			Query,
			FOnOpenMobileNativeStepCountBackendQueryComplete::CreateLambda(
				[RequestId, Token = Entry->BackendToken](
					const FOpenMobileSensorOperationResult& Operation,
					const FOpenMobileStepsSensorSample& Sample)
				{
					CompleteFromBackend(
						RequestId,
						Token,
						Operation,
						Sample
					);
				}
			)
		);
	if (!StartResult.IsSuccess())
	{
		TSharedPtr<FQueryEntry, ESPMode::ThreadSafe> FailedEntry;
		if (Queries.RemoveAndCopyValue(RequestId, FailedEntry) && FailedEntry)
		{
			FOpenMobileNativeStepCountQueryResult Result;
			Result.RequestId = RequestId;
			Result.Query = Query;
			Result.Operation = StartResult;
			Deliver(MoveTemp(FailedEntry->Completion), MoveTemp(Result));
		}
	}
	return RequestId;
}

bool FOpenMobileNativeStepQueryService::Cancel(
	const FGuid& OwnerIdentifier,
	const FGuid& RequestId
)
{
	check(IsInGameThread());
	using namespace OpenMobileNativeStepQueryServicePrivate;
	const TSharedPtr<FQueryEntry, ESPMode::ThreadSafe>* Found =
		Queries.Find(RequestId);
	if (!Found || !*Found || (*Found)->OwnerIdentifier != OwnerIdentifier)
	{
		return false;
	}
	return CancelEntry(*Found);
}

void FOpenMobileNativeStepQueryService::CancelOwner(
	const FGuid& OwnerIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileNativeStepQueryServicePrivate;
	TArray<TSharedPtr<FQueryEntry, ESPMode::ThreadSafe>> Pending;
	for (const TPair<FGuid, TSharedPtr<FQueryEntry, ESPMode::ThreadSafe>>& Pair
		: Queries)
	{
		if (Pair.Value && Pair.Value->OwnerIdentifier == OwnerIdentifier)
		{
			Pending.Add(Pair.Value);
		}
	}
	for (const TSharedPtr<FQueryEntry, ESPMode::ThreadSafe>& Entry : Pending)
	{
		CancelEntry(Entry);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileNativeStepQueryService::ResetForTests()
{
	check(IsInGameThread());
	BeginShutdown();
	OpenMobileNativeStepQueryServicePrivate::Queries.Reset();
	Start();
}
#endif
