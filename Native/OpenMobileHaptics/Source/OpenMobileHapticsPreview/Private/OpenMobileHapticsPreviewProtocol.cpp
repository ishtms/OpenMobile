#include "OpenMobileHapticsPreviewProtocol.h"

#include "Misc/Crc.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace OpenMobileHapticsPreviewProtocolPrivate
{
	constexpr uint32 Magic = 0x4f4d4850;
	constexpr int32 HeaderBytes = 12;
	constexpr int32 MaximumLabelCharacters = 32;
	constexpr int32 MaximumNameCharacters = 32;
	constexpr int32 MaximumErrorCharacters = 96;

	template<typename ValueType>
	/** Folds protocol-visible pattern values into a capability-independent source hash. */
	void HashValue(uint32& Hash, const ValueType& Value)
	{
		Hash = FCrc::TypeCrc32(Value, Hash);
	}

	/** Reads or writes a length-prefixed string while enforcing its field-specific character cap before allocation. */
	bool SerializeString(
		FArchive& Archive,
		FString& Value,
		int32 MaximumCharacters,
		FString& OutError
	)
	{
		uint16 ByteCount = 0;
		FTCHARToUTF8 Encoded(Archive.IsSaving() ? *Value : TEXT(""));
		if (Archive.IsSaving())
		{
			if (Value.Len() > MaximumCharacters
				|| Encoded.Length() > MaximumCharacters * 4)
			{
				OutError = TEXT("A preview text field exceeds its limit.");
				return false;
			}
			ByteCount = static_cast<uint16>(Encoded.Length());
		}
		Archive << ByteCount;
		if (Archive.IsError()
			|| ByteCount > MaximumCharacters * 4
			|| (Archive.IsLoading()
				&& Archive.Tell() + ByteCount > Archive.TotalSize()))
		{
			OutError = TEXT("A preview text field is malformed.");
			return false;
		}
		if (Archive.IsSaving())
		{
			if (ByteCount > 0)
			{
				Archive.Serialize(
					const_cast<ANSICHAR*>(Encoded.Get()),
					ByteCount
				);
			}
			return !Archive.IsError();
		}

		TArray<ANSICHAR> Bytes;
		Bytes.SetNumUninitialized(ByteCount + 1);
		if (ByteCount > 0)
		{
			Archive.Serialize(Bytes.GetData(), ByteCount);
		}
		Bytes[ByteCount] = 0;
		Value = UTF8_TO_TCHAR(Bytes.GetData());
		if (Value.Len() > MaximumCharacters)
		{
			OutError = TEXT("A preview text field exceeds its limit.");
			return false;
		}
		return !Archive.IsError();
	}

	/** Accepts bounded printable identifiers only, network packets shouldn't carry paths or control text into names. */
	bool IsSafeIdentifier(const FString& Value, bool bAllowEmpty)
	{
		if (Value.IsEmpty())
		{
			return bAllowEmpty;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_')
				&& Character != TEXT('-') && Character != TEXT('.')
				&& Character != TEXT(' '))
			{
				return false;
			}
		}
		return true;
	}

	/** Combines bounded string serialization with identifier validation on both encode and decode. */
	bool SerializeIdentifier(
		FArchive& Archive,
		FString& Value,
		bool bAllowEmpty,
		FString& OutError
	)
	{
		if (!SerializeString(
			Archive,
			Value,
			MaximumNameCharacters,
			OutError
		))
		{
			return false;
		}
		if (!IsSafeIdentifier(Value, bAllowEmpty))
		{
			OutError = TEXT("A preview identifier is invalid.");
			return false;
		}
		return true;
	}

	/** Serializes the preview capability subset and validates every enum and limit before accepting a packet. */
	bool SerializeCapabilities(
		FArchive& Archive,
		FOpenMobileHapticsPreviewCapabilities& Capabilities,
		FString& OutError
	)
	{
		uint8 Availability = static_cast<uint8>(Capabilities.Availability);
		uint8 Basic = static_cast<uint8>(Capabilities.BasicVibration);
		uint8 Rich = static_cast<uint8>(Capabilities.RichHaptics);
		uint8 Primitives = static_cast<uint8>(Capabilities.Primitives);
		uint8 Waveform = static_cast<uint8>(Capabilities.WaveformTiming);
		uint8 Frequency = static_cast<uint8>(Capabilities.FrequencyControl);
		uint8 Dynamic = static_cast<uint8>(Capabilities.DynamicParameters);
		uint8 AHAP = static_cast<uint8>(Capabilities.AHAP);
		Archive << Availability;
		Archive << Basic;
		Archive << Rich;
		Archive << Primitives;
		Archive << Waveform;
		Archive << Frequency;
		Archive << Dynamic;
		Archive << AHAP;
		Archive << Capabilities.MaximumEventCount;
		Archive << Capabilities.MaximumControlPointCount;
		Archive << Capabilities.MaximumDurationSeconds;
		Archive << Capabilities.Signature;
		if (Archive.IsError()
			|| Availability > static_cast<uint8>(
				EOpenMobileHapticAvailability::TemporarilyUnavailable)
			|| Basic > static_cast<uint8>(EOpenMobileHapticSupportState::Unsupported)
			|| Rich > static_cast<uint8>(EOpenMobileHapticSupportState::Unsupported)
			|| Primitives > static_cast<uint8>(EOpenMobileHapticSupportState::Unsupported)
			|| Waveform > static_cast<uint8>(EOpenMobileHapticSupportState::Unsupported)
			|| Frequency > static_cast<uint8>(EOpenMobileHapticSupportState::Unsupported)
			|| Dynamic > static_cast<uint8>(EOpenMobileHapticSupportState::Unsupported)
			|| AHAP > static_cast<uint8>(EOpenMobileHapticSupportState::Unsupported)
			|| Capabilities.MaximumEventCount < 0
			|| Capabilities.MaximumControlPointCount < 0
			|| !FMath::IsFinite(Capabilities.MaximumDurationSeconds)
			|| Capabilities.MaximumDurationSeconds < 0.0)
		{
			OutError = TEXT("Preview capability data is invalid.");
			return false;
		}
		if (Archive.IsLoading())
		{
			Capabilities.Availability =
				static_cast<EOpenMobileHapticAvailability>(Availability);
			Capabilities.BasicVibration =
				static_cast<EOpenMobileHapticSupportState>(Basic);
			Capabilities.RichHaptics =
				static_cast<EOpenMobileHapticSupportState>(Rich);
			Capabilities.Primitives =
				static_cast<EOpenMobileHapticSupportState>(Primitives);
			Capabilities.WaveformTiming =
				static_cast<EOpenMobileHapticSupportState>(Waveform);
			Capabilities.FrequencyControl =
				static_cast<EOpenMobileHapticSupportState>(Frequency);
			Capabilities.DynamicParameters =
				static_cast<EOpenMobileHapticSupportState>(Dynamic);
			Capabilities.AHAP =
				static_cast<EOpenMobileHapticSupportState>(AHAP);
		}
		return true;
	}

	/** Serializes cooked events, curves, and points with hard count caps before arrays are resized. */
	bool SerializeCookedPattern(
		FArchive& Archive,
		FOpenMobileHapticCookedPatternData& Pattern,
		FString& OutError
	)
	{
		Archive << Pattern.DataFormatVersion;
		Archive << Pattern.SourceHash;
		Archive << Pattern.DurationMicroseconds;
		Archive << Pattern.GranularityMicroseconds;
		int32 EventCount = Pattern.Events.Num();
		Archive << EventCount;
		if (Archive.IsError() || EventCount < 0
			|| EventCount > FOpenMobileHapticsPreviewProtocol::MaximumEvents)
		{
			OutError = TEXT("Preview event count is invalid.");
			return false;
		}
		if (Archive.IsLoading())
		{
			Pattern.Events.SetNum(EventCount);
		}
		for (FOpenMobileHapticCookedPatternEvent& Event : Pattern.Events)
		{
			uint8 Type = static_cast<uint8>(Event.Type);
			Archive << Type;
			Archive << Event.StartTimeMicroseconds;
			Archive << Event.DurationMicroseconds;
			Archive << Event.Intensity;
			Archive << Event.Sharpness;
			Archive << Event.FrequencyIntent;
			if (Type > static_cast<uint8>(
				EOpenMobileHapticPatternEventType::Silence))
			{
				OutError = TEXT("Preview event type is invalid.");
				return false;
			}
			if (Archive.IsLoading())
			{
				Event.Type = static_cast<EOpenMobileHapticPatternEventType>(Type);
			}
		}

		int32 CurveCount = Pattern.ParameterCurves.Num();
		Archive << CurveCount;
		if (Archive.IsError() || CurveCount < 0
			|| CurveCount > FOpenMobileHapticsPreviewProtocol::MaximumCurves)
		{
			OutError = TEXT("Preview curve count is invalid.");
			return false;
		}
		if (Archive.IsLoading())
		{
			Pattern.ParameterCurves.SetNum(CurveCount);
		}
		int32 TotalPointCount = 0;
		for (FOpenMobileHapticCookedParameterCurve& Curve :
			Pattern.ParameterCurves)
		{
			uint8 Parameter = static_cast<uint8>(Curve.Parameter);
			Archive << Parameter;
			Archive << Curve.StartTimeMicroseconds;
			int32 PointCount = Curve.ControlPoints.Num();
			Archive << PointCount;
			if (Archive.IsError()
				|| Parameter > static_cast<uint8>(
					EOpenMobileHapticCurveParameter::SharpnessControl)
				|| PointCount < 0
				|| PointCount
					> FOpenMobileHapticsPreviewProtocol::MaximumControlPoints
						- TotalPointCount)
			{
				OutError = TEXT("Preview control-point data is invalid.");
				return false;
			}
			TotalPointCount += PointCount;
			if (Archive.IsLoading())
			{
				Curve.Parameter =
					static_cast<EOpenMobileHapticCurveParameter>(Parameter);
				Curve.ControlPoints.SetNum(PointCount);
			}
			for (FOpenMobileHapticCookedCurvePoint& Point : Curve.ControlPoints)
			{
				Archive << Point.RelativeTimeMicroseconds;
				Archive << Point.Value;
			}
		}
		return !Archive.IsError();
	}

	/** Serializes portable pattern and playback options without allowing device preview to reference project assets. */
	bool SerializePreviewPattern(
		FArchive& Archive,
		FOpenMobileHapticsPreviewPattern& Pattern,
		FString& OutError
	)
	{
		if (!SerializeCookedPattern(Archive, Pattern.CookedPattern, OutError))
		{
			return false;
		}
		uint8 Loop = Pattern.Loop.bLoop ? 1 : 0;
		Archive << Loop;
		Archive << Pattern.Loop.RepeatCount;
		Archive << Pattern.Loop.RepeatStartTimeSeconds;
		Archive << Pattern.Loop.MaximumDurationSeconds;
		if (!SerializeIdentifier(
			Archive,
			Pattern.Category,
			false,
			OutError
		))
		{
			return false;
		}
		uint8 FallbackPolicy = static_cast<uint8>(Pattern.FallbackPolicy);
		uint8 FallbackFloor = static_cast<uint8>(Pattern.LowestAllowedFallback);
		uint8 AllowSemantic = Pattern.bAllowSemanticFallback ? 1 : 0;
		uint8 Semantic = static_cast<uint8>(Pattern.SemanticFallback);
		Archive << FallbackPolicy;
		Archive << FallbackFloor;
		if (!SerializeIdentifier(
			Archive,
			Pattern.PrimitiveOrPresetFallback,
			true,
			OutError
		))
		{
			return false;
		}
		Archive << AllowSemantic;
		Archive << Semantic;
		Archive << Pattern.CapabilitySignature;
		if (Archive.IsError() || Loop > 1 || AllowSemantic > 1
			|| FallbackPolicy > static_cast<uint8>(
				EOpenMobileHapticFallbackPolicy::NoEffectAllowed)
			|| FallbackFloor > static_cast<uint8>(
				EOpenMobileHapticFallbackFloor::BasicVibration)
			|| Semantic > static_cast<uint8>(
				EOpenMobileHapticSemanticEffect::Achievement))
		{
			OutError = TEXT("Preview fallback data is invalid.");
			return false;
		}
		if (Archive.IsLoading())
		{
			Pattern.Loop.bLoop = Loop != 0;
			Pattern.FallbackPolicy =
				static_cast<EOpenMobileHapticFallbackPolicy>(FallbackPolicy);
			Pattern.LowestAllowedFallback =
				static_cast<EOpenMobileHapticFallbackFloor>(FallbackFloor);
			Pattern.bAllowSemanticFallback = AllowSemantic != 0;
			Pattern.SemanticFallback =
				static_cast<EOpenMobileHapticSemanticEffect>(Semantic);
		}
		return true;
	}

	/** Dispatches each message type to its exact bounded payload contract and rejects fields that don't belong. */
	bool SerializeMessageBody(
		FArchive& Archive,
		FOpenMobileHapticsPreviewMessage& Message,
		FString& OutError
	)
	{
		Archive << Message.SenderId;
		if (!Message.SenderId.IsValid())
		{
			OutError = TEXT("Preview sender identity is missing.");
			return false;
		}
		switch (Message.Type)
		{
		case EOpenMobileHapticsPreviewMessageType::Discover:
			return true;
		case EOpenMobileHapticsPreviewMessageType::Announce:
			if (!SerializeString(
					Archive,
					Message.Label,
					MaximumLabelCharacters,
					OutError
				) || Message.Label.IsEmpty())
			{
				OutError = TEXT("Preview receiver label is invalid.");
				return false;
			}
			return SerializeCapabilities(Archive, Message.Capabilities, OutError);
		case EOpenMobileHapticsPreviewMessageType::PairRequest:
			Archive << Message.RequestId;
			if (!Message.RequestId.IsValid() || !SerializeString(
				Archive,
				Message.Label,
				MaximumLabelCharacters,
				OutError
			) || Message.Label.IsEmpty())
			{
				OutError = TEXT("Preview pairing request is invalid.");
				return false;
			}
			return true;
		case EOpenMobileHapticsPreviewMessageType::PairPending:
			Archive << Message.RequestId;
			if (!Message.RequestId.IsValid() || !SerializeString(
				Archive,
				Message.PairingCode,
				6,
				OutError
			) || Message.PairingCode.Len() != 6)
			{
				OutError = TEXT("Preview pairing response is invalid.");
				return false;
			}
			for (const TCHAR Character : Message.PairingCode)
			{
				if (!FChar::IsDigit(Character))
				{
					OutError = TEXT("Preview pairing code is invalid.");
					return false;
				}
			}
			return true;
		case EOpenMobileHapticsPreviewMessageType::PairAccepted:
			Archive << Message.RequestId;
			Archive << Message.SessionId;
			if (!Message.RequestId.IsValid() || !Message.SessionId.IsValid())
			{
				OutError = TEXT("Preview pairing acceptance is invalid.");
				return false;
			}
			return true;
		case EOpenMobileHapticsPreviewMessageType::PairRejected:
			Archive << Message.RequestId;
			if (!Message.RequestId.IsValid())
			{
				OutError = TEXT("Preview pairing response is invalid.");
				return false;
			}
			break;
		case EOpenMobileHapticsPreviewMessageType::Preview:
			Archive << Message.SessionId;
			Archive << Message.Revision;
			if (!Message.SessionId.IsValid() || Message.Revision == 0)
			{
				OutError = TEXT("Preview session metadata is invalid.");
				return false;
			}
			return SerializePreviewPattern(Archive, Message.Pattern, OutError);
		case EOpenMobileHapticsPreviewMessageType::Result:
			Archive << Message.SessionId;
			Archive << Message.Revision;
			if (!Message.SessionId.IsValid() || Message.Revision == 0)
			{
				OutError = TEXT("Preview result metadata is invalid.");
				return false;
			}
			break;
		case EOpenMobileHapticsPreviewMessageType::Stop:
		case EOpenMobileHapticsPreviewMessageType::Heartbeat:
		case EOpenMobileHapticsPreviewMessageType::Goodbye:
			Archive << Message.SessionId;
			if (!Message.SessionId.IsValid())
			{
				OutError = TEXT("Preview session identity is invalid.");
				return false;
			}
			return true;
		default:
			OutError = TEXT("Preview message type is invalid.");
			return false;
		}

		uint8 ResultCode = static_cast<uint8>(Message.ResultCode);
		Archive << ResultCode;
		if (ResultCode > static_cast<uint8>(
			EOpenMobileHapticsPreviewResultCode::Disabled))
		{
			OutError = TEXT("Preview result code is invalid.");
			return false;
		}
		if (Archive.IsLoading())
		{
			Message.ResultCode =
				static_cast<EOpenMobileHapticsPreviewResultCode>(ResultCode);
		}
		if (Message.Type == EOpenMobileHapticsPreviewMessageType::Result)
		{
			if (!SerializeCapabilities(Archive, Message.Capabilities, OutError)
				|| !SerializeString(
					Archive,
					Message.ResolvedPath,
					MaximumNameCharacters,
					OutError
				))
			{
				return false;
			}
			Archive << Message.ResolvedStartTimeSeconds;
			Archive << Message.EstimatedPrecisionSeconds;
			if (!FMath::IsFinite(Message.ResolvedStartTimeSeconds)
				|| !FMath::IsFinite(Message.EstimatedPrecisionSeconds)
				|| Message.EstimatedPrecisionSeconds < 0.0)
			{
				OutError = TEXT("Preview timing data is invalid.");
				return false;
			}
		}
		return SerializeString(
			Archive,
			Message.Error,
			MaximumErrorCharacters,
			OutError
		);
	}
}

