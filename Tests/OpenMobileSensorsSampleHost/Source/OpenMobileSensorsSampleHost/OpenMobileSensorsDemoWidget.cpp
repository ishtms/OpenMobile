#include "OpenMobileSensorsDemoWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "OpenMobileSensorsBlueprintExamples.h"

namespace OpenMobileSensorsDemoPrivate
{
	const FLinearColor MutedColor(0.72f, 0.77f, 0.85f, 1.0f);
	const FLinearColor SuccessColor(0.42f, 0.94f, 0.67f, 1.0f);
	const FLinearColor WarningColor(1.0f, 0.70f, 0.32f, 1.0f);
	const FLinearColor ErrorColor(1.0f, 0.42f, 0.42f, 1.0f);

	void SetFontSize(UTextBlock* TextBlock, int32 Size)
	{
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = Size;
		TextBlock->SetFont(Font);
	}

	void AddWithPadding(
		UVerticalBox* Parent,
		UWidget* Child,
		const FMargin& Padding
	)
	{
		UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
		Slot->SetPadding(Padding);
		Slot->SetHorizontalAlignment(HAlign_Fill);
	}

	UButton* AddButton(
		UWidgetTree* WidgetTree,
		UUniformGridPanel* Grid,
		const FName Name,
		const FString& Label,
		int32 Row,
		int32 Column
	)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			Name
		);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*(Name.ToString() + TEXT("Label")))
		);
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		SetFontSize(Text, 14);
		Button->SetContent(Text);
		UUniformGridSlot* Slot = Grid->AddChildToUniformGrid(
			Button,
			Row,
			Column
		);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
		return Button;
	}

	template <typename EnumType>
	FString EnumName(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum
			? Enum->GetNameStringByValue(static_cast<int64>(Value))
			: TEXT("Unknown");
	}

	FString OperationFailure(
		EOpenMobileSensorType SensorType,
		const FOpenMobileSensorOperationResult& Operation
	)
	{
		return FString::Printf(
			TEXT("%s: %s. %s"),
			*FOpenMobileSensorTypes::GetStableName(SensorType).ToString(),
			*Operation.Error.Message,
			*Operation.Failure.Correction
		);
	}
}

