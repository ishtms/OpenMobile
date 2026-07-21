#include "OpenMobileHapticTimelineEditorModel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Misc/Base64.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "OpenMobileHapticTimelineEditorModel"

namespace OpenMobileHapticTimelineEditorModelPrivate
{
	constexpr TCHAR ClipboardHeader[] = TEXT("OpenMobileHapticsTimeline\t1");

	FOpenMobileHapticCapabilities MakeCapabilities(
		EOpenMobileHapticEditorCapabilityTier Tier
	)
	{
		FOpenMobileHapticCapabilities Capabilities;
		if (Tier == EOpenMobileHapticEditorCapabilityTier::Unavailable)
		{
			Capabilities.Availability =
				EOpenMobileHapticAvailability::NoActuator;
			return Capabilities;
		}
		Capabilities.Availability =
			EOpenMobileHapticAvailability::BasicVibration;
		Capabilities.BasicVibration =
			EOpenMobileHapticSupportState::Supported;
		if (Tier == EOpenMobileHapticEditorCapabilityTier::Basic)
		{
			return Capabilities;
		}
		Capabilities.SemanticEffects =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.SemanticFeedback =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.Primitives = EOpenMobileHapticSupportState::Supported;
		Capabilities.PredefinedEffects =
			EOpenMobileHapticSupportState::Supported;
		if (Tier == EOpenMobileHapticEditorCapabilityTier::Standard)
		{
			return Capabilities;
		}
		Capabilities.RichHaptics = EOpenMobileHapticSupportState::Supported;
		Capabilities.Availability =
			EOpenMobileHapticAvailability::RichHaptics;
		Capabilities.TransientEvents =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.ContinuousEvents =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.WaveformTiming =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.AmplitudeControl =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.Envelopes =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.FrequencyControl =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.DynamicParameters =
			EOpenMobileHapticSupportState::Supported;
		Capabilities.AHAP = EOpenMobileHapticSupportState::Supported;
		Capabilities.AudioEvents = EOpenMobileHapticSupportState::Supported;
		return Capabilities;
	}

	bool AllowsFallback(
		const UOpenMobileHapticPatternAsset& Asset,
		EOpenMobileHapticFallbackFloor Step
	)
	{
		return static_cast<uint8>(Step)
			<= static_cast<uint8>(Asset.LowestAllowedFallback);
	}

	FString EncodeName(FName Name)
	{
		FTCHARToUTF8 Utf8(*Name.ToString());
		return FBase64::Encode(
			reinterpret_cast<const uint8*>(Utf8.Get()),
			Utf8.Length()
		);
	}

	bool DecodeName(const FString& Encoded, FName& OutName)
	{
		TArray<uint8> Bytes;
		if (!FBase64::Decode(Encoded, Bytes))
		{
			return false;
		}
		Bytes.Add(0);
		OutName = FName(UTF8_TO_TCHAR(
			reinterpret_cast<const ANSICHAR*>(Bytes.GetData())
		));
		return !OutName.IsNone();
	}

	bool ParseDouble(const FString& Value, double& OutValue)
	{
		return LexTryParseString(OutValue, *Value)
			&& FMath::IsFinite(OutValue);
	}

	bool ParseFloat(const FString& Value, float& OutValue)
	{
		return LexTryParseString(OutValue, *Value)
			&& FMath::IsFinite(OutValue);
	}

	bool SameEvent(
		const FOpenMobileHapticPatternEvent& Left,
		const FOpenMobileHapticPatternEvent& Right
	)
	{
		return Left.Type == Right.Type
			&& Left.StartTimeSeconds == Right.StartTimeSeconds
			&& Left.DurationSeconds == Right.DurationSeconds
			&& Left.Intensity == Right.Intensity
			&& Left.Sharpness == Right.Sharpness
			&& Left.FrequencyIntent == Right.FrequencyIntent;
	}

	bool SameMarker(
		const FOpenMobileHapticPatternMarker& Left,
		const FOpenMobileHapticPatternMarker& Right
	)
	{
		return Left.Name == Right.Name
			&& Left.TimeSeconds == Right.TimeSeconds;
	}
}

FOpenMobileHapticTimelineEditorModel::FOpenMobileHapticTimelineEditorModel(
	UOpenMobileHapticPatternAsset* InAsset
)
	: Asset(InAsset)
	, StatusText(LOCTEXT("Ready", "Ready"))
{
}

UOpenMobileHapticPatternAsset*
FOpenMobileHapticTimelineEditorModel::GetAsset() const
{
	return Asset.Get();
}

const TSet<int32>&
FOpenMobileHapticTimelineEditorModel::GetSelectedEvents() const
{
	return SelectedEvents;
}

const TSet<int32>&
FOpenMobileHapticTimelineEditorModel::GetSelectedMarkers() const
{
	return SelectedMarkers;
}

int32 FOpenMobileHapticTimelineEditorModel::GetPrimaryEventIndex() const
{
	return SelectedEvents.Num() == 1
		? *SelectedEvents.CreateConstIterator()
		: INDEX_NONE;
}

