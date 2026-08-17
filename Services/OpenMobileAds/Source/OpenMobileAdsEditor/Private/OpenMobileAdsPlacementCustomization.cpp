#include "OpenMobileAdsPlacementCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "Interfaces/IProjectManager.h"
#include "IPropertyUtilities.h"
#include "OpenMobileAdsTypes.h"
#include "OpenMobileAdsConfiguration.h"
#include "ProjectDescriptor.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenMobileAdsPlacementCustomization"

namespace OpenMobileAdsPlacementCustomizationPrivate
{
	bool IsProjectPlatformEnabled(EOpenMobileAdsPlatform Platform)
	{
		const FProjectDescriptor* Project =
			IProjectManager::Get().GetCurrentProject();
		if (!Project || Project->TargetPlatforms.IsEmpty())
		{
			return true;
		}
		const FName Target = Platform == EOpenMobileAdsPlatform::Android
			? FName(TEXT("Android"))
			: FName(TEXT("IOS"));
		return Project->TargetPlatforms.Contains(Target);
	}

	TArray<EOpenMobileAdsPlatform> GetProjectMobilePlatforms()
	{
		TArray<EOpenMobileAdsPlatform> Platforms;
		if (IsProjectPlatformEnabled(EOpenMobileAdsPlatform::Android))
		{
			Platforms.Add(EOpenMobileAdsPlatform::Android);
		}
		if (IsProjectPlatformEnabled(EOpenMobileAdsPlatform::IOS))
		{
			Platforms.Add(EOpenMobileAdsPlatform::IOS);
		}
		return Platforms;
	}
}

bool FOpenMobileAdsPlacementFieldVisibility::IsVisible(
	EOpenMobileAdFormat Format,
	FName PropertyName
)
{
	const bool bPersistent = Format == EOpenMobileAdFormat::Banner
		|| Format == EOpenMobileAdFormat::AnchoredAdaptiveBanner
		|| Format == EOpenMobileAdFormat::MediumRectangle;
	const bool bRewarded = Format == EOpenMobileAdFormat::Rewarded
		|| Format == EOpenMobileAdFormat::RewardedInterstitial;
	if (PropertyName == TEXT("AppOpenPolicy")
		|| PropertyName == TEXT("bOverrideAppOpenPolicy"))
	{
		return Format == EOpenMobileAdFormat::AppOpen;
	}
	if (PropertyName == TEXT("BannerLayout")
		|| PropertyName == TEXT("bOverrideBannerLayout")
		|| PropertyName == TEXT("HideCachePolicy")
		|| PropertyName == TEXT("RefreshIntervalSeconds")
		|| PropertyName == TEXT("bOverrideRefreshInterval"))
	{
		return bPersistent;
	}
	if (PropertyName == TEXT("ServerVerification")
		|| PropertyName == TEXT("bOverrideServerVerification")
		|| PropertyName == TEXT("FallbackRewardType")
		|| PropertyName == TEXT("FallbackRewardAmount"))
	{
		return bRewarded;
	}
	if (PropertyName == TEXT("CooldownSeconds")
		|| PropertyName == TEXT("bOverrideCooldown"))
	{
		return !bPersistent;
	}
	return true;
}

TSharedRef<IPropertyTypeCustomization>
FOpenMobileAdsPlacementCustomization::MakeInstance()
{
	return MakeShared<FOpenMobileAdsPlacementCustomization>();
}

void FOpenMobileAdsPlacementCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils
)
{
	PlacementProperty = StructPropertyHandle;
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		StructPropertyHandle->CreatePropertyValueWidget()
	];
}

void FOpenMobileAdsPlacementCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils
)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	FormatProperty = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FOpenMobileAdsPlacementSettings, Format)
	);
	if (FormatProperty.IsValid())
	{
		FormatProperty->SetOnPropertyValueChanged(
			FSimpleDelegate::CreateSP(
				this,
				&FOpenMobileAdsPlacementCustomization::HandleFormatChanged
			)
		);
	}

	uint32 ChildCount = 0;
	StructPropertyHandle->GetNumChildren(ChildCount);
	for (uint32 Index = 0; Index < ChildCount; ++Index)
	{
		TSharedPtr<IPropertyHandle> Child =
			StructPropertyHandle->GetChildHandle(Index);
		if (!Child.IsValid())
		{
			continue;
		}
		const FName PropertyName = Child->GetProperty()->GetFName();
		ChildBuilder.AddProperty(Child.ToSharedRef())
			.Visibility(TAttribute<EVisibility>::CreateSP(
				this,
				&FOpenMobileAdsPlacementCustomization::GetPropertyVisibility,
				PropertyName
			));
	}
	ChildBuilder.AddCustomRow(LOCTEXT("PlacementValidationSearch", "Placement Validation"))
		.WholeRowContent()
		[
			SNew(STextBlock)
				.Text(
					this,
					&FOpenMobileAdsPlacementCustomization::GetPlacementValidationText
				)
				.ColorAndOpacity(
					this,
					&FOpenMobileAdsPlacementCustomization::GetPlacementValidationColor
				)
				.AutoWrapText(true)
		];
}

