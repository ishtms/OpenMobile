#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FOpenMobileHapticTimelineEditorModel;
class FOpenMobileHapticsPreviewTransport;

class SOpenMobileHapticTimelineEditor final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOpenMobileHapticTimelineEditor) {}
		SLATE_ARGUMENT(TSharedPtr<FOpenMobileHapticTimelineEditorModel>, Model)
	SLATE_END_ARGS()

	/** Disconnects model and transport delegates before Slate releases the widget tree. */
	~SOpenMobileHapticTimelineEditor() override;
	/** Builds controls around one shared model and starts preview transport only for this editor widget. */
	void Construct(const FArguments& InArgs);

private:
	/** Formats the asset's current fallback policy for a compact toolbar control. */
	FText GetFallbackPolicyText() const;
	/** Formats the selected preview platform without exposing the widget's byte storage. */
	FText GetPreviewPlatformText() const;
	/** Formats the simulated capability tier used by offline preview resolution. */
	FText GetCapabilityTierText() const;
	/** Resolves preview path and warnings on demand so the text always matches latest asset edits. */
	FText GetPreviewText() const;
	/** Uses error colour only for failed validation, ordinary status remains readable in editor themes. */
	FSlateColor GetStatusColor() const;
	/** Shows event controls only when the model has one usable primary event. */
	EVisibility GetEventInspectorVisibility() const;
	/** Shows marker controls independently because event and marker selection can coexist. */
	EVisibility GetMarkerInspectorVisibility() const;
	/** Invalidates Slate after network discovery, pairing, or receiver status changes. */
	void HandleTransportChanged();

	TSharedPtr<FOpenMobileHapticTimelineEditorModel> Model;
	TSharedPtr<FOpenMobileHapticsPreviewTransport> PreviewTransport;
	uint8 PreviewPlatform = 0;
	uint8 CapabilityTier = 0;
};
