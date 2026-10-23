#include "pattern_scan.h"
#include <sstream>
#include <stdexcept>
#include <string>

std::vector<size_t> PatternScan::FindOffsets(const uint8_t* data, size_t size, std::string_view pattern) {
    std::istringstream input{std::string(pattern)};
    std::vector<int> bytes;
    std::string token;
    while (input >> token) {
        if (token == "?" || token == "??") { bytes.push_back(-1); continue; }
        if (token.size() != 2 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
            throw std::invalid_argument("Invalid byte pattern");
        bytes.push_back(std::stoi(token, nullptr, 16));
    }
    std::vector<size_t> result;
    if (!data || bytes.empty() || size < bytes.size()) return result;
    for (size_t i = 0; i <= size - bytes.size(); ++i) {
        size_t j = 0;
        for (; j < bytes.size(); ++j)
            if (bytes[j] >= 0 && data[i+j] != bytes[j]) break;
        if (j == bytes.size()) result.push_back(i);
    }
    return result;
}
