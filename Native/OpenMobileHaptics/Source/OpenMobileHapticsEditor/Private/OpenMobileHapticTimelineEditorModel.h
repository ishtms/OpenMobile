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
	/** Holds the asset weakly because editor shutdown and asset replacement can outlive Slate references to the model. */
	explicit FOpenMobileHapticTimelineEditorModel(
		UOpenMobileHapticPatternAsset* InAsset
	);

	/** Resolves the weak asset at use time, callers have to handle an editor closing underneath them. */
	UOpenMobileHapticPatternAsset* GetAsset() const;
	/** Exposes selected event indices read-only so widgets can't bypass range and primary-selection rules. */
	const TSet<int32>& GetSelectedEvents() const;
	/** Exposes marker selection without allowing stale indices to be inserted externally. */
	const TSet<int32>& GetSelectedMarkers() const;
	/** Returns the event used by single-value controls when several events are selected. */
	int32 GetPrimaryEventIndex() const;
	/** Returns the marker used by details controls, or no index when selection isn't usable. */
	int32 GetPrimaryMarkerIndex() const;
	/** Keeps toolbar and canvas snapping on the same interval. */
	double GetSnapIntervalSeconds() const;
	/** Gives every timeline row one shared horizontal scale. */
	double GetZoomPixelsPerSecond() const;
	/** Returns the insertion and paste time after it has been clamped to timeline range. */
	double GetCursorTimeSeconds() const;
	/** Shares the latest edit or validation result with all editor widgets. */
	const FText& GetStatusText() const;
	/** Lets Slate style failed validation separately from ordinary edit feedback. */
	bool IsStatusError() const;

	/** Accepts only finite positive snap intervals, zero would break drag and paste quantization. */
	void SetSnapIntervalSeconds(double Value);
	/** Clamps zoom to a usable editor range so accidental scroll input can't make the timeline disappear. */
	void SetZoomPixelsPerSecond(double Value);
	/** Clamps and snaps cursor time once so add and paste commands agree. */
	void SetCursorTimeSeconds(double Value);
	/** Applies single, toggle, or contiguous range selection while retaining one primary event. */
	void SelectEvent(int32 Index, bool bToggle, bool bRange);
	/** Toggles marker selection independently from events because their edit commands don't overlap. */
	void SelectMarker(int32 Index, bool bToggle);
	/** Clears both selection sets and the remembered range anchor together. */
	void ClearSelection();
	/** Selects only valid event indices from the current asset after external edits may have changed count. */
	void SelectAllEvents();

	/** Inserts a valid default event at the cursor through Unreal transaction tracking. */
	bool AddEvent(EOpenMobileHapticPatternEventType Type);
	/** Adds a marker with a unique name so lookups don't become ambiguous immediately. */
	bool AddMarker();
	/** Removes selected events and markers from highest index to lowest, avoiding index shifts during deletion. */
	bool DeleteSelection();
	/** Moves selected events as a group and rejects the edit when any event would pass time zero. */
	bool MoveSelectedEvents(double DeltaSeconds);
	/** Changes type for every selected event while preserving fields still valid for the new type. */
	bool SetSelectedEventType(EOpenMobileHapticPatternEventType Value);
	/** Moves selected starts by the primary event's delta so their spacing stays intact. */
	bool SetSelectedEventStart(double Value);
	/** Applies duration only to event types that consume it and leaves transient timing valid. */
	bool SetSelectedEventDuration(double Value);
	/** Applies a clamped intensity to the whole event selection in one undo transaction. */
	bool SetSelectedEventIntensity(float Value);
	/** Applies clamped sharpness together so multi-selection doesn't create separate undo steps. */
	bool SetSelectedEventSharpness(float Value);
	/** Applies clamped frequency intent while keeping unsupported platform behavior visible in preview warnings. */
	bool SetSelectedEventFrequencyIntent(float Value);
	/** Renames the primary marker only when the name stays non-empty and unique. */
	bool SetSelectedMarkerName(FName Value);
	/** Moves the primary marker to a finite snapped time without changing event selection. */
	bool SetSelectedMarkerTime(double Value);
	/** Changes the asset category through transaction tracking so Blueprint defaults and undo refresh correctly. */
	bool SetDefaultCategory(FName Value);
	/** Updates portable loop intent and leaves platform feasibility to validation and preview. */
	bool SetLoopEnabled(bool bEnabled);
	/** Applies asset fallback policy once, preview then resolves each platform from that same value. */
	bool SetFallbackPolicy(EOpenMobileHapticFallbackPolicy Value);
	/** Runs the runtime compiler contract against editor data and publishes the most useful failed index. */
	bool ValidateAsset();

	/** Enables copy only when at least one currently valid event or marker is selected. */
	bool CanCopy() const;
	/** Enables paste only when clipboard text carries the supported Haptics payload version. */
	bool CanPaste() const;
	/** Writes a versioned selection payload so ordinary text clipboard contents remain harmless. */
	void CopySelection() const;
	/** Recreates copied items relative to the cursor and resolves marker-name conflicts before committing. */
	bool PasteAtCursor();
	/** Prunes stale selection and reruns validation after details, undo, redo, or asset reload bypasses model edits. */
	void RefreshAfterExternalChange();

	/** Runs platform policies without playing anything, giving authors the actual route and warnings for one capability tier. */
	FOpenMobileHapticEditorPreview ResolvePreview(
		EOpenMobileHapticEditorPreviewPlatform Platform,
		EOpenMobileHapticEditorCapabilityTier Tier
	) const;

	/** Lets every timeline widget refresh from one model change signal. */
	FSimpleMulticastDelegate& OnChanged();

private:
	/** Wraps one asset mutation in transaction, dirtying, validation, and change broadcast so commands can't forget any part. */
	bool ApplyEdit(
		const FText& TransactionText,
		TFunctionRef<void(UOpenMobileHapticPatternAsset&)> Edit
	);
	/** Quantizes finite timeline values using the model's current snap interval. */
	double SnapTime(double Value) const;
	/** Stores feedback and broadcasts it with model changes so status widgets don't need separate wiring. */
	void SetStatus(FText Status, bool bError);
	/** Finds the first unused marker suffix in the current asset, copied and newly added markers share this rule. */
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
