#pragma once

#include <Windows.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wardogs {

enum class HotkeyTrigger { single, double_press, long_press };

[[nodiscard]] std::wstring_view hotkey_trigger_name(HotkeyTrigger trigger);
[[nodiscard]] HotkeyTrigger hotkey_trigger_from_name(std::wstring_view name);

struct Hotkey {
    UINT modifiers{};
    UINT virtual_key{};
    std::wstring display;

    // An empty binding parses to a disabled hotkey (virtual_key == 0). Disabled
    // entries never match, never participate in duplicate validation, and keep
    // their position so callback indices stay stable.
    [[nodiscard]] bool enabled() const noexcept { return virtual_key != 0; }
    bool operator==(const Hotkey&) const = default;
};

struct HotkeyBinding {
    Hotkey hotkey;
    HotkeyTrigger trigger{HotkeyTrigger::single};
};

Hotkey parse_hotkey(std::wstring_view text);
void validate_unique_hotkeys(std::span<const Hotkey> hotkeys);
// Two bindings may share the same physical key when their trigger type
// differs (single / double / long); identical enabled pairs are rejected.
void validate_unique_bindings(std::span<const HotkeyBinding> bindings);

class HotkeyMatcher {
public:
    explicit HotkeyMatcher(std::span<const Hotkey> hotkeys);

    // Returns the matched binding index on the initial key-down and again on
    // the matching key-up. Key repeat while held is suppressed.
    std::optional<std::size_t> handle_key_event(UINT virtual_key, bool pressed,
                                                UINT active_modifiers);

private:
    std::vector<Hotkey> hotkeys_;
    std::array<bool, 256> pressed_keys_{};
    std::array<std::optional<std::size_t>, 256> active_matches_{};
};

// One action bound to a physical key group.
struct HotkeyGestureBinding {
    std::size_t action{};
    HotkeyTrigger trigger{HotkeyTrigger::single};
};

// Classifies press/release timing into single, double, and long actions.
// Qt-free and clock-injected so every transition is unit tested without
// timers; the caller owns the timer that polls next_deadline_ms()/fire_due().
class HotkeyGestureRecognizer {
public:
    HotkeyGestureRecognizer() = default;

    void add_group(std::size_t group,
                   std::span<const HotkeyGestureBinding> bindings);
    void set_timing(int long_press_ms, int double_press_ms);

    [[nodiscard]] std::vector<std::size_t> handle(std::size_t group,
                                                  bool pressed,
                                                  std::int64_t now_ms);
    [[nodiscard]] std::vector<std::size_t> fire_due(std::int64_t now_ms);
    [[nodiscard]] std::optional<std::int64_t> next_deadline_ms() const;

private:
    struct GroupState {
        std::optional<std::size_t> single;
        std::optional<std::size_t> double_press;
        std::optional<std::size_t> long_press;
        std::optional<std::int64_t> long_deadline;
        std::optional<std::int64_t> pending_single;
        bool long_fired{};
        bool awaiting_second_release{};
    };

    std::vector<GroupState> groups_;
    int long_press_ms_{400};
    int double_press_ms_{500};
};

class GlobalHotkeyListener {
public:
    // index: matched binding; pressed: true for key-down, false for key-up.
    using Callback = std::function<void(std::size_t, bool)>;

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