TSharedRef<SWidget> UOpenMobileSensorsDemoWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UOpenMobileSensorsDemoWidget::BuildWidgetTree()
{
	using namespace OpenMobileSensorsDemoPrivate;
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("SensorsBackground")
	);
	Background->SetBrushColor(FLinearColor(0.018f, 0.024f, 0.035f, 1.0f));
	Background->SetPadding(FMargin(20.0f));
	WidgetTree->RootWidget = Background;

	UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("SensorsScrollBox")
	);
	Background->SetContent(ScrollBox);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("SensorsColumn")
	);
	ScrollBox->AddChild(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SensorsTitle")
	);
	Title->SetText(FText::FromString(TEXT("OpenMobile Sensors")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.37f, 0.86f, 1.0f, 1.0f)));
	SetFontSize(Title, 28);
	AddWithPadding(Column, Title, FMargin(0.0f, 0.0f, 0.0f, 4.0f));

	UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SensorsSubtitle")
	);
	Subtitle->SetText(FText::FromString(TEXT(
		"Low-rate discovery, controls, gameplay recipes, diagnostics, and device validation"
	)));
	Subtitle->SetColorAndOpacity(FSlateColor(MutedColor));
	Subtitle->SetAutoWrapText(true);
	SetFontSize(Subtitle, 15);
	AddWithPadding(Column, Subtitle, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	UUniformGridPanel* Controls = WidgetTree->ConstructWidget<UUniformGridPanel>(
		UUniformGridPanel::StaticClass(),
		TEXT("SensorControls")
	);
	Controls->SetMinDesiredSlotWidth(150.0f);
	Controls->SetMinDesiredSlotHeight(42.0f);
	RefreshButton = AddButton(WidgetTree, Controls, TEXT("RefreshButton"), TEXT("Refresh"), 0, 0);
	StartButton = AddButton(WidgetTree, Controls, TEXT("StartButton"), TEXT("Start examples"), 0, 1);
	StopButton = AddButton(WidgetTree, Controls, TEXT("StopButton"), TEXT("Stop all"), 0, 2);
	DrainButton = AddButton(WidgetTree, Controls, TEXT("DrainButton"), TEXT("Drain buffer"), 1, 0);
	RecenterButton = AddButton(WidgetTree, Controls, TEXT("RecenterButton"), TEXT("Recenter attitude"), 1, 1);
	ScreenRotationButton = AddButton(WidgetTree, Controls, TEXT("ScreenRotationButton"), TEXT("Rotate screen basis"), 1, 2);
	ResetStepsButton = AddButton(WidgetTree, Controls, TEXT("ResetStepsButton"), TEXT("Reset steps"), 2, 0);
	PermissionButton = AddButton(WidgetTree, Controls, TEXT("PermissionButton"), TEXT("Motion permission"), 2, 1);
	RecordingButton = AddButton(WidgetTree, Controls, TEXT("RecordingButton"), TEXT("Record or stop"), 2, 2);
	ReplayButton = AddButton(WidgetTree, Controls, TEXT("ReplayButton"), TEXT("Replay or cancel"), 3, 0);
	AddWithPadding(Column, Controls, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SensorStatus")
	);
	StatusText->SetAutoWrapText(true);
	StatusText->SetColorAndOpacity(FSlateColor(MutedColor));
	SetFontSize(StatusText, 14);
	AddWithPadding(Column, StatusText, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	CapabilityText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SensorCapabilities")
	);
	CapabilityText->SetAutoWrapText(true);
	CapabilityText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SetFontSize(CapabilityText, 13);
	AddWithPadding(Column, CapabilityText, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	LiveText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SensorLiveValues")
	);
	LiveText->SetAutoWrapText(true);
	LiveText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SetFontSize(LiveText, 13);
	AddWithPadding(Column, LiveText, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	DiagnosticsText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SensorDiagnostics")
	);
	DiagnosticsText->SetAutoWrapText(true);
	DiagnosticsText->SetColorAndOpacity(FSlateColor(MutedColor));
	SetFontSize(DiagnosticsText, 12);
	AddWithPadding(Column, DiagnosticsText, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	UTextBlock* AxisText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SensorAxes")
	);
	AxisText->SetText(UOpenMobileSensorsBlueprintExamples::GetAxisConvention());
	AxisText->SetAutoWrapText(true);
	AxisText->SetColorAndOpacity(FSlateColor(MutedColor));
	SetFontSize(AxisText, 12);
	AddWithPadding(Column, AxisText, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	UTextBlock* Recipes = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SensorBlueprintRecipes")
	);
	Recipes->SetText(UOpenMobileSensorsBlueprintExamples::GetBlueprintRecipes());
	Recipes->SetAutoWrapText(true);
	Recipes->SetColorAndOpacity(FSlateColor(MutedColor));
	SetFontSize(Recipes, 12);
	AddWithPadding(Column, Recipes, FMargin(0.0f, 0.0f, 0.0f, 20.0f));
}

void UOpenMobileSensorsDemoWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandleRefreshClicked);
	StartButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandleStartClicked);
	StopButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandleStopClicked);
	DrainButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandleDrainClicked);
	RecenterButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandleRecenterClicked);
	ScreenRotationButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandleScreenRotationClicked);
	ResetStepsButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandleResetStepsClicked);
	PermissionButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandlePermissionClicked);
	RecordingButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandleRecordingClicked);
	ReplayButton->OnClicked.AddUniqueDynamic(this, &UOpenMobileSensorsDemoWidget::HandleReplayClicked);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		SensorsSubsystem = GameInstance->GetSubsystem<UOpenMobileSensorsSubsystem>();
	}
	if (!SensorsSubsystem)
	{
		SetStatus(TEXT("Sensors subsystem unavailable."), OpenMobileSensorsDemoPrivate::ErrorColor);
		return;
	}
	SetStatus(TEXT("Ready. Start examples uses safe 15 Hz requests."), OpenMobileSensorsDemoPrivate::MutedColor);
	RefreshAll();
	NextRefreshSeconds = FPlatformTime::Seconds() + 0.1;
}

