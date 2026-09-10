#pragma once

#include <Windows.h>

#include <span>
#include <string>
#include <string_view>

namespace wardogs {

struct Hotkey {
    UINT modifiers{};
    UINT virtual_key{};
    std::wstring display;

    bool operator==(const Hotkey&) const = default;
};

Hotkey parse_hotkey(std::wstring_view text);
void validate_unique_hotkeys(std::span<const Hotkey> hotkeys);

}  // namespace wardogs