FOpenMobileHapticsPreviewCapabilities
FOpenMobileHapticsPreviewCapabilities::FromRuntime(
	const FOpenMobileHapticCapabilities& Capabilities
)
{
	using namespace OpenMobileHapticsPreviewProtocolPrivate;
	FOpenMobileHapticsPreviewCapabilities Result;
	Result.Availability = Capabilities.Availability;
	Result.BasicVibration = Capabilities.BasicVibration;
	Result.RichHaptics = Capabilities.RichHaptics;
	Result.Primitives = Capabilities.Primitives;
	Result.WaveformTiming = Capabilities.WaveformTiming;
	Result.FrequencyControl = Capabilities.FrequencyControl;
	Result.DynamicParameters = Capabilities.DynamicParameters;
	Result.AHAP = Capabilities.AHAP;
	Result.MaximumEventCount = Capabilities.MaximumEventCount.bKnown
		? Capabilities.MaximumEventCount.Value : 0;
	Result.MaximumControlPointCount =
		Capabilities.MaximumControlPointCount.bKnown
			? Capabilities.MaximumControlPointCount.Value : 0;
	Result.MaximumDurationSeconds = Capabilities.MaximumDurationSeconds.bKnown
		? Capabilities.MaximumDurationSeconds.Seconds : 0.0;
	uint32 Hash = 0;
	HashValue(Hash, Result.Availability);
	HashValue(Hash, Result.BasicVibration);
	HashValue(Hash, Result.RichHaptics);
	HashValue(Hash, Result.Primitives);
	HashValue(Hash, Result.WaveformTiming);
	HashValue(Hash, Result.FrequencyControl);
	HashValue(Hash, Result.DynamicParameters);
	HashValue(Hash, Result.AHAP);
	HashValue(Hash, Result.MaximumEventCount);
	HashValue(Hash, Result.MaximumControlPointCount);
	HashValue(Hash, Result.MaximumDurationSeconds);
	Result.Signature = Hash;
	return Result;
}