void UOpenMobileSensorsDemoWidget::NativeDestruct()
{
	if (SensorsSubsystem)
	{
		if (PermissionRequest.IsValid())
		{
			SensorsSubsystem->CancelPermissionRequestNative(PermissionRequest);
		}
		if (RecordingIdentifier.IsValid())
		{
			SensorsSubsystem->CancelRecordingNative(RecordingIdentifier);
		}
		if (ReplayIdentifier.IsValid())
		{
			SensorsSubsystem->CancelReplayNative(ReplayIdentifier);
		}
		SensorsSubsystem->StopAllSubscriptionsNative();
	}
	Handles.Reset();
	LastSequences.Reset();
	SensorsSubsystem = nullptr;
	Super::NativeDestruct();
}

void UOpenMobileSensorsDemoWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime
)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const double NowSeconds = FPlatformTime::Seconds();
	if (SensorsSubsystem && NowSeconds >= NextRefreshSeconds)
	{
		RefreshLiveSamples();
		RefreshDiagnostics();
		NextRefreshSeconds = NowSeconds + 0.1;
	}
}

void UOpenMobileSensorsDemoWidget::RefreshAll()
{
	RefreshCapabilities();
	RefreshLiveSamples();
	RefreshDiagnostics();
}

void UOpenMobileSensorsDemoWidget::RefreshCapabilities()
{
	using namespace OpenMobileSensorsDemoPrivate;
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		SensorsSubsystem->GetCapabilitySnapshotNative();
	const TArray<FOpenMobileSensorMetadata> Metadata =
		SensorsSubsystem->GetMetadataNative();
	FString Text = FString::Printf(
		TEXT("CAPABILITIES | backend %s | %d sensors | %d metadata rows\n"),
		*Snapshot.BackendName.ToString(),
		Snapshot.Sensors.Num(),
		Metadata.Num()
	);
	for (const FOpenMobileSensorCapability& Capability : Snapshot.Sensors)
	{
		Text += FString::Printf(
			TEXT("%s: %s | %s\n"),
			*FOpenMobileSensorTypes::GetStableName(Capability.Sensor.Type).ToString(),
			*EnumName(Capability.Availability.State),
			*EnumName(Capability.Source)
		);
	}
	const FOpenMobilePermissionResult Motion =
		SensorsSubsystem->GetPermissionStatusNative(
			EOpenMobileSensorPermission::MotionActivity
		);
	Text += FString::Printf(
		TEXT("Motion permission: %s"),
		*EnumName(Motion.Status)
	);
	CapabilityText->SetText(FText::FromString(Text));
}