int32 FOpenMobileHapticTimelineEditorModel::GetPrimaryMarkerIndex() const
{
	return SelectedMarkers.Num() == 1
		? *SelectedMarkers.CreateConstIterator()
		: INDEX_NONE;
}

double FOpenMobileHapticTimelineEditorModel::GetSnapIntervalSeconds() const
{
	return SnapIntervalSeconds;
}

double FOpenMobileHapticTimelineEditorModel::GetZoomPixelsPerSecond() const
{
	return ZoomPixelsPerSecond;
}

double FOpenMobileHapticTimelineEditorModel::GetCursorTimeSeconds() const
{
	return CursorTimeSeconds;
}

const FText& FOpenMobileHapticTimelineEditorModel::GetStatusText() const
{
	return StatusText;
}

bool FOpenMobileHapticTimelineEditorModel::IsStatusError() const
{
	return bStatusError;
}

void FOpenMobileHapticTimelineEditorModel::SetSnapIntervalSeconds(double Value)
{
	if (FMath::IsFinite(Value))
	{
		SnapIntervalSeconds = FMath::Clamp(Value, 0.001, 1.0);
		Changed.Broadcast();
	}
}

void FOpenMobileHapticTimelineEditorModel::SetZoomPixelsPerSecond(double Value)
{
	if (FMath::IsFinite(Value))
	{
		ZoomPixelsPerSecond = FMath::Clamp(Value, 40.0, 1600.0);
		Changed.Broadcast();
	}
}

void FOpenMobileHapticTimelineEditorModel::SetCursorTimeSeconds(double Value)
{
	CursorTimeSeconds = SnapTime(Value);
	Changed.Broadcast();
}

void FOpenMobileHapticTimelineEditorModel::SelectEvent(
	int32 Index,
	bool bToggle,
	bool bRange
)
{
	UOpenMobileHapticPatternAsset* PatternAsset = GetAsset();
	if (!PatternAsset || !PatternAsset->SourcePattern.Events.IsValidIndex(Index))
	{
		return;
	}
	SelectedMarkers.Reset();
	if (bRange && LastSelectedEvent != INDEX_NONE)
	{
		SelectedEvents.Reset();
		for (int32 SelectedIndex = FMath::Min(LastSelectedEvent, Index);
			SelectedIndex <= FMath::Max(LastSelectedEvent, Index);
			++SelectedIndex)
		{
			SelectedEvents.Add(SelectedIndex);
		}
	}
	else if (bToggle)
	{
		if (SelectedEvents.Contains(Index))
		{
			SelectedEvents.Remove(Index);
		}
		else
		{
			SelectedEvents.Add(Index);
		}
	}
	else
	{
		SelectedEvents.Reset();
		SelectedEvents.Add(Index);
	}
	LastSelectedEvent = Index;
	Changed.Broadcast();
}

void FOpenMobileHapticTimelineEditorModel::SelectMarker(
	int32 Index,
	bool bToggle
)
{
	UOpenMobileHapticPatternAsset* PatternAsset = GetAsset();
	if (!PatternAsset || !PatternAsset->Markers.IsValidIndex(Index))
	{
		return;
	}
	SelectedEvents.Reset();
	LastSelectedEvent = INDEX_NONE;
	if (bToggle && SelectedMarkers.Contains(Index))
	{
		SelectedMarkers.Remove(Index);
	}
	else
	{
		if (!bToggle)
		{
			SelectedMarkers.Reset();
		}
		SelectedMarkers.Add(Index);
	}
	Changed.Broadcast();
}

void FOpenMobileHapticTimelineEditorModel::ClearSelection()
{
	SelectedEvents.Reset();
	SelectedMarkers.Reset();
	LastSelectedEvent = INDEX_NONE;
	Changed.Broadcast();
}

void FOpenMobileHapticTimelineEditorModel::SelectAllEvents()
{
	SelectedEvents.Reset();
	SelectedMarkers.Reset();
	if (const UOpenMobileHapticPatternAsset* PatternAsset = GetAsset())
	{
		for (int32 Index = 0;
			Index < PatternAsset->SourcePattern.Events.Num();
			++Index)
		{
			SelectedEvents.Add(Index);
		}
	}
	Changed.Broadcast();
}

bool FOpenMobileHapticTimelineEditorModel::AddEvent(
	EOpenMobileHapticPatternEventType Type
)
{
	FOpenMobileHapticPatternEvent Event;
	Event.Type = Type;
	Event.StartTimeSeconds = CursorTimeSeconds;
	Event.DurationSeconds = Type == EOpenMobileHapticPatternEventType::Transient
		? 0.0
		: FMath::Max(0.05, SnapIntervalSeconds);
	if (Type == EOpenMobileHapticPatternEventType::Silence)
	{
		Event.Intensity = 0.0f;
	}
	const bool bApplied = ApplyEdit(
		LOCTEXT("AddEventTransaction", "Add Haptic Timeline Event"),
		[Event](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			PatternAsset.SourcePattern.Events.Add(Event);
		}
	);
	if (bApplied)
	{
		ClearSelection();
	}
	return bApplied;
}

