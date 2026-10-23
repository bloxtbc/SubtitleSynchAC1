#include <fstream>
#include <cstdio>
#include <cctype>
#include <cmath>

#include "subtitle_core.h"

SubtitleEngine g_SubtitleEngine;

bool shouldDisplaySubtitle(SubtitlePriority active, SubtitlePriority incoming) {
    if (incoming == SubtitlePriority::None) return false;
    return active != SubtitlePriority::Primary || incoming == SubtitlePriority::Primary;
}

int secondaryRank(uint32_t voiceId) {
    switch (voiceId >> 16) {
    case 0x2097: case 0x20d3: return 3;
    case 0x2098: case 0x20b6: return 2;
    default: return 1;
    }
}

bool shouldReplaceSecondary(const SecondaryCandidate& active, bool activeSpeaking,
                            const SecondaryCandidate& incoming) {
    if (!activeSpeaking) return true;
    const int a = secondaryRank(active.id), b = secondaryRank(incoming.id);
    if (a != b) return b > a;
    // Same kind of speaker: keep the nearer (louder) one on screen.
    if (!active.levelKnown || !incoming.levelKnown) return true;
    return incoming.levelDb >= active.levelDb;
}

std::string removeSoundCaptions(const std::string& text) {
    std::string out;
    int depth = 0;
    for (char c : text) {
        if (c == '[') { ++depth; continue; }
        if (c == ']' && depth) { --depth; continue; }
        if (depth) continue;
        if (std::isspace(static_cast<unsigned char>(c)) && c != '\n') {
            if (!out.empty() && out.back() != ' ' && out.back() != '\n') out.push_back(' ');
            continue;
        }
        if (c == '\n' && !out.empty() && out.back() == ' ') out.pop_back();
        if (out.empty() && c == '\n') continue;
        out.push_back(c);
    }
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
    return out;
}

bool SubtitleEngine::loadFile(
    const std::string& filepath,
    std::unordered_map<std::string, std::string>& destination) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        printf("[SubtitleEngine] FAILED to open file: %s\n", filepath.c_str());
        return false;
    }

    std::string s(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    destination.clear();
    size_t pos = 0;
    size_t count = 0;

    auto skip_ws = [&](size_t& p) {
        while (p < s.size() && std::isspace((unsigned char)s[p])) ++p;
    };

    auto read_string = [&](size_t& p) -> std::string {
        std::string out;
        if (p >= s.size() || s[p] != '"') return out;

        ++p;
        while (p < s.size()) {
            char c = s[p++];
            if (c == '"') break;

            if (c == '\\' && p < s.size()) {
                char esc = s[p++];
                if (esc == 'n') out += '\n';
                else if (esc == 't') out += '\t';
                else out += esc;
            } else {
                out += c;
            }
        }
        return out;
    };

    while (pos < s.size()) {
        skip_ws(pos);
        if (pos >= s.size()) break;
        if (s[pos] != '"') { ++pos; continue; }

        std::string key = read_string(pos);

        skip_ws(pos);
        while (pos < s.size() && s[pos] != ':') ++pos;
        if (pos >= s.size()) break;
        ++pos;

        skip_ws(pos);

        std::string value;
        if (pos < s.size() && s[pos] == '"') {
            value = read_string(pos);
        } else {
            size_t start = pos;
            while (pos < s.size() && s[pos] != ',' && s[pos] != '}') ++pos;
            value = s.substr(start, pos - start);
        }

        if (!key.empty()) {
            destination[key] = value;
            count++;
        }

        while (pos < s.size() && s[pos] != ',') ++pos;
        if (pos < s.size()) ++pos;
    }

    printf("[SubtitleEngine] LOAD complete: %zu entries\n", count);
    return true;
}

bool SubtitleEngine::load(const std::string& filepath) {
    m_secondaryDb.clear();
    return loadFile(filepath, m_primaryDb);
}

bool SubtitleEngine::loadSecondary(const std::string& filepath) {
    return loadFile(filepath, m_secondaryDb);
}

std::string SubtitleEngine::makeKey(uint32_t voiceId) {
    char buf[32];
    std::sprintf(buf, "0x%08x", voiceId);
    return buf;
}

std::string SubtitleEngine::getRaw(uint32_t voiceId) const
{
    // Resource type bits are part of the ID. Do not turn arbitrary resources
    // into voice lines by replacing their high nibble with 0x2.
    return getMatch(voiceId).raw;
}

SubtitleMatch SubtitleEngine::getMatch(uint32_t voiceId) const
{
    const auto key = makeKey(voiceId);
    auto primary = m_primaryDb.find(key);
    if (primary != m_primaryDb.end())
        return { primary->second, SubtitlePriority::Primary };
    auto secondary = m_secondaryDb.find(key);
    if (secondary != m_secondaryDb.end())
        return { secondary->second, SubtitlePriority::Secondary };
    return {};
}

SubtitlePriority SubtitleEngine::getPriority(uint32_t voiceId) const
{
    return getMatch(voiceId).priority;
}

std::string SubtitleEngine::stripTags(const std::string& s) {
    size_t p = s.find("<duration=");
    if (p == std::string::npos) return s;

    size_t end = s.find('>', p);
    if (end == std::string::npos) return s;

    std::string out = s;
    out.erase(p, end - p + 1);
    return out;
}

double SubtitleEngine::extractDuration(const std::string& s) {
    size_t p = s.find("<duration=");
    if (p == std::string::npos) return 0.0;

    size_t end = s.find('>', p);
    if (end == std::string::npos) return 0.0;

    try {
        double value = std::stod(s.substr(p + 10, end - (p + 10)));
        return std::isfinite(value) && value > 0.0 ? value : 0.0;
    } catch (...) {
        return 0.0;
    }
}

std::vector<SubtitleSegment> SubtitleEngine::parseSegments(const std::string& s)
{
    std::vector<SubtitleSegment> out;

    std::string cur;
    size_t pos = 0;

    auto flush = [&](double waitAfter)
    {
        size_t a = 0;
        while (a < cur.size() && std::isspace((unsigned char)cur[a])) a++;

        size_t b = cur.size();
        while (b > a && std::isspace((unsigned char)cur[b - 1])) b--;

        if (b > a)
        {
            out.push_back({ cur.substr(a, b - a), waitAfter });
        }

        cur.clear();
    };

    while (pos < s.size())
    {
        if (s[pos] == '<')
        {
            size_t end = s.find('>', pos);

            if (end != std::string::npos)
            {
                std::string token = s.substr(pos + 1, end - pos - 1);

                if (token.rfind("wait=", 0) == 0)
                {
                    double wait = 0.0;
                    try { wait = std::stod(token.substr(5)); }
                    catch (...) {}

                    flush(wait);

                    pos = end + 1;
                    continue;
                }
            }
        }

        cur.push_back(s[pos++]);
    }

    flush(0.0);

    return out;
}

std::vector<SubtitleSegment> SubtitleEngine::getSegments(uint32_t voiceId) const {
    auto raw = getRaw(voiceId);
    if (raw.empty()) {
        return {};
    }

    auto stripped = stripTags(raw);
    return parseSegments(stripped);
}
