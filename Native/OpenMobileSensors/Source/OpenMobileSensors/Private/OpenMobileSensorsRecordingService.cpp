#include "OpenMobileSensorsRecordingService.h"

#include "Async/Async.h"
#include "Algo/BinarySearch.h"
#include "Algo/StableSort.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileSensorRecordingCodec.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsRecordingServicePrivate
{
	constexpr int32 MaximumConcurrentRecordings = 4;
	constexpr int32 MaximumConcurrentReplays = 4;
	constexpr int32 MaximumRecordedSensors = 32;
	constexpr int64 MinimumRecordingBytes = 1024ll * 1024;
	constexpr int64 FooterReserveBytes = 256;
	constexpr int64 EstimatedVectorSampleBytes = 192;
	constexpr int64 MaximumReplayFileBytes = 256ll * 1024 * 1024;
	constexpr double MinimumPlaybackSpeed = 0.01;
	constexpr double MaximumPlaybackSpeed = 100.0;

	enum class EWorkerState : uint8
	{
		Initializing,
		Ready,
		Completed,
		Failed
	};

	struct FPendingVectorBatch
	{
		FOpenMobileVectorSensorBatch Batch;
		int64 EstimatedBytes = 0;
	};

	struct FRecordingWorkerState
	{
		FCriticalSection Mutex;
		TArray<FPendingVectorBatch> PendingBatches;
		FEvent* WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
		TAtomic<EWorkerState> State{EWorkerState::Initializing};
		TAtomic<bool> bStopRequested{false};
		TAtomic<bool> bDiscardFile{false};
		FString TemporaryPath;
		FString FinalPath;
		FOpenMobileSensorRecordingHeader Header;
		int64 MaximumBytes = 0;
		int32 MaximumBufferedBatches = 0;
		int64 QueuedEstimatedBytes = 0;
		int64 BytesWritten = 0;
		int64 BatchCount = 0;
		int64 SampleCount = 0;
		int64 DroppedSamples = 0;
		double FirstTimestampSeconds = 0.0;
		double LastTimestampSeconds = 0.0;
		bool bHasTimestamp = false;
		FString FailureCode;
		FString FailureMessage;

		~FRecordingWorkerState()
		{
			if (WakeEvent)
			{
				FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
				WakeEvent = nullptr;
			}
		}
	};

	struct FRecordingEntry
	{
		FGuid OwnerIdentifier;
		FGuid StreamOwnerIdentifier;
		FGuid RequestId;
		FOpenMobileSensorRecordingOptions Options;
		EOpenMobileSensorRecordingState State =
			EOpenMobileSensorRecordingState::Starting;
		TSharedPtr<FRecordingWorkerState, ESPMode::ThreadSafe> Worker;
		TFuture<void> WorkerFuture;
		TArray<FOpenMobileSensorSubscriptionHandle> SubscriptionHandles;
		TFunction<void(const FOpenMobileSensorRecordingResult&)> StartCompletion;
		TFunction<void(const FOpenMobileSensorRecordingResult&)> StopCompletion;
		FOpenMobileSensorRecordingResult FinalResult;
		double StartWallTimeSeconds = 0.0;
		bool bStartCompletionDelivered = false;
		bool bSubscriptionsStarted = false;
		bool bRecordingWasActive = false;
		bool bStopRequested = false;
		bool bAutoStopped = false;
	};

	enum class EReplayLoadState : uint8
	{
		Loading,
		Ready,
		Failed
	};

	struct FReplayLoadResult
	{
		FCriticalSection Mutex;
		TAtomic<EReplayLoadState> State{EReplayLoadState::Loading};
		FOpenMobileSensorRecordingDocument Document;
		EOpenMobileSensorRecordingDecodeStatus DecodeStatus =
			EOpenMobileSensorRecordingDecodeStatus::InvalidData;
		FString Error;
	};

	struct FReplayEntry
	{
		FGuid OwnerIdentifier;
		FGuid RequestId;
		FString FilePath;
		FOpenMobileSensorReplayOptions Options;
		TSharedPtr<FReplayLoadResult, ESPMode::ThreadSafe> LoadResult;
		TFuture<void> LoadFuture;
		TFunction<void(const FOpenMobileSensorReplayResult&)> Completion;
		TArray<FOpenMobileVectorSensorSample> Samples;
		int32 NextSampleIndex = 0;
		double RecordingStartSeconds = 0.0;
		double RecordingDurationSeconds = 0.0;
		double PlaybackWallStartSeconds = 0.0;
		double TimestampBaseSeconds = 0.0;
		double LastPublishedTimestampSeconds = -1.0;
		bool bPlaying = false;
	};

	TMap<FGuid, TSharedPtr<FRecordingEntry, ESPMode::ThreadSafe>> Recordings;
	TMap<FGuid, TSharedPtr<FReplayEntry, ESPMode::ThreadSafe>> Replays;
	FDelegateHandle VectorBatchHandle;
	FTSTicker::FDelegateHandle TickHandle;
	bool bShuttingDown = true;

	FOpenMobileSensorOperationResult MakeFailure(
		EOpenMobileSensorFailureReason Reason,
		const TCHAR* NativeCode,
		const FString& Message = FString()
	)
	{
		FOpenMobileSensorOperationResult Result =
			FOpenMobileSensorsErrorMapper::Map(Reason);
		Result.Failure.NativeDomain = TEXT("OpenMobileSensors.Recording");
		Result.Failure.NativeCode = NativeCode;
		if (!Message.IsEmpty())
		{
			Result.Error.Message = Message;
			Result.Error.NativeCode = NativeCode;
			Result.Error.Provider = TEXT("OpenMobileSensors.Recording");
		}
		return Result;
	}

	FOpenMobileSensorOperationResult MakeSuccess(
		EOpenMobileSensorResultCode Code = EOpenMobileSensorResultCode::Success
	)
	{
		FOpenMobileSensorOperationResult Result;
		Result.Code = Code;
		return Result;
	}

	bool IsVectorSensor(EOpenMobileSensorType Type)
	{
		switch (Type)
		{
		case EOpenMobileSensorType::Accelerometer:
		case EOpenMobileSensorType::AccelerometerUncalibrated:
		case EOpenMobileSensorType::Gyroscope:
		case EOpenMobileSensorType::GyroscopeUncalibrated:
		case EOpenMobileSensorType::Magnetometer:
		case EOpenMobileSensorType::MagnetometerUncalibrated:
		case EOpenMobileSensorType::Gravity:
		case EOpenMobileSensorType::LinearAcceleration:
			return true;
		default:
			return false;
		}
	}

	FString GetUnits(EOpenMobileSensorType Type)
	{
		switch (Type)
		{
		case EOpenMobileSensorType::Accelerometer:
		case EOpenMobileSensorType::AccelerometerUncalibrated:
		case EOpenMobileSensorType::Gravity:
		case EOpenMobileSensorType::LinearAcceleration:
			return TEXT("m/s^2");
		case EOpenMobileSensorType::Gyroscope:
		case EOpenMobileSensorType::GyroscopeUncalibrated:
			return TEXT("rad/s");
		case EOpenMobileSensorType::Magnetometer:
		case EOpenMobileSensorType::MagnetometerUncalibrated:
			return TEXT("uT");
		default:
			return FString();
		}
	}

	bool ValidateOptions(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorRecordingOptions& Options,
		FOpenMobileSensorOperationResult& OutFailure
	)
	{
		const UOpenMobileSensorsSettings* Settings =
			GetDefault<UOpenMobileSensorsSettings>();
		if (!OwnerIdentifier.IsValid()
			|| Options.Sensors.IsEmpty()
			|| Options.Sensors.Num() > MaximumRecordedSensors
			|| !FMath::IsFinite(Options.MaximumDurationSeconds)
			|| Options.MaximumDurationSeconds < 1.0
			|| !Settings
			|| Options.MaximumDurationSeconds >
				Settings->MaximumRecordingDurationSeconds
			|| Options.MaximumBytes < MinimumRecordingBytes
			|| Options.MaximumBytes > FMath::Min(
				Settings->MaximumRecordingBytes,
				MaximumReplayFileBytes))
		{
			OutFailure = MakeFailure(
				EOpenMobileSensorFailureReason::InvalidRequest,
				TEXT("InvalidRecordingOptions")
			);
			return false;
		}
		TSet<FOpenMobileSensorIdentifier> UniqueSensors;
		for (const FOpenMobileSensorIdentifier& Sensor : Options.Sensors)
		{
			if (!Sensor.IsValid()
				|| !IsVectorSensor(Sensor.Type)
				|| UniqueSensors.Contains(Sensor))
			{
				OutFailure = MakeFailure(
					EOpenMobileSensorFailureReason::InvalidRequest,
					TEXT("InvalidRecordingSensor")
				);
				return false;
			}
			UniqueSensors.Add(Sensor);
		}
		return true;
	}

	FOpenMobileSensorRecordingHeader MakeHeader(
		const FOpenMobileSensorRecordingOptions& Options
	)
	{
		FOpenMobileSensorRecordingHeader Header;
		Header.FormatVersion =
			FOpenMobileSensorRecordingCodec::CurrentFormatVersion;
		const TSharedPtr<IPlugin> Plugin =
			IPluginManager::Get().FindPlugin(TEXT("OpenMobileSensors"));
		Header.PluginVersion = Plugin.IsValid()
			? Plugin->GetDescriptor().VersionName
			: TEXT("Unknown");
		Header.PlatformName = ANSI_TO_TCHAR(
			FPlatformProperties::PlatformName());
		Header.UnitsConvention = TEXT("SI units per stream descriptor");
		Header.CoordinateConvention =
			TEXT("Unreal device-fixed: X forward, Y right, Z up");
		const FOpenMobileSensorCapabilitySnapshot Capabilities =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		for (const FOpenMobileSensorIdentifier& Sensor : Options.Sensors)
		{
			FOpenMobileSensorRecordingStreamDescriptor& Stream =
				Header.Streams.AddDefaulted_GetRef();
			Stream.Sensor = Sensor;
			Stream.Family = EOpenMobileSensorSampleFamily::Vector;
			Stream.Units = GetUnits(Sensor.Type);
			if (const FOpenMobileSensorCapability* Capability =
				Capabilities.Sensors.FindByPredicate(
					[&Sensor](const FOpenMobileSensorCapability& Candidate)
					{
						return Candidate.Sensor.Type == Sensor.Type
							&& (Sensor.InstanceId.IsNone()
								|| Candidate.Sensor.InstanceId ==
									Sensor.InstanceId);
					}))
			{
				Stream.Capability = *Capability;
				Stream.Capability.Sensor = Sensor;
			}
			else
			{
				Stream.Capability.Sensor = Sensor;
				Stream.Capability.Availability.Name =
					FOpenMobileSensorTypes::GetStableName(Sensor.Type);
				Stream.Capability.Availability.State =
					EOpenMobileCapabilityState::Available;
			}
		}
		return Header;
	}

	void SetWorkerFailure(
		const TSharedRef<FRecordingWorkerState, ESPMode::ThreadSafe>& Worker,
		const TCHAR* Code,
		const FString& Message
	)
	{
		FScopeLock Lock(&Worker->Mutex);
		Worker->FailureCode = Code;
		Worker->FailureMessage = Message;
	}

	bool PopPendingBatch(
		const TSharedRef<FRecordingWorkerState, ESPMode::ThreadSafe>& Worker,
		FPendingVectorBatch& OutPending
	)
	{
		FScopeLock Lock(&Worker->Mutex);
		if (Worker->PendingBatches.IsEmpty())
		{
			return false;
		}
		OutPending = MoveTemp(Worker->PendingBatches[0]);
		Worker->PendingBatches.RemoveAt(0, 1, EAllowShrinking::No);
		Worker->QueuedEstimatedBytes -= OutPending.EstimatedBytes;
		return true;
	}

	void DropPendingBatch(
		const TSharedRef<FRecordingWorkerState, ESPMode::ThreadSafe>& Worker,
		const FPendingVectorBatch& Pending
	)
	{
		FScopeLock Lock(&Worker->Mutex);
		Worker->DroppedSamples += Pending.Batch.Samples.Num();
	}

	bool WritePendingBatch(
		const TSharedRef<FRecordingWorkerState, ESPMode::ThreadSafe>& Worker,
		IFileHandle& File,
		const FPendingVectorBatch& Pending,
		TArray<uint8>& Bytes,
		FString& Error
	)
	{
		if (!FOpenMobileSensorRecordingCodec::EncodeVectorBatch(
			Pending.Batch, Bytes, Error))
		{
			DropPendingBatch(Worker, Pending);
			return true;
		}
		{
			FScopeLock Lock(&Worker->Mutex);
			if (Worker->BytesWritten + Bytes.Num() + FooterReserveBytes
				> Worker->MaximumBytes)
			{
				Worker->DroppedSamples += Pending.Batch.Samples.Num();
				return true;
			}
		}
		if (!File.Write(Bytes.GetData(), Bytes.Num()))
		{
			SetWorkerFailure(
				Worker,
				TEXT("RecordingDataWriteFailed"),
				TEXT("Sensor recording data could not be written.")
			);
			return false;
		}
		FScopeLock Lock(&Worker->Mutex);
		Worker->BytesWritten += Bytes.Num();
		++Worker->BatchCount;
		Worker->SampleCount += Pending.Batch.Samples.Num();
		for (const FOpenMobileVectorSensorSample& Sample
			: Pending.Batch.Samples)
		{
			if (!Worker->bHasTimestamp)
			{
				Worker->FirstTimestampSeconds =
					Sample.Header.TimestampSeconds;
				Worker->LastTimestampSeconds =
					Sample.Header.TimestampSeconds;
				Worker->bHasTimestamp = true;
			}
			else
			{
				Worker->FirstTimestampSeconds = FMath::Min(
					Worker->FirstTimestampSeconds,
					Sample.Header.TimestampSeconds);
				Worker->LastTimestampSeconds = FMath::Max(
					Worker->LastTimestampSeconds,
					Sample.Header.TimestampSeconds);
			}
		}
		return true;
	}

	bool DrainPendingBatches(
		const TSharedRef<FRecordingWorkerState, ESPMode::ThreadSafe>& Worker,
		IFileHandle& File,
		bool bCanWrite,
		TArray<uint8>& Bytes,
		FString& Error
	)
	{
		FPendingVectorBatch Pending;
		while (PopPendingBatch(Worker, Pending))
		{
			if (!bCanWrite)
			{
				DropPendingBatch(Worker, Pending);
				continue;
			}
			bCanWrite = WritePendingBatch(
				Worker, File, Pending, Bytes, Error);
		}
		return bCanWrite;
	}

	void RunWriter(
		TSharedRef<FRecordingWorkerState, ESPMode::ThreadSafe> Worker
	)
	{
		IPlatformFile& PlatformFile =
			FPlatformFileManager::Get().GetPlatformFile();
		const FString Directory = FPaths::GetPath(Worker->TemporaryPath);
		if (!PlatformFile.CreateDirectoryTree(*Directory))
		{
			SetWorkerFailure(
				Worker,
				TEXT("RecordingDirectoryFailed"),
				TEXT("The recording directory could not be created.")
			);
			Worker->State.Store(EWorkerState::Failed);
			return;
		}
		TUniquePtr<IFileHandle> File(
			PlatformFile.OpenWrite(*Worker->TemporaryPath, false, false));
		if (!File)
		{
			SetWorkerFailure(
				Worker,
				TEXT("RecordingOpenFailed"),
				TEXT("The recording file could not be opened.")
			);
			Worker->State.Store(EWorkerState::Failed);
			return;
		}
		TArray<uint8> Bytes;
		FString Error;
		if (!FOpenMobileSensorRecordingCodec::EncodeHeader(
			Worker->Header, Bytes, Error)
			|| Bytes.Num() + FooterReserveBytes > Worker->MaximumBytes
			|| !File->Write(Bytes.GetData(), Bytes.Num()))
		{
			SetWorkerFailure(
				Worker,
				TEXT("RecordingHeaderWriteFailed"),
				Error.IsEmpty()
					? TEXT("The recording header could not be written.")
					: Error
			);
			File.Reset();
			IFileManager::Get().Delete(*Worker->TemporaryPath);
			Worker->State.Store(EWorkerState::Failed);
			return;
		}
		{
			FScopeLock Lock(&Worker->Mutex);
			Worker->BytesWritten = Bytes.Num();
		}
		Worker->State.Store(EWorkerState::Ready);
		Worker->WakeEvent->Trigger();

		bool bWriteSucceeded = true;
		while (!Worker->bStopRequested.Load())
		{
			Worker->WakeEvent->Wait(100);
			bWriteSucceeded = DrainPendingBatches(
				Worker, *File, bWriteSucceeded, Bytes, Error);
			if (!bWriteSucceeded)
			{
				break;
			}
		}
		bWriteSucceeded = DrainPendingBatches(
			Worker, *File, bWriteSucceeded, Bytes, Error);

		if (bWriteSucceeded && !Worker->bDiscardFile.Load())
		{
			FOpenMobileSensorRecordingFooter Footer;
			{
				FScopeLock Lock(&Worker->Mutex);
				Footer.BatchCount = Worker->BatchCount;
				Footer.SampleCount = Worker->SampleCount;
				Footer.DroppedSamples = Worker->DroppedSamples;
				Footer.DurationSeconds = Worker->bHasTimestamp
					? FMath::Max(
						0.0,
						Worker->LastTimestampSeconds
							- Worker->FirstTimestampSeconds)
					: 0.0;
			}
			if (!FOpenMobileSensorRecordingCodec::EncodeFooter(
				Footer, Bytes, Error)
				|| !File->Write(Bytes.GetData(), Bytes.Num())
				|| !File->Flush(true))
			{
				SetWorkerFailure(
					Worker,
					TEXT("RecordingFooterWriteFailed"),
					Error.IsEmpty()
						? TEXT("The recording footer could not be written.")
						: Error
				);
				bWriteSucceeded = false;
			}
			else
			{
				FScopeLock Lock(&Worker->Mutex);
				Worker->BytesWritten += Bytes.Num();
			}
		}
		File.Reset();

		if (!bWriteSucceeded || Worker->bDiscardFile.Load())
		{
			IFileManager::Get().Delete(*Worker->TemporaryPath);
			Worker->State.Store(
				!bWriteSucceeded
					? EWorkerState::Failed
					: EWorkerState::Completed);
			return;
		}
		if (!IFileManager::Get().Move(
			*Worker->FinalPath,
			*Worker->TemporaryPath,
			true,
			false,
			false,
			true))
		{
			SetWorkerFailure(
				Worker,
				TEXT("RecordingFinalizeFailed"),
				TEXT("The recording file could not be finalized.")
			);
			IFileManager::Get().Delete(*Worker->TemporaryPath);
			Worker->State.Store(EWorkerState::Failed);
			return;
		}
		Worker->State.Store(EWorkerState::Completed);
	}

	FOpenMobileSensorRecordingSnapshot MakeSnapshot(
		const FRecordingEntry& Entry,
		EOpenMobileSensorRecordingState State
	)
	{
		FOpenMobileSensorRecordingSnapshot Snapshot;
		Snapshot.RequestId = Entry.RequestId;
		Snapshot.State = State;
		Snapshot.FormatVersion =
			FOpenMobileSensorRecordingCodec::CurrentFormatVersion;
		Snapshot.FilePath = Entry.Worker->FinalPath;
		FScopeLock Lock(&Entry.Worker->Mutex);
		Snapshot.BytesWritten = Entry.Worker->BytesWritten;
		Snapshot.DroppedSamples = Entry.Worker->DroppedSamples;
		Snapshot.DurationSeconds = Entry.Worker->bHasTimestamp
			? FMath::Max(
				0.0,
				Entry.Worker->LastTimestampSeconds
					- Entry.Worker->FirstTimestampSeconds)
			: 0.0;
		return Snapshot;
	}

	FOpenMobileSensorRecordingResult MakeWorkerFailure(
		const FRecordingEntry& Entry
	)
	{
		FOpenMobileSensorRecordingResult Result;
		Result.Recording = MakeSnapshot(
			Entry, EOpenMobileSensorRecordingState::Failed);
		FString Code;
		FString Message;
		{
			FScopeLock Lock(&Entry.Worker->Mutex);
			Code = Entry.Worker->FailureCode;
			Message = Entry.Worker->FailureMessage;
		}
		Result.Operation = MakeFailure(
			EOpenMobileSensorFailureReason::OperationalFailure,
			Code.IsEmpty() ? TEXT("RecordingFailed") : *Code,
			Message
		);
		return Result;
	}

	void CaptureVectorBatch(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		const FOpenMobileVectorSensorBatch& Batch
	)
	{
		check(IsInGameThread());
		for (const TPair<
			FGuid,
			TSharedPtr<FRecordingEntry, ESPMode::ThreadSafe>
		>& Pair : Recordings)
		{
			const TSharedPtr<FRecordingEntry, ESPMode::ThreadSafe>& Entry =
				Pair.Value;
			if (Entry->State != EOpenMobileSensorRecordingState::Recording
				|| Entry->StreamOwnerIdentifier != OwnerIdentifier
				|| !Entry->SubscriptionHandles.Contains(Handle))
			{
				continue;
			}
			FOpenMobileVectorSensorBatch RecordedBatch;
			for (const FOpenMobileVectorSensorSample& Sample : Batch.Samples)
			{
				if ((Sample.Header.SourceFlags & static_cast<int32>(
					EOpenMobileSensorSourceFlags::Replay)) == 0)
				{
					RecordedBatch.Samples.Add(Sample);
				}
			}
			if (RecordedBatch.Samples.IsEmpty())
			{
				return;
			}
			const int64 EstimatedBytes = 64
				+ RecordedBatch.Samples.Num() * EstimatedVectorSampleBytes;
			{
				FScopeLock Lock(&Entry->Worker->Mutex);
				if (Entry->Worker->bStopRequested.Load()
					|| Entry->Worker->PendingBatches.Num() >=
						Entry->Worker->MaximumBufferedBatches
					|| Entry->Worker->BytesWritten
						+ Entry->Worker->QueuedEstimatedBytes
						+ EstimatedBytes + FooterReserveBytes
						> Entry->Worker->MaximumBytes)
				{
					Entry->Worker->DroppedSamples +=
						RecordedBatch.Samples.Num();
					return;
				}
				FPendingVectorBatch& Pending =
					Entry->Worker->PendingBatches.AddDefaulted_GetRef();
				Pending.Batch = MoveTemp(RecordedBatch);
				Pending.EstimatedBytes = EstimatedBytes;
				Entry->Worker->QueuedEstimatedBytes += EstimatedBytes;
			}
			Entry->Worker->WakeEvent->Trigger();
			return;
		}
	}

	void StopHiddenSubscriptions(FRecordingEntry& Entry)
	{
		for (const FOpenMobileSensorSubscriptionHandle& Handle
			: Entry.SubscriptionHandles)
		{
			FOpenMobileSensorStreamDiagnostics Diagnostics;
			if (FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
				Entry.StreamOwnerIdentifier,
				Handle,
				FPlatformTime::Seconds(),
				Diagnostics))
			{
				FScopeLock Lock(&Entry.Worker->Mutex);
				Entry.Worker->DroppedSamples += Diagnostics.DroppedSamples;
			}
			FOpenMobileSensorsSubscriptionService::StopSubscription(
				Entry.StreamOwnerIdentifier, Handle);
		}
		Entry.SubscriptionHandles.Reset();
	}

	bool StartHiddenSubscriptions(FRecordingEntry& Entry)
	{
		const UOpenMobileSensorsSettings* Settings =
			GetDefault<UOpenMobileSensorsSettings>();
		if (!Settings)
		{
			return false;
		}
		for (const FOpenMobileSensorIdentifier& Sensor
			: Entry.Options.Sensors)
		{
			FOpenMobileSensorSubscriptionRequest Request;
			Request.Sensor = Sensor;
			Request.Options = Settings->DefaultStreamOptions;
			Request.Options.DeliveryMode =
				EOpenMobileSensorDeliveryMode::EventBatches;
			Request.Options.CoordinateSpace =
				EOpenMobileSensorCoordinateSpace::DeviceFixed;
			Request.Options.BufferCapacitySamples = 4096;
			Request.Options.OverflowPolicy =
				EOpenMobileSensorOverflowPolicy::RejectNewest;
			Request.Options.Filters = {};
			const FOpenMobileSensorSubscriptionResult Result =
				FOpenMobileSensorsSubscriptionService::StartSubscription(
					Entry.StreamOwnerIdentifier, Request);
			if (!Result.Operation.IsSuccess())
			{
				StopHiddenSubscriptions(Entry);
				Entry.FinalResult.Operation = Result.Operation;
				return false;
			}
			Entry.SubscriptionHandles.Add(Result.Handle);
		}
		return true;
	}

	enum class EHiddenSubscriptionStartStatus : uint8
	{
		Pending,
		Active,
		Failed
	};

	EHiddenSubscriptionStartStatus GetHiddenSubscriptionStartStatus(
		const FRecordingEntry& Entry,
		FOpenMobileSensorOperationResult& OutFailure
	)
	{
		bool bAllActive = true;
		for (const FOpenMobileSensorSubscriptionHandle& Handle
			: Entry.SubscriptionHandles)
		{
			FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
			if (!FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
				Entry.StreamOwnerIdentifier, Handle, Snapshot))
			{
				OutFailure = MakeFailure(
					EOpenMobileSensorFailureReason::OperationalFailure,
					TEXT("RecordingStreamLost"));
				return EHiddenSubscriptionStartStatus::Failed;
			}
			switch (Snapshot.State)
			{
			case EOpenMobileSensorSubscriptionState::Active:
			case EOpenMobileSensorSubscriptionState::Paused:
				break;
			case EOpenMobileSensorSubscriptionState::Accepted:
			case EOpenMobileSensorSubscriptionState::Starting:
				bAllActive = false;
				break;
			case EOpenMobileSensorSubscriptionState::Invalid:
			case EOpenMobileSensorSubscriptionState::Stopping:
			case EOpenMobileSensorSubscriptionState::Stopped:
			case EOpenMobileSensorSubscriptionState::Failed:
			default:
				OutFailure = MakeFailure(
					EOpenMobileSensorFailureReason::OperationalFailure,
					TEXT("RecordingStreamStartFailed"));
				if (Snapshot.Error.IsSet())
				{
					OutFailure.Error = Snapshot.Error;
				}
				return EHiddenSubscriptionStartStatus::Failed;
			}
		}
		return bAllActive
			? EHiddenSubscriptionStartStatus::Active
			: EHiddenSubscriptionStartStatus::Pending;
	}

	void RequestWorkerStop(FRecordingEntry& Entry, bool bDiscard)
	{
		Entry.Worker->bDiscardFile.Store(bDiscard);
		Entry.Worker->bStopRequested.Store(true);
		Entry.Worker->WakeEvent->Trigger();
	}

	FOpenMobileSensorReplayResult MakeReplayFailure(
		const FGuid& RequestId,
		EOpenMobileSensorFailureReason Reason,
		const TCHAR* Code,
		const FString& Message = FString()
	)
	{
		FOpenMobileSensorReplayResult Result;
		Result.RequestId = RequestId;
		Result.Operation = MakeFailure(Reason, Code, Message);
		return Result;
	}

	void RunReplayLoader(
		const FString FilePath,
		TSharedRef<FReplayLoadResult, ESPMode::ThreadSafe> LoadResult
	)
	{
		const int64 FileSize = IFileManager::Get().FileSize(*FilePath);
		if (FileSize < 0 || FileSize > MaximumReplayFileBytes)
		{
			FScopeLock Lock(&LoadResult->Mutex);
			LoadResult->Error = FileSize < 0
				? TEXT("The replay file could not be found.")
				: TEXT("The replay file exceeds the size limit.");
			LoadResult->DecodeStatus = FileSize < 0
				? EOpenMobileSensorRecordingDecodeStatus::InvalidData
				: EOpenMobileSensorRecordingDecodeStatus::LimitExceeded;
			LoadResult->State.Store(EReplayLoadState::Failed);
			return;
		}
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
		{
			FScopeLock Lock(&LoadResult->Mutex);
			LoadResult->Error = TEXT("The replay file could not be read.");
			LoadResult->DecodeStatus =
				EOpenMobileSensorRecordingDecodeStatus::InvalidData;
			LoadResult->State.Store(EReplayLoadState::Failed);
			return;
		}
		FOpenMobileSensorRecordingDocument Document;
		EOpenMobileSensorRecordingDecodeStatus Status;
		FString Error;
		if (!FOpenMobileSensorRecordingCodec::DecodeComplete(
			Bytes, Document, Status, Error))
		{
			FScopeLock Lock(&LoadResult->Mutex);
			LoadResult->Error = MoveTemp(Error);
			LoadResult->DecodeStatus = Status;
			LoadResult->State.Store(EReplayLoadState::Failed);
			return;
		}
		{
			FScopeLock Lock(&LoadResult->Mutex);
			LoadResult->Document = MoveTemp(Document);
			LoadResult->DecodeStatus =
				EOpenMobileSensorRecordingDecodeStatus::Success;
		}
		LoadResult->State.Store(EReplayLoadState::Ready);
	}

	bool PrepareReplay(FReplayEntry& Entry, double NowSeconds)
	{
		FOpenMobileSensorRecordingDocument Document;
		{
			FScopeLock Lock(&Entry.LoadResult->Mutex);
			Document = MoveTemp(Entry.LoadResult->Document);
		}
		int64 SampleCount = 0;
		for (const FOpenMobileVectorSensorBatch& Batch
			: Document.VectorBatches)
		{
			SampleCount += Batch.Samples.Num();
		}
		if (SampleCount <= 0 || SampleCount > MAX_int32)
		{
			return false;
		}
		Entry.Samples.Reserve(static_cast<int32>(SampleCount));
		for (FOpenMobileVectorSensorBatch& Batch : Document.VectorBatches)
		{
			Entry.Samples.Append(MoveTemp(Batch.Samples));
		}
		Algo::StableSort(
			Entry.Samples,
			[](const FOpenMobileVectorSensorSample& Left,
				const FOpenMobileVectorSensorSample& Right)
			{
				return Left.Header.TimestampSeconds
					< Right.Header.TimestampSeconds;
			});
		Entry.RecordingStartSeconds =
			Entry.Samples[0].Header.TimestampSeconds;
		Entry.RecordingDurationSeconds = FMath::Max(
			0.0,
			Entry.Samples.Last().Header.TimestampSeconds
				- Entry.RecordingStartSeconds);
		if (Entry.Options.StartTimeSeconds > Entry.RecordingDurationSeconds
			|| (Entry.Options.bLoop
				&& Entry.RecordingDurationSeconds
					- Entry.Options.StartTimeSeconds <= 0.0))
		{
			return false;
		}
		Entry.NextSampleIndex = Algo::LowerBoundBy(
			Entry.Samples,
			Entry.RecordingStartSeconds + Entry.Options.StartTimeSeconds,
			[](const FOpenMobileVectorSensorSample& Sample)
			{
				return Sample.Header.TimestampSeconds;
			});
		Entry.PlaybackWallStartSeconds = NowSeconds;
		Entry.TimestampBaseSeconds = NowSeconds;
		Entry.bPlaying = true;
		return Entry.NextSampleIndex < Entry.Samples.Num();
	}

	void TickReplays(
		double NowSeconds,
		TArray<TFunction<void()>>& Callbacks
	)
	{
		TArray<FGuid> RemoveAfterTick;
		for (const TPair<
			FGuid,
			TSharedPtr<FReplayEntry, ESPMode::ThreadSafe>
		>& Pair : Replays)
		{
			const TSharedPtr<FReplayEntry, ESPMode::ThreadSafe>& Entry =
				Pair.Value;
			if (!Entry->bPlaying)
			{
				const EReplayLoadState LoadState =
					Entry->LoadResult->State.Load();
				if (LoadState == EReplayLoadState::Loading)
				{
					continue;
				}
				if (LoadState == EReplayLoadState::Failed)
				{
					FString Message;
					EOpenMobileSensorRecordingDecodeStatus DecodeStatus;
					{
						FScopeLock Lock(&Entry->LoadResult->Mutex);
						Message = Entry->LoadResult->Error;
						DecodeStatus = Entry->LoadResult->DecodeStatus;
					}
					const FOpenMobileSensorReplayResult Result =
						MakeReplayFailure(
							Entry->RequestId,
							EOpenMobileSensorFailureReason::InvalidRequest,
							DecodeStatus ==
								EOpenMobileSensorRecordingDecodeStatus::IncompatibleVersion
								? TEXT("IncompatibleRecordingVersion")
								: TEXT("InvalidRecordingFile"),
							Message);
					auto Completion = MoveTemp(Entry->Completion);
					Callbacks.Add([Completion = MoveTemp(Completion), Result]() mutable
					{
						Completion(Result);
					});
					RemoveAfterTick.Add(Entry->RequestId);
					continue;
				}
				if (!PrepareReplay(*Entry, NowSeconds))
				{
					const FOpenMobileSensorReplayResult Result =
						MakeReplayFailure(
							Entry->RequestId,
							EOpenMobileSensorFailureReason::InvalidRequest,
							TEXT("InvalidReplayTimeline"));
					auto Completion = MoveTemp(Entry->Completion);
					Callbacks.Add([Completion = MoveTemp(Completion), Result]() mutable
					{
						Completion(Result);
					});
					RemoveAfterTick.Add(Entry->RequestId);
					continue;
				}
			}

			const double PlaybackTimeSeconds = FMath::Min(
				Entry->RecordingDurationSeconds,
				Entry->Options.StartTimeSeconds
					+ (NowSeconds - Entry->PlaybackWallStartSeconds)
						* Entry->Options.PlaybackSpeed);
			FOpenMobileVectorSensorBatch DueBatch;
			while (Entry->NextSampleIndex < Entry->Samples.Num()
				&& DueBatch.Samples.Num() < 4096)
			{
				const FOpenMobileVectorSensorSample& Recorded =
					Entry->Samples[Entry->NextSampleIndex];
				const double RelativeSeconds =
					Recorded.Header.TimestampSeconds
						- Entry->RecordingStartSeconds;
				if (RelativeSeconds > PlaybackTimeSeconds)
				{
					break;
				}
				FOpenMobileVectorSensorSample& Replayed =
					DueBatch.Samples.Add_GetRef(Recorded);
				Replayed.Header.TimestampSeconds = FMath::Max(
					Entry->TimestampBaseSeconds
						+ (RelativeSeconds
							- Entry->Options.StartTimeSeconds)
							/ Entry->Options.PlaybackSpeed,
					Entry->LastPublishedTimestampSeconds + 0.000001);
				Entry->LastPublishedTimestampSeconds =
					Replayed.Header.TimestampSeconds;
				Replayed.Header.GameThreadReceiptSeconds = 0.0;
				Replayed.Header.bHasGameThreadReceiptTime = false;
				Replayed.Header.Sequence = 0;
				Replayed.Header.TimestampIssueFlags = 0;
				Replayed.Header.bSourceChanged = false;
				FOpenMobileSensorSourcePolicy::MarkReplayed(Replayed.Header);
				++Entry->NextSampleIndex;
			}
			if (!DueBatch.Samples.IsEmpty())
			{
				FOpenMobileSensorsSampleService::PublishVectorBatch(DueBatch);
			}
			if (Entry->NextSampleIndex < Entry->Samples.Num())
			{
				continue;
			}
			if (Entry->Options.bLoop)
			{
				const double CycleWallDuration =
					(Entry->RecordingDurationSeconds
						- Entry->Options.StartTimeSeconds)
					/ Entry->Options.PlaybackSpeed;
				Entry->PlaybackWallStartSeconds += CycleWallDuration;
				Entry->TimestampBaseSeconds = FMath::Max(
					Entry->PlaybackWallStartSeconds,
					Entry->LastPublishedTimestampSeconds + 0.000001);
				Entry->NextSampleIndex = Algo::LowerBoundBy(
					Entry->Samples,
					Entry->RecordingStartSeconds
						+ Entry->Options.StartTimeSeconds,
					[](const FOpenMobileVectorSensorSample& Sample)
					{
						return Sample.Header.TimestampSeconds;
					});
				continue;
			}
			FOpenMobileSensorReplayResult Result;
			Result.RequestId = Entry->RequestId;
			Result.Operation = MakeSuccess();
			Result.PlaybackTimeSeconds = Entry->RecordingDurationSeconds;
			auto Completion = MoveTemp(Entry->Completion);
			Callbacks.Add([Completion = MoveTemp(Completion), Result]() mutable
			{
				Completion(Result);
			});
			RemoveAfterTick.Add(Entry->RequestId);
		}
		for (const FGuid& RequestId : RemoveAfterTick)
		{
			Replays.Remove(RequestId);
		}
	}

	bool Tick(float DeltaSeconds);

	void EnsureTicker()
	{
		if (!TickHandle.IsValid())
		{
			TickHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateStatic(&Tick),
				0.01f
			);
		}
	}

	void StopTicker()
	{
		if (TickHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
			TickHandle.Reset();
		}
	}

	void TickAt(double NowSeconds)
	{
		check(IsInGameThread());
		TArray<TFunction<void()>> Callbacks;
		TArray<FGuid> RemoveAfterTick;
		for (const TPair<
			FGuid,
			TSharedPtr<FRecordingEntry, ESPMode::ThreadSafe>
		>& Pair : Recordings)
		{
			const TSharedPtr<FRecordingEntry, ESPMode::ThreadSafe>& Entry =
				Pair.Value;
			const EWorkerState WorkerState = Entry->Worker->State.Load();
			if (Entry->State == EOpenMobileSensorRecordingState::Starting
				&& WorkerState == EWorkerState::Ready)
			{
				if (Entry->bStopRequested)
				{
					Entry->State = EOpenMobileSensorRecordingState::Stopping;
					RequestWorkerStop(*Entry, false);
				}
				else
				{
					bool bSubscriptionStartFailed = false;
					if (!Entry->bSubscriptionsStarted)
					{
						Entry->bSubscriptionsStarted =
							StartHiddenSubscriptions(*Entry);
						bSubscriptionStartFailed =
							!Entry->bSubscriptionsStarted;
					}
					FOpenMobileSensorOperationResult StreamFailure;
					const EHiddenSubscriptionStartStatus StartStatus =
						bSubscriptionStartFailed
							? EHiddenSubscriptionStartStatus::Failed
							: GetHiddenSubscriptionStartStatus(
								*Entry, StreamFailure);
					if (StartStatus == EHiddenSubscriptionStartStatus::Active)
					{
						Entry->State =
							EOpenMobileSensorRecordingState::Recording;
						Entry->bRecordingWasActive = true;
						Entry->StartWallTimeSeconds = NowSeconds;
						FOpenMobileSensorRecordingResult Result;
						Result.Operation = MakeSuccess(
							EOpenMobileSensorResultCode::Accepted);
						Result.Recording = MakeSnapshot(
							*Entry,
							EOpenMobileSensorRecordingState::Recording);
						Entry->bStartCompletionDelivered = true;
						if (Entry->StartCompletion)
						{
							auto Completion =
								MoveTemp(Entry->StartCompletion);
							Callbacks.Add([
								Completion = MoveTemp(Completion),
								Result
							]() mutable
							{
								Completion(Result);
							});
						}
					}
					else if (StartStatus ==
						EHiddenSubscriptionStartStatus::Failed)
					{
						StopHiddenSubscriptions(*Entry);
						Entry->State =
							EOpenMobileSensorRecordingState::Failed;
						if (!Entry->FinalResult.Operation.Failure.IsSet())
						{
							Entry->FinalResult.Operation = StreamFailure;
						}
						Entry->FinalResult.Recording = MakeSnapshot(
							*Entry,
							EOpenMobileSensorRecordingState::Failed);
						RequestWorkerStop(*Entry, true);
						Entry->bStartCompletionDelivered = true;
						if (Entry->StartCompletion)
						{
							auto Completion =
								MoveTemp(Entry->StartCompletion);
							const FOpenMobileSensorRecordingResult Result =
								Entry->FinalResult;
							Callbacks.Add([
								Completion = MoveTemp(Completion),
								Result
							]() mutable
							{
								Completion(Result);
							});
						}
					}
				}
			}
			else if (Entry->State ==
					EOpenMobileSensorRecordingState::Starting
				&& WorkerState == EWorkerState::Failed)
			{
				Entry->State = EOpenMobileSensorRecordingState::Failed;
				Entry->FinalResult = MakeWorkerFailure(*Entry);
				if (Entry->StartCompletion)
				{
					auto Completion = MoveTemp(Entry->StartCompletion);
					const FOpenMobileSensorRecordingResult Result =
						Entry->FinalResult;
					Callbacks.Add([Completion = MoveTemp(Completion), Result]() mutable
					{
						Completion(Result);
					});
				}
				RemoveAfterTick.Add(Entry->RequestId);
			}
			if (Entry->State == EOpenMobileSensorRecordingState::Recording
				&& WorkerState == EWorkerState::Failed)
			{
				StopHiddenSubscriptions(*Entry);
				Entry->State = EOpenMobileSensorRecordingState::Failed;
				Entry->FinalResult = MakeWorkerFailure(*Entry);
			}
			if (Entry->State == EOpenMobileSensorRecordingState::Recording
				&& NowSeconds - Entry->StartWallTimeSeconds >=
					Entry->Options.MaximumDurationSeconds)
			{
				StopHiddenSubscriptions(*Entry);
				Entry->State = EOpenMobileSensorRecordingState::Stopping;
				Entry->bAutoStopped = true;
				RequestWorkerStop(*Entry, false);
			}
			if (Entry->State == EOpenMobileSensorRecordingState::Stopping
				&& (WorkerState == EWorkerState::Completed
					|| WorkerState == EWorkerState::Failed))
			{
				Entry->FinalResult = WorkerState == EWorkerState::Completed
					? FOpenMobileSensorRecordingResult{}
					: MakeWorkerFailure(*Entry);
				if (WorkerState == EWorkerState::Completed)
				{
					Entry->FinalResult.Operation = MakeSuccess();
					Entry->FinalResult.Recording = MakeSnapshot(
						*Entry,
						EOpenMobileSensorRecordingState::Completed);
					Entry->State = EOpenMobileSensorRecordingState::Completed;
				}
				else
				{
					Entry->State = EOpenMobileSensorRecordingState::Failed;
				}
				if (Entry->StopCompletion)
				{
					auto Completion = MoveTemp(Entry->StopCompletion);
					const FOpenMobileSensorRecordingResult Result =
						Entry->FinalResult;
					Callbacks.Add([Completion = MoveTemp(Completion), Result]() mutable
					{
						Completion(Result);
					});
					RemoveAfterTick.Add(Entry->RequestId);
				}
			}
			if ((Entry->State == EOpenMobileSensorRecordingState::Failed
					&& !Entry->StopCompletion
					&& !Entry->bRecordingWasActive)
				&& Entry->WorkerFuture.IsReady())
			{
				RemoveAfterTick.AddUnique(Entry->RequestId);
			}
		}
		for (const FGuid& RequestId : RemoveAfterTick)
		{
			Recordings.Remove(RequestId);
		}
		TickReplays(NowSeconds, Callbacks);
		for (TFunction<void()>& Callback : Callbacks)
		{
			Callback();
		}
	}

	bool Tick(float DeltaSeconds)
	{
		static_cast<void>(DeltaSeconds);
		if (!bShuttingDown)
		{
			TickAt(FPlatformTime::Seconds());
		}
		return true;
	}

	void CancelEntries(const FGuid* OwnerIdentifier)
	{
		check(IsInGameThread());
		TArray<TSharedPtr<FRecordingEntry, ESPMode::ThreadSafe>> Cancelled;
		for (const TPair<
			FGuid,
			TSharedPtr<FRecordingEntry, ESPMode::ThreadSafe>
		>& Pair : Recordings)
		{
			if (!OwnerIdentifier
				|| Pair.Value->OwnerIdentifier == *OwnerIdentifier)
			{
				StopHiddenSubscriptions(*Pair.Value);
				RequestWorkerStop(*Pair.Value, true);
				Cancelled.Add(Pair.Value);
			}
		}
		for (const TSharedPtr<FRecordingEntry, ESPMode::ThreadSafe>& Entry
			: Cancelled)
		{
			Entry->WorkerFuture.Wait();
			Recordings.Remove(Entry->RequestId);
		}
		TArray<TSharedPtr<FReplayEntry, ESPMode::ThreadSafe>> CancelledReplays;
		for (const TPair<
			FGuid,
			TSharedPtr<FReplayEntry, ESPMode::ThreadSafe>
		>& Pair : Replays)
		{
			if (!OwnerIdentifier
				|| Pair.Value->OwnerIdentifier == *OwnerIdentifier)
			{
				CancelledReplays.Add(Pair.Value);
			}
		}
		for (const TSharedPtr<FReplayEntry, ESPMode::ThreadSafe>& Entry
			: CancelledReplays)
		{
			Entry->LoadFuture.Wait();
			Replays.Remove(Entry->RequestId);
		}
	}
}