void UOpenMobileSensorsDemoWidget::RefreshLiveSamples()
{
	using namespace OpenMobileSensorsDemoPrivate;
	FString Text(TEXT("LIVE GAMEPLAY EXAMPLES\n"));
	FOpenMobileSensorReadResult Read;

	if (const FOpenMobileSensorSubscriptionHandle* Handle =
		FindHandle(EOpenMobileSensorType::Accelerometer))
	{
		FOpenMobileVectorSensorSample Sample;
		if (SensorsSubsystem->GetLatestVectorSampleNative(
			*Handle,
			SequenceFor(EOpenMobileSensorType::Accelerometer),
			Read,
			Sample
		))
		{
			LastSequences[EOpenMobileSensorType::Accelerometer] = Sample.Header.Sequence;
			const FVector2D Tilt =
				UOpenMobileSensorsBlueprintExamples::ComputeTiltSteering(Sample.Value);
			Text += FString::Printf(
				TEXT("Tilt steering %.2f, %.2f | accel %s | %s\n"),
				Tilt.X,
				Tilt.Y,
				*Sample.Value.ToCompactString(),
				*UOpenMobileSensorsBlueprintExamples::DescribeSampleHeader(
					Sample.Header,
					Read.SampleAgeSeconds
				)
			);
		}
	}

	if (const FOpenMobileSensorSubscriptionHandle* Handle =
		FindHandle(EOpenMobileSensorType::Gyroscope))
	{
		FOpenMobileVectorSensorSample Sample;
		if (SensorsSubsystem->GetLatestVectorSampleNative(
			*Handle,
			SequenceFor(EOpenMobileSensorType::Gyroscope),
			Read,
			Sample
		))
		{
			LastSequences[EOpenMobileSensorType::Gyroscope] = Sample.Header.Sequence;
			const FVector2D Aim =
				UOpenMobileSensorsBlueprintExamples::ComputeGyroAimDelta(
					Sample.Value,
					0.1
				);
			Text += FString::Printf(
				TEXT("Gyro aim delta %.2f, %.2f deg | angular %s\n"),
				Aim.X,
				Aim.Y,
				*Sample.Value.ToCompactString()
			);
		}
	}

	if (const FOpenMobileSensorSubscriptionHandle* Handle =
		FindHandle(EOpenMobileSensorType::Attitude))
	{
		FOpenMobileAttitudeSensorSample Sample;
		if (SensorsSubsystem->GetLatestAttitudeSampleNative(
			*Handle,
			SequenceFor(EOpenMobileSensorType::Attitude),
			Read,
			Sample
		))
		{
			LastSequences[EOpenMobileSensorType::Attitude] = Sample.Header.Sequence;
			Text += FString::Printf(
				TEXT("Attitude %s | basis X %s Y %s Z %s | screen %s\n"),
				*Sample.Quaternion.ToString(),
				*Sample.RotationMatrix.XAxis.ToCompactString(),
				*Sample.RotationMatrix.YAxis.ToCompactString(),
				*Sample.RotationMatrix.ZAxis.ToCompactString(),
				*EnumName(Sample.Header.ScreenRotation)
			);
		}
	}

	if (const FOpenMobileSensorSubscriptionHandle* Handle =
		FindHandle(EOpenMobileSensorType::MagneticHeading))
	{
		FOpenMobileHeadingSensorSample Sample;
		if (SensorsSubsystem->GetLatestHeadingSampleNative(
			*Handle,
			SequenceFor(EOpenMobileSensorType::MagneticHeading),
			Read,
			Sample
		))
		{
			LastSequences[EOpenMobileSensorType::MagneticHeading] = Sample.Header.Sequence;
			Text += FString::Printf(
				TEXT("Compass %.1f deg | accuracy %s | age %.3fs\n"),
				Sample.HeadingDegrees,
				*EnumName(Sample.Header.Accuracy),
				Read.SampleAgeSeconds
			);
		}
	}

	if (const FOpenMobileSensorSubscriptionHandle* Handle =
		FindHandle(EOpenMobileSensorType::Shake))
	{
		FOpenMobileVectorSensorSample Sample;
		if (SensorsSubsystem->GetLatestVectorSampleNative(
			*Handle,
			SequenceFor(EOpenMobileSensorType::Shake),
			Read,
			Sample
		))
		{
			LastSequences[EOpenMobileSensorType::Shake] = Sample.Header.Sequence;
			Text += Sample.bHasShakeEvent
				? FString::Printf(
					TEXT("Shake %.2f m/s^2 | %d impulses\n"),
					Sample.ShakeEvent.StrengthMetresPerSecondSquared,
					Sample.ShakeEvent.ImpulseCount
				)
				: TEXT("Shake: waiting for gesture\n");
		}
	}

	if (StepSessionHandle.IsValid())
	{
		FOpenMobileStepsSensorSample Sample;
		if (SensorsSubsystem->ReadStepCountSessionNative(
			StepSessionHandle,
			SequenceFor(EOpenMobileSensorType::StepCounter),
			Read,
			Sample
		))
		{
			LastSequences[EOpenMobileSensorType::StepCounter] = Sample.Header.Sequence;
			Text += FString::Printf(TEXT("Step session %lld | origin %s\n"),
				Sample.Count,
				*EnumName(Sample.Origin));
		}
	}

	for (const EOpenMobileSensorType ActivityType : {
		EOpenMobileSensorType::MotionActivity,
		EOpenMobileSensorType::ActivityTransition
	})
	{
		if (const FOpenMobileSensorSubscriptionHandle* Handle =
			FindHandle(ActivityType))
		{
			FOpenMobileActivitySensorSample Sample;
			if (SensorsSubsystem->GetLatestActivitySampleNative(
				*Handle,
				SequenceFor(ActivityType),
				Read,
				Sample
			))
			{
				LastSequences[ActivityType] = Sample.Header.Sequence;
				if (Sample.Transition != EOpenMobileActivityTransition::None
					|| Sample.Activity != PreviousActivity)
				{
					LastActivityTransition = FString::Printf(
						TEXT("%s %s (%s)"),
						*EnumName(Sample.Activity),
						*EnumName(Sample.Transition),
						*EnumName(Sample.Confidence)
					);
					PreviousActivity = Sample.Activity;
				}
			}
		}
	}
	if (!LastActivityTransition.IsEmpty())
	{
		Text += TEXT("Activity: ") + LastActivityTransition + TEXT("\n");
	}

	if (const FOpenMobileSensorSubscriptionHandle* Handle =
		FindHandle(EOpenMobileSensorType::BarometricPressure))
	{
		FOpenMobileScalarSensorSample Sample;
		if (SensorsSubsystem->GetLatestScalarSampleNative(
			*Handle,
			SequenceFor(EOpenMobileSensorType::BarometricPressure),
			Read,
			Sample
		))
		{
			LastSequences[EOpenMobileSensorType::BarometricPressure] = Sample.Header.Sequence;
			Text += FString::Printf(TEXT("Pressure %.2f hPa | age %.3fs\n"),
				Sample.Value,
				Read.SampleAgeSeconds);
		}
	}

	if (RelativeAltitudeHandle.IsValid())
	{
		FOpenMobileScalarSensorSample Sample;
		if (SensorsSubsystem->ReadRelativeAltitudeSessionNative(
			RelativeAltitudeHandle,
			SequenceFor(EOpenMobileSensorType::RelativeAltitude),
			Read,
			Sample
		))
		{
			LastSequences[EOpenMobileSensorType::RelativeAltitude] = Sample.Header.Sequence;
			Text += FString::Printf(TEXT("Relative altitude %.2f m\n"), Sample.Value);
		}
	}
	LiveText->SetText(FText::FromString(Text));
}

