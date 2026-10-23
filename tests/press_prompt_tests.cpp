#include "press_prompt_detector.h"
#include "hooks/press_prompt_hook.h"
#include "pattern_scan.h"

#include <Windows.h>
#include <MinHook.h>
#include <xbyak/xbyak.h>
#include <iostream>
#include <stdexcept>
#include <string>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    try {
        check(isInteractionPrompt(L"<\\ PRESS ANY BUTTON TO INTERACT >"), "English interaction prompt");
        check(isInteractionPrompt(L"Press any key to interact"), "case folding");
        check(isInteractionPrompt(L"PRESS  ANY\tBUTTON TO INTERACT"), "whitespace and button icons are independent");
        check(!isInteractionPrompt(L"< Press Any Key To Continue >"), "title prompt must not move subtitles");
        check(!isInteractionPrompt(L"Please, Tamir, I meant no offence!"), "Tamir scene is not an interaction prompt");
        check(!isInteractionPrompt(L"Saving progress. Please wait."), "save activity uses its own observer");
        InteractionPromptTracker tracker;
        check(!tracker.visible(0), "no prompt before an actual draw");
        tracker.observe(L"PRESS ANY BUTTON TO INTERACT", 1000);
        check(tracker.visible(1000), "draw activates prompt lift");
        tracker.observe(L"unrelated frame", 1200);
        check(!tracker.visible(1251), "unrelated scene text cannot keep prompt lift alive");
        tracker.observe(L"PRESS ANY BUTTON TO INTERACT", 1400);
        check(tracker.visible(1650), "brief frame gaps do not flicker");
        check(!tracker.visible(1651), "prompt removal expires predictably");

        const auto pattern = std::string(PressPromptHook::Pattern);
        std::vector<uint8_t> fixture;
        for (size_t i = 0; i < pattern.size(); i += 3)
            fixture.push_back(static_cast<uint8_t>(std::stoul(pattern.substr(i, 2), nullptr, 16)));
        check(PatternScan::FindOffsets(fixture.data(), fixture.size(), pattern).size() == 1,
            "exact draw signature matches");
        auto doubled = fixture;
        doubled.insert(doubled.end(), fixture.begin(), fixture.end());
        check(PatternScan::FindOffsets(doubled.data(), doubled.size(), pattern).size() == 2,
            "ambiguous draw signature is distinguishable");

        check(MH_Initialize() == MH_OK, "initialize MinHook");
        unsigned calls = 0;
        float xmmObserved = 0.0f, xmmInput = 3.25f;
        Xbyak::CodeGenerator target;
        // Observe incoming registers, carry and XMM state before overwriting
        // them. The production detour must then execute this original body.
        target.adc(target.eax, target.edx);
        target.nop(); target.nop(); target.nop();
        target.mov(target.ecx, reinterpret_cast<uintptr_t>(&xmmObserved));
        target.movss(target.ptr[target.ecx], target.xmm0);
        target.mov(target.ecx, reinterpret_cast<uintptr_t>(&calls));
        target.inc(target.dword[target.ecx]);
        target.ret(); target.ready();
        check(PressPromptHook::install(reinterpret_cast<uintptr_t>(target.getCode())), "install production UI observer");
        Xbyak::CodeGenerator invoke;
        invoke.push(invoke.edi);
        invoke.mov(invoke.ecx, invoke.dword[invoke.esp + 8]);
        invoke.mov(invoke.edx, invoke.dword[invoke.esp + 12]);
        invoke.push(invoke.edx); // end, argument 12
        invoke.push(0); // start, argument 11
        for (unsigned i = 0; i < 9; ++i) invoke.push(0);
        invoke.push(invoke.ecx);
        invoke.mov(invoke.ecx, reinterpret_cast<uintptr_t>(&xmmInput));
        invoke.movss(invoke.xmm0, invoke.ptr[invoke.ecx]);
        invoke.mov(invoke.eax, 100); invoke.mov(invoke.edx, 7);
        invoke.mov(invoke.edi, reinterpret_cast<uintptr_t>(target.getCode()));
        invoke.stc(); invoke.call(invoke.edi);
        invoke.add(invoke.esp, 48); invoke.pop(invoke.edi); invoke.ret(); invoke.ready();
        auto draw = invoke.getCode<unsigned(*)(uintptr_t, unsigned)>();
        check(draw(1, 1) == 108 && calls == 1, "invalid text pointer is safe and original return survives");
        check(xmmObserved == xmmInput, "font draw receives original XMM registers");
        const std::wstring english = L"<\\ PRESS ANY BUTTON TO INTERACT >";
        uint32_t gameString[7]{};
        gameString[1] = reinterpret_cast<uintptr_t>(english.c_str());
        gameString[5] = static_cast<uint32_t>(english.size());
        gameString[6] = static_cast<uint32_t>(english.size());
        check(draw(reinterpret_cast<uintptr_t>(gameString), 0) == 108 && calls == 2 &&
            !PressPromptHook::visible(GetTickCount64()), "empty text span cannot trigger prompt lift");
        check(draw(reinterpret_cast<uintptr_t>(gameString), static_cast<unsigned>(english.size())) == 108 && calls == 3,
            "valid text draw preserves incoming register state and side effects");
        check(PressPromptHook::visible(GetTickCount64()), "production observer sees rendered string");
        MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
        std::cout << "PASS: interaction text, no scene-color false positives, expiry, native draw preservation\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
