#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

enum class EOpenMobileAdFormat : uint8;
class IPropertyHandle;
class IPropertyUtilities;

struct FOpenMobileAdsPlacementFieldVisibility
{
	static bool IsVisible(EOpenMobileAdFormat Format, FName PropertyName);
};

class FOpenMobileAdsPlacementCustomization final :
	public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils
	) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils
	) override;

private:
	EVisibility GetPropertyVisibility(FName PropertyName) const;
	FText GetPlacementValidationText() const;
	FSlateColor GetPlacementValidationColor() const;
	void HandleFormatChanged();

	TSharedPtr<IPropertyHandle> PlacementProperty;
	TSharedPtr<IPropertyHandle> FormatProperty;
	TWeakPtr<IPropertyUtilities> PropertyUtilities;
};

class FOpenMobileAdsPlatformPlacementCustomization final :
	public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils
	) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils
	) override;

private:
	EVisibility GetPropertyVisibility(FName PropertyName) const;
	void HandleFormatChanged();

	TSharedPtr<IPropertyHandle> FormatProperty;
	TWeakPtr<IPropertyUtilities> PropertyUtilities;
};