void UOpenMobileSensorsDemoWidget::RefreshDiagnostics()
{
	using namespace OpenMobileSensorsDemoPrivate;
	const FOpenMobileSensorDiagnosticsSnapshot Snapshot =
		SensorsSubsystem->GetDiagnosticsSnapshotNative();
	FString Text = FString::Printf(
		TEXT("DIAGNOSTICS | %d logical | %d physical | recordings %d | replay %d\n"),
		Snapshot.Streams.Num(),
		Snapshot.PhysicalStreams.Num(),
		Snapshot.ActiveRecordingCount,
		Snapshot.ActiveReplayCount
	);
	for (const FOpenMobileSensorStreamDiagnostics& Stream : Snapshot.Streams)
	{
		Text += FString::Printf(
			TEXT("%s | requested %.1f Hz | applied %.1f Hz | measured %.1f Hz | age %.3fs | dropped %lld | source 0x%X | quality %s\n"),
			*FOpenMobileSensorTypes::GetStableName(Stream.Subscription.Sensor.Type).ToString(),
			Stream.Rate.RequestedFrequencyHz,
			Stream.Rate.AppliedFrequencyHz,
			Stream.Rate.MeanFrequencyHz,
			Stream.LatestSampleAgeSeconds,
			Stream.DroppedSamples,
			Stream.SourceFlags,
			Stream.bHasAccuracy
				? *EnumName(Stream.Accuracy.Accuracy)
				: TEXT("Unknown")
		);
	}
	for (const FOpenMobileSensorErrorReport& Error : Snapshot.RecentErrorReports)
	{
		Text += FString::Printf(
			TEXT("Error %s: %s | correction: %s\n"),
			*FOpenMobileSensorTypes::GetStableName(Error.Context.Sensor.Type).ToString(),
			*Error.LikelyCause.ToString(),
			*Error.Correction.ToString()
		);
	}
	DiagnosticsText->SetText(FText::FromString(Text));
}

