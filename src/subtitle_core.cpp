#include <fstream>

#include "subtitle_core.h"

SubtitleEngine g_SubtitleEngine;

bool SubtitleEngine::load(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        printf("[SubtitleEngine] FAILED to open file: %s\n", filepath.c_str());
        return false;
    }

    std::string s(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

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
            m_db[key] = value;
            count++;
        }

        while (pos < s.size() && s[pos] != ',') ++pos;
        if (pos < s.size()) ++pos;
    }

    printf("[SubtitleEngine] LOAD complete: %zu entries\n", count);
    return true;
}

std::string SubtitleEngine::makeKey(uint32_t voiceId) {
    char buf[32];
    std::sprintf(buf, "0x%08x", voiceId);
    return buf;
}

std::string SubtitleEngine::makeAltKey(uint32_t voiceId) {
    char buf[32];
    uint32_t mod = (voiceId & 0x0FFFFFFF) | 0x20000000;
    std::sprintf(buf, "0x%08x", mod);
    return buf;
}


static uint32_t normalizeVoiceId(uint32_t id)
{
    if (id & 0x20000000)
        return id;

    return (id & 0x0FFFFFFF) | 0x20000000;
}

std::string SubtitleEngine::getRaw(uint32_t voiceId) const
{
    uint32_t n = normalizeVoiceId(voiceId);
    std::string key = makeKey(n);

    auto it = m_db.find(key);
    if (it != m_db.end())
        return it->second;

    it = m_db.find(makeAltKey(voiceId));
    if (it != m_db.end())
        return it->second;

    it = m_db.find(makeKey(voiceId));
    if (it != m_db.end())
        return it->second;

    return {};
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
        return std::stod(s.substr(p + 10, end - (p + 10)));
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