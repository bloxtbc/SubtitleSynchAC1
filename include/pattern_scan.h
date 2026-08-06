#pragma once

#include <cstdint>
#include <string_view>

namespace PatternScan
{
    uintptr_t Find(std::string_view pattern);

    uintptr_t Find(void* module, std::string_view pattern);
}