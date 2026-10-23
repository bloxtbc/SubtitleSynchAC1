#pragma once
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <vector>
namespace PatternScan {
std::vector<size_t> FindOffsets(const uint8_t* data, size_t size, std::string_view pattern);
uintptr_t Find(std::string_view pattern);
uintptr_t Find(void* module, std::string_view pattern);
}
