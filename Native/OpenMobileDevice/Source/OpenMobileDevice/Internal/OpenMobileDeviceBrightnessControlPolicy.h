#pragma once

#include "OpenMobileDeviceBrightnessControl.h"

class FOpenMobileDeviceBrightnessControlPolicy final
{
public:
	static bool Validate(
		const FOpenMobileBrightnessRequest& Request,
		FOpenMobileError& OutError
	);
};

class FOpenMobileDeviceBrightnessRequestStack final
{
public:
	void Add(uint64 RequestId, const FOpenMobileBrightnessRequest& Request);
	void Remove(uint64 RequestId);
	TOptional<FOpenMobileBrightnessRequest> GetEffectiveRequest() const;
	bool IsEmpty() const { return Requests.IsEmpty(); }
	void Reset() { Requests.Reset(); }

private:
	struct FEntry
	{
		uint64 RequestId = 0;
		FOpenMobileBrightnessRequest Request;
	};

	TArray<FEntry> Requests;
};
