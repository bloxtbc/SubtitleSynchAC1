#include "pattern_scan.h"

#include <Windows.h>
#include <vector>
#include <optional>
#include <stdexcept>

namespace
{
    struct PatternByte
    {
        uint8_t value;
        bool wildcard;
    };

    int HexToInt(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';

        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;

        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;

        return -1;
    }

    std::vector<PatternByte> ParsePattern(std::string_view pattern)
    {
        std::vector<PatternByte> bytes;

        size_t i = 0;

        while (i < pattern.size())
        {
            if (pattern[i] == ' ')
            {
                ++i;
                continue;
            }

            if (pattern[i] == '?')
            {
                bytes.push_back({0, true});

                ++i;

                if (i < pattern.size() && pattern[i] == '?')
                    ++i;

                continue;
            }

            if (i + 1 >= pattern.size())
                throw std::runtime_error("Invalid pattern.");

            int hi = HexToInt(pattern[i]);
            int lo = HexToInt(pattern[i + 1]);

            if (hi < 0 || lo < 0)
                throw std::runtime_error("Invalid hex digit.");

            bytes.push_back({
                static_cast<uint8_t>((hi << 4) | lo),
                false
            });

            i += 2;
        }

        return bytes;
    }
}

namespace PatternScan
{
    uintptr_t Find(void* module, std::string_view pattern)
    {
        if (!module)
            return 0;

        auto parsed = ParsePattern(pattern);

        auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
        auto* nt =
            reinterpret_cast<PIMAGE_NT_HEADERS>(
                reinterpret_cast<uint8_t*>(module) + dos->e_lfanew);

        auto* image = reinterpret_cast<uint8_t*>(module);
        size_t imageSize = nt->OptionalHeader.SizeOfImage;

        for (size_t i = 0; i <= imageSize - parsed.size(); ++i)
        {
            bool match = true;

            for (size_t j = 0; j < parsed.size(); ++j)
            {
                if (!parsed[j].wildcard &&
                    image[i + j] != parsed[j].value)
                {
                    match = false;
                    break;
                }
            }

            if (match)
                return reinterpret_cast<uintptr_t>(image + i);
        }

        return 0;
    }

    uintptr_t Find(std::string_view pattern)
    {
        uintptr_t found = Find(GetModuleHandle(nullptr), pattern);
        printf("[PatternScan] Pattern '%s' found at 0x%p\n", pattern.data(), (void*)found);
        return found;
    }
}