bool FOpenMobileHapticTimelineEditorModel::AddMarker()
{
	FOpenMobileHapticPatternMarker Marker;
	Marker.Name = MakeUniqueMarkerName();
	Marker.TimeSeconds = CursorTimeSeconds;
	const bool bApplied = ApplyEdit(
		LOCTEXT("AddMarkerTransaction", "Add Haptic Timeline Marker"),
		[Marker](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			PatternAsset.Markers.Add(Marker);
		}
	);
	if (bApplied)
	{
		SelectedEvents.Reset();
		SelectedMarkers.Reset();
		if (const UOpenMobileHapticPatternAsset* PatternAsset = GetAsset())
		{
			for (int32 Index = 0; Index < PatternAsset->Markers.Num(); ++Index)
			{
				if (PatternAsset->Markers[Index].Name == Marker.Name)
				{
					SelectedMarkers.Add(Index);
					break;
				}
			}
		}
		Changed.Broadcast();
	}
	return bApplied;
}

bool FOpenMobileHapticTimelineEditorModel::DeleteSelection()
{
	if (!CanCopy())
	{
		return false;
	}
	TArray<int32> EventIndices = SelectedEvents.Array();
	TArray<int32> MarkerIndices = SelectedMarkers.Array();
	EventIndices.Sort(TGreater<int32>());
	MarkerIndices.Sort(TGreater<int32>());
	const bool bApplied = ApplyEdit(
		LOCTEXT("DeleteSelectionTransaction", "Delete Haptic Timeline Selection"),
		[EventIndices, MarkerIndices](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			for (const int32 Index : EventIndices)
			{
				if (PatternAsset.SourcePattern.Events.IsValidIndex(Index))
				{
					PatternAsset.SourcePattern.Events.RemoveAt(Index);
				}
			}
			for (const int32 Index : MarkerIndices)
			{
				if (PatternAsset.Markers.IsValidIndex(Index))
				{
					PatternAsset.Markers.RemoveAt(Index);
				}
			}
		}
	);
	if (bApplied)
	{
		ClearSelection();
	}
	return bApplied;
}

bool FOpenMobileHapticTimelineEditorModel::MoveSelectedEvents(
	double DeltaSeconds
)
{
	if (SelectedEvents.IsEmpty() || !FMath::IsFinite(DeltaSeconds))
	{
		return false;
	}
	const TSet<int32> Indices = SelectedEvents;
	const bool bApplied = ApplyEdit(
		LOCTEXT("MoveEventsTransaction", "Move Haptic Timeline Events"),
		[this, Indices, DeltaSeconds](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			for (const int32 Index : Indices)
			{
				if (PatternAsset.SourcePattern.Events.IsValidIndex(Index))
				{
					FOpenMobileHapticPatternEvent& Event =
						PatternAsset.SourcePattern.Events[Index];
					Event.StartTimeSeconds = SnapTime(FMath::Max(
						0.0,
						Event.StartTimeSeconds + DeltaSeconds
					));
				}
			}
		}
	);
	return bApplied;
}

bool FOpenMobileHapticTimelineEditorModel::SetSelectedEventStart(double Value)
{
	const int32 Index = GetPrimaryEventIndex();
	if (Index == INDEX_NONE || !FMath::IsFinite(Value))
	{
		return false;
	}
	const bool bApplied = ApplyEdit(
		LOCTEXT("SetEventStartTransaction", "Set Haptic Event Start"),
		[this, Index, Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			PatternAsset.SourcePattern.Events[Index].StartTimeSeconds =
				SnapTime(Value);
		}
	);
	return bApplied;
}

bool FOpenMobileHapticTimelineEditorModel::SetSelectedEventType(
	EOpenMobileHapticPatternEventType Value
)
{
	if (SelectedEvents.IsEmpty())
	{
		return false;
	}
	const TSet<int32> Indices = SelectedEvents;
	return ApplyEdit(
		LOCTEXT("SetEventTypeTransaction", "Set Haptic Event Type"),
		[this, Indices, Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			for (const int32 Index : Indices)
			{
				if (!PatternAsset.SourcePattern.Events.IsValidIndex(Index))
				{
					continue;
				}
				FOpenMobileHapticPatternEvent& Event =
					PatternAsset.SourcePattern.Events[Index];
				Event.Type = Value;
				if (Value == EOpenMobileHapticPatternEventType::Transient)
				{
					Event.DurationSeconds = 0.0;
				}
				else
				{
					Event.DurationSeconds = FMath::Max(
						Event.DurationSeconds,
						SnapIntervalSeconds
					);
				}
				if (Value == EOpenMobileHapticPatternEventType::Silence)
				{
					Event.Intensity = 0.0f;
				}
			}
		}
	);
}

