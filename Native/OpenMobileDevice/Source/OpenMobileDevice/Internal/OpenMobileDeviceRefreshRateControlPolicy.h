#pragma once

#include "OpenMobileDeviceRefreshRateControl.h"

class FOpenMobileDeviceRefreshRateControlPolicy final
{
public:
	static bool Validate(
		const FOpenMobilePreferredRefreshRateRequest& Request,
		FOpenMobileError& OutError
	);
};

class FOpenMobileDeviceRefreshRateRequestStack final
{
public:
	void Add(
		uint64 RequestId,
		const FOpenMobilePreferredRefreshRateRequest& Request
	);
	void Remove(uint64 RequestId);
	TOptional<FOpenMobilePreferredRefreshRateRequest> GetEffectiveRequest() const;
	bool IsEmpty() const { return Requests.IsEmpty(); }
	void Reset() { Requests.Reset(); }

private:
	struct FEntry
	{
		uint64 RequestId = 0;
		FOpenMobilePreferredRefreshRateRequest Request;
	};

	TArray<FEntry> Requests;
};
