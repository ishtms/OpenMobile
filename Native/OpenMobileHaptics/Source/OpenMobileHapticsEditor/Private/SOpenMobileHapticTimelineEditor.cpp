#include "SOpenMobileHapticTimelineEditor.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "OpenMobileHapticTimelineEditorModel.h"
#include "OpenMobileHapticsPreviewTransport.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SOpenMobileHapticTimelineEditor"

namespace OpenMobileHapticTimelineEditorPrivate
{
	constexpr float LeftGutter = 72.0f;
	constexpr float RulerHeight = 28.0f;
	constexpr float EventLaneHeight = 38.0f;
	constexpr float MarkerHeight = 24.0f;
	constexpr float CurveLaneHeight = 42.0f;

	float EventLaneY(EOpenMobileHapticPatternEventType Type)
	{
		return RulerHeight + MarkerHeight
			+ static_cast<uint8>(Type) * EventLaneHeight;
	}

	FText EventTypeText(EOpenMobileHapticPatternEventType Type)
	{
		switch (Type)
		{
		case EOpenMobileHapticPatternEventType::Transient:
			return LOCTEXT("Transient", "Transient");
		case EOpenMobileHapticPatternEventType::Continuous:
			return LOCTEXT("Continuous", "Continuous");
		case EOpenMobileHapticPatternEventType::Silence:
			return LOCTEXT("Silence", "Silence");
		default:
			return LOCTEXT("Unknown", "Unknown");
		}
	}

	FLinearColor EventColor(EOpenMobileHapticPatternEventType Type)
	{
		switch (Type)
		{
		case EOpenMobileHapticPatternEventType::Transient:
			return FLinearColor(0.20f, 0.72f, 0.96f);
		case EOpenMobileHapticPatternEventType::Continuous:
			return FLinearColor(0.28f, 0.78f, 0.42f);
		case EOpenMobileHapticPatternEventType::Silence:
			return FLinearColor(0.48f, 0.50f, 0.54f);
		default:
			return FLinearColor::White;
		}
	}

