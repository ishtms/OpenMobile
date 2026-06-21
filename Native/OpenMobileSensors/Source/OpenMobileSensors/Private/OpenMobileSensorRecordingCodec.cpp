#include "OpenMobileSensorRecordingCodec.h"

#include "Misc/Crc.h"

namespace OpenMobileSensorRecordingCodecPrivate
{
	constexpr uint8 FileMagic[] = {'O', 'M', 'S', 'E', 'N', 'S', 'R', '1'};
	constexpr uint8 BlockMagic[] = {'O', 'M', 'S', 'B'};
	constexpr uint32 FooterMarker = 0xc0dec0de;
	constexpr int32 MaximumStringBytes = 4096;
	constexpr int32 MaximumStreams = 256;
	constexpr int32 MaximumSamplesPerBatch = 4096;
	constexpr int32 MaximumBatches = 1000000;
	constexpr int64 MaximumDecodedBytes = 256ll * 1024 * 1024;

	enum class EBlockType : uint8
	{
		Header = 1,
		VectorBatch = 2,
		Footer = 255
	};

	class FByteWriter
	{
	public:
		explicit FByteWriter(TArray<uint8>& InBytes)
			: Bytes(InBytes)
		{
		}

		void WriteUInt8(uint8 Value)
		{
			Bytes.Add(Value);
		}

