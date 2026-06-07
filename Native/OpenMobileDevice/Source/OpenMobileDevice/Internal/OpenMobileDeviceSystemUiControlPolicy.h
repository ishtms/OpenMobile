#pragma once

#include "OpenMobileDeviceSystemUiControl.h"

class FOpenMobileDeviceSystemUiControlPolicy final
{
public:
	static bool Validate(
		const FOpenMobileSystemUiRequest& Request,
		FOpenMobileError& OutError
	);
};

class FOpenMobileDeviceSystemUiRequestStack final
{
public:
	void Add(uint64 RequestId, const FOpenMobileSystemUiRequest& Request);
	void Remove(uint64 RequestId);
	TOptional<FOpenMobileSystemUiRequest> GetEffectiveRequest() const;
	bool IsEmpty() const { return Requests.IsEmpty(); }
	void Reset() { Requests.Reset(); }

private:
	struct FEntry
	{
		uint64 RequestId = 0;
		FOpenMobileSystemUiRequest Request;
	};

	TArray<FEntry> Requests;
};