	class SOpenMobileHapticTimelineCanvas final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SOpenMobileHapticTimelineCanvas) {}
			SLATE_ARGUMENT(
				TSharedPtr<FOpenMobileHapticTimelineEditorModel>,
				Model
			)
		SLATE_END_ARGS()

		~SOpenMobileHapticTimelineCanvas() override
		{
			if (Model)
			{
				Model->OnChanged().RemoveAll(this);
			}
		}

		void Construct(const FArguments& InArgs)
		{
			Model = InArgs._Model;
			if (Model)
			{
				Model->OnChanged().AddSP(
					this,
					&SOpenMobileHapticTimelineCanvas::HandleModelChanged
				);
			}
		}

		FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			static_cast<void>(LayoutScaleMultiplier);
			const UOpenMobileHapticPatternAsset* Asset = Model
				? Model->GetAsset() : nullptr;
			double EndSeconds = 5.0;
			int32 CurveCount = 0;
			if (Asset)
			{
				for (const FOpenMobileHapticPatternEvent& Event :
					Asset->SourcePattern.Events)
				{
					EndSeconds = FMath::Max(
						EndSeconds,
						Event.StartTimeSeconds + Event.DurationSeconds
					);
				}
				for (const FOpenMobileHapticPatternMarker& Marker : Asset->Markers)
				{
					EndSeconds = FMath::Max(EndSeconds, Marker.TimeSeconds);
				}
				CurveCount = Asset->SourcePattern.ParameterCurves.Num();
				for (const FOpenMobileHapticParameterCurve& Curve :
					Asset->SourcePattern.ParameterCurves)
				{
					const double CurveEnd = Curve.ControlPoints.IsEmpty()
						? Curve.StartTimeSeconds
						: Curve.StartTimeSeconds
							+ Curve.ControlPoints.Last().RelativeTimeSeconds;
					EndSeconds = FMath::Max(EndSeconds, CurveEnd);
				}
			}
			const double Zoom = Model
				? Model->GetZoomPixelsPerSecond() : 160.0;
			return FVector2D(
				LeftGutter + (EndSeconds + 1.0) * Zoom,
				RulerHeight + MarkerHeight + 3.0f * EventLaneHeight
					+ FMath::Max(1, CurveCount) * CurveLaneHeight + 12.0f
			);
		}

		bool SupportsKeyboardFocus() const override
		{
			return true;
		}

		int32 OnPaint(
			const FPaintArgs& Args,
			const FGeometry& AllottedGeometry,
			const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements,
			int32 LayerId,
			const FWidgetStyle& InWidgetStyle,
			bool bParentEnabled
		) const override
		{
			static_cast<void>(Args);
			static_cast<void>(MyCullingRect);
			static_cast<void>(InWidgetStyle);
			static_cast<void>(bParentEnabled);
			const UOpenMobileHapticPatternAsset* Asset = Model
				? Model->GetAsset() : nullptr;
			if (!Asset)
			{
				return LayerId;
			}
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			const double Zoom = Model->GetZoomPixelsPerSecond();
			const FSlateBrush* WhiteBrush = FAppStyle::GetBrush(TEXT("WhiteBrush"));
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.025f, 0.028f, 0.034f)
			);

			auto DrawLine = [&](
				int32 Layer,
				FVector2D Start,
				FVector2D End,
				FLinearColor Color,
				float Thickness = 1.0f
			)
			{
				TArray<FVector2D> Points{Start, End};
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					Layer,
					AllottedGeometry.ToPaintGeometry(),
					Points,
					ESlateDrawEffect::None,
					Color,
					true,
					Thickness
				);
			};

			const double MajorTickSeconds = Zoom >= 300.0 ? 0.25
				: Zoom >= 120.0 ? 0.5 : 1.0;
			const int32 TickCount = FMath::CeilToInt(
				FMath::Max(0.0, (Size.X - LeftGutter) / Zoom)
				/ MajorTickSeconds
			);
			for (int32 Tick = 0; Tick <= TickCount; ++Tick)
			{
				const double Seconds = Tick * MajorTickSeconds;
				const float X = LeftGutter + Seconds * Zoom;
				DrawLine(
					LayerId + 1,
					FVector2D(X, 0.0f),
					FVector2D(X, Size.Y),
					FLinearColor(0.13f, 0.14f, 0.16f)
				);
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId + 2,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(56.0f, 18.0f),
						FSlateLayoutTransform(FVector2D(X + 3.0f, 3.0f))
					),
					FText::AsNumber(Seconds),
					FAppStyle::GetFontStyle(TEXT("SmallFont")),
					ESlateDrawEffect::None,
					FLinearColor(0.62f, 0.65f, 0.70f)
				);
			}

			for (uint8 Type = 0; Type <= static_cast<uint8>(
				EOpenMobileHapticPatternEventType::Silence); ++Type)
			{
				const EOpenMobileHapticPatternEventType EventType =
					static_cast<EOpenMobileHapticPatternEventType>(Type);
				const float Y = EventLaneY(EventType);
				DrawLine(
					LayerId + 1,
					FVector2D(0.0f, Y + EventLaneHeight),
					FVector2D(Size.X, Y + EventLaneHeight),
					FLinearColor(0.20f, 0.21f, 0.24f)
				);
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId + 2,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(68.0f, 18.0f),
						FSlateLayoutTransform(FVector2D(4.0f, Y + 9.0f))
					),
					EventTypeText(EventType),
					FAppStyle::GetFontStyle(TEXT("SmallFont")),
					ESlateDrawEffect::None,
					FLinearColor(0.72f, 0.74f, 0.78f)
				);
			}

			for (int32 Index = 0;
				Index < Asset->SourcePattern.Events.Num();
				++Index)
			{
				const FOpenMobileHapticPatternEvent& Event =
					Asset->SourcePattern.Events[Index];
				const bool bSelected =
					Model->GetSelectedEvents().Contains(Index);
				const double PreviewOffset = bSelected ? DragPreviewSeconds : 0.0;
				const float X = LeftGutter
					+ (Event.StartTimeSeconds + PreviewOffset) * Zoom;
				const float Width = static_cast<float>(Event.Type
					== EOpenMobileHapticPatternEventType::Transient
						? 8.0f
						: FMath::Max(8.0, Event.DurationSeconds * Zoom));
				const float Y = EventLaneY(Event.Type) + 5.0f;
				FLinearColor Color = EventColor(Event.Type);
				Color.A = bSelected ? 1.0f : 0.72f;
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId + 3,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(Width, EventLaneHeight - 10.0f),
						FSlateLayoutTransform(FVector2D(X, Y))
					),
					WhiteBrush,
					ESlateDrawEffect::None,
					Color
				);
				if (bSelected)
				{
					DrawLine(
						LayerId + 4,
						FVector2D(X, Y),
						FVector2D(X + Width, Y),
						FLinearColor::Yellow,
						2.0f
					);
				}
			}

			for (int32 Index = 0; Index < Asset->Markers.Num(); ++Index)
			{
				const FOpenMobileHapticPatternMarker& Marker = Asset->Markers[Index];
				const float X = LeftGutter + Marker.TimeSeconds * Zoom;
				const FLinearColor Color = Model->GetSelectedMarkers().Contains(Index)
					? FLinearColor::Yellow
					: FLinearColor(0.95f, 0.55f, 0.18f);
				TArray<FVector2D> Triangle{
					FVector2D(X, RulerHeight),
					FVector2D(X - 5.0f, RulerHeight + 9.0f),
					FVector2D(X + 5.0f, RulerHeight + 9.0f),
					FVector2D(X, RulerHeight)
				};
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId + 4,
					AllottedGeometry.ToPaintGeometry(),
					Triangle,
					ESlateDrawEffect::None,
					Color,
					true,
					2.0f
				);
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId + 4,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(120.0f, 18.0f),
						FSlateLayoutTransform(FVector2D(X + 7.0f, RulerHeight + 1.0f))
					),
					FText::FromName(Marker.Name),
					FAppStyle::GetFontStyle(TEXT("SmallFont")),
					ESlateDrawEffect::None,
					Color
				);
			}

			const float CurveTop = RulerHeight + MarkerHeight
				+ 3.0f * EventLaneHeight;
			for (int32 CurveIndex = 0;
				CurveIndex < Asset->SourcePattern.ParameterCurves.Num();
				++CurveIndex)
			{
				const FOpenMobileHapticParameterCurve& Curve =
					Asset->SourcePattern.ParameterCurves[CurveIndex];
				const float Y = CurveTop + CurveIndex * CurveLaneHeight;
				const FLinearColor Color = Curve.Parameter
					== EOpenMobileHapticCurveParameter::IntensityControl
						? FLinearColor(0.16f, 0.64f, 0.95f)
						: FLinearColor(0.95f, 0.28f, 0.62f);
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId + 2,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(68.0f, 18.0f),
						FSlateLayoutTransform(FVector2D(4.0f, Y + 11.0f))
					),
					Curve.Parameter == EOpenMobileHapticCurveParameter::IntensityControl
						? LOCTEXT("IntensityCurve", "Intensity")
						: LOCTEXT("SharpnessCurve", "Sharpness"),
					FAppStyle::GetFontStyle(TEXT("SmallFont")),
					ESlateDrawEffect::None,
					Color
				);
				TArray<FVector2D> Points;
				for (const FOpenMobileHapticCurvePoint& Point : Curve.ControlPoints)
				{
					Points.Add(FVector2D(
						LeftGutter + (Curve.StartTimeSeconds
							+ Point.RelativeTimeSeconds) * Zoom,
						Y + (1.0f - Point.Value) * (CurveLaneHeight - 8.0f) + 4.0f
					));
				}
				if (Points.Num() > 1)
				{
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						LayerId + 3,
						AllottedGeometry.ToPaintGeometry(),
						Points,
						ESlateDrawEffect::None,
						Color,
						true,
						2.0f
					);
				}
			}

			const float CursorX = LeftGutter
				+ Model->GetCursorTimeSeconds() * Zoom;
			DrawLine(
				LayerId + 5,
				FVector2D(CursorX, 0.0f),
				FVector2D(CursorX, Size.Y),
				FLinearColor(0.96f, 0.24f, 0.20f),
				1.5f
			);
			return LayerId + 5;
		}

		FReply OnMouseButtonDown(
			const FGeometry& MyGeometry,
			const FPointerEvent& MouseEvent
		) override
		{
			if (!Model || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled();
			}
			const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
			if (!Asset)
			{
				return FReply::Unhandled();
			}
			const FVector2D Local = MyGeometry.AbsoluteToLocal(
				MouseEvent.GetScreenSpacePosition()
			);
			const double Time = FMath::Max(
				0.0,
				(Local.X - LeftGutter) / Model->GetZoomPixelsPerSecond()
			);
			for (int32 Index = Asset->Markers.Num() - 1; Index >= 0; --Index)
			{
				const float MarkerX = LeftGutter
					+ Asset->Markers[Index].TimeSeconds
						* Model->GetZoomPixelsPerSecond();
				if (Local.Y >= RulerHeight
					&& Local.Y <= RulerHeight + MarkerHeight
					&& FMath::Abs(Local.X - MarkerX) <= 7.0f)
				{
					Model->SelectMarker(Index, MouseEvent.IsControlDown());
					return FReply::Handled().SetUserFocus(
						SharedThis(this),
						EFocusCause::Mouse
					);
				}
			}
			for (int32 Index = Asset->SourcePattern.Events.Num() - 1;
				Index >= 0;
				--Index)
			{
				const FOpenMobileHapticPatternEvent& Event =
					Asset->SourcePattern.Events[Index];
				const float X = LeftGutter + Event.StartTimeSeconds
					* Model->GetZoomPixelsPerSecond();
				const float Width = static_cast<float>(Event.Type
					== EOpenMobileHapticPatternEventType::Transient
						? 8.0f
						: FMath::Max(
							8.0,
							Event.DurationSeconds
								* Model->GetZoomPixelsPerSecond()
						));
				const float Y = EventLaneY(Event.Type) + 3.0f;
				if (Local.X >= X - 2.0f && Local.X <= X + Width + 2.0f
					&& Local.Y >= Y && Local.Y <= Y + EventLaneHeight - 6.0f)
				{
					Model->SelectEvent(
						Index,
						MouseEvent.IsControlDown(),
						MouseEvent.IsShiftDown()
					);
					DragStartSeconds = Time;
					DragPreviewSeconds = 0.0;
					bDraggingEvents = true;
					return FReply::Handled()
						.SetUserFocus(SharedThis(this), EFocusCause::Mouse)
						.CaptureMouse(SharedThis(this));
				}
			}
			Model->ClearSelection();
			Model->SetCursorTimeSeconds(Time);
			return FReply::Handled().SetUserFocus(
				SharedThis(this),
				EFocusCause::Mouse
			);
		}

		FReply OnMouseMove(
			const FGeometry& MyGeometry,
			const FPointerEvent& MouseEvent
		) override
		{
			if (!Model || !bDraggingEvents || !HasMouseCapture())
			{
				return FReply::Unhandled();
			}
			const FVector2D Local = MyGeometry.AbsoluteToLocal(
				MouseEvent.GetScreenSpacePosition()
			);
			const double Time = FMath::Max(
				0.0,
				(Local.X - LeftGutter) / Model->GetZoomPixelsPerSecond()
			);
			DragPreviewSeconds = FMath::GridSnap(
				Time - DragStartSeconds,
				Model->GetSnapIntervalSeconds()
			);
			Invalidate(EInvalidateWidgetReason::Paint);
			return FReply::Handled();
		}

		FReply OnMouseButtonUp(
			const FGeometry& MyGeometry,
			const FPointerEvent& MouseEvent
		) override
		{
			static_cast<void>(MyGeometry);
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton
				|| !bDraggingEvents)
			{
				return FReply::Unhandled();
			}
			bDraggingEvents = false;
			const double Delta = DragPreviewSeconds;
			DragPreviewSeconds = 0.0;
			if (Model && !FMath::IsNearlyZero(Delta))
			{
				Model->MoveSelectedEvents(Delta);
			}
			return FReply::Handled().ReleaseMouseCapture();
		}

		FReply OnMouseWheel(
			const FGeometry& MyGeometry,
			const FPointerEvent& MouseEvent
		) override
		{
			static_cast<void>(MyGeometry);
			if (!Model)
			{
				return FReply::Unhandled();
			}
			Model->SetZoomPixelsPerSecond(
				Model->GetZoomPixelsPerSecond()
					* FMath::Pow(1.15, MouseEvent.GetWheelDelta())
			);
			return FReply::Handled();
		}

		FReply OnKeyDown(
			const FGeometry& MyGeometry,
			const FKeyEvent& KeyEvent
		) override
		{
			static_cast<void>(MyGeometry);
			if (!Model)
			{
				return FReply::Unhandled();
			}
			const FKey Key = KeyEvent.GetKey();
			if (Key == EKeys::Delete || Key == EKeys::BackSpace)
			{
				Model->DeleteSelection();
				return FReply::Handled();
			}
			if (KeyEvent.IsControlDown() && Key == EKeys::C)
			{
				Model->CopySelection();
				return FReply::Handled();
			}
			if (KeyEvent.IsControlDown() && Key == EKeys::V)
			{
				Model->PasteAtCursor();
				return FReply::Handled();
			}
			if (KeyEvent.IsControlDown() && Key == EKeys::A)
			{
				Model->SelectAllEvents();
				return FReply::Handled();
			}
			if (Key == EKeys::Left || Key == EKeys::Right)
			{
				const double Direction = Key == EKeys::Left ? -1.0 : 1.0;
				const double Scale = KeyEvent.IsShiftDown() ? 10.0 : 1.0;
				Model->MoveSelectedEvents(
					Direction * Scale * Model->GetSnapIntervalSeconds()
				);
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

	private:
		void HandleModelChanged()
		{
			Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
		}

		TSharedPtr<FOpenMobileHapticTimelineEditorModel> Model;
		double DragStartSeconds = 0.0;
		double DragPreviewSeconds = 0.0;
		bool bDraggingEvents = false;
	};
}