FOpenMobileHapticsPreviewPattern
FOpenMobileHapticsPreviewPattern::FromAsset(
	const UOpenMobileHapticPatternAsset& Asset,
	uint32 InCapabilitySignature
)
{
	FOpenMobileHapticsPreviewPattern Result;
	Result.CookedPattern = Asset.GetCookedPattern();
	Result.Loop = Asset.Loop;
	if (Result.Loop.bLoop)
	{
		Result.Loop.RepeatCount = FMath::Clamp(
			Result.Loop.RepeatCount == 0 ? 1 : Result.Loop.RepeatCount,
			1,
			3
		);
		Result.Loop.MaximumDurationSeconds = FMath::Clamp(
			Result.Loop.MaximumDurationSeconds,
			0.001,
			FOpenMobileHapticsPreviewProtocol::MaximumPatternDurationSeconds
		);
	}
	Result.Category = Asset.DefaultCategory.ToString();
	Result.FallbackPolicy = Asset.FallbackPolicy;
	Result.LowestAllowedFallback = Asset.LowestAllowedFallback;
	Result.PrimitiveOrPresetFallback =
		Asset.PrimitiveOrPresetFallback.ToString();
	Result.bAllowSemanticFallback = Asset.bAllowSemanticFallback;
	Result.SemanticFallback = Asset.SemanticFallback;
	Result.CapabilitySignature = InCapabilitySignature;
	return Result;
}

