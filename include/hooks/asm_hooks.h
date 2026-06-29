#pragma once

#include <xbyak/xbyak.h>
#include <cstdint>
#include <functional>
#include <memory>

extern bool gamePaused;

struct HookTarget {
    uintptr_t dx9;
    uintptr_t dx10;
    size_t size;
};

struct MidHook
{
    uintptr_t address;
    size_t size;
    uintptr_t returnAddress;
    std::unique_ptr<Xbyak::CodeGenerator> code;
};

struct HookDef
{
    uintptr_t address;
    size_t size;
    std::function<void(Xbyak::CodeGenerator&, MidHook&)> builder;
};

template <typename Func> MidHook MakeMidHook(uintptr_t address, size_t overwriteSize, Func&& builder);
void applyASMPatches(void);