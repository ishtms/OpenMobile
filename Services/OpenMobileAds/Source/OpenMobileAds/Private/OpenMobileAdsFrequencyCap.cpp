#include "OpenMobileAdsFrequencyCap.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace OpenMobileAdsFrequencyCapPrivate
{
	constexpr uint32 Magic = 0x43414D4F;
	constexpr uint32 Version = 1;
	constexpr int64 MaxFileBytes = 1024 * 1024;
	constexpr int32 MaxPlacements = 1024;
	constexpr int32 MaxRecordsPerPlacement =
		FOpenMobileAdsFrequencyCap::MaximumRollingImpressions;
	constexpr int32 MaxTotalRecords = 65536;
	constexpr int32 MaxPlacementBytes = 1024;

	template <typename ValueType>
	void WriteValue(TArray<uint8>& Data, const ValueType& Value)
	{
		const int32 Offset = Data.AddUninitialized(sizeof(ValueType));
		FMemory::Memcpy(Data.GetData() + Offset, &Value, sizeof(ValueType));
	}

	class FBoundedReader
	{
	public:
		explicit FBoundedReader(TConstArrayView<uint8> InData)
			: Data(InData)
		{
		}

		template <typename ValueType>
		bool Read(ValueType& OutValue)
		{
			if (Offset > Data.Num() - static_cast<int32>(sizeof(ValueType)))
			{
				return false;
			}
			FMemory::Memcpy(&OutValue, Data.GetData() + Offset, sizeof(ValueType));
			Offset += sizeof(ValueType);
			return true;
		}

		bool ReadPlacement(FName& OutPlacement)
		{
			int32 ByteCount = 0;
			if (
				!Read(ByteCount)
				|| ByteCount <= 0
				|| ByteCount > MaxPlacementBytes
				|| Offset > Data.Num() - ByteCount
			)
			{
				return false;
			}
			FUTF8ToTCHAR Converted(
				reinterpret_cast<const ANSICHAR*>(Data.GetData() + Offset),
				ByteCount
			);
			Offset += ByteCount;
			const FString Placement(Converted.Length(), Converted.Get());
			if (Placement.IsEmpty())
			{
				return false;
			}
			OutPlacement = FName(*Placement);
			return !OutPlacement.IsNone();
		}

		bool IsAtEnd() const
		{
			return Offset == Data.Num();
		}

	private:
		TConstArrayView<uint8> Data;
		int32 Offset = 0;
	};

	void WritePlacement(TArray<uint8>& Data, FName Placement)
	{
		const FString PlacementString = Placement.ToString();
		const FTCHARToUTF8 Converted(*PlacementString);
		const int32 ByteCount = Converted.Length();
		WriteValue(Data, ByteCount);
		Data.Append(
			reinterpret_cast<const uint8*>(Converted.Get()),
			ByteCount
		);
	}

	class FMemoryStore final : public IOpenMobileAdsFrequencyCapStore
	{
	public:
		virtual bool Load(TArray<uint8>& OutData) override
		{
			if (Data.IsEmpty())
			{
				return false;
			}
			OutData = Data;
			return true;
		}

		virtual bool Save(TConstArrayView<uint8> InData) override
		{
			Data.Reset(InData.Num());
			Data.Append(InData.GetData(), InData.Num());
			return true;
		}

	private:
		TArray<uint8> Data;
	};

	class FFileStore final : public IOpenMobileAdsFrequencyCapStore
	{
	public:
		FFileStore()
			: Path(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("OpenMobileAds"),
				TEXT("FrequencyCaps.dat")
			))
		{
		}

		virtual bool Load(TArray<uint8>& OutData) override
		{
			const int64 Size = IFileManager::Get().FileSize(*Path);
			return Size > 0
				&& Size <= MaxFileBytes
				&& FFileHelper::LoadFileToArray(OutData, *Path);
		}

		virtual bool Save(TConstArrayView<uint8> Data) override
		{
			if (Data.IsEmpty() || Data.Num() > MaxFileBytes)
			{
				return false;
			}
			IFileManager& FileManager = IFileManager::Get();
			if (!FileManager.MakeDirectory(*FPaths::GetPath(Path), true))
			{
				return false;
			}
			const FString TemporaryPath = Path + TEXT(".tmp");
			TArray64<uint8> OwnedData;
			OwnedData.Append(Data.GetData(), Data.Num());
			if (!FFileHelper::SaveArrayToFile(OwnedData, *TemporaryPath))
			{
				FileManager.Delete(*TemporaryPath, false, true, true);
				return false;
			}
			if (!FileManager.Move(*Path, *TemporaryPath, true, true, true, true))
			{
				FileManager.Delete(*TemporaryPath, false, true, true);
				return false;
			}
			return true;
		}

	private:
		FString Path;
	};
}