bool FOpenMobileHapticsPreviewPattern::Validate(FString& OutError) const
{
	using namespace OpenMobileHapticsPreviewProtocolPrivate;
	OutError.Reset();
	if (CookedPattern.DataFormatVersion
			!= FOpenMobileHapticCookedPatternData::CurrentFormatVersion
		|| CookedPattern.Events.IsEmpty()
		|| CookedPattern.Events.Num()
			> FOpenMobileHapticsPreviewProtocol::MaximumEvents
		|| CookedPattern.ParameterCurves.Num()
			> FOpenMobileHapticsPreviewProtocol::MaximumCurves
		|| CookedPattern.DurationMicroseconds
			> FOpenMobileHapticsPreviewProtocol::MaximumPatternDurationSeconds
				* 1000000.0
		|| CookedPattern.GranularityMicroseconds == 0
		|| (CookedPattern.DurationMicroseconds > 0
			&& CookedPattern.GranularityMicroseconds
				> CookedPattern.DurationMicroseconds))
	{
		OutError = TEXT("Preview pattern limits are invalid.");
		return false;
	}
	uint32 PreviousStart = 0;
	uint32 PreviousEnd = 0;
	uint32 ResolvedDuration = 0;
	for (int32 Index = 0; Index < CookedPattern.Events.Num(); ++Index)
	{
		const FOpenMobileHapticCookedPatternEvent& Event =
			CookedPattern.Events[Index];
		if ((Index > 0 && (Event.StartTimeMicroseconds < PreviousStart
				|| Event.StartTimeMicroseconds < PreviousEnd))
			|| Event.StartTimeMicroseconds > CookedPattern.DurationMicroseconds
			|| Event.DurationMicroseconds
				> CookedPattern.DurationMicroseconds
					- Event.StartTimeMicroseconds
			|| static_cast<uint8>(Event.Type) > static_cast<uint8>(
				EOpenMobileHapticPatternEventType::Silence))
		{
			OutError = TEXT("Preview event timing is invalid.");
			return false;
		}
		if (Event.Type == EOpenMobileHapticPatternEventType::Transient
			&& Event.DurationMicroseconds != 0)
		{
			OutError = TEXT("Preview transient duration is invalid.");
			return false;
		}
		if (Event.Type != EOpenMobileHapticPatternEventType::Transient
			&& Event.DurationMicroseconds
				< CookedPattern.GranularityMicroseconds)
		{
			OutError = TEXT("Preview continuous duration is invalid.");
			return false;
		}
		if (Event.Type == EOpenMobileHapticPatternEventType::Silence
			&& Event.Intensity != 0)
		{
			OutError = TEXT("Preview silence intensity is invalid.");
			return false;
		}
		PreviousStart = Event.StartTimeMicroseconds;
		PreviousEnd = Event.StartTimeMicroseconds
			+ Event.DurationMicroseconds;
		ResolvedDuration = FMath::Max(ResolvedDuration, PreviousEnd);
	}
	if (ResolvedDuration != CookedPattern.DurationMicroseconds)
	{
		OutError = TEXT("Preview pattern duration is invalid.");
		return false;
	}
	int32 TotalPoints = 0;
	uint32 PreviousCurveStart = 0;
	uint32 PreviousCurveEnds[2] = {0, 0};
	bool bHasPreviousCurve = false;
	for (const FOpenMobileHapticCookedParameterCurve& Curve :
		CookedPattern.ParameterCurves)
	{
		TotalPoints += Curve.ControlPoints.Num();
		if (TotalPoints
			> FOpenMobileHapticsPreviewProtocol::MaximumControlPoints
			|| Curve.ControlPoints.Num() < 2
			|| static_cast<uint8>(Curve.Parameter) > static_cast<uint8>(
				EOpenMobileHapticCurveParameter::SharpnessControl)
			|| (bHasPreviousCurve
				&& Curve.StartTimeMicroseconds < PreviousCurveStart))
		{
			OutError = TEXT("Preview curve limits are invalid.");
			return false;
		}
		const int32 ParameterIndex = static_cast<int32>(Curve.Parameter);
		if (Curve.StartTimeMicroseconds
			< PreviousCurveEnds[ParameterIndex])
		{
			OutError = TEXT("Preview parameter curves overlap.");
			return false;
		}
		uint32 PreviousTime = 0;
		for (int32 Index = 0; Index < Curve.ControlPoints.Num(); ++Index)
		{
			const FOpenMobileHapticCookedCurvePoint& Point =
				Curve.ControlPoints[Index];
			if ((Index == 0 && Point.RelativeTimeMicroseconds != 0)
				|| (Index > 0
					&& Point.RelativeTimeMicroseconds <= PreviousTime)
				|| Curve.StartTimeMicroseconds
					> CookedPattern.DurationMicroseconds
				|| Point.RelativeTimeMicroseconds
					> CookedPattern.DurationMicroseconds
						- Curve.StartTimeMicroseconds)
			{
				OutError = TEXT("Preview curve timing is invalid.");
				return false;
			}
			PreviousTime = Point.RelativeTimeMicroseconds;
		}
		PreviousCurveStart = Curve.StartTimeMicroseconds;
		bHasPreviousCurve = true;
		PreviousCurveEnds[ParameterIndex] = Curve.StartTimeMicroseconds
			+ Curve.ControlPoints.Last().RelativeTimeMicroseconds;
	}
	if (Category.Len() > MaximumNameCharacters
		|| PrimitiveOrPresetFallback.Len() > MaximumNameCharacters
		|| !IsSafeIdentifier(Category, false)
		|| !IsSafeIdentifier(PrimitiveOrPresetFallback, true)
		|| static_cast<uint8>(FallbackPolicy) > static_cast<uint8>(
			EOpenMobileHapticFallbackPolicy::NoEffectAllowed)
		|| static_cast<uint8>(LowestAllowedFallback) > static_cast<uint8>(
			EOpenMobileHapticFallbackFloor::BasicVibration)
		|| static_cast<uint8>(SemanticFallback) > static_cast<uint8>(
			EOpenMobileHapticSemanticEffect::Achievement))
	{
		OutError = TEXT("Preview fallback metadata is invalid.");
		return false;
	}
	if (Loop.bLoop
		&& (Loop.RepeatCount < 1 || Loop.RepeatCount > 3
			|| !FMath::IsFinite(Loop.RepeatStartTimeSeconds)
			|| Loop.RepeatStartTimeSeconds < 0.0
			|| Loop.RepeatStartTimeSeconds
				>= CookedPattern.DurationMicroseconds / 1000000.0
			|| !FMath::IsFinite(Loop.MaximumDurationSeconds)
			|| Loop.MaximumDurationSeconds <= 0.0
			|| Loop.MaximumDurationSeconds
				> FOpenMobileHapticsPreviewProtocol::MaximumPatternDurationSeconds))
	{
		OutError = TEXT("Preview loop limits are invalid.");
		return false;
	}
	return true;
}