		void WriteUInt16(uint16 Value)
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				Bytes.Add(static_cast<uint8>(Value >> (Index * 8)));
			}
		}

		void WriteUInt32(uint32 Value)
		{
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Bytes.Add(static_cast<uint8>(Value >> (Index * 8)));
			}
		}

		void WriteUInt64(uint64 Value)
		{
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Bytes.Add(static_cast<uint8>(Value >> (Index * 8)));
			}
		}

		void WriteInt32(int32 Value)
		{
			WriteUInt32(static_cast<uint32>(Value));
		}

		void WriteInt64(int64 Value)
		{
			WriteUInt64(static_cast<uint64>(Value));
		}

		void WriteDouble(double Value)
		{
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			WriteUInt64(Bits);
		}

		void WriteBool(bool bValue)
		{
			WriteUInt8(bValue ? 1 : 0);
		}

		bool WriteString(const FString& Value, FString& OutError)
		{
			FTCHARToUTF8 Converted(*Value);
			if (Converted.Length() > MaximumStringBytes)
			{
				OutError = TEXT("Recording text exceeds the format limit.");
				return false;
			}
			WriteUInt32(static_cast<uint32>(Converted.Length()));
			if (Converted.Length() > 0)
			{
				Bytes.Append(
					reinterpret_cast<const uint8*>(Converted.Get()),
					Converted.Length()
				);
			}
			return true;
		}

		void WriteBytes(TConstArrayView<uint8> Value)
		{
			Bytes.Append(Value.GetData(), Value.Num());
		}

	private:
		TArray<uint8>& Bytes;
	};

	class FByteReader
	{
	public:
		explicit FByteReader(TConstArrayView<uint8> InBytes)
			: Bytes(InBytes)
		{
		}

		bool ReadUInt8(uint8& OutValue)
		{
			if (!CanRead(1))
			{
				return false;
			}
			OutValue = Bytes[Offset++];
			return true;
		}

		bool ReadUInt16(uint16& OutValue)
		{
			uint64 Value = 0;
			if (!ReadLittleEndian(2, Value))
			{
				return false;
			}
			OutValue = static_cast<uint16>(Value);
			return true;
		}

		bool ReadUInt32(uint32& OutValue)
		{
			uint64 Value = 0;
			if (!ReadLittleEndian(4, Value))
			{
				return false;
			}
			OutValue = static_cast<uint32>(Value);
			return true;
		}

		bool ReadUInt64(uint64& OutValue)
		{
			return ReadLittleEndian(8, OutValue);
		}

		bool ReadInt32(int32& OutValue)
		{
			uint32 Value = 0;
			if (!ReadUInt32(Value))
			{
				return false;
			}
			OutValue = static_cast<int32>(Value);
			return true;
		}

		bool ReadInt64(int64& OutValue)
		{
			uint64 Value = 0;
			if (!ReadUInt64(Value))
			{
				return false;
			}
			OutValue = static_cast<int64>(Value);
			return true;
		}

		bool ReadDouble(double& OutValue)
		{
			uint64 Bits = 0;
			if (!ReadUInt64(Bits))
			{
				return false;
			}
			FMemory::Memcpy(&OutValue, &Bits, sizeof(Bits));
			return true;
		}

		bool ReadBool(bool& bOutValue)
		{
			uint8 Value = 0;
			if (!ReadUInt8(Value) || Value > 1)
			{
				return false;
			}
			bOutValue = Value != 0;
			return true;
		}

		bool ReadString(FString& OutValue)
		{
			uint32 ByteCount = 0;
			if (!ReadUInt32(ByteCount)
				|| ByteCount > MaximumStringBytes
				|| !CanRead(static_cast<int32>(ByteCount)))
			{
				return false;
			}
			if (ByteCount == 0)
			{
				OutValue.Reset();
				return true;
			}
			FUTF8ToTCHAR Converted(
				reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Offset),
				static_cast<int32>(ByteCount)
			);
			OutValue = FString(Converted.Length(), Converted.Get());
			Offset += static_cast<int32>(ByteCount);
			return true;
		}

		bool ReadBytes(int32 Count, TConstArrayView<uint8>& OutValue)
		{
			if (!CanRead(Count))
			{
				return false;
			}
			OutValue = Bytes.Slice(Offset, Count);
			Offset += Count;
			return true;
		}

		bool AtEnd() const
		{
			return Offset == Bytes.Num();
		}

		int32 GetRemaining() const
		{
			return Bytes.Num() - Offset;
		}

	private:
		bool CanRead(int32 Count) const
		{
			return Count >= 0 && Count <= Bytes.Num() - Offset;
		}

		bool ReadLittleEndian(int32 Count, uint64& OutValue)
		{
			if (!CanRead(Count))
			{
				return false;
			}
			OutValue = 0;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				OutValue |= static_cast<uint64>(Bytes[Offset++])
					<< (Index * 8);
			}
			return true;
		}

		TConstArrayView<uint8> Bytes;
		int32 Offset = 0;
	};

	bool IsValidSensorType(uint8 Value)
	{
		return Value > static_cast<uint8>(EOpenMobileSensorType::Unknown)
			&& Value <= static_cast<uint8>(
				EOpenMobileSensorType::PhysicalOrientation);
	}

	bool IsValidFamily(uint8 Value)
	{
		return Value > static_cast<uint8>(EOpenMobileSensorSampleFamily::Unknown)
			&& Value <= static_cast<uint8>(
				EOpenMobileSensorSampleFamily::Proximity);
	}

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	bool WriteSensor(
		FByteWriter& Writer,
		const FOpenMobileSensorIdentifier& Sensor,
		FString& OutError
	)
	{
		if (!Sensor.IsValid())
		{
			OutError = TEXT("Recording contains an invalid sensor identifier.");
			return false;
		}
		Writer.WriteUInt8(static_cast<uint8>(Sensor.Type));
		return Writer.WriteString(Sensor.InstanceId.ToString(), OutError);
	}

	bool ReadSensor(
		FByteReader& Reader,
		FOpenMobileSensorIdentifier& OutSensor
	)
	{
		uint8 Type = 0;
		FString InstanceId;
		if (!Reader.ReadUInt8(Type)
			|| !IsValidSensorType(Type)
			|| !Reader.ReadString(InstanceId))
		{
			return false;
		}
		OutSensor.Type = static_cast<EOpenMobileSensorType>(Type);
		OutSensor.InstanceId = FName(*InstanceId);
		return true;
	}

	bool WriteCapability(
		FByteWriter& Writer,
		const FOpenMobileSensorCapability& Capability,
		FString& OutError
	)
	{
		if (!FMath::IsFinite(Capability.MinimumFrequencyHz)
			|| !FMath::IsFinite(Capability.MaximumFrequencyHz)
			|| !WriteSensor(Writer, Capability.Sensor, OutError)
			|| !Writer.WriteString(
				Capability.Availability.Name.ToString(), OutError))
		{
			return false;
		}
		Writer.WriteUInt8(static_cast<uint8>(Capability.Availability.State));
		if (!Writer.WriteString(Capability.Availability.Detail, OutError))
		{
			return false;
		}
		Writer.WriteUInt8(static_cast<uint8>(Capability.Source));
		if (!Writer.WriteString(
			Capability.RequiredPermission.ToString(), OutError))
		{
			return false;
		}
		Writer.WriteUInt8(static_cast<uint8>(Capability.ActiveRestriction));
		Writer.WriteDouble(Capability.MinimumFrequencyHz);
		Writer.WriteDouble(Capability.MaximumFrequencyHz);
		Writer.WriteBool(Capability.bSupportsNativeBatching);
		Writer.WriteUInt8(static_cast<uint8>(Capability.BackgroundSupport));
		return true;
	}

	bool ReadCapability(
		FByteReader& Reader,
		FOpenMobileSensorCapability& OutCapability
	)
	{
		FString Name;
		FString Permission;
		uint8 State = 0;
		uint8 Source = 0;
		uint8 Restriction = 0;
		uint8 Background = 0;
		if (!ReadSensor(Reader, OutCapability.Sensor)
			|| !Reader.ReadString(Name)
			|| !Reader.ReadUInt8(State)
			|| State > static_cast<uint8>(
				EOpenMobileCapabilityState::TemporarilyUnavailable)
			|| !Reader.ReadString(OutCapability.Availability.Detail)
			|| !Reader.ReadUInt8(Source)
			|| Source > static_cast<uint8>(
				EOpenMobileSensorAvailabilitySource::Replay)
			|| !Reader.ReadString(Permission)
			|| !Reader.ReadUInt8(Restriction)
			|| Restriction > static_cast<uint8>(
				EOpenMobileSensorRestriction::TemporarilyUnavailable)
			|| !Reader.ReadDouble(OutCapability.MinimumFrequencyHz)
			|| !Reader.ReadDouble(OutCapability.MaximumFrequencyHz)
			|| !FMath::IsFinite(OutCapability.MinimumFrequencyHz)
			|| !FMath::IsFinite(OutCapability.MaximumFrequencyHz)
			|| !Reader.ReadBool(OutCapability.bSupportsNativeBatching)
			|| !Reader.ReadUInt8(Background)
			|| Background > static_cast<uint8>(
				EOpenMobileSensorBackgroundSupport::Supported))
		{
			return false;
		}
		OutCapability.Availability.Name = FName(*Name);
		OutCapability.Availability.State =
			static_cast<EOpenMobileCapabilityState>(State);
		OutCapability.Source =
			static_cast<EOpenMobileSensorAvailabilitySource>(Source);
		OutCapability.RequiredPermission = FName(*Permission);
		OutCapability.ActiveRestriction =
			static_cast<EOpenMobileSensorRestriction>(Restriction);
		OutCapability.BackgroundSupport =
			static_cast<EOpenMobileSensorBackgroundSupport>(Background);
		return true;
	}

	bool WriteSampleHeader(
		FByteWriter& Writer,
		const FOpenMobileSensorSampleHeader& Header,
		FString& OutError
	)
	{
		if (!WriteSensor(Writer, Header.Sensor, OutError))
		{
			return false;
		}
		Writer.WriteDouble(Header.TimestampSeconds);
		Writer.WriteDouble(Header.GameThreadReceiptSeconds);
		Writer.WriteBool(Header.bHasGameThreadReceiptTime);
		Writer.WriteInt64(Header.Sequence);
		Writer.WriteInt32(Header.TimestampIssueFlags);
		Writer.WriteBool(Header.bStatefulProcessingReset);
		Writer.WriteBool(Header.bUnitsNormalized);
		Writer.WriteBool(Header.bCoordinatesNormalized);
		Writer.WriteBool(Header.bValid);
		Writer.WriteUInt8(static_cast<uint8>(Header.Accuracy));
		Writer.WriteBool(Header.bCalibrationRequired);
		Writer.WriteUInt8(static_cast<uint8>(Header.CoordinateSpace));
		Writer.WriteUInt8(static_cast<uint8>(Header.ScreenRotation));
		Writer.WriteDouble(Header.ScreenRotationTimestampSeconds);
		Writer.WriteInt64(Header.ScreenRotationSequence);
		Writer.WriteBool(Header.bNaturalOrientationLandscape);
		Writer.WriteInt32(Header.SourceFlags);
		Writer.WriteBool(Header.bSourceChanged);
		Writer.WriteUInt8(static_cast<uint8>(Header.Fusion.Quality));
		Writer.WriteBool(Header.Fusion.bHasNativeQualityReport);
		Writer.WriteUInt8(static_cast<uint8>(Header.Fusion.NativeQuality));
		Writer.WriteInt64(Header.Fusion.ExpectedInputMask);
		Writer.WriteInt64(Header.Fusion.ContributingInputMask);
		Writer.WriteInt64(Header.Fusion.MissingInputMask);
		Writer.WriteInt64(Header.Fusion.DegradedInputMask);
		Writer.WriteBool(Header.bHasEstimatedError);
		Writer.WriteDouble(Header.EstimatedError);
		return true;
	}

	bool ReadSampleHeader(
		FByteReader& Reader,
		FOpenMobileSensorSampleHeader& OutHeader
	)
	{
		uint8 Accuracy = 0;
		uint8 CoordinateSpace = 0;
		uint8 ScreenRotation = 0;
		uint8 FusionQuality = 0;
		uint8 NativeFusionQuality = 0;
		if (!ReadSensor(Reader, OutHeader.Sensor)
			|| !Reader.ReadDouble(OutHeader.TimestampSeconds)
			|| !Reader.ReadDouble(OutHeader.GameThreadReceiptSeconds)
			|| !Reader.ReadBool(OutHeader.bHasGameThreadReceiptTime)
			|| !Reader.ReadInt64(OutHeader.Sequence)
			|| !Reader.ReadInt32(OutHeader.TimestampIssueFlags)
			|| !Reader.ReadBool(OutHeader.bStatefulProcessingReset)
			|| !Reader.ReadBool(OutHeader.bUnitsNormalized)
			|| !Reader.ReadBool(OutHeader.bCoordinatesNormalized)
			|| !Reader.ReadBool(OutHeader.bValid)
			|| !Reader.ReadUInt8(Accuracy)
			|| Accuracy > static_cast<uint8>(EOpenMobileSensorAccuracy::High)
			|| !Reader.ReadBool(OutHeader.bCalibrationRequired)
			|| !Reader.ReadUInt8(CoordinateSpace)
			|| CoordinateSpace > static_cast<uint8>(
				EOpenMobileSensorCoordinateSpace::CurrentScreen)
			|| !Reader.ReadUInt8(ScreenRotation)
			|| ScreenRotation > static_cast<uint8>(
				EOpenMobileSensorScreenRotation::Rotation270)
			|| !Reader.ReadDouble(OutHeader.ScreenRotationTimestampSeconds)
			|| !Reader.ReadInt64(OutHeader.ScreenRotationSequence)
			|| !Reader.ReadBool(OutHeader.bNaturalOrientationLandscape)
			|| !Reader.ReadInt32(OutHeader.SourceFlags)
			|| !Reader.ReadBool(OutHeader.bSourceChanged)
			|| !Reader.ReadUInt8(FusionQuality)
			|| FusionQuality > static_cast<uint8>(
				EOpenMobileSensorFusionQuality::Nominal)
			|| !Reader.ReadBool(OutHeader.Fusion.bHasNativeQualityReport)
			|| !Reader.ReadUInt8(NativeFusionQuality)
			|| NativeFusionQuality > static_cast<uint8>(
				EOpenMobileSensorFusionQuality::Nominal)
			|| !Reader.ReadInt64(OutHeader.Fusion.ExpectedInputMask)
			|| !Reader.ReadInt64(OutHeader.Fusion.ContributingInputMask)
			|| !Reader.ReadInt64(OutHeader.Fusion.MissingInputMask)
			|| !Reader.ReadInt64(OutHeader.Fusion.DegradedInputMask)
			|| !Reader.ReadBool(OutHeader.bHasEstimatedError)
			|| !Reader.ReadDouble(OutHeader.EstimatedError))
		{
			return false;
		}
		OutHeader.Accuracy = static_cast<EOpenMobileSensorAccuracy>(Accuracy);
		OutHeader.CoordinateSpace =
			static_cast<EOpenMobileSensorCoordinateSpace>(CoordinateSpace);
		OutHeader.ScreenRotation =
			static_cast<EOpenMobileSensorScreenRotation>(ScreenRotation);
		OutHeader.Fusion.Quality =
			static_cast<EOpenMobileSensorFusionQuality>(FusionQuality);
		OutHeader.Fusion.NativeQuality =
			static_cast<EOpenMobileSensorFusionQuality>(NativeFusionQuality);
		return FMath::IsFinite(OutHeader.TimestampSeconds)
			&& OutHeader.TimestampSeconds >= 0.0
			&& FMath::IsFinite(OutHeader.GameThreadReceiptSeconds)
			&& FMath::IsFinite(OutHeader.ScreenRotationTimestampSeconds)
			&& FMath::IsFinite(OutHeader.EstimatedError);
	}

	bool WriteVectorSample(
		FByteWriter& Writer,
		const FOpenMobileVectorSensorSample& Sample,
		FString& OutError
	)
	{
		if (!WriteSampleHeader(Writer, Sample.Header, OutError))
		{
			return false;
		}
		Writer.WriteDouble(Sample.Value.X);
		Writer.WriteDouble(Sample.Value.Y);
		Writer.WriteDouble(Sample.Value.Z);
		Writer.WriteBool(Sample.bHasBias);
		Writer.WriteDouble(Sample.Bias.X);
		Writer.WriteDouble(Sample.Bias.Y);
		Writer.WriteDouble(Sample.Bias.Z);
		return true;
	}

	bool ReadVectorSample(
		FByteReader& Reader,
		FOpenMobileVectorSensorSample& OutSample
	)
	{
		return ReadSampleHeader(Reader, OutSample.Header)
			&& Reader.ReadDouble(OutSample.Value.X)
			&& Reader.ReadDouble(OutSample.Value.Y)
			&& Reader.ReadDouble(OutSample.Value.Z)
			&& Reader.ReadBool(OutSample.bHasBias)
			&& Reader.ReadDouble(OutSample.Bias.X)
			&& Reader.ReadDouble(OutSample.Bias.Y)
			&& Reader.ReadDouble(OutSample.Bias.Z)
			&& IsFiniteVector(OutSample.Value)
			&& IsFiniteVector(OutSample.Bias);
	}

	bool WriteBlock(
		EBlockType Type,
		TConstArrayView<uint8> Payload,
		TArray<uint8>& OutBytes,
		FString& OutError
	)
	{
		if (Payload.Num() < 0)
		{
			OutError = TEXT("Recording block exceeds the format limit.");
			return false;
		}
		FByteWriter Writer(OutBytes);
		Writer.WriteBytes(MakeArrayView(BlockMagic));
		Writer.WriteUInt8(static_cast<uint8>(Type));
		Writer.WriteUInt8(0);
		Writer.WriteUInt16(0);
		Writer.WriteUInt32(static_cast<uint32>(Payload.Num()));
		Writer.WriteUInt32(FCrc::MemCrc32(Payload.GetData(), Payload.Num()));
		Writer.WriteBytes(Payload);
		return true;
	}

	bool ReadBlock(
		FByteReader& Reader,
		EBlockType& OutType,
		TConstArrayView<uint8>& OutPayload,
		EOpenMobileSensorRecordingDecodeStatus& OutStatus,
		FString& OutError
	)
	{
		TConstArrayView<uint8> Magic;
		uint8 Type = 0;
		uint8 Flags = 0;
		uint16 Reserved = 0;
		uint32 PayloadSize = 0;
		uint32 Checksum = 0;
		if (!Reader.ReadBytes(UE_ARRAY_COUNT(BlockMagic), Magic)
			|| !Reader.ReadUInt8(Type)
			|| !Reader.ReadUInt8(Flags)
			|| !Reader.ReadUInt16(Reserved)
			|| !Reader.ReadUInt32(PayloadSize)
			|| !Reader.ReadUInt32(Checksum))
		{
			OutStatus = EOpenMobileSensorRecordingDecodeStatus::Truncated;
			OutError = TEXT("Recording block header is truncated.");
			return false;
		}
		if (FMemory::Memcmp(
			Magic.GetData(), BlockMagic, UE_ARRAY_COUNT(BlockMagic)) != 0)
		{
			OutStatus = EOpenMobileSensorRecordingDecodeStatus::InvalidData;
			OutError = TEXT("Recording block marker is invalid.");
			return false;
		}
		if (Flags != 0 || Reserved != 0)
		{
			OutStatus = EOpenMobileSensorRecordingDecodeStatus::InvalidData;
			OutError = TEXT("Recording block flags are invalid.");
			return false;
		}
		if (PayloadSize > static_cast<uint32>(Reader.GetRemaining()))
		{
			OutStatus = EOpenMobileSensorRecordingDecodeStatus::Truncated;
			OutError = TEXT("Recording block payload is truncated.");
			return false;
		}
		if (!Reader.ReadBytes(static_cast<int32>(PayloadSize), OutPayload))
		{
			OutStatus = EOpenMobileSensorRecordingDecodeStatus::Truncated;
			OutError = TEXT("Recording block payload is truncated.");
			return false;
		}
		if (FCrc::MemCrc32(OutPayload.GetData(), OutPayload.Num()) != Checksum)
		{
			OutStatus = EOpenMobileSensorRecordingDecodeStatus::ChecksumMismatch;
			OutError = TEXT("Recording block checksum does not match.");
			return false;
		}
		OutType = static_cast<EBlockType>(Type);
		return true;
	}

	bool DecodeHeaderPayload(
		TConstArrayView<uint8> Payload,
		FOpenMobileSensorRecordingHeader& OutHeader
	)
	{
		FByteReader Reader(Payload);
		uint32 StreamCount = 0;
		if (!Reader.ReadString(OutHeader.PluginVersion)
			|| !Reader.ReadString(OutHeader.PlatformName)
			|| !Reader.ReadString(OutHeader.UnitsConvention)
			|| !Reader.ReadString(OutHeader.CoordinateConvention)
			|| !Reader.ReadUInt32(StreamCount)
			|| StreamCount > MaximumStreams)
		{
			return false;
		}
		OutHeader.Streams.Reserve(static_cast<int32>(StreamCount));
		for (uint32 Index = 0; Index < StreamCount; ++Index)
		{
			FOpenMobileSensorRecordingStreamDescriptor& Stream =
				OutHeader.Streams.AddDefaulted_GetRef();
			uint8 Family = 0;
			if (!ReadSensor(Reader, Stream.Sensor)
				|| !Reader.ReadUInt8(Family)
				|| !IsValidFamily(Family)
				|| !Reader.ReadString(Stream.Units)
				|| !ReadCapability(Reader, Stream.Capability)
				|| Stream.Capability.Sensor != Stream.Sensor)
			{
				return false;
			}
			Stream.Family =
				static_cast<EOpenMobileSensorSampleFamily>(Family);
		}
		return Reader.AtEnd();
	}

	bool DecodeVectorPayload(
		TConstArrayView<uint8> Payload,
		FOpenMobileVectorSensorBatch& OutBatch
	)
	{
		FByteReader Reader(Payload);
		uint32 SampleCount = 0;
		if (!Reader.ReadUInt32(SampleCount)
			|| SampleCount == 0
			|| SampleCount > MaximumSamplesPerBatch)
		{
			return false;
		}
		OutBatch.Samples.Reserve(static_cast<int32>(SampleCount));
		for (uint32 Index = 0; Index < SampleCount; ++Index)
		{
			if (!ReadVectorSample(
				Reader, OutBatch.Samples.AddDefaulted_GetRef()))
			{
				return false;
			}
		}
		return Reader.AtEnd();
	}

	bool DecodeFooterPayload(
		TConstArrayView<uint8> Payload,
		FOpenMobileSensorRecordingFooter& OutFooter
	)
	{
		FByteReader Reader(Payload);
		uint32 Marker = 0;
		return Reader.ReadInt64(OutFooter.BatchCount)
			&& Reader.ReadInt64(OutFooter.SampleCount)
			&& Reader.ReadInt64(OutFooter.DroppedSamples)
			&& Reader.ReadDouble(OutFooter.DurationSeconds)
			&& Reader.ReadUInt32(Marker)
			&& Marker == FooterMarker
			&& OutFooter.BatchCount >= 0
			&& OutFooter.SampleCount >= 0
			&& OutFooter.DroppedSamples >= 0
			&& FMath::IsFinite(OutFooter.DurationSeconds)
			&& OutFooter.DurationSeconds >= 0.0
			&& Reader.AtEnd();
	}
}