bool FOpenMobileHapticTimelineEditorModel::SetSelectedEventDuration(double Value)
{
	const int32 Index = GetPrimaryEventIndex();
	if (Index == INDEX_NONE || !FMath::IsFinite(Value))
	{
		return false;
	}
	return ApplyEdit(
		LOCTEXT("SetEventDurationTransaction", "Set Haptic Event Duration"),
		[this, Index, Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			FOpenMobileHapticPatternEvent& Event =
				PatternAsset.SourcePattern.Events[Index];
			Event.DurationSeconds = Event.Type
				== EOpenMobileHapticPatternEventType::Transient
					? 0.0
					: FMath::Max(SnapIntervalSeconds, SnapTime(Value));
		}
	);
}

bool FOpenMobileHapticTimelineEditorModel::SetSelectedEventIntensity(float Value)
{
	if (SelectedEvents.IsEmpty() || !FMath::IsFinite(Value))
	{
		return false;
	}
	const TSet<int32> Indices = SelectedEvents;
	return ApplyEdit(
		LOCTEXT("SetEventIntensityTransaction", "Set Haptic Event Intensity"),
		[Indices, Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			for (const int32 Index : Indices)
			{
				if (PatternAsset.SourcePattern.Events.IsValidIndex(Index)
					&& PatternAsset.SourcePattern.Events[Index].Type
						!= EOpenMobileHapticPatternEventType::Silence)
				{
					PatternAsset.SourcePattern.Events[Index].Intensity =
						FMath::Clamp(Value, 0.0f, 1.0f);
				}
			}
		}
	);
}

bool FOpenMobileHapticTimelineEditorModel::SetSelectedEventSharpness(float Value)
{
	if (SelectedEvents.IsEmpty() || !FMath::IsFinite(Value))
	{
		return false;
	}
	const TSet<int32> Indices = SelectedEvents;
	return ApplyEdit(
		LOCTEXT("SetEventSharpnessTransaction", "Set Haptic Event Sharpness"),
		[Indices, Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			for (const int32 Index : Indices)
			{
				if (PatternAsset.SourcePattern.Events.IsValidIndex(Index))
				{
					PatternAsset.SourcePattern.Events[Index].Sharpness =
						FMath::Clamp(Value, 0.0f, 1.0f);
				}
			}
		}
	);
}

bool FOpenMobileHapticTimelineEditorModel::SetSelectedEventFrequencyIntent(
	float Value
)
{
	if (SelectedEvents.IsEmpty() || !FMath::IsFinite(Value))
	{
		return false;
	}
	const TSet<int32> Indices = SelectedEvents;
	return ApplyEdit(
		LOCTEXT("SetEventFrequencyTransaction", "Set Haptic Event Frequency Intent"),
		[Indices, Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			for (const int32 Index : Indices)
			{
				if (PatternAsset.SourcePattern.Events.IsValidIndex(Index))
				{
					PatternAsset.SourcePattern.Events[Index].FrequencyIntent =
						FMath::Clamp(Value, 0.0f, 1.0f);
				}
			}
		}
	);
}

bool FOpenMobileHapticTimelineEditorModel::SetSelectedMarkerName(FName Value)
{
	const int32 Index = GetPrimaryMarkerIndex();
	if (Index == INDEX_NONE || Value.IsNone())
	{
		return false;
	}
	return ApplyEdit(
		LOCTEXT("SetMarkerNameTransaction", "Rename Haptic Timeline Marker"),
		[Index, Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			PatternAsset.Markers[Index].Name = Value;
		}
	);
}

bool FOpenMobileHapticTimelineEditorModel::SetSelectedMarkerTime(double Value)
{
	const int32 Index = GetPrimaryMarkerIndex();
	if (Index == INDEX_NONE || !FMath::IsFinite(Value))
	{
		return false;
	}
	const bool bApplied = ApplyEdit(
		LOCTEXT("SetMarkerTimeTransaction", "Set Haptic Marker Time"),
		[this, Index, Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			PatternAsset.Markers[Index].TimeSeconds = SnapTime(Value);
		}
	);
	return bApplied;
}

bool FOpenMobileHapticTimelineEditorModel::SetDefaultCategory(FName Value)
{
	if (Value.IsNone())
	{
		SetStatus(LOCTEXT("CategoryRequired", "Category cannot be empty."), true);
		Changed.Broadcast();
		return false;
	}
	return ApplyEdit(
		LOCTEXT("SetCategoryTransaction", "Set Haptic Pattern Category"),
		[Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			PatternAsset.DefaultCategory = Value;
		}
	);
}

bool FOpenMobileHapticTimelineEditorModel::SetLoopEnabled(bool bEnabled)
{
	return ApplyEdit(
		LOCTEXT("SetLoopTransaction", "Set Haptic Pattern Loop"),
		[bEnabled](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			PatternAsset.Loop.bLoop = bEnabled;
		}
	);
}

bool FOpenMobileHapticTimelineEditorModel::SetFallbackPolicy(
	EOpenMobileHapticFallbackPolicy Value
)
{
	return ApplyEdit(
		LOCTEXT("SetFallbackTransaction", "Set Haptic Fallback Policy"),
		[Value](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			PatternAsset.FallbackPolicy = Value;
		}
	);
}