bool FOpenMobileHapticsPreviewProtocol::Encode(
	const FOpenMobileHapticsPreviewMessage& Message,
	TArray<uint8>& OutPacket,
	FString& OutError
)
{
	using namespace OpenMobileHapticsPreviewProtocolPrivate;
	OutPacket.Reset();
	OutError.Reset();
	FOpenMobileHapticsPreviewMessage MutableMessage = Message;
	if (!MutableMessage.SenderId.IsValid())
	{
		OutError = TEXT("Preview sender identity is missing.");
		return false;
	}
	if (MutableMessage.Type == EOpenMobileHapticsPreviewMessageType::Preview
		&& !MutableMessage.Pattern.Validate(OutError))
	{
		return false;
	}
	TArray<uint8> Body;
	FMemoryWriter BodyWriter(Body, true);
	if (!SerializeMessageBody(BodyWriter, MutableMessage, OutError)
		|| BodyWriter.IsError())
	{
		return false;
	}
	BodyWriter.Close();
	if (Body.Num() + HeaderBytes > MaximumPacketBytes)
	{
		OutError = TEXT("Preview packet exceeds the payload limit.");
		return false;
	}
	FMemoryWriter Writer(OutPacket, true);
	uint32 PacketMagic = Magic;
	uint16 ProtocolVersion = Version;
	uint8 MessageType = static_cast<uint8>(MutableMessage.Type);
	uint8 Flags = 0;
	uint32 BodyBytes = Body.Num();
	Writer << PacketMagic;
	Writer << ProtocolVersion;
	Writer << MessageType;
	Writer << Flags;
	Writer << BodyBytes;
	if (!Body.IsEmpty())
	{
		Writer.Serialize(Body.GetData(), Body.Num());
	}
	Writer.Close();
	return !Writer.IsError();
}