FOpenMobileAdsFrequencyCapTracker::FOpenMobileAdsFrequencyCapTracker(
	TSharedRef<IOpenMobileAdsFrequencyCapStore> InStore
)
	: Store(MoveTemp(InStore))
{
}

void FOpenMobileAdsFrequencyCapTracker::Initialize(
	FDateTime UtcNow,
	double MonotonicSeconds
)
{
	SessionImpressionCounts.Reset();
	RollingImpressionMonotonicTimes.Reset();
	bInitialized = true;
	TArray<uint8> Data;
	if (Store->Load(Data))
	{
		Deserialize(Data, UtcNow, MonotonicSeconds);
	}
}

FOpenMobileAdsFrequencyCapDecision FOpenMobileAdsFrequencyCapTracker::Evaluate(
	FName Placement,
	const FOpenMobileAdsFrequencyCap& Policy,
	FDateTime UtcNow,
	double MonotonicSeconds
) const
{
	FOpenMobileAdsFrequencyCapDecision Decision;
	if (!bInitialized || Placement.IsNone() || !FMath::IsFinite(MonotonicSeconds))
	{
		return Decision;
	}
	if (
		Policy.IsSessionLimitEnabled()
		&& SessionImpressionCounts.FindRef(Placement)
			>= Policy.MaxSessionImpressions
	)
	{
		Decision.Scope = EOpenMobileAdsFrequencyCapScope::Session;
		return Decision;
	}
	if (!Policy.IsRollingWindowEnabled() || !FMath::IsFinite(Policy.WindowSeconds))
	{
		return Decision;
	}
	const TArray<double>* Times = RollingImpressionMonotonicTimes.Find(Placement);
	if (!Times || Times->Num() < Policy.MaxImpressions)
	{
		return Decision;
	}
	const int32 FirstCappedIndex = Times->Num() - Policy.MaxImpressions;
	const double SecondsUntilEligible =
		(*Times)[FirstCappedIndex] + Policy.WindowSeconds - MonotonicSeconds;
	if (SecondsUntilEligible > 0.0 && FMath::IsFinite(SecondsUntilEligible))
	{
		Decision.Scope = EOpenMobileAdsFrequencyCapScope::RollingWindow;
		const double MaximumFutureSeconds =
			(FDateTime::MaxValue() - UtcNow).GetTotalSeconds();
		Decision.NextEligibleAt = SecondsUntilEligible >= MaximumFutureSeconds
			? FDateTime::MaxValue()
			: UtcNow + FTimespan::FromSeconds(SecondsUntilEligible);
	}
	return Decision;
}

bool FOpenMobileAdsFrequencyCapTracker::RecordImpression(
	FName Placement,
	const FOpenMobileAdsFrequencyCap& Policy,
	FDateTime UtcNow,
	double MonotonicSeconds
)
{
	if (!bInitialized || Placement.IsNone() || !FMath::IsFinite(MonotonicSeconds))
	{
		return false;
	}
	int32& SessionCount = SessionImpressionCounts.FindOrAdd(Placement);
	if (SessionCount < MAX_int32)
	{
		++SessionCount;
	}
	if (!Policy.IsRollingWindowEnabled() || !FMath::IsFinite(Policy.WindowSeconds))
	{
		return true;
	}
	TArray<double>& Times = RollingImpressionMonotonicTimes.FindOrAdd(Placement);
	const double OldestAllowed = MonotonicSeconds - Policy.WindowSeconds;
	int32 ExpiredCount = 0;
	while (ExpiredCount < Times.Num() && Times[ExpiredCount] <= OldestAllowed)
	{
		++ExpiredCount;
	}
	if (ExpiredCount > 0)
	{
		Times.RemoveAt(0, ExpiredCount, EAllowShrinking::No);
	}
	Times.Add(MonotonicSeconds);
	if (Times.Num() > Policy.MaxImpressions)
	{
		Times.RemoveAt(
			0,
			Times.Num() - Policy.MaxImpressions,
			EAllowShrinking::No
		);
	}
	return Flush(UtcNow, MonotonicSeconds);
}

bool FOpenMobileAdsFrequencyCapTracker::Flush(
	FDateTime UtcNow,
	double MonotonicSeconds
)
{
	if (!bInitialized || !FMath::IsFinite(MonotonicSeconds))
	{
		return false;
	}
	if (RollingImpressionMonotonicTimes.IsEmpty())
	{
		return true;
	}
	TArray<uint8> Data;
	return Serialize(Data, UtcNow, MonotonicSeconds) && Store->Save(Data);
}

