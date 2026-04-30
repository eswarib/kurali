#include "TextUtils.h"
#include <algorithm>
#include <cctype>
#include <vector>

namespace {

std::string collapseAsciiWhitespace(std::string s)
{
    std::string out;
    out.reserve(s.size());
    bool inSpace = false;
    for (unsigned char c : s) {
        if (std::isspace(c)) {
            if (!inSpace) {
                out += ' ';
                inSpace = true;
            }
        } else {
            out += static_cast<char>(c);
            inSpace = false;
        }
    }
    return TextUtils::trim(out);
}

std::string stripTrailingSentencePunct(std::string s)
{
    while (!s.empty()) {
        unsigned char c = static_cast<unsigned char>(s.back());
        if (c == '.' || c == '!' || c == '?' || c == ',') {
            s.pop_back();
        } else {
            break;
        }
    }
    return TextUtils::trim(s);
}

} // namespace

std::string TextUtils::trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), ::isspace);
    auto end = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
    return (start < end) ? std::string(start, end) : std::string();
}

std::string TextUtils::removeSpecialChars(const std::string& s, const std::unordered_set<char>& charsToRemove) {
    std::string result;
    for (char c : s) {
        if (charsToRemove.find(c) == charsToRemove.end()) {
            result += c;
        }
    }
    return result;
}

std::string TextUtils::removeSpecialSubstrings(const std::string& s, const std::vector<std::string>& substringsToRemove) {
    std::string result = s;
    for (const auto& sub : substringsToRemove) {
        size_t pos = 0;
        while ((pos = result.find(sub, pos)) != std::string::npos) {
            result.erase(pos, sub.length());
        }
    }
    return result;
}

void TextUtils::lowercaseFirstNonSpace(std::string& s) {
    for (char& c : s) {
        if (!std::isspace(c)) {
            c = std::tolower(c);
            break;
        }
    }
}

void TextUtils::toLower(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
}

bool TextUtils::shouldDiscardTranscript(const std::string& raw)
{
    std::string t = trim(raw);
    if (t.empty()) {
        return true;
    }

    // Single bracketed tag only, e.g. [music], [blank_audio]
    if (t.size() >= 2 && t.front() == '[' && t.back() == ']') {
        return true;
    }

    std::string n = t;
    toLower(n);
    n = collapseAsciiWhitespace(n);
    n = stripTrailingSentencePunct(std::move(n));
    if (n.empty()) {
        return true;
    }

    if (n == "music") {
        return true;
    }

    static const std::vector<std::string> kBlockedPhrases = {
        "foreign language",
        "(speaking in foreign language)",
        "[speaking in foreign language]",
        "speaking in foreign language",
        "speaking in a foreign language",
        "[non-english speech]",
        "(non-english speech)",
        "[silence]",
        "[inaudible]",
        "[music]",
        "[blank audio]",
        "[blank_audio]",
        "blank audio",
        "(music)",
    };

    for (const auto& phrase : kBlockedPhrases) {
        if (n == phrase) {
            return true;
        }
    }

    // Very short non-alphanumeric artifacts (keep 1-2 letter/digit tokens like "no", "ok")
    if (n.size() <= 2) {
        for (unsigned char c : n) {
            if (!std::isalnum(c)) {
                return true;
            }
        }
    }

    return false;
}