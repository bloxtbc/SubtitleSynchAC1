#include "globals.h"

eExeVersion g_version = eExeVersion::UNKNOWN;
char dllPath[MAX_PATH]{};

uintptr_t ResolveAddr(uintptr_t dx9, uintptr_t dx10)
{
    switch (g_version)
    {
        case eExeVersion::DX9:  return dx9;
        case eExeVersion::DX10: return dx10;
        default: return 0;
    }
}