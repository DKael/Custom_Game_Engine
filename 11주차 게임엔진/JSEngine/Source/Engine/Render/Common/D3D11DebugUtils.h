#pragma once

#include <cstring>
#include <d3d11.h>

namespace D3D11Debug
{
	inline void SetDebugName(ID3D11DeviceChild* Resource, const char* Name)
	{
#if defined(_DEBUG)
		if (Resource && Name && Name[0] != '\0')
		{
			Resource->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen(Name)), Name);
		}
#else
		(void)Resource;
		(void)Name;
#endif
	}
}
