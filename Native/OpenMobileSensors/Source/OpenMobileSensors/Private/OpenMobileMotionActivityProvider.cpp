#include "IOpenMobileMotionActivityProvider.h"

FName IOpenMobileMotionActivityProvider::GetModularFeatureName()
{
	static const FName Name(TEXT("OpenMobileMotionActivityProvider"));
	return Name;
}