bool FOpenMobileHapticsPreviewProtocol::Decode(
	TConstArrayView<uint8> Packet,
	FOpenMobileHapticsPreviewMessage& OutMessage,
	FString& OutError
)
{
	using namespace OpenMobileHapticsPreviewProtocolPrivate;
	OutMessage = {};
	OutError.Reset();
	if (Packet.Num() < HeaderBytes || Packet.Num() > MaximumPacketBytes)
	{
		OutError = TEXT("Preview packet size is invalid.");
		return false;
	}
	FMemoryReaderView Reader(Packet, true);
	uint32 PacketMagic = 0;
	uint16 ProtocolVersion = 0;
	uint8 MessageType = 0;
	uint8 Flags = 0;
	uint32 BodyBytes = 0;
	Reader << PacketMagic;
	Reader << ProtocolVersion;
	Reader << MessageType;
	Reader << Flags;
	Reader << BodyBytes;
	if (Reader.IsError() || PacketMagic != Magic || ProtocolVersion != Version
		|| Flags != 0 || BodyBytes != Packet.Num() - HeaderBytes
		|| MessageType < static_cast<uint8>(
			EOpenMobileHapticsPreviewMessageType::Discover)
		|| MessageType > static_cast<uint8>(
			EOpenMobileHapticsPreviewMessageType::Goodbye))
	{
		OutError = TEXT("Preview protocol header is incompatible.");
		return false;
	}
	OutMessage.Type =
		static_cast<EOpenMobileHapticsPreviewMessageType>(MessageType);
	if (!SerializeMessageBody(Reader, OutMessage, OutError)
		|| Reader.IsError() || Reader.Tell() != Reader.TotalSize())
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Preview message body is malformed.");
		}
		return false;
	}
	return true;
}

