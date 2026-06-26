#include "OpenMobileSensorProvenanceCodec.h"

#include "OpenMobileSensorFusionQuality.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace OpenMobileSensorProvenanceCodecPrivate
{
	constexpr uint8 ProvenanceVersion = 2;
	constexpr uint8 LegacyProvenanceVersion = 1;

	void SerializeFusion(
		FArchive& Archive,
		FOpenMobileSensorFusionContext& Fusion,
		uint8 Version
	)
	{
		uint8 Quality = static_cast<uint8>(Fusion.Quality);
		uint8 HasNativeQuality = Fusion.bHasNativeQualityReport ? 1 : 0;
		uint8 NativeQuality = static_cast<uint8>(Fusion.NativeQuality);
		Archive << Quality;
		Archive << HasNativeQuality;
		Archive << NativeQuality;
		Archive << Fusion.ExpectedInputMask;
		Archive << Fusion.ContributingInputMask;
		Archive << Fusion.MissingInputMask;
		Archive << Fusion.DegradedInputMask;
		if (Version >= 2)
		{
			uint8 HasEstimatedLag = Fusion.bHasEstimatedLag ? 1 : 0;
			Archive << HasEstimatedLag;
			Archive << Fusion.EstimatedLagSeconds;
			if (Archive.IsLoading())
			{
				Fusion.bHasEstimatedLag = HasEstimatedLag != 0;
			}
		}
		if (Archive.IsLoading())
		{
			Fusion.Quality =
				static_cast<EOpenMobileSensorFusionQuality>(Quality);
			Fusion.bHasNativeQualityReport = HasNativeQuality != 0;
			Fusion.NativeQuality =
				static_cast<EOpenMobileSensorFusionQuality>(NativeQuality);
		}
	}
}

bool FOpenMobileSensorProvenanceCodec::Encode(
	const FOpenMobileSensorSampleHeader& Header,
	TArray<uint8>& OutBytes
)
{
	OutBytes.Reset();
	if (!FOpenMobileSensorSourcePolicy::ValidateSourceFlags(
			Header.SourceFlags)
		|| !FOpenMobileSensorFusionQualityEvaluator::ValidateContext(
			Header.Fusion))
	{
		return false;
	}
	FMemoryWriter Writer(OutBytes, true);
	uint8 Version =
		OpenMobileSensorProvenanceCodecPrivate::ProvenanceVersion;
	int32 SourceFlags = Header.SourceFlags;
	FOpenMobileSensorFusionContext Fusion = Header.Fusion;
	Writer << Version;
	Writer << SourceFlags;
	OpenMobileSensorProvenanceCodecPrivate::SerializeFusion(
		Writer,
		Fusion,
		Version
	);
	return !Writer.IsError();
}

bool FOpenMobileSensorProvenanceCodec::Decode(
	const TArray<uint8>& Bytes,
	bool bMarkReplayed,
	FOpenMobileSensorSampleHeader& OutHeader
)
{
	FMemoryReader Reader(Bytes, true);
	uint8 Version = 0;
	int32 SourceFlags = 0;
	FOpenMobileSensorFusionContext Fusion;
	Reader << Version;
	if (Reader.IsError()
		|| (Version != OpenMobileSensorProvenanceCodecPrivate::ProvenanceVersion
			&& Version !=
				OpenMobileSensorProvenanceCodecPrivate::LegacyProvenanceVersion))
	{
		return false;
	}
	Reader << SourceFlags;
	OpenMobileSensorProvenanceCodecPrivate::SerializeFusion(
		Reader,
		Fusion,
		Version
	);
	if (Reader.IsError()
		|| !Reader.AtEnd()
		|| !FOpenMobileSensorSourcePolicy::ValidateSourceFlags(SourceFlags)
		|| !FOpenMobileSensorFusionQualityEvaluator::ValidateContext(Fusion))
	{
		return false;
	}
	OutHeader.SourceFlags = SourceFlags;
	OutHeader.Fusion = Fusion;
	if (bMarkReplayed)
	{
		FOpenMobileSensorSourcePolicy::MarkReplayed(OutHeader);
	}
	return true;
}
