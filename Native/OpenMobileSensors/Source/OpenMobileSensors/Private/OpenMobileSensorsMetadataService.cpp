#include "OpenMobileSensorsMetadataService.h"

#include "IOpenMobileSensorsBackend.h"
#include "Misc/Crc.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsBackendTypes.h"

namespace OpenMobileSensorsMetadataServicePrivate
{
	struct FNormalizedMetadataCandidate
	{
		FOpenMobileSensorMetadata Metadata;
		FString NativeDiagnostic;
		bool bBackendPreferred = false;
	};

	bool bStarted = false;
	bool bBackendDirty = true;
	bool bHasDiscovery = false;
	IOpenMobileSensorsBackend* CachedBackend = nullptr;
	FOpenMobileSensorsBackendToken CachedBackendToken;
	TArray<FOpenMobileSensorBackendMetadata> CachedBackendMetadata;
	TArray<FOpenMobileSensorMetadata> CachedMetadata;
	TArray<FString> CachedVerboseNativeMetadata;

	double GetMeasurementScale(EOpenMobileSensorMetadataUnit Unit)
	{
		switch (Unit)
		{
		case EOpenMobileSensorMetadataUnit::StandardGravity:
			return 9.80665;
		case EOpenMobileSensorMetadataUnit::DegreesPerSecond:
			return UE_DOUBLE_PI / 180.0;
		case EOpenMobileSensorMetadataUnit::Tesla:
			return 1000000.0;
		case EOpenMobileSensorMetadataUnit::Pascal:
			return 0.01;
		case EOpenMobileSensorMetadataUnit::Centimeters:
			return 0.01;
		case EOpenMobileSensorMetadataUnit::Millimeters:
			return 0.001;
		case EOpenMobileSensorMetadataUnit::Portable:
		default:
			return 1.0;
		}
	}

	double GetIntervalScale(EOpenMobileSensorMetadataTimeUnit Unit)
	{
		switch (Unit)
		{
		case EOpenMobileSensorMetadataTimeUnit::Milliseconds:
			return 0.001;
		case EOpenMobileSensorMetadataTimeUnit::Microseconds:
			return 0.000001;
		case EOpenMobileSensorMetadataTimeUnit::Nanoseconds:
			return 0.000000001;
		case EOpenMobileSensorMetadataTimeUnit::Seconds:
		default:
			return 1.0;
		}
	}

	void NormalizeNumber(
		FOpenMobileSensorOptionalNumber& Number,
		double Scale
	)
	{
		if (!Number.bAvailable)
		{
			Number.Value = 0.0;
			return;
		}
		const double Normalized = Number.Value * Scale;
		if (!FMath::IsFinite(Number.Value)
			|| Number.Value < 0.0
			|| !FMath::IsFinite(Normalized))
		{
			Number = {};
			return;
		}
		Number.Value = Normalized;
	}

	void NormalizeMeasurement(
		FOpenMobileSensorMetadata& Metadata,
		EOpenMobileSensorMetadataUnit Unit
	)
	{
		const double Scale = GetMeasurementScale(Unit);
		NormalizeNumber(Metadata.MaximumRange, Scale);
		NormalizeNumber(Metadata.Resolution, Scale);
	}

	void NormalizeInterval(
		FOpenMobileSensorMetadata& Metadata,
		EOpenMobileSensorMetadataTimeUnit Unit
	)
	{
		const double Scale = GetIntervalScale(Unit);
		NormalizeNumber(Metadata.MinimumIntervalSeconds, Scale);
		NormalizeNumber(Metadata.MaximumIntervalSeconds, Scale);
	}

	FString SanitizePortableText(const FString& Value)
	{
		FString Sanitized;
		Sanitized.Reserve(FMath::Min(Value.Len(), 128));
		for (const TCHAR Character : Value)
		{
			if (Character >= TEXT(' ') && Character != TCHAR(0x7f))
			{
				Sanitized.AppendChar(Character);
			}
			if (Sanitized.Len() == 128)
			{
				break;
			}
		}
		Sanitized.TrimStartAndEndInline();
		return Sanitized;
	}