bool FOpenMobileSensorRecordingCodec::EncodeHeader(
	const FOpenMobileSensorRecordingHeader& Header,
	TArray<uint8>& OutBytes,
	FString& OutError
)
{
	using namespace OpenMobileSensorRecordingCodecPrivate;
	OutBytes.Reset();
	OutError.Reset();
	if (Header.FormatVersion != CurrentFormatVersion
		|| Header.Streams.IsEmpty()
		|| Header.Streams.Num() > MaximumStreams)
	{
		OutError = TEXT("Recording header is invalid.");
		return false;
	}
	TArray<uint8> Payload;
	FByteWriter PayloadWriter(Payload);
	if (!PayloadWriter.WriteString(Header.PluginVersion, OutError)
		|| !PayloadWriter.WriteString(Header.PlatformName, OutError)
		|| !PayloadWriter.WriteString(Header.UnitsConvention, OutError)
		|| !PayloadWriter.WriteString(Header.CoordinateConvention, OutError))
	{
		return false;
	}
	PayloadWriter.WriteUInt32(static_cast<uint32>(Header.Streams.Num()));
	for (const FOpenMobileSensorRecordingStreamDescriptor& Stream
		: Header.Streams)
	{
		if (!WriteSensor(PayloadWriter, Stream.Sensor, OutError)
			|| !IsValidFamily(static_cast<uint8>(Stream.Family)))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Recording stream descriptor is invalid.");
			}
			return false;
		}
		PayloadWriter.WriteUInt8(static_cast<uint8>(Stream.Family));
		if (!PayloadWriter.WriteString(Stream.Units, OutError)
			|| Stream.Capability.Sensor != Stream.Sensor
			|| !WriteCapability(PayloadWriter, Stream.Capability, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Recording stream descriptor is invalid.");
			}
			return false;
		}
	}
	FByteWriter Writer(OutBytes);
	Writer.WriteBytes(MakeArrayView(FileMagic));
	Writer.WriteUInt16(CurrentFormatVersion);
	Writer.WriteUInt16(0);
	return WriteBlock(EBlockType::Header, Payload, OutBytes, OutError);
}

