#include "OpenMobileSensorProvenanceCodec.h"

#include "OpenMobileSensorFusionQuality.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace OpenMobileSensorProvenanceCodecPrivate
{
	constexpr uint8 ProvenanceVersion = 1;

	void SerializeFusion(
		FArchive& Archive,
		FOpenMobileSensorFusionContext& Fusion
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
	OpenMobileSensorProvenanceCodecPrivate::SerializeFusion(Writer, Fusion);
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
	Reader << SourceFlags;
	OpenMobileSensorProvenanceCodecPrivate::SerializeFusion(Reader, Fusion);
	if (Reader.IsError()
		|| !Reader.AtEnd()
		|| Version !=
			OpenMobileSensorProvenanceCodecPrivate::ProvenanceVersion
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
