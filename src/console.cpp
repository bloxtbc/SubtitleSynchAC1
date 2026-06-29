#include <Windows.h>
#include <stdio.h>

#include "console.h"

FILE* conin = nullptr;
FILE* conout = nullptr;
FILE* conerr = nullptr;

void init_console()
{
    AllocConsole();

    freopen_s(&conin,  "conin$",  "r", stdin);
    freopen_s(&conout, "conout$", "w", stdout);
    freopen_s(&conerr, "conout$", "w", stderr);
}