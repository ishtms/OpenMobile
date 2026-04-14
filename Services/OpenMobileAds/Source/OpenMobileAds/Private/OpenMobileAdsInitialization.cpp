#include "OpenMobileAdsInitialization.h"

const FOpenMobileAdsInitializationComponentStatus*
FOpenMobileAdsInitializationStatusSnapshot::FindComponent(
	EOpenMobileAdsInitializationComponentType Type,
	FName Name,
	FName Parent
) const
{
	return Components.FindByPredicate(
		[Type, Name, Parent](const FOpenMobileAdsInitializationComponentStatus& Component)
		{
			return Component.Type == Type
				&& Component.Name == Name
				&& (Parent.IsNone() || Component.Parent == Parent);
		}
	);
}
