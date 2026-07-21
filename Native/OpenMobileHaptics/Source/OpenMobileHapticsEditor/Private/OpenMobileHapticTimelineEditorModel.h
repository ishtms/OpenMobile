#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPatternAsset.h"

enum class EOpenMobileHapticEditorPreviewPlatform : uint8
{
	Android,
	IOS
};

enum class EOpenMobileHapticEditorCapabilityTier : uint8
{
	Rich,
	Standard,
	Basic,
	Unavailable
};

struct FOpenMobileHapticEditorPreview
{
	FText ResolvedPath;
	TArray<FText> Warnings;
	bool bRejected = false;
};

class FOpenMobileHapticTimelineEditorModel final
	: public TSharedFromThis<FOpenMobileHapticTimelineEditorModel>
{
public:
	explicit FOpenMobileHapticTimelineEditorModel(
		UOpenMobileHapticPatternAsset* InAsset
	);

	UOpenMobileHapticPatternAsset* GetAsset() const;
	const TSet<int32>& GetSelectedEvents() const;
	const TSet<int32>& GetSelectedMarkers() const;
	int32 GetPrimaryEventIndex() const;
	int32 GetPrimaryMarkerIndex() const;
	double GetSnapIntervalSeconds() const;
	double GetZoomPixelsPerSecond() const;
	double GetCursorTimeSeconds() const;
	const FText& GetStatusText() const;
	bool IsStatusError() const;

	void SetSnapIntervalSeconds(double Value);
	void SetZoomPixelsPerSecond(double Value);
	void SetCursorTimeSeconds(double Value);
	void SelectEvent(int32 Index, bool bToggle, bool bRange);
	void SelectMarker(int32 Index, bool bToggle);
	void ClearSelection();
	void SelectAllEvents();

	bool AddEvent(EOpenMobileHapticPatternEventType Type);
	bool AddMarker();
	bool DeleteSelection();
	bool MoveSelectedEvents(double DeltaSeconds);
	bool SetSelectedEventType(EOpenMobileHapticPatternEventType Value);
	bool SetSelectedEventStart(double Value);
	bool SetSelectedEventDuration(double Value);
	bool SetSelectedEventIntensity(float Value);
	bool SetSelectedEventSharpness(float Value);
	bool SetSelectedEventFrequencyIntent(float Value);
	bool SetSelectedMarkerName(FName Value);
	bool SetSelectedMarkerTime(double Value);
	bool SetDefaultCategory(FName Value);
	bool SetLoopEnabled(bool bEnabled);
	bool SetFallbackPolicy(EOpenMobileHapticFallbackPolicy Value);
	bool ValidateAsset();

	bool CanCopy() const;
	bool CanPaste() const;
	void CopySelection() const;
	bool PasteAtCursor();
	void RefreshAfterExternalChange();

	FOpenMobileHapticEditorPreview ResolvePreview(
		EOpenMobileHapticEditorPreviewPlatform Platform,
		EOpenMobileHapticEditorCapabilityTier Tier
	) const;

	FSimpleMulticastDelegate& OnChanged();

private:
	bool ApplyEdit(
		const FText& TransactionText,
		TFunctionRef<void(UOpenMobileHapticPatternAsset&)> Edit
	);
	double SnapTime(double Value) const;
	void SetStatus(FText Status, bool bError);
	FName MakeUniqueMarkerName() const;

	TWeakObjectPtr<UOpenMobileHapticPatternAsset> Asset;
	TSet<int32> SelectedEvents;
	TSet<int32> SelectedMarkers;
	int32 LastSelectedEvent = INDEX_NONE;
	double SnapIntervalSeconds = 0.01;
	double ZoomPixelsPerSecond = 160.0;
	double CursorTimeSeconds = 0.0;
	FText StatusText;
	bool bStatusError = false;
	FSimpleMulticastDelegate Changed;
};