FString FOpenMobileHapticsPreviewProtocol::SanitizeText(
	const FString& Value,
	int32 MaximumLength
)
{
	FString Sanitized;
	Sanitized.Reserve(FMath::Min(Value.Len(), MaximumLength));
	for (const TCHAR Character : Value)
	{
		if (Sanitized.Len() >= MaximumLength)
		{
			break;
		}
		if (Character >= 32 && Character != 127
			&& Character != TEXT('\t') && Character != TEXT('\n')
			&& Character != TEXT('\r'))
		{
			Sanitized.AppendChar(Character);
		}
	}
	return Sanitized;
}

EOpenMobileHapticsPreviewPairDecision
FOpenMobileHapticsPreviewSessionPolicy::BeginPairing(
	const FGuid& EditorId,
	const FGuid& RequestId
)
{
	if (!EditorId.IsValid() || !RequestId.IsValid())
	{
		return EOpenMobileHapticsPreviewPairDecision::Invalid;
	}
	if (IsPaired())
	{
		return EditorId == PairedEditorId && RequestId == PairedRequestId
			? EOpenMobileHapticsPreviewPairDecision::ExistingSession
			: EOpenMobileHapticsPreviewPairDecision::Busy;
	}
	if (IsPairingPending())
	{
		return EditorId == PendingEditorId && RequestId == PendingRequestId
			? EOpenMobileHapticsPreviewPairDecision::ExistingPending
			: EOpenMobileHapticsPreviewPairDecision::Busy;
	}
	PendingEditorId = EditorId;
	PendingRequestId = RequestId;
	return EOpenMobileHapticsPreviewPairDecision::Started;
}