void FOpenMobileSensorsRecordingService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsRecordingServicePrivate;
	if (!bShuttingDown)
	{
		return;
	}
	bShuttingDown = false;
	if (!VectorBatchHandle.IsValid())
	{
		VectorBatchHandle = FOpenMobileSensorsSampleService::OnVectorBatch()
			.AddStatic(&CaptureVectorBatch);
	}
	EnsureTicker();
}

void FOpenMobileSensorsRecordingService::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsRecordingServicePrivate;
	if (bShuttingDown)
	{
		return;
	}
	bShuttingDown = true;
	StopTicker();
	if (VectorBatchHandle.IsValid())
	{
		FOpenMobileSensorsSampleService::OnVectorBatch().Remove(
			VectorBatchHandle);
		VectorBatchHandle.Reset();
	}
	CancelEntries(nullptr);
}

FGuid FOpenMobileSensorsRecordingService::StartRecording(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorRecordingOptions& Options,
	TFunction<void(const FOpenMobileSensorRecordingResult&)>&& Completion
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsRecordingServicePrivate;
	const FGuid RequestId = FGuid::NewGuid();
	FOpenMobileSensorOperationResult Failure;
	if (bShuttingDown
		|| Recordings.Num() >= MaximumConcurrentRecordings
		|| !ValidateOptions(OwnerIdentifier, Options, Failure))
	{
		FOpenMobileSensorRecordingResult Result;
		Result.Recording.RequestId = RequestId;
		Result.Recording.State = EOpenMobileSensorRecordingState::Failed;
		Result.Operation = Failure.Failure.IsSet()
			? Failure
			: MakeFailure(
				EOpenMobileSensorFailureReason::TemporarilyUnavailable,
				TEXT("RecordingServiceUnavailable"));
		Completion(Result);
		return RequestId;
	}

	const FString BaseName = RequestId.ToString(EGuidFormats::Digits);
	const FString Directory = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("OpenMobile"),
		TEXT("Sensors"),
		TEXT("Recordings"));
	TSharedRef<FRecordingWorkerState, ESPMode::ThreadSafe> Worker =
		MakeShared<FRecordingWorkerState, ESPMode::ThreadSafe>();
	Worker->TemporaryPath = FPaths::Combine(
		Directory, BaseName + TEXT(".omsensors.partial"));
	Worker->FinalPath = FPaths::Combine(
		Directory, BaseName + TEXT(".omsensors"));
	Worker->Header = MakeHeader(Options);
	Worker->MaximumBytes = Options.MaximumBytes;
	Worker->MaximumBufferedBatches = FMath::Clamp(
		GetDefault<UOpenMobileSensorsSettings>()->
			MaximumRecordingBufferedBatches,
		1,
		4096);

	TSharedRef<FRecordingEntry, ESPMode::ThreadSafe> Entry =
		MakeShared<FRecordingEntry, ESPMode::ThreadSafe>();
	Entry->OwnerIdentifier = OwnerIdentifier;
	Entry->StreamOwnerIdentifier = FGuid::NewGuid();
	Entry->RequestId = RequestId;
	Entry->Options = Options;
	Entry->Worker = Worker;
	Entry->StartCompletion = MoveTemp(Completion);
	Entry->WorkerFuture = Async(
		EAsyncExecution::ThreadPool,
		[Worker]()
		{
			RunWriter(Worker);
		});
	Recordings.Add(RequestId, Entry);
	EnsureTicker();
	return RequestId;
}