bool FOpenMobileSensorRecordingCodec::EncodeVectorBatch(
	const FOpenMobileVectorSensorBatch& Batch,
	TArray<uint8>& OutBytes,
	FString& OutError
)
{
	using namespace OpenMobileSensorRecordingCodecPrivate;
	OutBytes.Reset();
	OutError.Reset();
	if (Batch.Samples.IsEmpty()
		|| Batch.Samples.Num() > MaximumSamplesPerBatch)
	{
		OutError = TEXT("Recording vector batch is invalid.");
		return false;
	}
	TArray<uint8> Payload;
	FByteWriter Writer(Payload);
	Writer.WriteUInt32(static_cast<uint32>(Batch.Samples.Num()));
	for (const FOpenMobileVectorSensorSample& Sample : Batch.Samples)
	{
		if (!IsFiniteVector(Sample.Value)
			|| !IsFiniteVector(Sample.Bias)
			|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
			|| Sample.Header.TimestampSeconds < 0.0
			|| !WriteVectorSample(Writer, Sample, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Recording vector sample is invalid.");
			}
			return false;
		}
	}
	return WriteBlock(EBlockType::VectorBatch, Payload, OutBytes, OutError);
}

bool FOpenMobileSensorRecordingCodec::EncodeFooter(
	const FOpenMobileSensorRecordingFooter& Footer,
	TArray<uint8>& OutBytes,
	FString& OutError
)
{
	using namespace OpenMobileSensorRecordingCodecPrivate;
	OutBytes.Reset();
	OutError.Reset();
	if (Footer.BatchCount < 0
		|| Footer.SampleCount < 0
		|| Footer.DroppedSamples < 0
		|| !FMath::IsFinite(Footer.DurationSeconds)
		|| Footer.DurationSeconds < 0.0)
	{
		OutError = TEXT("Recording footer is invalid.");
		return false;
	}
	TArray<uint8> Payload;
	FByteWriter Writer(Payload);
	Writer.WriteInt64(Footer.BatchCount);
	Writer.WriteInt64(Footer.SampleCount);
	Writer.WriteInt64(Footer.DroppedSamples);
	Writer.WriteDouble(Footer.DurationSeconds);
	Writer.WriteUInt32(FooterMarker);
	return WriteBlock(EBlockType::Footer, Payload, OutBytes, OutError);
}