void UOpenMobileSensorsDemoWidget::StartSensor(
	EOpenMobileSensorType SensorType,
	EOpenMobileSensorDeliveryMode DeliveryMode,
	EOpenMobileSensorCoordinateSpace CoordinateSpace
)
{
	if (Handles.Contains(SensorType))
	{
		return;
	}
	const FOpenMobileSensorSubscriptionRequest Request =
		UOpenMobileSensorsBlueprintExamples::MakeLowRateRequest(
			SensorType,
			DeliveryMode,
			CoordinateSpace
		);
	const FOpenMobileSensorSubscriptionResult Result =
		SensorsSubsystem->StartSubscriptionNative(Request);
	if (Result.Operation.IsSuccess() && Result.Handle.IsValid())
	{
		Handles.Add(SensorType, Result.Handle);
	}
	else
	{
		SetStatus(
			OpenMobileSensorsDemoPrivate::OperationFailure(
				SensorType,
				Result.Operation
			),
			OpenMobileSensorsDemoPrivate::WarningColor
		);
	}
}

const FOpenMobileSensorSubscriptionHandle*
UOpenMobileSensorsDemoWidget::FindHandle(
	EOpenMobileSensorType SensorType
) const
{
	return Handles.Find(SensorType);
}

int64& UOpenMobileSensorsDemoWidget::SequenceFor(
	EOpenMobileSensorType SensorType
)
{
	return LastSequences.FindOrAdd(SensorType);
}

void UOpenMobileSensorsDemoWidget::SetStatus(
	const FString& Message,
	const FLinearColor& Color
)
{
	StatusText->SetText(FText::FromString(Message));
	StatusText->SetColorAndOpacity(FSlateColor(Color));
}

void UOpenMobileSensorsDemoWidget::HandleRefreshClicked()
{
	RefreshAll();
	SetStatus(TEXT("Snapshots refreshed without starting hardware."), OpenMobileSensorsDemoPrivate::SuccessColor);
}

void UOpenMobileSensorsDemoWidget::HandleStartClicked()
{
	StartSensor(
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorDeliveryMode::Buffered,
		EOpenMobileSensorCoordinateSpace::CurrentScreen
	);
	StartSensor(EOpenMobileSensorType::Gyroscope,
		EOpenMobileSensorDeliveryMode::LatestValue,
		EOpenMobileSensorCoordinateSpace::CurrentScreen);
	StartSensor(EOpenMobileSensorType::Attitude,
		EOpenMobileSensorDeliveryMode::LatestValue,
		EOpenMobileSensorCoordinateSpace::CurrentScreen);
	StartSensor(EOpenMobileSensorType::MagneticHeading);
	StartSensor(EOpenMobileSensorType::Shake);
	StartSensor(EOpenMobileSensorType::MotionActivity);
	StartSensor(EOpenMobileSensorType::ActivityTransition);
	StartSensor(EOpenMobileSensorType::BarometricPressure);

	if (!StepSessionHandle.IsValid())
	{
		const FOpenMobileSensorSubscriptionResult Steps =
			SensorsSubsystem->BeginStepCountSessionNative(
				UOpenMobileSensorsBlueprintExamples::MakeLowRateRequest(
					EOpenMobileSensorType::StepCounter
				).Options
			);
		if (Steps.Operation.IsSuccess())
		{
			StepSessionHandle = Steps.Handle;
		}
	}
	if (!RelativeAltitudeHandle.IsValid())
	{
		const FOpenMobileSensorSubscriptionResult Altitude =
			SensorsSubsystem->BeginRelativeAltitudeSessionNative(
				UOpenMobileSensorsBlueprintExamples::MakeLowRateRequest(
					EOpenMobileSensorType::RelativeAltitude
				).Options
			);
		if (Altitude.Operation.IsSuccess())
		{
			RelativeAltitudeHandle = Altitude.Handle;
		}
	}
	SetStatus(
		FString::Printf(TEXT("Started %d low-rate example streams."), Handles.Num()),
		OpenMobileSensorsDemoPrivate::SuccessColor
	);
	RefreshAll();
}

void UOpenMobileSensorsDemoWidget::HandleStopClicked()
{
	const int32 Stopped = SensorsSubsystem->StopAllSubscriptionsNative();
	Handles.Reset();
	LastSequences.Reset();
	StepSessionHandle.Reset();
	RelativeAltitudeHandle.Reset();
	SetStatus(FString::Printf(TEXT("Stopped %d owned subscriptions."), Stopped),
		OpenMobileSensorsDemoPrivate::SuccessColor);
	RefreshAll();
}