FGuid FOpenMobileSensorsRecordingService::StopRecording(
	const FGuid& OwnerIdentifier,
	const FGuid& RequestId,
	TFunction<void(const FOpenMobileSensorRecordingResult&)>&& Completion
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsRecordingServicePrivate;
	const TSharedPtr<FRecordingEntry, ESPMode::ThreadSafe>* Found =
		Recordings.Find(RequestId);
	if (!OwnerIdentifier.IsValid()
		|| !RequestId.IsValid()
		|| !Found
		|| (*Found)->OwnerIdentifier != OwnerIdentifier)
	{
		FOpenMobileSensorRecordingResult Result;
		Result.Recording.RequestId = RequestId;
		Result.Recording.State = EOpenMobileSensorRecordingState::Failed;
		Result.Operation = MakeFailure(
			!RequestId.IsValid()
				? EOpenMobileSensorFailureReason::InvalidRequest
				: EOpenMobileSensorFailureReason::InvalidHandle,
			TEXT("InvalidRecordingHandle"));
		Completion(Result);
		return RequestId;
	}
	FRecordingEntry& Entry = **Found;
	if (Entry.State == EOpenMobileSensorRecordingState::Completed
		|| Entry.State == EOpenMobileSensorRecordingState::Failed)
	{
		const FOpenMobileSensorRecordingResult Result = Entry.FinalResult;
		Recordings.Remove(RequestId);
		Completion(Result);
		return RequestId;
	}
	if (Entry.bStopRequested || Entry.StopCompletion)
	{
		FOpenMobileSensorRecordingResult Result;
		Result.Recording = MakeSnapshot(
			Entry, EOpenMobileSensorRecordingState::Stopping);
		Result.Operation = MakeFailure(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable,
			TEXT("RecordingAlreadyStopping"));
		Completion(Result);
		return RequestId;
	}
	Entry.StopCompletion = MoveTemp(Completion);
	Entry.bStopRequested = true;
	TFunction<void(const FOpenMobileSensorRecordingResult&)>
		CancelledStartCompletion;
	FOpenMobileSensorRecordingResult CancelledStartResult;
	if (!Entry.bStartCompletionDelivered && Entry.StartCompletion)
	{
		CancelledStartResult.Recording = MakeSnapshot(
			Entry, EOpenMobileSensorRecordingState::Cancelled);
		CancelledStartResult.Operation = MakeFailure(
			EOpenMobileSensorFailureReason::Cancelled,
			TEXT("RecordingStartCancelled"));
		CancelledStartCompletion = MoveTemp(Entry.StartCompletion);
		Entry.bStartCompletionDelivered = true;
	}
	StopHiddenSubscriptions(Entry);
	Entry.State = EOpenMobileSensorRecordingState::Stopping;
	RequestWorkerStop(Entry, false);
	if (CancelledStartCompletion)
	{
		CancelledStartCompletion(CancelledStartResult);
	}
	return RequestId;
}