bool FOpenMobileHapticTimelineEditorModel::ValidateAsset()
{
	UOpenMobileHapticPatternAsset* PatternAsset = GetAsset();
	TArray<FString> Errors;
	if (!PatternAsset || !PatternAsset->ValidateForEditor(Errors))
	{
		SetStatus(
			Errors.IsEmpty()
				? LOCTEXT("ValidationUnavailable", "Asset validation is unavailable.")
				: FText::FromString(Errors[0]),
			true
		);
		Changed.Broadcast();
		return false;
	}
	SetStatus(LOCTEXT("ValidationPassed", "Pattern is valid for save and cook."), false);
	Changed.Broadcast();
	return true;
}

bool FOpenMobileHapticTimelineEditorModel::CanCopy() const
{
	return !SelectedEvents.IsEmpty() || !SelectedMarkers.IsEmpty();
}

bool FOpenMobileHapticTimelineEditorModel::CanPaste() const
{
	FString Clipboard;
	FPlatformApplicationMisc::ClipboardPaste(Clipboard);
	return Clipboard.StartsWith(
		OpenMobileHapticTimelineEditorModelPrivate::ClipboardHeader
	);
}

void FOpenMobileHapticTimelineEditorModel::CopySelection() const
{
	using namespace OpenMobileHapticTimelineEditorModelPrivate;
	const UOpenMobileHapticPatternAsset* PatternAsset = GetAsset();
	if (!PatternAsset || !CanCopy())
	{
		return;
	}
	double Origin = TNumericLimits<double>::Max();
	for (const int32 Index : SelectedEvents)
	{
		if (PatternAsset->SourcePattern.Events.IsValidIndex(Index))
		{
			Origin = FMath::Min(
				Origin,
				PatternAsset->SourcePattern.Events[Index].StartTimeSeconds
			);
		}
	}
	for (const int32 Index : SelectedMarkers)
	{
		if (PatternAsset->Markers.IsValidIndex(Index))
		{
			Origin = FMath::Min(Origin, PatternAsset->Markers[Index].TimeSeconds);
		}
	}
	if (!FMath::IsFinite(Origin))
	{
		return;
	}

	FString Clipboard = FString::Printf(
		TEXT("%s\t%.17g\n"),
		ClipboardHeader,
		Origin
	);
	TArray<int32> EventIndices = SelectedEvents.Array();
	EventIndices.Sort();
	for (const int32 Index : EventIndices)
	{
		if (!PatternAsset->SourcePattern.Events.IsValidIndex(Index))
		{
			continue;
		}
		const FOpenMobileHapticPatternEvent& Event =
			PatternAsset->SourcePattern.Events[Index];
		Clipboard += FString::Printf(
			TEXT("Event\t%u\t%.17g\t%.17g\t%.9g\t%.9g\t%.9g\n"),
			static_cast<uint8>(Event.Type),
			Event.StartTimeSeconds - Origin,
			Event.DurationSeconds,
			Event.Intensity,
			Event.Sharpness,
			Event.FrequencyIntent
		);
	}
	TArray<int32> MarkerIndices = SelectedMarkers.Array();
	MarkerIndices.Sort();
	for (const int32 Index : MarkerIndices)
	{
		if (!PatternAsset->Markers.IsValidIndex(Index))
		{
			continue;
		}
		const FOpenMobileHapticPatternMarker& Marker =
			PatternAsset->Markers[Index];
		Clipboard += FString::Printf(
			TEXT("Marker\t%.17g\t%s\n"),
			Marker.TimeSeconds - Origin,
			*EncodeName(Marker.Name)
		);
	}
	FPlatformApplicationMisc::ClipboardCopy(*Clipboard);
}