void UOpenMobileSensorsDemoWidget::HandleDrainClicked()
{
	const FOpenMobileSensorSubscriptionHandle* Handle =
		FindHandle(EOpenMobileSensorType::Accelerometer);
	if (!Handle)
	{
		SetStatus(TEXT("Start the buffered accelerometer example first."), OpenMobileSensorsDemoPrivate::WarningColor);
		return;
	}
	FOpenMobileSensorBufferReadResult Result;
	FOpenMobileVectorSensorBatch Batch;
	if (SensorsSubsystem->GetBufferedVectorSamplesNative(
		*Handle,
		128,
		Result,
		Batch
	))
	{
		SetStatus(FString::Printf(
			TEXT("Drained %d samples. Dropped %lld, high-water %d."),
			Batch.Samples.Num(),
			Result.DroppedSamples,
			Result.BufferHighWaterMark
		), OpenMobileSensorsDemoPrivate::SuccessColor);
	}
}

void UOpenMobileSensorsDemoWidget::HandleRecenterClicked()
{
	const FOpenMobileSensorSubscriptionHandle* Handle =
		FindHandle(EOpenMobileSensorType::Attitude);
	if (!Handle)
	{
		SetStatus(TEXT("Start the attitude example first."), OpenMobileSensorsDemoPrivate::WarningColor);
		return;
	}
	const FOpenMobileSensorRecenterResult Result =
		SensorsSubsystem->RecenterSubscription(
			*Handle,
			EOpenMobileSensorRecenterMode::YawOnly
		);
	SetStatus(Result.Operation.IsSuccess()
		? TEXT("Attitude yaw recentered without restarting the shared stream.")
		: Result.Operation.Error.Message,
		Result.Operation.IsSuccess()
			? OpenMobileSensorsDemoPrivate::SuccessColor
			: OpenMobileSensorsDemoPrivate::WarningColor);
}

void UOpenMobileSensorsDemoWidget::HandleScreenRotationClicked()
{
	ScreenRotation = static_cast<EOpenMobileSensorScreenRotation>(
		(static_cast<uint8>(ScreenRotation) + 1) % 4
	);
	const bool bUpdated = SensorsSubsystem->UpdateApplicationWindowRotationNative(
		ScreenRotation,
		FPlatformTime::Seconds(),
		false
	);
	SetStatus(FString::Printf(TEXT("Screen basis %s: %s"),
		*OpenMobileSensorsDemoPrivate::EnumName(ScreenRotation),
		bUpdated ? TEXT("updated") : TEXT("rejected")),
		bUpdated
			? OpenMobileSensorsDemoPrivate::SuccessColor
			: OpenMobileSensorsDemoPrivate::WarningColor);
}

void UOpenMobileSensorsDemoWidget::HandleResetStepsClicked()
{
	if (!StepSessionHandle.IsValid())
	{
		SetStatus(TEXT("Start the step session first."), OpenMobileSensorsDemoPrivate::WarningColor);
		return;
	}
	const FOpenMobileSensorOperationResult Result =
		SensorsSubsystem->ResetStepCountSessionNative(StepSessionHandle);
	SetStatus(Result.IsSuccess() ? TEXT("Step session baseline reset.") : Result.Error.Message,
		Result.IsSuccess()
			? OpenMobileSensorsDemoPrivate::SuccessColor
			: OpenMobileSensorsDemoPrivate::WarningColor);
}

void UOpenMobileSensorsDemoWidget::HandlePermissionClicked()
{
	if (PermissionRequest.IsValid())
	{
		return;
	}
	PermissionRequest = SensorsSubsystem->RequestPermissionNative(
		EOpenMobileSensorPermission::MotionActivity,
		FOnOpenMobilePermissionRequestComplete::CreateWeakLambda(
			this,
			[this](const FOpenMobilePermissionResult& Result)
			{
				PermissionRequest.Reset();
				SetStatus(FString::Printf(TEXT("Motion permission: %s"),
					*OpenMobileSensorsDemoPrivate::EnumName(Result.Status)),
					Result.Status == EOpenMobilePermissionStatus::Granted
						? OpenMobileSensorsDemoPrivate::SuccessColor
						: OpenMobileSensorsDemoPrivate::WarningColor);
				RefreshCapabilities();
			}
		)
	);
}