	void SanitizeOptionalText(FOpenMobileSensorOptionalText& Text)
	{
		if (!Text.bAvailable)
		{
			Text.Value.Reset();
			return;
		}
		Text.Value = SanitizePortableText(Text.Value);
		if (Text.Value.IsEmpty())
		{
			Text.bAvailable = false;
		}
	}

	FString SanitizeNativeIdentifier(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return {};
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsAlnum(Character)
				&& Character != TEXT('.')
				&& Character != TEXT('_')
				&& Character != TEXT('-'))
			{
				return TEXT("redacted");
			}
		}
		return Value.Left(64);
	}

	bool IsPortableInstanceId(const FString& Value)
	{
		if (Value.IsEmpty()
			|| Value.Len() > 64
			|| Value.StartsWith(TEXT("0x"), ESearchCase::IgnoreCase)
			|| !FChar::IsAlpha(Value[0]))
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsAlnum(Character)
				&& Character != TEXT('.')
				&& Character != TEXT('_')
				&& Character != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}

	FName MakeStableInstanceId(
		const FOpenMobileSensorBackendMetadata& Candidate
	)
	{
		const FName ProvidedName = Candidate.Metadata.Sensor.InstanceId;
		const FString Provided = ProvidedName.ToString();
		if (!ProvidedName.IsNone() && IsPortableInstanceId(Provided))
		{
			return FName(*Provided);
		}

		const FString StableSource = FString::Printf(
			TEXT("%s|%s|%s|%s|%lld"),
			*FOpenMobileSensorTypes::GetStableName(
				Candidate.Metadata.Sensor.Type
			).ToString(),
			*Candidate.NativeIdentifier,
			*Candidate.Metadata.Vendor.Value,
			*Candidate.Metadata.NativeName.Value,
			Candidate.Metadata.Version.Value
		);
		const uint32 FirstHash = FCrc::StrCrc32(*StableSource);
		const uint32 SecondHash = FCrc::StrCrc32(
			*(StableSource + TEXT("|OpenMobile"))
		);
		return FName(*FString::Printf(
			TEXT("Sensor-%08X%08X"),
			FirstHash,
			SecondHash
		));
	}

	FNormalizedMetadataCandidate NormalizeCandidate(
		const FOpenMobileSensorBackendMetadata& Candidate
	)
	{
		FNormalizedMetadataCandidate Normalized;
		Normalized.Metadata = Candidate.Metadata;
		Normalized.bBackendPreferred = Candidate.Metadata.bPreferred;
		Normalized.Metadata.bPreferred = false;
		Normalized.Metadata.Sensor.InstanceId =
			MakeStableInstanceId(Candidate);
		SanitizeOptionalText(Normalized.Metadata.Vendor);
		SanitizeOptionalText(Normalized.Metadata.NativeName);
		if (!Normalized.Metadata.Version.bAvailable
			|| Normalized.Metadata.Version.Value < 0)
		{
			Normalized.Metadata.Version = {};
		}
		NormalizeMeasurement(Normalized.Metadata, Candidate.MeasurementUnit);
		NormalizeNumber(Normalized.Metadata.EstimatedPowerMilliwatts, 1.0);
		NormalizeInterval(Normalized.Metadata, Candidate.IntervalUnit);
		if (!Normalized.Metadata.FifoCapacitySamples.bAvailable
			|| Normalized.Metadata.FifoCapacitySamples.Value < 0)
		{
			Normalized.Metadata.FifoCapacitySamples = {};
		}
		if (!Normalized.Metadata.WakeUpBehavior.bAvailable)
		{
			Normalized.Metadata.WakeUpBehavior.bValue = false;
		}
		if (!Normalized.Metadata.bReportingModeAvailable)
		{
			Normalized.Metadata.ReportingMode =
				EOpenMobileSensorReportingMode::Unknown;
		}
		Normalized.NativeDiagnostic =
			SanitizeNativeIdentifier(Candidate.NativeIdentifier);
		return Normalized;
	}

	int32 CompareText(const FString& Left, const FString& Right)
	{
		return Left.Compare(Right, ESearchCase::CaseSensitive);
	}

	void RebuildPublicMetadata()
	{
		TArray<FNormalizedMetadataCandidate> Candidates;
		Candidates.Reserve(CachedBackendMetadata.Num());
		for (const FOpenMobileSensorBackendMetadata& Candidate
			: CachedBackendMetadata)
		{
			if (Candidate.Metadata.Sensor.Type !=
				EOpenMobileSensorType::Unknown)
			{
				Candidates.Add(NormalizeCandidate(Candidate));
			}
		}
		Candidates.Sort(
			[](const FNormalizedMetadataCandidate& Left,
				const FNormalizedMetadataCandidate& Right)
			{
				if (Left.Metadata.Sensor.Type != Right.Metadata.Sensor.Type)
				{
					return static_cast<uint8>(Left.Metadata.Sensor.Type)
						< static_cast<uint8>(Right.Metadata.Sensor.Type);
				}
				if (Left.bBackendPreferred != Right.bBackendPreferred)
				{
					return Left.bBackendPreferred;
				}
				const int32 IdentifierOrder = CompareText(
					Left.Metadata.Sensor.InstanceId.ToString(),
					Right.Metadata.Sensor.InstanceId.ToString()
				);
				if (IdentifierOrder != 0)
				{
					return IdentifierOrder < 0;
				}
				const int32 NameOrder = CompareText(
					Left.Metadata.NativeName.Value,
					Right.Metadata.NativeName.Value
				);
				if (NameOrder != 0)
				{
					return NameOrder < 0;
				}
				const int32 VendorOrder = CompareText(
					Left.Metadata.Vendor.Value,
					Right.Metadata.Vendor.Value
				);
				if (VendorOrder != 0)
				{
					return VendorOrder < 0;
				}
				return CompareText(
					Left.NativeDiagnostic,
					Right.NativeDiagnostic
				) < 0;
			}
		);

		CachedMetadata.Reset(Candidates.Num());
		CachedVerboseNativeMetadata.Reset(Candidates.Num());
		TSet<FOpenMobileSensorIdentifier> UsedIdentifiers;
		EOpenMobileSensorType PreviousType = EOpenMobileSensorType::Unknown;
		for (FNormalizedMetadataCandidate& Candidate : Candidates)
		{
			FOpenMobileSensorIdentifier Identifier = Candidate.Metadata.Sensor;
			const FName BaseInstanceId = Identifier.InstanceId;
			int32 Suffix = 2;
			while (UsedIdentifiers.Contains(Identifier))
			{
				Identifier.InstanceId = FName(*FString::Printf(
					TEXT("%s-%d"),
					*BaseInstanceId.ToString(),
					Suffix++
				));
			}
			Candidate.Metadata.Sensor = Identifier;
			Candidate.Metadata.bPreferred = Identifier.Type != PreviousType;
			PreviousType = Identifier.Type;
			UsedIdentifiers.Add(Identifier);
			CachedMetadata.Add(Candidate.Metadata);
#if !UE_BUILD_SHIPPING
			if (!Candidate.NativeDiagnostic.IsEmpty())
			{
				CachedVerboseNativeMetadata.Add(FString::Printf(
					TEXT("%s=%s"),
					*Identifier.InstanceId.ToString(),
					*Candidate.NativeDiagnostic
				));
			}
#endif
		}
	}

	void DiscoverMetadata()
	{
		bBackendDirty = false;
		bHasDiscovery = true;
		CachedBackend = nullptr;
		CachedBackendToken = {};
		CachedBackendMetadata.Reset();
		CachedMetadata.Reset();
		CachedVerboseNativeMetadata.Reset();
		IOpenMobileSensorsBackend* Backend =
			FOpenMobileSensorsBackendRegistry::FindBackend();
		if (!Backend)
		{
			return;
		}
		const FOpenMobileSensorsBackendToken Token =
			FOpenMobileSensorsBackendRegistry::CaptureToken();
		if (Token.Generation == 0)
		{
			return;
		}
		TArray<FOpenMobileSensorBackendMetadata> Metadata =
			Backend->GetSensorMetadata();
		if (!FOpenMobileSensorsBackendRegistry::IsTokenCurrent(Token))
		{
			bBackendDirty = true;
			bHasDiscovery = false;
			return;
		}
		CachedBackend = Backend;
		CachedBackendToken = Token;
		CachedBackendMetadata = MoveTemp(Metadata);
		RebuildPublicMetadata();
	}

	void RefreshMutableMetadata()
	{
		if (!CachedBackend
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				CachedBackendToken
			))
		{
			bBackendDirty = true;
			DiscoverMetadata();
			return;
		}

		TArray<FOpenMobileSensorBackendMetadata> ImmutableMetadata;
		TArray<FOpenMobileSensorBackendMetadata> MutableMetadata;
		for (const FOpenMobileSensorBackendMetadata& Candidate
			: CachedBackendMetadata)
		{
			(Candidate.bMutable ? MutableMetadata : ImmutableMetadata)
				.Add(Candidate);
		}
		if (MutableMetadata.IsEmpty())
		{
			return;
		}

		const TArray<FOpenMobileSensorBackendMetadata> OriginalMutableMetadata =
			MutableMetadata;
		CachedBackend->RefreshMutableSensorMetadata(MutableMetadata);
		if (!FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
			CachedBackendToken
			)
			|| MutableMetadata.Num() != OriginalMutableMetadata.Num())
		{
			return;
		}
		for (int32 Index = 0; Index < MutableMetadata.Num(); ++Index)
		{
			MutableMetadata[Index].Metadata.Sensor =
				OriginalMutableMetadata[Index].Metadata.Sensor;
			MutableMetadata[Index].Metadata.bPreferred =
				OriginalMutableMetadata[Index].Metadata.bPreferred;
			MutableMetadata[Index].NativeIdentifier =
				OriginalMutableMetadata[Index].NativeIdentifier;
			MutableMetadata[Index].MeasurementUnit =
				OriginalMutableMetadata[Index].MeasurementUnit;
			MutableMetadata[Index].IntervalUnit =
				OriginalMutableMetadata[Index].IntervalUnit;
			MutableMetadata[Index].bMutable = true;
		}
		CachedBackendMetadata = MoveTemp(ImmutableMetadata);
		CachedBackendMetadata.Append(MoveTemp(MutableMetadata));
		RebuildPublicMetadata();
	}
}

