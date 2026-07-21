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

	~SOpenMobileHapticTimelineEditor() override;
	void Construct(const FArguments& InArgs);

private:
	FText GetFallbackPolicyText() const;
	FText GetPreviewPlatformText() const;
	FText GetCapabilityTierText() const;
	FText GetPreviewText() const;
	FSlateColor GetStatusColor() const;
	EVisibility GetEventInspectorVisibility() const;
	EVisibility GetMarkerInspectorVisibility() const;
	void HandleTransportChanged();

	TSharedPtr<FOpenMobileHapticTimelineEditorModel> Model;
	TSharedPtr<FOpenMobileHapticsPreviewTransport> PreviewTransport;
	uint8 PreviewPlatform = 0;
	uint8 CapabilityTier = 0;
};
