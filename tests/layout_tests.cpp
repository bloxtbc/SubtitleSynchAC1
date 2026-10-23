#include "subtitle_layout.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    ImGui::CreateContext();
    try {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImFont* font = io.Fonts->AddFontDefaultVector();
        SubtitleSettings s;
        check(subtitleFontSize(s, {1920,1080}) == 30, "1080p base size");
        check(subtitleFontSize(s, {2560,1440}) == 40, "1440p scale");
        check(subtitleFontSize(s, {3840,2160}) == 60, "4K scale");
        s.fontSize = 40;
        s.scale = 1.5f;
        check(subtitleFontSize(s, {3840,2160}) == 120, "custom base and multiplier");
        s.autoScale = false;
        check(subtitleFontSize(s, {3840,2160}) == 60, "fixed size mode");
        s = SubtitleSettings{};
        float previousLines = 0;
        const char* text = u8"Altaïr crosses the crowded souk of Jerusalem: this is a long subtitle that must wrap automatically into several lines at the screen edge. ";
        std::string longText;
        for (int i=0; i<5; ++i) longText += text;
        for (ImVec2 display : {ImVec2(1280,720), ImVec2(1920,1080), ImVec2(3840,2160), ImVec2(1080,1920)}) {
            io.DisplaySize = display;
            io.DeltaTime = 1.0f / 60;
            ImGui::NewFrame();
            ImGui::PushFont(font, subtitleFontSize(s, display));
            const auto layout = measureSubtitle(s, display, longText.c_str());
            const auto shortLayout = measureSubtitle(s, display, "short", 0.0f);
            const auto lifted = measureSubtitle(s, display, "short", 90.0f);
            check(std::abs((shortLayout.position.y - lifted.position.y) - 90.0f * display.y / 1080.0f) < .01f,
                  "save activity adds a resolution-scaled vertical lift");
            check(layout.lines.size() > 1, "wrapped lines exposed for centered drawing");
            for (const auto& line : layout.lines)
                check(std::abs(line.offset.x + line.width / 2 - layout.size.x / 2) < .01f,
                      "every wrapped line shares the block center");
            const auto measured = ImGui::CalcTextSize(longText.c_str(), nullptr, false, layout.wrapWidth);
            check(measured.y > ImGui::GetFontSize(), "long text wraps");
            check(layout.size.x <= display.x * .65f + 1, "horizontal screen budget");
            check(layout.position.x >= 0 && layout.position.y >= 0, "nonnegative position");
            check(layout.position.y + layout.size.y <= display.y + 1, "wrapped block stays above bottom");
            const float lines = measured.y / ImGui::GetFontSize();
            if (display.x > display.y && previousLines)
                check(std::abs(lines - previousLines) <= 1, "proportional resolution keeps wrapping stable");
            previousLines = lines;
            const auto explicitBreak = measureSubtitle(s, display, "one\ntwo");
            const auto unequal = measureSubtitle(s, display, "longer\nx");
            check(unequal.lines.size() == 2 && unequal.lines[1].offset.x > unequal.lines[0].offset.x,
                  "short explicit line is centered rather than left aligned");
            check(unequal.lines[1].backgroundSize.x < unequal.lines[0].backgroundSize.x &&
                  std::abs(unequal.lines[0].backgroundOffset.x + unequal.lines[0].backgroundSize.x / 2 -
                           unequal.lines[1].backgroundOffset.x - unequal.lines[1].backgroundSize.x / 2) < .01f,
                  "each line has its own centered background width");
            const auto blanks = measureSubtitle(s, display, "one\n\ntwo\n");
            check(blanks.lines.size() == 4 && blanks.lines[1].text.empty() && blanks.lines[3].text.empty(),
                  "explicit blank lines and trailing newline preserved");
            check(explicitBreak.size.y >= 2 * ImGui::GetFontSize(), "manual line break preserved");
            const std::string unbroken(300, 'W');
            const auto word = measureSubtitle(s, display, unbroken.c_str());
            check(word.size.y > ImGui::GetFontSize() && word.size.x <= display.x * .65f + 1,
                  "unbroken token wraps within width budget");
            s.autoPosition = false; s.position = {-100,100000}; s.padding.y = -25;
            const auto manual = measureSubtitle(s, display, "short");
            check(manual.position.x == 0 && manual.position.y + manual.size.y <= display.y + 1, "manual position clamped after resize");
            check(manual.padding.y == 0, "legacy negative padding cannot clip text");
            s = SubtitleSettings{};
            ImGui::PopFont();
            ImGui::EndFrame();
        }
        s.fontSize = std::numeric_limits<float>::quiet_NaN();
        s.referenceHeight = 0;
        check(std::isfinite(subtitleFontSize(s, {1920,1080})), "invalid config stays finite");
        ImGui::DestroyContext();
        std::cout << "PASS: font scaling, real font wrapping, UTF-8, explicit breaks, resized placement\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; ImGui::DestroyContext(); return 1;
    }
}
