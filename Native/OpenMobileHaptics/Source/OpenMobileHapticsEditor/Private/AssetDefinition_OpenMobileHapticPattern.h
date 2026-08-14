#pragma once

#include "AssetDefinitionDefault.h"
#include "AssetDefinition_OpenMobileHapticPattern.generated.h"

UCLASS()
class UAssetDefinition_OpenMobileHapticPattern final
	: public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	/** Gives portable haptic patterns one stable name in content browser and asset menus. */
	virtual FText GetAssetDisplayName() const override;
	/** Keeps these assets visually identifiable without coupling their editor to a custom thumbnail renderer. */
	virtual FLinearColor GetAssetColor() const override;
	/** Associates this definition only with portable pattern assets, platform overrides keep their ordinary definitions. */
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	/** Places patterns in the OpenMobile asset category instead of Unreal's generic miscellaneous group. */
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	/** Opens every selected pattern in the dedicated timeline toolkit and reports partial failures back to Asset Tools. */
	virtual EAssetCommandResult OpenAssets(
		const FAssetOpenArgs& OpenArgs
	) const override;
};