bool FOpenMobileHapticsPreviewSessionPolicy::ApprovePairing(
	const FGuid& RequestId,
	double NowSeconds
)
{
	if (!FMath::IsFinite(NowSeconds) || !IsPairingPending()
		|| RequestId != PendingRequestId)
	{
		return false;
	}
	PairedEditorId = PendingEditorId;
	PairedRequestId = PendingRequestId;
	SessionId = FGuid::NewGuid();
	PendingEditorId.Invalidate();
	PendingRequestId.Invalidate();
	SessionExpirationSeconds = NowSeconds
		+ FOpenMobileHapticsPreviewProtocol::SessionLifetimeSeconds;
	LastSessionActivitySeconds = NowSeconds;
	LastRevision = 0;
	RecentRequestTimes.Reset();
	return true;
}

bool FOpenMobileHapticsPreviewSessionPolicy::RejectPairing(
	const FGuid& RequestId
)
{
	if (!IsPairingPending() || RequestId != PendingRequestId)
	{
		return false;
	}
	PendingEditorId.Invalidate();
	PendingRequestId.Invalidate();
	return true;
}

bool FOpenMobileHapticsPreviewSessionPolicy::IsPairingPending() const
{
	return PendingEditorId.IsValid() && PendingRequestId.IsValid();
}

bool FOpenMobileHapticsPreviewSessionPolicy::IsPaired() const
{
	return PairedEditorId.IsValid() && PairedRequestId.IsValid()
		&& SessionId.IsValid();
}

bool FOpenMobileHapticsPreviewSessionPolicy::MatchesSession(
	const FGuid& EditorId,
	const FGuid& InSessionId
) const
{
	return IsPaired() && EditorId == PairedEditorId
		&& InSessionId == SessionId;
}

bool FOpenMobileHapticsPreviewSessionPolicy::IsSessionExpired(
	double NowSeconds
) const
{
	return IsPaired() && (!FMath::IsFinite(NowSeconds)
		|| NowSeconds >= SessionExpirationSeconds
		|| NowSeconds - LastSessionActivitySeconds
			>= FOpenMobileHapticsPreviewProtocol::SessionIdleTimeoutSeconds);
}

void FOpenMobileHapticsPreviewSessionPolicy::TouchSession(double NowSeconds)
{
	if (IsPaired() && FMath::IsFinite(NowSeconds))
	{
		LastSessionActivitySeconds = NowSeconds;
	}
}

EOpenMobileHapticsPreviewResultCode
FOpenMobileHapticsPreviewSessionPolicy::AdmitPreview(
	uint64 Revision,
	double NowSeconds,
	int32 QueueDepth
)
{
	if (!IsPaired() || Revision == 0 || !FMath::IsFinite(NowSeconds))
	{
		return EOpenMobileHapticsPreviewResultCode::SessionExpired;
	}
	if (Revision <= LastRevision)
	{
		return EOpenMobileHapticsPreviewResultCode::StaleRevision;
	}
	RecentRequestTimes.RemoveAll(
		[NowSeconds](double Time) { return NowSeconds - Time >= 1.0; }
	);
	if (RecentRequestTimes.Num()
		>= FOpenMobileHapticsPreviewProtocol::MaximumRequestsPerSecond)
	{
		return EOpenMobileHapticsPreviewResultCode::RateLimited;
	}
	if (QueueDepth < 0 || QueueDepth
		>= FOpenMobileHapticsPreviewProtocol::MaximumPreviewQueueDepth)
	{
		return EOpenMobileHapticsPreviewResultCode::QueueFull;
	}
	RecentRequestTimes.Add(NowSeconds);
	LastRevision = Revision;
	return EOpenMobileHapticsPreviewResultCode::Accepted;
}

void FOpenMobileHapticsPreviewSessionPolicy::Disconnect()
{
	PairedEditorId.Invalidate();
	PairedRequestId.Invalidate();
	SessionId.Invalidate();
	SessionExpirationSeconds = 0.0;
	LastSessionActivitySeconds = 0.0;
	LastRevision = 0;
	RecentRequestTimes.Reset();
}

void FOpenMobileHapticsPreviewSessionPolicy::Reset()
{
	PendingEditorId.Invalidate();
	PendingRequestId.Invalidate();
	Disconnect();
}

const FGuid&
FOpenMobileHapticsPreviewSessionPolicy::GetPendingRequestId() const
{
	return PendingRequestId;
}

const FGuid&
FOpenMobileHapticsPreviewSessionPolicy::GetPairedEditorId() const
{
	return PairedEditorId;
}

const FGuid&
FOpenMobileHapticsPreviewSessionPolicy::GetPairedRequestId() const
{
	return PairedRequestId;
}

const FGuid& FOpenMobileHapticsPreviewSessionPolicy::GetSessionId() const
{
	return SessionId;
}
