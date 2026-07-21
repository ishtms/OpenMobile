#include "AssetDefinition_OpenMobileHapticPattern.h"

#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPatternEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_OpenMobileHapticPattern"

FText UAssetDefinition_OpenMobileHapticPattern::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "OpenMobile Haptic Pattern");
}

FLinearColor UAssetDefinition_OpenMobileHapticPattern::GetAssetColor() const
{
	return FLinearColor(0.16f, 0.64f, 0.88f);
}

TSoftClassPtr<UObject>
UAssetDefinition_OpenMobileHapticPattern::GetAssetClass() const
{
	return UOpenMobileHapticPatternAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath>
UAssetDefinition_OpenMobileHapticPattern::GetAssetCategories() const
{
	static const TArray<FAssetCategoryPath> Categories = {
		EAssetCategoryPaths::Misc
	};
	return Categories;
}

EAssetCommandResult UAssetDefinition_OpenMobileHapticPattern::OpenAssets(
	const FAssetOpenArgs& OpenArgs
) const
{
	const EToolkitMode::Type Mode = OpenArgs.ToolkitHost.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;
	for (UOpenMobileHapticPatternAsset* Asset :
		OpenArgs.LoadObjects<UOpenMobileHapticPatternAsset>())
	{
		TSharedRef<FOpenMobileHapticPatternEditorToolkit> Editor =
			MakeShared<FOpenMobileHapticPatternEditorToolkit>();
		Editor->Initialize(Asset, Mode, OpenArgs.ToolkitHost);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