SOpenMobileHapticTimelineEditor::~SOpenMobileHapticTimelineEditor()
{
	if (PreviewTransport)
	{
		PreviewTransport->OnChanged().RemoveAll(this);
		PreviewTransport->Shutdown();
	}
}

void SOpenMobileHapticTimelineEditor::Construct(const FArguments& InArgs)
{
	using namespace OpenMobileHapticTimelineEditorPrivate;
	Model = InArgs._Model;
	check(Model);
	PreviewTransport = MakeShared<FOpenMobileHapticsPreviewTransport>();
	PreviewTransport->OnChanged().AddSP(
		this,
		&SOpenMobileHapticTimelineEditor::HandleTransportChanged
	);

	auto AddEvent = [this](EOpenMobileHapticPatternEventType Type)
	{
		return FOnClicked::CreateLambda([this, Type]
		{
			Model->AddEvent(Type);
			return FReply::Handled();
		});
	};

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f)
		[
			SNew(SWrapBox)
			.UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddTransient", "+ Transient"))
				.OnClicked(AddEvent(EOpenMobileHapticPatternEventType::Transient))
			]
			+ SWrapBox::Slot().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddContinuous", "+ Continuous"))
				.OnClicked(AddEvent(EOpenMobileHapticPatternEventType::Continuous))
			]
			+ SWrapBox::Slot().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddSilence", "+ Silence"))
				.OnClicked(AddEvent(EOpenMobileHapticPatternEventType::Silence))
			]
			+ SWrapBox::Slot().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddMarker", "+ Marker"))
				.OnClicked_Lambda([this]
				{
					Model->AddMarker();
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Copy", "Copy"))
				.IsEnabled_Lambda([this] { return Model->CanCopy(); })
				.OnClicked_Lambda([this]
				{
					Model->CopySelection();
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Paste", "Paste"))
				.IsEnabled_Lambda([this] { return Model->CanPaste(); })
				.OnClicked_Lambda([this]
				{
					Model->PasteAtCursor();
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Delete", "Delete"))
				.IsEnabled_Lambda([this] { return Model->CanCopy(); })
				.OnClicked_Lambda([this]
				{
					Model->DeleteSelection();
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Undo", "Undo"))
				.OnClicked_Lambda([]
				{
					if (GEditor)
					{
						GEditor->UndoTransaction();
					}
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Redo", "Redo"))
				.OnClicked_Lambda([]
				{
					if (GEditor)
					{
						GEditor->RedoTransaction();
					}
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(8.0f, 2.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("CursorLabel", "Cursor"))
			]
			+ SWrapBox::Slot().Padding(2.0f).FillLineWhenSizeLessThan(120.0f)
			[
				SNew(SBox).WidthOverride(90.0f)
				[
					SNew(SNumericEntryBox<double>)
					.MinValue(0.0)
					.AllowSpin(true)
					.Value_Lambda([this]
					{
						return TOptional<double>(Model->GetCursorTimeSeconds());
					})
					.OnValueCommitted_Lambda([this](double Value, ETextCommit::Type)
					{
						Model->SetCursorTimeSeconds(Value);
					})
				]
			]
			+ SWrapBox::Slot().Padding(8.0f, 2.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("SnapLabel", "Snap"))
			]
			+ SWrapBox::Slot().Padding(2.0f).FillLineWhenSizeLessThan(120.0f)
			[
				SNew(SBox).WidthOverride(90.0f)
				[
					SNew(SNumericEntryBox<double>)
					.MinValue(0.001)
					.MaxValue(1.0)
					.Value_Lambda([this]
					{
						return TOptional<double>(Model->GetSnapIntervalSeconds());
					})
					.OnValueCommitted_Lambda([this](double Value, ETextCommit::Type)
					{
						Model->SetSnapIntervalSeconds(Value);
					})
				]
			]
			+ SWrapBox::Slot().Padding(8.0f, 2.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("ZoomLabel", "Zoom"))
			]
			+ SWrapBox::Slot().Padding(2.0f).FillLineWhenSizeLessThan(120.0f)
			[
				SNew(SBox).WidthOverride(90.0f)
				[
					SNew(SNumericEntryBox<double>)
					.MinValue(40.0)
					.MaxValue(1600.0)
					.Value_Lambda([this]
					{
						return TOptional<double>(Model->GetZoomPixelsPerSecond());
					})
					.OnValueCommitted_Lambda([this](double Value, ETextCommit::Type)
					{
						Model->SetZoomPixelsPerSecond(Value);
					})
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 0.0f, 6.0f, 6.0f)
		[
			SNew(SWrapBox)
			.UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(3.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("Category", "Category"))
			]
			+ SWrapBox::Slot().Padding(3.0f).FillLineWhenSizeLessThan(190.0f)
			[
				SNew(SBox).WidthOverride(150.0f)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([this]
					{
						const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
						return Asset ? FText::FromName(Asset->DefaultCategory) : FText();
					})
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
					{
						Model->SetDefaultCategory(FName(*Text.ToString().TrimStartAndEnd()));
					})
				]
			]
			+ SWrapBox::Slot().Padding(12.0f, 3.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]
				{
					const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
					return Asset && Asset->Loop.bLoop
						? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					Model->SetLoopEnabled(State == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock).Text(LOCTEXT("Loop", "Loop"))
				]
			]
			+ SWrapBox::Slot().Padding(3.0f)
			[
				SNew(SButton)
				.Text(this, &SOpenMobileHapticTimelineEditor::GetFallbackPolicyText)
				.ToolTipText(LOCTEXT("FallbackTip", "Cycle the pattern fallback policy. Configure fallback floor and names in Details."))
				.OnClicked_Lambda([this]
				{
					const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
					if (Asset)
					{
						const uint8 Next = (static_cast<uint8>(Asset->FallbackPolicy) + 1) % 4;
						Model->SetFallbackPolicy(
							static_cast<EOpenMobileHapticFallbackPolicy>(Next)
						);
					}
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(12.0f, 3.0f)
			[
				SNew(SButton)
				.Text(this, &SOpenMobileHapticTimelineEditor::GetPreviewPlatformText)
				.OnClicked_Lambda([this]
				{
					PreviewPlatform = (PreviewPlatform + 1) % 2;
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(3.0f)
			[
				SNew(SButton)
				.Text(this, &SOpenMobileHapticTimelineEditor::GetCapabilityTierText)
				.OnClicked_Lambda([this]
				{
					CapabilityTier = (CapabilityTier + 1) % 4;
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(3.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Validate", "Validate"))
				.OnClicked_Lambda([this]
				{
					Model->ValidateAsset();
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(12.0f, 3.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("FindDevice", "Find Device"))
				.OnClicked_Lambda([this]
				{
					PreviewTransport->Discover();
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(3.0f)
			[
				SNew(SButton)
				.Text_Lambda([this]
				{
					return PreviewTransport->GetSelectedDeviceText();
				})
				.OnClicked_Lambda([this]
				{
					PreviewTransport->SelectNextDevice();
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(3.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("PairDevice", "Pair"))
				.IsEnabled_Lambda([this]
				{
					return PreviewTransport->CanRequestPairing();
				})
				.OnClicked_Lambda([this]
				{
					PreviewTransport->RequestPairing();
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(3.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SendDevicePreview", "Preview on Device"))
				.IsEnabled_Lambda([this]
				{
					return PreviewTransport->CanSendPreview();
				})
				.OnClicked_Lambda([this]
				{
					if (const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset())
					{
						PreviewTransport->SendPreview(*Asset);
					}
					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot().Padding(3.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("StopDevicePreview", "Stop Device"))
				.IsEnabled_Lambda([this]
				{
					return PreviewTransport->CanSendPreview();
				})
				.OnClicked_Lambda([this]
				{
					PreviewTransport->StopPreview();
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 6.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(this, &SOpenMobileHapticTimelineEditor::GetPreviewText)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 6.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([this]
			{
				return PreviewTransport->GetStatusText();
			})
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(6.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				+ SScrollBox::Slot()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Vertical)
					+ SScrollBox::Slot()
					[
						SNew(SOpenMobileHapticTimelineCanvas).Model(Model)
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 2.0f)
		[
			SNew(SBorder)
			.Visibility(this, &SOpenMobileHapticTimelineEditor::GetEventInspectorVisibility)
			.Padding(6.0f)
			[
				SNew(SWrapBox)
				.UseAllottedSize(true)
				+ SWrapBox::Slot().Padding(3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("EventType", "Type"))
				]
				+ SWrapBox::Slot().Padding(3.0f)
				[
					SNew(SButton).Text(LOCTEXT("TypeTransient", "Transient"))
					.OnClicked_Lambda([this]
					{
						Model->SetSelectedEventType(EOpenMobileHapticPatternEventType::Transient);
						return FReply::Handled();
					})
				]
				+ SWrapBox::Slot().Padding(3.0f)
				[
					SNew(SButton).Text(LOCTEXT("TypeContinuous", "Continuous"))
					.OnClicked_Lambda([this]
					{
						Model->SetSelectedEventType(EOpenMobileHapticPatternEventType::Continuous);
						return FReply::Handled();
					})
				]
				+ SWrapBox::Slot().Padding(3.0f)
				[
					SNew(SButton).Text(LOCTEXT("TypeSilence", "Silence"))
					.OnClicked_Lambda([this]
					{
						Model->SetSelectedEventType(EOpenMobileHapticPatternEventType::Silence);
						return FReply::Handled();
					})
				]
				+ SWrapBox::Slot().Padding(8.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("Start", "Start"))
				]
				+ SWrapBox::Slot().Padding(3.0f).FillLineWhenSizeLessThan(115.0f)
				[
					SNew(SBox).WidthOverride(90.0f)
					[
						SNew(SNumericEntryBox<double>)
						.MinValue(0.0)
						.Value_Lambda([this]() -> TOptional<double>
						{
							const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
							const int32 Index = Model->GetPrimaryEventIndex();
							return Asset && Asset->SourcePattern.Events.IsValidIndex(Index)
								? TOptional<double>(Asset->SourcePattern.Events[Index].StartTimeSeconds)
								: TOptional<double>();
						})
						.OnValueCommitted_Lambda([this](double Value, ETextCommit::Type)
						{
							Model->SetSelectedEventStart(Value);
						})
					]
				]
				+ SWrapBox::Slot().Padding(8.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("Duration", "Duration"))
				]
				+ SWrapBox::Slot().Padding(3.0f).FillLineWhenSizeLessThan(115.0f)
				[
					SNew(SBox).WidthOverride(90.0f)
					[
						SNew(SNumericEntryBox<double>)
						.MinValue(0.0)
						.Value_Lambda([this]() -> TOptional<double>
						{
							const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
							const int32 Index = Model->GetPrimaryEventIndex();
							return Asset && Asset->SourcePattern.Events.IsValidIndex(Index)
								? TOptional<double>(Asset->SourcePattern.Events[Index].DurationSeconds)
								: TOptional<double>();
						})
						.OnValueCommitted_Lambda([this](double Value, ETextCommit::Type)
						{
							Model->SetSelectedEventDuration(Value);
						})
					]
				]
				+ SWrapBox::Slot().Padding(8.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("Intensity", "Intensity"))
				]
				+ SWrapBox::Slot().Padding(3.0f).FillLineWhenSizeLessThan(115.0f)
				[
					SNew(SBox).WidthOverride(90.0f)
					[
						SNew(SNumericEntryBox<float>)
						.MinValue(0.0f).MaxValue(1.0f)
						.Value_Lambda([this]() -> TOptional<float>
						{
							const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
							const int32 Index = Model->GetPrimaryEventIndex();
							return Asset && Asset->SourcePattern.Events.IsValidIndex(Index)
								? TOptional<float>(Asset->SourcePattern.Events[Index].Intensity)
								: TOptional<float>();
						})
						.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type)
						{
							Model->SetSelectedEventIntensity(Value);
						})
					]
				]
				+ SWrapBox::Slot().Padding(8.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("Sharpness", "Sharpness"))
				]
				+ SWrapBox::Slot().Padding(3.0f).FillLineWhenSizeLessThan(115.0f)
				[
					SNew(SBox).WidthOverride(90.0f)
					[
						SNew(SNumericEntryBox<float>)
						.MinValue(0.0f).MaxValue(1.0f)
						.Value_Lambda([this]() -> TOptional<float>
						{
							const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
							const int32 Index = Model->GetPrimaryEventIndex();
							return Asset && Asset->SourcePattern.Events.IsValidIndex(Index)
								? TOptional<float>(Asset->SourcePattern.Events[Index].Sharpness)
								: TOptional<float>();
						})
						.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type)
						{
							Model->SetSelectedEventSharpness(Value);
						})
					]
				]
				+ SWrapBox::Slot().Padding(8.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("Frequency", "Frequency Intent"))
				]
				+ SWrapBox::Slot().Padding(3.0f).FillLineWhenSizeLessThan(115.0f)
				[
					SNew(SBox).WidthOverride(90.0f)
					[
						SNew(SNumericEntryBox<float>)
						.MinValue(0.0f).MaxValue(1.0f)
						.Value_Lambda([this]() -> TOptional<float>
						{
							const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
							const int32 Index = Model->GetPrimaryEventIndex();
							return Asset && Asset->SourcePattern.Events.IsValidIndex(Index)
								? TOptional<float>(Asset->SourcePattern.Events[Index].FrequencyIntent)
								: TOptional<float>();
						})
						.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type)
						{
							Model->SetSelectedEventFrequencyIntent(Value);
						})
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 2.0f)
		[
			SNew(SBorder)
			.Visibility(this, &SOpenMobileHapticTimelineEditor::GetMarkerInspectorVisibility)
			.Padding(6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("MarkerName", "Marker"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([this]
					{
						const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
						const int32 Index = Model->GetPrimaryMarkerIndex();
						return Asset && Asset->Markers.IsValidIndex(Index)
							? FText::FromName(Asset->Markers[Index].Name) : FText();
					})
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
					{
						Model->SetSelectedMarkerName(
							FName(*Text.ToString().TrimStartAndEnd())
						);
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 3.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("MarkerTime", "Time"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f)
				[
					SNew(SBox).WidthOverride(100.0f)
					[
						SNew(SNumericEntryBox<double>)
						.MinValue(0.0)
						.Value_Lambda([this]() -> TOptional<double>
						{
							const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
							const int32 Index = Model->GetPrimaryMarkerIndex();
							return Asset && Asset->Markers.IsValidIndex(Index)
								? TOptional<double>(Asset->Markers[Index].TimeSeconds)
								: TOptional<double>();
						})
						.OnValueCommitted_Lambda([this](double Value, ETextCommit::Type)
						{
							Model->SetSelectedMarkerTime(Value);
						})
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 3.0f, 8.0f, 7.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this] { return Model->GetStatusText(); })
			.ColorAndOpacity(this, &SOpenMobileHapticTimelineEditor::GetStatusColor)
		]
	];
}

FText SOpenMobileHapticTimelineEditor::GetFallbackPolicyText() const
{
	const UOpenMobileHapticPatternAsset* Asset = Model->GetAsset();
	if (!Asset)
	{
		return LOCTEXT("FallbackUnavailable", "Fallback: unavailable");
	}
	const FText Policy = [&]()
	{
		switch (Asset->FallbackPolicy)
		{
		case EOpenMobileHapticFallbackPolicy::Automatic:
			return LOCTEXT("Automatic", "Automatic");
		case EOpenMobileHapticFallbackPolicy::NoBasicVibration:
			return LOCTEXT("NoBasic", "No Basic Vibration");
		case EOpenMobileHapticFallbackPolicy::ExactOnly:
			return LOCTEXT("ExactOnly", "Exact Only");
		case EOpenMobileHapticFallbackPolicy::NoEffectAllowed:
			return LOCTEXT("NoEffect", "No Effect Allowed");
		default:
			return LOCTEXT("UnknownFallback", "Unknown");
		}
	}();
	return FText::Format(LOCTEXT("FallbackFormat", "Fallback: {0}"), Policy);
}

FText SOpenMobileHapticTimelineEditor::GetPreviewPlatformText() const
{
	return PreviewPlatform == 0
		? LOCTEXT("PreviewAndroid", "Preview: Android")
		: LOCTEXT("PreviewIOS", "Preview: iOS");
}

FText SOpenMobileHapticTimelineEditor::GetCapabilityTierText() const
{
	switch (CapabilityTier)
	{
	case 0:
		return LOCTEXT("TierRich", "Tier: Rich");
	case 1:
		return LOCTEXT("TierStandard", "Tier: Standard");
	case 2:
		return LOCTEXT("TierBasic", "Tier: Basic");
	default:
		return LOCTEXT("TierUnavailable", "Tier: Unavailable");
	}
}

FText SOpenMobileHapticTimelineEditor::GetPreviewText() const
{
	const FOpenMobileHapticEditorPreview Preview = Model->ResolvePreview(
		static_cast<EOpenMobileHapticEditorPreviewPlatform>(PreviewPlatform),
		static_cast<EOpenMobileHapticEditorCapabilityTier>(CapabilityTier)
	);
	FString Text = FString::Printf(
		TEXT("Resolved path: %s"),
		*Preview.ResolvedPath.ToString()
	);
	for (const FText& Warning : Preview.Warnings)
	{
		Text += TEXT("  |  ");
		Text += Warning.ToString();
	}
	return FText::FromString(Text);
}

FSlateColor SOpenMobileHapticTimelineEditor::GetStatusColor() const
{
	return Model->IsStatusError()
		? FSlateColor(FLinearColor(0.95f, 0.24f, 0.20f))
		: FSlateColor(FLinearColor(0.28f, 0.78f, 0.42f));
}

EVisibility
SOpenMobileHapticTimelineEditor::GetEventInspectorVisibility() const
{
	return Model->GetSelectedEvents().IsEmpty()
		? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility
SOpenMobileHapticTimelineEditor::GetMarkerInspectorVisibility() const
{
	return Model->GetPrimaryMarkerIndex() == INDEX_NONE
		? EVisibility::Collapsed : EVisibility::Visible;
}

void SOpenMobileHapticTimelineEditor::HandleTransportChanged()
{
	Invalidate(EInvalidateWidgetReason::PaintAndVolatility);
}

#undef LOCTEXT_NAMESPACE