void UOpenMobileSensorsDemoWidget::HandleRecordingClicked()
{
	if (RecordingIdentifier.IsValid())
	{
		SensorsSubsystem->StopRecordingNative(
			RecordingIdentifier,
			FOnOpenMobileSensorRecordingComplete::CreateWeakLambda(
				this,
				[this](const FOpenMobileSensorRecordingResult& Result)
				{
					RecordingIdentifier.Invalidate();
					LastRecordingPath = Result.Recording.FilePath;
					SetStatus(Result.Operation.IsSuccess()
						? FString::Printf(TEXT("Recording saved as %s"),
							*FPaths::GetCleanFilename(LastRecordingPath))
						: Result.Operation.Error.Message,
						Result.Operation.IsSuccess()
							? OpenMobileSensorsDemoPrivate::SuccessColor
							: OpenMobileSensorsDemoPrivate::WarningColor);
				}
			)
		);
		return;
	}

	FOpenMobileSensorRecordingOptions Options;
	Options.MaximumDurationSeconds = 60.0;
	Options.MaximumBytes = 8ll * 1024ll * 1024ll;
	Options.bIncludeSensitiveLocationContext = false;
	for (const EOpenMobileSensorType Type : {
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorType::Gyroscope,
		EOpenMobileSensorType::Attitude,
		EOpenMobileSensorType::MagneticHeading
	})
	{
		FOpenMobileSensorIdentifier& Sensor = Options.Sensors.AddDefaulted_GetRef();
		Sensor.Type = Type;
	}
	RecordingIdentifier = SensorsSubsystem->StartRecordingNative(
		Options,
		FOnOpenMobileSensorRecordingComplete::CreateWeakLambda(
			this,
			[this](const FOpenMobileSensorRecordingResult& Result)
			{
				if (!Result.Operation.IsSuccess())
				{
					RecordingIdentifier.Invalidate();
				}
				SetStatus(Result.Operation.IsSuccess()
					? TEXT("Recording started with sensitive location disabled.")
					: Result.Operation.Error.Message,
					Result.Operation.IsSuccess()
						? OpenMobileSensorsDemoPrivate::SuccessColor
						: OpenMobileSensorsDemoPrivate::WarningColor);
			}
		)
	);
}

void UOpenMobileSensorsDemoWidget::HandleReplayClicked()
{
	if (ReplayIdentifier.IsValid())
	{
		const FOpenMobileSensorOperationResult Result =
			SensorsSubsystem->CancelReplayNative(ReplayIdentifier);
		ReplayIdentifier.Invalidate();
		SetStatus(Result.IsSuccess() ? TEXT("Replay cancelled.") : Result.Error.Message,
			Result.IsSuccess()
				? OpenMobileSensorsDemoPrivate::SuccessColor
				: OpenMobileSensorsDemoPrivate::WarningColor);
		return;
	}
	if (LastRecordingPath.IsEmpty())
	{
		SetStatus(TEXT("Record and stop a session before replaying it."), OpenMobileSensorsDemoPrivate::WarningColor);
		return;
	}
	FOpenMobileSensorReplayOptions Options;
	Options.PlaybackSpeed = 1.0;
	Options.bLoop = true;
	ReplayIdentifier = SensorsSubsystem->ReplayRecordingNative(
		LastRecordingPath,
		Options,
		FOnOpenMobileSensorReplayComplete::CreateWeakLambda(
			this,
			[this](const FOpenMobileSensorReplayResult& Result)
			{
				if (!Result.Operation.IsSuccess())
				{
					ReplayIdentifier.Invalidate();
				}
				SetStatus(Result.Operation.IsSuccess()
					? TEXT("Replay started at 1x with looping enabled.")
					: Result.Operation.Error.Message,
					Result.Operation.IsSuccess()
						? OpenMobileSensorsDemoPrivate::SuccessColor
						: OpenMobileSensorsDemoPrivate::WarningColor);
			}
		)
	);
}
