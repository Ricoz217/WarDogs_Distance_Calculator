#include "wardogs/core.hpp"

#include <cwctype>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace wardogs {
namespace {

double parse_number(std::wstring value,
                    std::size_t fractional_digits = std::wstring::npos) {
    std::wstring normalized;
    normalized.reserve(value.size());
    for (wchar_t ch : value) {
        if (std::iswspace(ch)) {
            continue;
        }
        switch (ch) {
            case L'l':
            case L'i':
            case L'I':
            case L'|': ch = L'1'; break;
            case L'o':
            case L'O': ch = L'0'; break;
            default: break;
        }
        normalized.push_back(ch);
    }
    const auto decimal = normalized.find(L'.');
    if (fractional_digits != std::wstring::npos && decimal != std::wstring::npos &&
        normalized.size() > decimal + 1 + fractional_digits) {
        normalized.resize(decimal + 1 + fractional_digits);
    }
    std::size_t consumed = 0;
    const double result = std::stod(normalized, &consumed);
    if (consumed != normalized.size()) {
        throw std::invalid_argument("coordinate capture is not numeric");
    }
    return result;
}

Point parse_last_match(const std::wstring& text, const std::wregex& expression) {
    std::wsregex_iterator current{text.begin(), text.end(), expression};
    const std::wsregex_iterator end;
    if (current == end) {
        throw std::invalid_argument("no complete coordinate pair found");
    }
    std::wsmatch latest;
    for (; current != end; ++current) {
        latest = *current;
    }
    if (latest.size() < 3 || !latest[1].matched || !latest[2].matched) {
        throw std::invalid_argument("coordinate pattern needs two capture groups");
    }
    return {parse_number(latest[1].str(), 2), parse_number(latest[2].str(), 2)};
}

}  // namespace

Point parse_ocr_coordinate(std::wstring_view text, std::wstring_view pattern) {
    try {
        const std::wregex expression(std::wstring{pattern},
                                     std::regex_constants::ECMAScript |
                                         std::regex_constants::icase);
        return parse_last_match(std::wstring{text}, expression);
    } catch (const std::regex_error& error) {
        throw std::invalid_argument(std::string{"invalid coordinate regex: "} + error.what());
    }
}

Point parse_manual_coordinate(std::wstring_view text) {
    static const std::wregex labelled{
        LR"(^\s*x\s*[:=]?\s*([-+]?\d+(?:\.\d+)?)\s*[,，;；]?\s*y\s*[:=]?\s*([-+]?\d+(?:\.\d+)?)\s*$)",
        std::regex_constants::ECMAScript | std::regex_constants::icase};
    static const std::wregex plain{
        LR"(^\s*([-+]?\d+(?:\.\d+)?)\s*[,，\s]\s*([-+]?\d+(?:\.\d+)?)\s*$)",
        std::regex_constants::ECMAScript};
    const std::wstring value{text};
    std::wsmatch match;
    if (std::regex_match(value, match, labelled) || std::regex_match(value, match, plain)) {
        return {parse_number(match[1].str()), parse_number(match[2].str())};
    }
    throw std::invalid_argument("enter x12.34, y56.78 or 12.34 56.78");
}

}  // namespace wardogs