bool FOpenMobileSensorRecordingCodec::EncodeComplete(
	const FOpenMobileSensorRecordingDocument& Document,
	TArray<uint8>& OutBytes,
	FString& OutError
)
{
	OutBytes.Reset();
	TArray<uint8> Block;
	if (!EncodeHeader(Document.Header, OutBytes, OutError))
	{
		return false;
	}
	FOpenMobileSensorRecordingFooter Footer = Document.Footer;
	Footer.BatchCount = Document.VectorBatches.Num();
	Footer.SampleCount = 0;
	for (const FOpenMobileVectorSensorBatch& Batch : Document.VectorBatches)
	{
		if (!EncodeVectorBatch(Batch, Block, OutError))
		{
			OutBytes.Reset();
			return false;
		}
		Footer.SampleCount += Batch.Samples.Num();
		OutBytes.Append(Block);
	}
	if (!EncodeFooter(Footer, Block, OutError))
	{
		OutBytes.Reset();
		return false;
	}
	OutBytes.Append(Block);
	return true;
}

bool FOpenMobileSensorRecordingCodec::DecodeComplete(
	TConstArrayView<uint8> Bytes,
	FOpenMobileSensorRecordingDocument& OutDocument,
	EOpenMobileSensorRecordingDecodeStatus& OutStatus,
	FString& OutError
)
{
	using namespace OpenMobileSensorRecordingCodecPrivate;
	OutDocument = {};
	OutStatus = EOpenMobileSensorRecordingDecodeStatus::InvalidData;
	OutError.Reset();
	if (Bytes.Num() > MaximumDecodedBytes)
	{
		OutStatus = EOpenMobileSensorRecordingDecodeStatus::LimitExceeded;
		OutError = TEXT("Recording exceeds the decode size limit.");
		return false;
	}
	FByteReader Reader(Bytes);
	TConstArrayView<uint8> Magic;
	uint16 Version = 0;
	uint16 Reserved = 0;
	if (!Reader.ReadBytes(UE_ARRAY_COUNT(FileMagic), Magic)
		|| !Reader.ReadUInt16(Version)
		|| !Reader.ReadUInt16(Reserved))
	{
		OutStatus = EOpenMobileSensorRecordingDecodeStatus::Truncated;
		OutError = TEXT("Recording prefix is truncated.");
		return false;
	}
	if (FMemory::Memcmp(
		Magic.GetData(), FileMagic, UE_ARRAY_COUNT(FileMagic)) != 0)
	{
		OutStatus = EOpenMobileSensorRecordingDecodeStatus::InvalidMagic;
		OutError = TEXT("Recording signature is invalid.");
		return false;
	}
	if (Version != CurrentFormatVersion)
	{
		OutStatus = EOpenMobileSensorRecordingDecodeStatus::IncompatibleVersion;
		OutError = TEXT("Recording format version is not supported.");
		return false;
	}
	if (Reserved != 0)
	{
		OutStatus = EOpenMobileSensorRecordingDecodeStatus::InvalidData;
		OutError = TEXT("Recording prefix flags are invalid.");
		return false;
	}
	OutDocument.Header.FormatVersion = Version;
	EBlockType BlockType;
	TConstArrayView<uint8> Payload;
	if (!ReadBlock(Reader, BlockType, Payload, OutStatus, OutError))
	{
		return false;
	}
	if (BlockType != EBlockType::Header
		|| !DecodeHeaderPayload(Payload, OutDocument.Header))
	{
		OutStatus = EOpenMobileSensorRecordingDecodeStatus::InvalidData;
		OutError = TEXT("Recording header is invalid.");
		return false;
	}
	int64 BatchCount = 0;
	int64 SampleCount = 0;
	bool bFoundFooter = false;
	while (!Reader.AtEnd())
	{
		if (!ReadBlock(Reader, BlockType, Payload, OutStatus, OutError))
		{
			return false;
		}
		if (BlockType == EBlockType::VectorBatch)
		{
			if (OutDocument.VectorBatches.Num() >= MaximumBatches)
			{
				OutStatus =
					EOpenMobileSensorRecordingDecodeStatus::LimitExceeded;
				OutError = TEXT("Recording contains too many batches.");
				return false;
			}
			FOpenMobileVectorSensorBatch& Batch =
				OutDocument.VectorBatches.AddDefaulted_GetRef();
			if (!DecodeVectorPayload(Payload, Batch))
			{
				OutStatus = EOpenMobileSensorRecordingDecodeStatus::InvalidData;
				OutError = TEXT("Recording vector batch is invalid.");
				return false;
			}
			++BatchCount;
			SampleCount += Batch.Samples.Num();
			continue;
		}
		if (BlockType != EBlockType::Footer
			|| !DecodeFooterPayload(Payload, OutDocument.Footer)
			|| !Reader.AtEnd())
		{
			OutStatus = EOpenMobileSensorRecordingDecodeStatus::InvalidData;
			OutError = TEXT("Recording contains an invalid block sequence.");
			return false;
		}
		bFoundFooter = true;
		break;
	}
	if (!bFoundFooter)
	{
		OutStatus = EOpenMobileSensorRecordingDecodeStatus::Truncated;
		OutError = TEXT("Recording footer is missing.");
		return false;
	}
	if (OutDocument.Footer.BatchCount != BatchCount
		|| OutDocument.Footer.SampleCount != SampleCount)
	{
		OutStatus = EOpenMobileSensorRecordingDecodeStatus::InvalidData;
		OutError = TEXT("Recording footer counts do not match its data.");
		return false;
	}
	OutStatus = EOpenMobileSensorRecordingDecodeStatus::Success;
	return true;
}