FText FOpenMobileAdsPlacementCustomization::GetPlacementValidationText() const
{
	if (!PlacementProperty.IsValid())
	{
		return FText::GetEmpty();
	}
	TArray<void*> RawData;
	PlacementProperty->AccessRawData(RawData);
	if (RawData.Num() != 1 || !RawData[0])
	{
		return LOCTEXT(
			"PlacementValidationUnavailable",
			"Placement validation is unavailable for multiple selections."
		);
	}
	const FOpenMobileAdsPlacementSettings* Placement =
		static_cast<const FOpenMobileAdsPlacementSettings*>(RawData[0]);
	const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
	const bool bRequireAdUnitIds = !(Settings->IsDevelopmentTestModeEnabled()
		&& Settings->bUseOfficialTestAdUnitIds);
	const TArray<FOpenMobileAdsConfigurationIssue> Issues =
		FOpenMobileAdsConfigurationValidator::ValidateForPlatforms(
			{*Placement},
			OpenMobileAdsPlacementCustomizationPrivate::GetProjectMobilePlatforms(),
			bRequireAdUnitIds
		);
	if (Issues.IsEmpty())
	{
		return LOCTEXT(
			"PlacementValidationPassed",
			"Placement configuration is valid for supported target platforms."
		);
	}
	FString Message;
	for (const FOpenMobileAdsConfigurationIssue& Issue : Issues)
	{
		if (!Message.IsEmpty())
		{
			Message.AppendChar(TEXT('\n'));
		}
		Message.Append(Issue.Message);
	}
	return FText::FromString(MoveTemp(Message));
}

FSlateColor FOpenMobileAdsPlacementCustomization::GetPlacementValidationColor() const
{
	return GetPlacementValidationText().EqualTo(LOCTEXT(
		"PlacementValidationPassed",
		"Placement configuration is valid for supported target platforms."
	))
		? FLinearColor(0.2f, 0.75f, 0.35f)
		: FLinearColor(0.9f, 0.2f, 0.2f);
}

EVisibility FOpenMobileAdsPlacementCustomization::GetPropertyVisibility(
	FName PropertyName
) const
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(
		FOpenMobileAdsPlacementSettings,
		Android
	))
	{
		return OpenMobileAdsPlacementCustomizationPrivate::IsProjectPlatformEnabled(
			EOpenMobileAdsPlatform::Android
		) ? EVisibility::Visible : EVisibility::Collapsed;
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(
		FOpenMobileAdsPlacementSettings,
		IOS
	))
	{
		return OpenMobileAdsPlacementCustomizationPrivate::IsProjectPlatformEnabled(
			EOpenMobileAdsPlatform::IOS
		) ? EVisibility::Visible : EVisibility::Collapsed;
	}
	uint8 FormatValue = static_cast<uint8>(EOpenMobileAdFormat::Rewarded);
	if (!FormatProperty.IsValid()
		|| FormatProperty->GetValue(FormatValue) != FPropertyAccess::Success)
	{
		return EVisibility::Visible;
	}
	return FOpenMobileAdsPlacementFieldVisibility::IsVisible(
		static_cast<EOpenMobileAdFormat>(FormatValue),
		PropertyName
	) ? EVisibility::Visible : EVisibility::Collapsed;
}

void FOpenMobileAdsPlacementCustomization::HandleFormatChanged()
{
	if (TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
	{
		Utilities->ForceRefresh();
	}
}

TSharedRef<IPropertyTypeCustomization>
FOpenMobileAdsPlatformPlacementCustomization::MakeInstance()
{
	return MakeShared<FOpenMobileAdsPlatformPlacementCustomization>();
}

void FOpenMobileAdsPlatformPlacementCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils
)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		StructPropertyHandle->CreatePropertyValueWidget()
	];
}

void FOpenMobileAdsPlatformPlacementCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils
)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	if (TSharedPtr<IPropertyHandle> Parent = StructPropertyHandle->GetParentHandle())
	{
		FormatProperty = Parent->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FOpenMobileAdsPlacementSettings, Format)
		);
	}
	if (FormatProperty.IsValid())
	{
		FormatProperty->SetOnPropertyValueChanged(
			FSimpleDelegate::CreateSP(
				this,
				&FOpenMobileAdsPlatformPlacementCustomization::HandleFormatChanged
			)
		);
	}
	uint32 ChildCount = 0;
	StructPropertyHandle->GetNumChildren(ChildCount);
	for (uint32 Index = 0; Index < ChildCount; ++Index)
	{
		TSharedPtr<IPropertyHandle> Child =
			StructPropertyHandle->GetChildHandle(Index);
		if (!Child.IsValid())
		{
			continue;
		}
		const FName PropertyName = Child->GetProperty()->GetFName();
		ChildBuilder.AddProperty(Child.ToSharedRef())
			.Visibility(TAttribute<EVisibility>::CreateSP(
				this,
				&FOpenMobileAdsPlatformPlacementCustomization::GetPropertyVisibility,
				PropertyName
			));
	}
}

EVisibility FOpenMobileAdsPlatformPlacementCustomization::GetPropertyVisibility(
	FName PropertyName
) const
{
	uint8 FormatValue = static_cast<uint8>(EOpenMobileAdFormat::Rewarded);
	if (!FormatProperty.IsValid()
		|| FormatProperty->GetValue(FormatValue) != FPropertyAccess::Success)
	{
		return EVisibility::Visible;
	}
	return FOpenMobileAdsPlacementFieldVisibility::IsVisible(
		static_cast<EOpenMobileAdFormat>(FormatValue),
		PropertyName
	) ? EVisibility::Visible : EVisibility::Collapsed;
}

void FOpenMobileAdsPlatformPlacementCustomization::HandleFormatChanged()
{
	if (TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
	{
		Utilities->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