void FOpenMobileSensorsMetadataService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsMetadataServicePrivate;
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	bBackendDirty = true;
	bHasDiscovery = false;
}

void FOpenMobileSensorsMetadataService::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsMetadataServicePrivate;
	bStarted = false;
	bBackendDirty = true;
	bHasDiscovery = false;
	CachedBackend = nullptr;
	CachedBackendToken = {};
	CachedBackendMetadata.Reset();
	CachedMetadata.Reset();
	CachedVerboseNativeMetadata.Reset();
}

TArray<FOpenMobileSensorMetadata>
FOpenMobileSensorsMetadataService::GetMetadata()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsMetadataServicePrivate;
	if (bBackendDirty || !bHasDiscovery)
	{
		DiscoverMetadata();
	}
	else
	{
		RefreshMutableMetadata();
	}
	return CachedMetadata;
}

TArray<FString>
FOpenMobileSensorsMetadataService::GetVerboseNativeMetadataForDiagnostics()
{
	check(IsInGameThread());
#if UE_BUILD_SHIPPING
	return {};
#else
	using namespace OpenMobileSensorsMetadataServicePrivate;
	if (bBackendDirty || !bHasDiscovery)
	{
		DiscoverMetadata();
	}
	return CachedVerboseNativeMetadata;
#endif
}

void FOpenMobileSensorsMetadataService::HandleBackendGenerationChanged()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsMetadataServicePrivate;
	bBackendDirty = true;
	bHasDiscovery = false;
	CachedBackend = nullptr;
	CachedBackendToken = {};
	CachedBackendMetadata.Reset();
	CachedMetadata.Reset();
	CachedVerboseNativeMetadata.Reset();
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileSensorsMetadataService::ResetForTests()
{
	check(IsInGameThread());
	BeginShutdown();
	Start();
}
#endif
