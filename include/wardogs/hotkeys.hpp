#pragma once

#include <Windows.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wardogs {

struct Hotkey {
    UINT modifiers{};
    UINT virtual_key{};
    std::wstring display;

    bool operator==(const Hotkey&) const = default;
};

Hotkey parse_hotkey(std::wstring_view text);
void validate_unique_hotkeys(std::span<const Hotkey> hotkeys);

class HotkeyMatcher {
public:
    explicit HotkeyMatcher(std::span<const Hotkey> hotkeys);

    std::optional<std::size_t> handle_key_event(UINT virtual_key, bool pressed,
                                                UINT active_modifiers);

private:
    std::vector<Hotkey> hotkeys_;
    std::array<bool, 256> pressed_keys_{};
};

class GlobalHotkeyListener {
public:
    using Callback = std::function<void(std::size_t)>;

    GlobalHotkeyListener();
    ~GlobalHotkeyListener();
    GlobalHotkeyListener(const GlobalHotkeyListener&) = delete;
    GlobalHotkeyListener& operator=(const GlobalHotkeyListener&) = delete;

    void start(std::span<const Hotkey> hotkeys, Callback callback);
    void stop() noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wardogs