FGuid FOpenMobileSensorsRecordingService::ReplayRecording(
	const FGuid& OwnerIdentifier,
	const FString& FilePath,
	const FOpenMobileSensorReplayOptions& Options,
	TFunction<void(const FOpenMobileSensorReplayResult&)>&& Completion
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsRecordingServicePrivate;
	const FGuid RequestId = FGuid::NewGuid();
	if (bShuttingDown
		|| Replays.Num() >= MaximumConcurrentReplays
		|| !OwnerIdentifier.IsValid()
		|| FilePath.TrimStartAndEnd().IsEmpty()
		|| !FMath::IsFinite(Options.PlaybackSpeed)
		|| Options.PlaybackSpeed < MinimumPlaybackSpeed
		|| Options.PlaybackSpeed > MaximumPlaybackSpeed
		|| !FMath::IsFinite(Options.StartTimeSeconds)
		|| Options.StartTimeSeconds < 0.0)
	{
		Completion(MakeReplayFailure(
			RequestId,
			EOpenMobileSensorFailureReason::InvalidRequest,
			TEXT("InvalidReplayOptions")));
		return RequestId;
	}
#if UE_BUILD_SHIPPING
	Completion(MakeReplayFailure(
		RequestId,
		EOpenMobileSensorFailureReason::ConfigurationBlocked,
		TEXT("ReplayDisabledInShipping")));
	return RequestId;
#else
	TSharedRef<FReplayLoadResult, ESPMode::ThreadSafe> LoadResult =
		MakeShared<FReplayLoadResult, ESPMode::ThreadSafe>();
	TSharedRef<FReplayEntry, ESPMode::ThreadSafe> Entry =
		MakeShared<FReplayEntry, ESPMode::ThreadSafe>();
	Entry->OwnerIdentifier = OwnerIdentifier;
	Entry->RequestId = RequestId;
	Entry->FilePath = FilePath;
	Entry->Options = Options;
	Entry->LoadResult = LoadResult;
	Entry->Completion = MoveTemp(Completion);
	Entry->LoadFuture = Async(
		EAsyncExecution::ThreadPool,
		[FilePath, LoadResult]()
		{
			RunReplayLoader(FilePath, LoadResult);
		});
	Replays.Add(RequestId, Entry);
	EnsureTicker();
	return RequestId;
#endif
}

void FOpenMobileSensorsRecordingService::CancelOwner(
	const FGuid& OwnerIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsRecordingServicePrivate;
	if (OwnerIdentifier.IsValid())
	{
		CancelEntries(&OwnerIdentifier);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileSensorsRecordingService::TickForTests(double NowSeconds)
{
	using namespace OpenMobileSensorsRecordingServicePrivate;
	if (!bShuttingDown)
	{
		TickAt(NowSeconds);
	}
}

void FOpenMobileSensorsRecordingService::ResetForTests()
{
	check(IsInGameThread());
	BeginShutdown();
	Start();
}
#endif