bool FOpenMobileHapticTimelineEditorModel::PasteAtCursor()
{
	using namespace OpenMobileHapticTimelineEditorModelPrivate;
	FString Clipboard;
	FPlatformApplicationMisc::ClipboardPaste(Clipboard);
	TArray<FString> Lines;
	Clipboard.ParseIntoArrayLines(Lines, true);
	if (Lines.IsEmpty())
	{
		return false;
	}
	TArray<FString> Header;
	Lines[0].ParseIntoArray(Header, TEXT("\t"), false);
	if (Header.Num() != 3
		|| FString::Printf(TEXT("%s\t%s"), *Header[0], *Header[1])
			!= ClipboardHeader)
	{
		SetStatus(LOCTEXT("ClipboardFormat", "Clipboard data is not an OpenMobile Haptics timeline selection."), true);
		return false;
	}

	TArray<FOpenMobileHapticPatternEvent> Events;
	TArray<FOpenMobileHapticPatternMarker> Markers;
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		TArray<FString> Fields;
		Lines[LineIndex].ParseIntoArray(Fields, TEXT("\t"), false);
		if (Fields.Num() == 7 && Fields[0] == TEXT("Event"))
		{
			int32 Type = 0;
			FOpenMobileHapticPatternEvent Event;
			if (!LexTryParseString(Type, *Fields[1])
				|| Type < 0 || Type > static_cast<int32>(
					EOpenMobileHapticPatternEventType::Silence)
				|| !ParseDouble(Fields[2], Event.StartTimeSeconds)
				|| !ParseDouble(Fields[3], Event.DurationSeconds)
				|| !ParseFloat(Fields[4], Event.Intensity)
				|| !ParseFloat(Fields[5], Event.Sharpness)
				|| !ParseFloat(Fields[6], Event.FrequencyIntent))
			{
				SetStatus(LOCTEXT("ClipboardEventInvalid", "Clipboard event data is invalid."), true);
				return false;
			}
			Event.Type = static_cast<EOpenMobileHapticPatternEventType>(Type);
			Event.StartTimeSeconds = SnapTime(
				CursorTimeSeconds + Event.StartTimeSeconds
			);
			Events.Add(Event);
		}
		else if (Fields.Num() == 3 && Fields[0] == TEXT("Marker"))
		{
			FOpenMobileHapticPatternMarker Marker;
			if (!ParseDouble(Fields[1], Marker.TimeSeconds)
				|| !DecodeName(Fields[2], Marker.Name))
			{
				SetStatus(LOCTEXT("ClipboardMarkerInvalid", "Clipboard marker data is invalid."), true);
				return false;
			}
			Marker.TimeSeconds = SnapTime(
				CursorTimeSeconds + Marker.TimeSeconds
			);
			Markers.Add(Marker);
		}
		else
		{
			SetStatus(LOCTEXT("ClipboardLineInvalid", "Clipboard timeline data is malformed."), true);
			return false;
		}
	}
	if (Events.IsEmpty() && Markers.IsEmpty())
	{
		return false;
	}
	const bool bApplied = ApplyEdit(
		LOCTEXT("PasteTransaction", "Paste Haptic Timeline Selection"),
		[Events, Markers](UOpenMobileHapticPatternAsset& PatternAsset)
		{
			PatternAsset.SourcePattern.Events.Append(Events);
			TSet<FName> Names;
			for (const FOpenMobileHapticPatternMarker& Existing : PatternAsset.Markers)
			{
				Names.Add(Existing.Name);
			}
			for (FOpenMobileHapticPatternMarker Marker : Markers)
			{
				const FString BaseName = Marker.Name.ToString();
				for (int32 Suffix = 1; Names.Contains(Marker.Name); ++Suffix)
				{
					Marker.Name = FName(*FString::Printf(
						TEXT("%s_Copy%d"),
						*BaseName,
						Suffix
					));
				}
				Names.Add(Marker.Name);
				PatternAsset.Markers.Add(Marker);
			}
		}
	);
	if (bApplied)
	{
		ClearSelection();
	}
	return bApplied;
}

void FOpenMobileHapticTimelineEditorModel::RefreshAfterExternalChange()
{
	SelectedEvents.Reset();
	SelectedMarkers.Reset();
	LastSelectedEvent = INDEX_NONE;
	if (UOpenMobileHapticPatternAsset* PatternAsset = GetAsset())
	{
		TArray<FString> Errors;
		const bool bRebuilt = PatternAsset->RebuildDerivedData(Errors);
		if (bRebuilt)
		{
			PatternAsset->ValidateForEditor(Errors);
		}
		if (!bRebuilt || !Errors.IsEmpty())
		{
			SetStatus(
				Errors.IsEmpty()
					? LOCTEXT("RefreshFailed", "Asset data could not be rebuilt.")
					: FText::FromString(Errors[0]),
				true
			);
		}
		else
		{
			SetStatus(LOCTEXT("AssetRefreshed", "Asset data refreshed."), false);
		}
	}
	Changed.Broadcast();
}