bool FOpenMobileAdsFrequencyCapTracker::Deserialize(
	TConstArrayView<uint8> Data,
	FDateTime UtcNow,
	double MonotonicSeconds
)
{
	using namespace OpenMobileAdsFrequencyCapPrivate;
	FBoundedReader Reader(Data);
	uint32 StoredMagic = 0;
	uint32 StoredVersion = 0;
	int64 SavedAtTicks = 0;
	int32 PlacementCount = 0;
	if (
		!Reader.Read(StoredMagic)
		|| !Reader.Read(StoredVersion)
		|| !Reader.Read(SavedAtTicks)
		|| !Reader.Read(PlacementCount)
		|| StoredMagic != Magic
		|| StoredVersion != Version
		|| SavedAtTicks < FDateTime::MinValue().GetTicks()
		|| SavedAtTicks > FDateTime::MaxValue().GetTicks()
		|| PlacementCount < 0
		|| PlacementCount > MaxPlacements
		|| !FMath::IsFinite(MonotonicSeconds)
	)
	{
		return false;
	}
	const FDateTime SavedAt(SavedAtTicks);
	const double WallElapsedSeconds = UtcNow > SavedAt
		? (UtcNow - SavedAt).GetTotalSeconds()
		: 0.0;
	if (!FMath::IsFinite(WallElapsedSeconds))
	{
		return false;
	}

	TMap<FName, TArray<double>> LoadedTimes;
	int32 TotalRecords = 0;
	for (int32 PlacementIndex = 0; PlacementIndex < PlacementCount; ++PlacementIndex)
	{
		FName Placement;
		int32 RecordCount = 0;
		if (
			!Reader.ReadPlacement(Placement)
			|| !Reader.Read(RecordCount)
			|| RecordCount < 0
			|| RecordCount > MaxRecordsPerPlacement
			|| TotalRecords > MaxTotalRecords - RecordCount
			|| LoadedTimes.Contains(Placement)
		)
		{
			return false;
		}
		TotalRecords += RecordCount;
		TArray<double>& Times = LoadedTimes.Add(Placement);
		Times.Reserve(RecordCount);
		for (int32 RecordIndex = 0; RecordIndex < RecordCount; ++RecordIndex)
		{
			double SavedAgeSeconds = 0.0;
			if (
				!Reader.Read(SavedAgeSeconds)
				|| !FMath::IsFinite(SavedAgeSeconds)
				|| SavedAgeSeconds < 0.0
			)
			{
				return false;
			}
			Times.Add(
				MonotonicSeconds - SavedAgeSeconds - WallElapsedSeconds
			);
		}
		Times.Sort();
	}
	if (!Reader.IsAtEnd())
	{
		return false;
	}
	RollingImpressionMonotonicTimes = MoveTemp(LoadedTimes);
	return true;
}

bool FOpenMobileAdsFrequencyCapTracker::Serialize(
	TArray<uint8>& OutData,
	FDateTime UtcNow,
	double MonotonicSeconds
) const
{
	using namespace OpenMobileAdsFrequencyCapPrivate;
	if (!FMath::IsFinite(MonotonicSeconds))
	{
		return false;
	}
	OutData.Reset();
	WriteValue(OutData, Magic);
	WriteValue(OutData, Version);
	const int64 SavedAtTicks = UtcNow.GetTicks();
	WriteValue(OutData, SavedAtTicks);
	const int32 PlacementCount = RollingImpressionMonotonicTimes.Num();
	if (PlacementCount > MaxPlacements)
	{
		return false;
	}
	WriteValue(OutData, PlacementCount);
	int32 TotalRecords = 0;
	for (const TPair<FName, TArray<double>>& Pair : RollingImpressionMonotonicTimes)
	{
		const int32 RecordCount = Pair.Value.Num();
		if (
			Pair.Key.IsNone()
			|| RecordCount > MaxRecordsPerPlacement
			|| TotalRecords > MaxTotalRecords - RecordCount
		)
		{
			return false;
		}
		TotalRecords += RecordCount;
		WritePlacement(OutData, Pair.Key);
		WriteValue(OutData, RecordCount);
		for (const double RecordedAt : Pair.Value)
		{
			const double AgeSeconds = FMath::Max(
				0.0,
				MonotonicSeconds - RecordedAt
			);
			if (!FMath::IsFinite(AgeSeconds))
			{
				return false;
			}
			WriteValue(OutData, AgeSeconds);
		}
	}
	return OutData.Num() <= MaxFileBytes;
}

TSharedRef<IOpenMobileAdsFrequencyCapStore>
OpenMobileAdsCreateFrequencyCapStore(bool bPersistent)
{
	using namespace OpenMobileAdsFrequencyCapPrivate;
	if (bPersistent)
	{
		return MakeShared<FFileStore>();
	}
	return MakeShared<FMemoryStore>();
}
