#pragma once

#include <Windows.h>

#include "console.h"

enum class eExeVersion
{
	DX9,
	DX10,
	UNKNOWN
};

extern eExeVersion g_version;
extern char dllPath[MAX_PATH];

uintptr_t ResolveAddr(uintptr_t dx9, uintptr_t dx10);