FOpenMobileHapticEditorPreview
FOpenMobileHapticTimelineEditorModel::ResolvePreview(
	EOpenMobileHapticEditorPreviewPlatform Platform,
	EOpenMobileHapticEditorCapabilityTier Tier
) const
{
	using namespace OpenMobileHapticTimelineEditorModelPrivate;
	FOpenMobileHapticEditorPreview Preview;
	Preview.Warnings.Add(LOCTEXT(
		"PhysicalPreviewWarning",
		"Capability preview resolves API paths only. Desktop output does not represent physical feel."
	));
	const UOpenMobileHapticPatternAsset* PatternAsset = GetAsset();
	if (!PatternAsset || !PatternAsset->IsDerivedDataCurrent())
	{
		Preview.ResolvedPath = LOCTEXT("InvalidPattern", "Invalid pattern");
		Preview.bRejected = true;
		return Preview;
	}

	const FOpenMobileHapticCapabilities Capabilities = MakeCapabilities(Tier);
	const EOpenMobileHapticOverridePlatform OverridePlatform = Platform
		== EOpenMobileHapticEditorPreviewPlatform::Android
			? EOpenMobileHapticOverridePlatform::Android
			: EOpenMobileHapticOverridePlatform::IOS;
	const int32 OSVersion = Platform
		== EOpenMobileHapticEditorPreviewPlatform::Android ? 36 : 18;
	const FSoftObjectPath OverridePath =
		PatternAsset->GetOverrideForPlatform(OverridePlatform);
	const UOpenMobileHapticPlatformPatternAsset* Override =
		Cast<UOpenMobileHapticPlatformPatternAsset>(OverridePath.ResolveObject());
	if (!Override && !OverridePath.IsNull())
	{
		Override = Cast<UOpenMobileHapticPlatformPatternAsset>(
			OverridePath.TryLoad()
		);
	}
	TArray<FString> OverrideErrors;
	if (Override
		&& Override->GetOverridePlatform() == OverridePlatform
		&& Override->Validate(OverrideErrors)
		&& Override->Supports(Capabilities, OSVersion))
	{
		Preview.ResolvedPath = LOCTEXT("ExactOverride", "Exact platform override");
		return Preview;
	}
	if (!OverridePath.IsNull())
	{
		Preview.Warnings.Add(LOCTEXT(
			"OverrideUnavailable",
			"The configured native override is unavailable for this capability tier."
		));
	}

	if (PatternAsset->FallbackPolicy
		== EOpenMobileHapticFallbackPolicy::ExactOnly)
	{
		Preview.ResolvedPath = LOCTEXT("ExactRejected", "Rejected by Exact Only policy");
		Preview.bRejected = true;
		return Preview;
	}
	const bool bPortableSupported = Tier
		== EOpenMobileHapticEditorCapabilityTier::Rich;
	if (bPortableSupported
		&& AllowsFallback(*PatternAsset, EOpenMobileHapticFallbackFloor::PortableRich))
	{
		Preview.ResolvedPath = LOCTEXT("PortableRich", "Portable rich pattern");
		if (Platform == EOpenMobileHapticEditorPreviewPlatform::Android
			&& !PatternAsset->SourcePattern.ParameterCurves.IsEmpty())
		{
			Preview.Warnings.Add(LOCTEXT(
				"AndroidCurveWarning",
				"Android waveform playback cannot preserve authored parameter curves and follows the fallback policy."
			));
		}
		return Preview;
	}
	if (!PatternAsset->PrimitiveOrPresetFallback.IsNone()
		&& Tier <= EOpenMobileHapticEditorCapabilityTier::Standard
		&& AllowsFallback(
			*PatternAsset,
			EOpenMobileHapticFallbackFloor::PrimitiveOrPredefined
		))
	{
		Preview.ResolvedPath = FText::Format(
			LOCTEXT("PrimitivePath", "Primitive or preset: {0}"),
			FText::FromName(PatternAsset->PrimitiveOrPresetFallback)
		);
		return Preview;
	}
	if (PatternAsset->bAllowSemanticFallback
		&& Tier <= EOpenMobileHapticEditorCapabilityTier::Standard
		&& AllowsFallback(*PatternAsset, EOpenMobileHapticFallbackFloor::Semantic))
	{
		Preview.ResolvedPath = LOCTEXT("SemanticPath", "Semantic fallback");
		return Preview;
	}
	if (Tier != EOpenMobileHapticEditorCapabilityTier::Unavailable
		&& PatternAsset->FallbackPolicy
			!= EOpenMobileHapticFallbackPolicy::NoBasicVibration
		&& AllowsFallback(
			*PatternAsset,
			EOpenMobileHapticFallbackFloor::BasicVibration
		))
	{
		Preview.ResolvedPath = LOCTEXT("BasicPath", "Basic vibration fallback");
		return Preview;
	}
	if (PatternAsset->FallbackPolicy
		== EOpenMobileHapticFallbackPolicy::NoEffectAllowed)
	{
		Preview.ResolvedPath = LOCTEXT("NoEffectPath", "Allowed no-effect result");
		return Preview;
	}
	Preview.ResolvedPath = LOCTEXT("RejectedPath", "Rejected");
	Preview.bRejected = true;
	return Preview;
}

FSimpleMulticastDelegate& FOpenMobileHapticTimelineEditorModel::OnChanged()
{
	return Changed;
}

bool FOpenMobileHapticTimelineEditorModel::ApplyEdit(
	const FText& TransactionText,
	TFunctionRef<void(UOpenMobileHapticPatternAsset&)> Edit
)
{
	UOpenMobileHapticPatternAsset* PatternAsset = GetAsset();
	if (!PatternAsset)
	{
		return false;
	}
	const FOpenMobileHapticPattern PreviousPattern = PatternAsset->SourcePattern;
	const TArray<FOpenMobileHapticPatternMarker> PreviousMarkers =
		PatternAsset->Markers;
	const TSet<int32> PreviousSelectedEvents = SelectedEvents;
	const TSet<int32> PreviousSelectedMarkers = SelectedMarkers;
	const int32 PreviousLastSelectedEvent = LastSelectedEvent;
	const FName PreviousCategory = PatternAsset->DefaultCategory;
	const FOpenMobileHapticLoopOptions PreviousLoop = PatternAsset->Loop;
	const EOpenMobileHapticFallbackPolicy PreviousFallbackPolicy =
		PatternAsset->FallbackPolicy;
	FScopedTransaction Transaction(TransactionText);
	PatternAsset->Modify();
	Edit(*PatternAsset);
	TArray<FOpenMobileHapticPatternEvent> EditedSelectedEvents;
	for (const int32 Index : SelectedEvents)
	{
		if (PatternAsset->SourcePattern.Events.IsValidIndex(Index))
		{
			EditedSelectedEvents.Add(PatternAsset->SourcePattern.Events[Index]);
		}
	}
	TArray<FOpenMobileHapticPatternMarker> EditedSelectedMarkers;
	for (const int32 Index : SelectedMarkers)
	{
		if (PatternAsset->Markers.IsValidIndex(Index))
		{
			EditedSelectedMarkers.Add(PatternAsset->Markers[Index]);
		}
	}
	PatternAsset->NormalizeEditorData();
	SelectedEvents.Reset();
	TSet<int32> UsedEventIndices;
	for (const FOpenMobileHapticPatternEvent& Selected : EditedSelectedEvents)
	{
		for (int32 Index = 0;
			Index < PatternAsset->SourcePattern.Events.Num();
			++Index)
		{
			if (!UsedEventIndices.Contains(Index)
				&& OpenMobileHapticTimelineEditorModelPrivate::SameEvent(
					Selected,
					PatternAsset->SourcePattern.Events[Index]
				))
			{
				SelectedEvents.Add(Index);
				UsedEventIndices.Add(Index);
				break;
			}
		}
	}
	SelectedMarkers.Reset();
	TSet<int32> UsedMarkerIndices;
	for (const FOpenMobileHapticPatternMarker& Selected : EditedSelectedMarkers)
	{
		for (int32 Index = 0; Index < PatternAsset->Markers.Num(); ++Index)
		{
			if (!UsedMarkerIndices.Contains(Index)
				&& OpenMobileHapticTimelineEditorModelPrivate::SameMarker(
					Selected,
					PatternAsset->Markers[Index]
				))
			{
				SelectedMarkers.Add(Index);
				UsedMarkerIndices.Add(Index);
				break;
			}
		}
	}
	LastSelectedEvent = SelectedEvents.IsEmpty()
		? INDEX_NONE : *SelectedEvents.CreateConstIterator();
	TArray<FString> Errors;
	const bool bRebuilt = PatternAsset->RebuildDerivedData(Errors);
	if (bRebuilt)
	{
		PatternAsset->ValidateForEditor(Errors);
	}
	if (!bRebuilt || !Errors.IsEmpty())
	{
		const FText Rejection = Errors.IsEmpty()
			? LOCTEXT("EditRejected", "The edit is not valid for this pattern.")
			: FText::FromString(Errors[0]);
		PatternAsset->SourcePattern = PreviousPattern;
		PatternAsset->Markers = PreviousMarkers;
		PatternAsset->DefaultCategory = PreviousCategory;
		PatternAsset->Loop = PreviousLoop;
		PatternAsset->FallbackPolicy = PreviousFallbackPolicy;
		SelectedEvents = PreviousSelectedEvents;
		SelectedMarkers = PreviousSelectedMarkers;
		LastSelectedEvent = PreviousLastSelectedEvent;
		TArray<FString> RestoreErrors;
		PatternAsset->RebuildDerivedData(RestoreErrors);
		Transaction.Cancel();
		SetStatus(
			Rejection,
			true
		);
		Changed.Broadcast();
		return false;
	}
	PatternAsset->MarkPackageDirty();
	PatternAsset->PostEditChange();
	SetStatus(LOCTEXT("EditApplied", "Timeline edit applied."), false);
	Changed.Broadcast();
	return true;
}

double FOpenMobileHapticTimelineEditorModel::SnapTime(double Value) const
{
	if (!FMath::IsFinite(Value))
	{
		return 0.0;
	}
	return FMath::Max(
		0.0,
		FMath::GridSnap(Value, SnapIntervalSeconds)
	);
}

void FOpenMobileHapticTimelineEditorModel::SetStatus(
	FText Status,
	bool bError
)
{
	StatusText = MoveTemp(Status);
	bStatusError = bError;
}

FName FOpenMobileHapticTimelineEditorModel::MakeUniqueMarkerName() const
{
	const UOpenMobileHapticPatternAsset* PatternAsset = GetAsset();
	for (int32 Suffix = 1; Suffix <= 64; ++Suffix)
	{
		const FName Candidate(*FString::Printf(TEXT("Marker_%d"), Suffix));
		if (!PatternAsset || !PatternAsset->Markers.ContainsByPredicate(
			[Candidate](const FOpenMobileHapticPatternMarker& Marker)
			{
				return Marker.Name == Candidate;
			}
		))
		{
			return Candidate;
		}
	}
	return NAME_None;
}

#undef LOCTEXT_NAMESPACE
