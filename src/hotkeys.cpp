#include "wardogs/hotkeys.hpp"

#include <algorithm>
#include <cwctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wardogs {
namespace {

std::wstring trim(std::wstring value) {
    const auto first = std::find_if_not(value.begin(), value.end(), std::iswspace);
    const auto last = std::find_if_not(value.rbegin(), value.rend(), std::iswspace).base();
    return first < last ? std::wstring(first, last) : std::wstring{};
}

std::wstring upper(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), std::towupper);
    return value;
}

}  // namespace

Hotkey parse_hotkey(std::wstring_view text) {
    std::vector<std::wstring> parts;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const auto end = text.find(L'+', begin);
        auto part = trim(std::wstring{text.substr(begin, end == std::wstring_view::npos
                                                          ? text.size() - begin
                                                          : end - begin)});
        if (!part.empty()) {
            parts.push_back(upper(std::move(part)));
        }
        if (end == std::wstring_view::npos) break;
        begin = end + 1;
    }
    if (parts.empty()) {
        throw std::invalid_argument("热键不能为空");
    }

    UINT modifiers = MOD_NOREPEAT;
    UINT key = 0;
    std::vector<std::wstring> display_modifiers;
    for (const auto& part : parts) {
        UINT modifier = 0;
        std::wstring label;
        if (part == L"CTRL" || part == L"CONTROL") {
            modifier = MOD_CONTROL; label = L"Ctrl";
        } else if (part == L"ALT") {
            modifier = MOD_ALT; label = L"Alt";
        } else if (part == L"SHIFT") {
            modifier = MOD_SHIFT; label = L"Shift";
        } else if (part == L"WIN" || part == L"META") {
            modifier = MOD_WIN; label = L"Win";
        }
        if (modifier) {
            if (modifiers & modifier || key) {
                throw std::invalid_argument("修饰键必须写在普通按键之前，且不能重复");
            }
            modifiers |= modifier;
            display_modifiers.push_back(std::move(label));
            continue;
        }
        if (key) {
            throw std::invalid_argument("每个热键只能包含一个非修饰键");
        }
        if (part.size() == 1 && ((part[0] >= L'A' && part[0] <= L'Z') ||
                                 (part[0] >= L'0' && part[0] <= L'9'))) {
            key = static_cast<UINT>(part[0]);
        } else if (part.size() >= 2 && part[0] == L'F') {
            const int number = std::stoi(part.substr(1));
            if (number >= 1 && number <= 24) key = VK_F1 + number - 1;
        } else if (part == L"SPACE") key = VK_SPACE;
        else if (part == L"TAB") key = VK_TAB;
        else if (part == L"ENTER" || part == L"RETURN") key = VK_RETURN;
        else if (part == L"ESC" || part == L"ESCAPE") key = VK_ESCAPE;
        else if (part == L"INSERT") key = VK_INSERT;
        else if (part == L"DELETE") key = VK_DELETE;
        else if (part == L"HOME") key = VK_HOME;
        else if (part == L"END") key = VK_END;
        else if (part == L"PAGEUP") key = VK_PRIOR;
        else if (part == L"PAGEDOWN") key = VK_NEXT;
        else if (part == L"UP") key = VK_UP;
        else if (part == L"DOWN") key = VK_DOWN;
        else if (part == L"LEFT") key = VK_LEFT;
        else if (part == L"RIGHT") key = VK_RIGHT;
        if (!key) {
            throw std::invalid_argument("热键包含不支持的按键");
        }
    }
    if (!key) {
        throw std::invalid_argument("热键必须包含一个非修饰键");
    }
    std::wstring display;
    for (const auto& item : display_modifiers) {
        if (!display.empty()) display += L'+';
        display += item;
    }
    if (!display.empty()) display += L'+';
    if (key >= VK_F1 && key <= VK_F24) {
        display += L"F" + std::to_wstring(key - VK_F1 + 1);
    } else if (key == VK_SPACE) display += L"Space";
    else if (key == VK_TAB) display += L"Tab";
    else if (key == VK_RETURN) display += L"Enter";
    else if (key == VK_ESCAPE) display += L"Esc";
    else if (key == VK_INSERT) display += L"Insert";
    else if (key == VK_DELETE) display += L"Delete";
    else if (key == VK_HOME) display += L"Home";
    else if (key == VK_END) display += L"End";
    else if (key == VK_PRIOR) display += L"PageUp";
    else if (key == VK_NEXT) display += L"PageDown";
    else if (key == VK_UP) display += L"Up";
    else if (key == VK_DOWN) display += L"Down";
    else if (key == VK_LEFT) display += L"Left";
    else if (key == VK_RIGHT) display += L"Right";
    else display.push_back(static_cast<wchar_t>(key));
    return {modifiers, key, display};
}

void validate_unique_hotkeys(std::span<const Hotkey> hotkeys) {
    for (std::size_t i = 0; i < hotkeys.size(); ++i) {
        for (std::size_t j = i + 1; j < hotkeys.size(); ++j) {
            if (hotkeys[i].modifiers == hotkeys[j].modifiers &&
                hotkeys[i].virtual_key == hotkeys[j].virtual_key) {
                throw std::invalid_argument("四个热键不能重复");
            }
        }
    }
}

HotkeyMatcher::HotkeyMatcher(std::span<const Hotkey> hotkeys)
    : hotkeys_(hotkeys.begin(), hotkeys.end()) {
    validate_unique_hotkeys(hotkeys_);
}

std::optional<std::size_t> HotkeyMatcher::handle_key_event(
    UINT virtual_key, bool pressed, UINT active_modifiers) {
    if (virtual_key >= pressed_keys_.size()) return std::nullopt;
    if (!pressed) {
        pressed_keys_[virtual_key] = false;
        return std::nullopt;
    }
    if (pressed_keys_[virtual_key]) return std::nullopt;
    pressed_keys_[virtual_key] = true;

    constexpr UINT modifier_mask = MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;
    const UINT current_modifiers = active_modifiers & modifier_mask;
    for (std::size_t index = 0; index < hotkeys_.size(); ++index) {
        const auto& hotkey = hotkeys_[index];
        if (hotkey.virtual_key == virtual_key &&
            (hotkey.modifiers & modifier_mask) == current_modifiers) {
            return index;
        }
    }
    return std::nullopt;
}

struct GlobalHotkeyListener::Impl {
    static inline Impl* active_instance{};

    HHOOK hook{};
    std::optional<HotkeyMatcher> matcher;
    Callback callback;

    static bool key_down(int virtual_key) {
        return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
    }

    static UINT active_modifiers(const KBDLLHOOKSTRUCT& event) {
        UINT modifiers = 0;
        if ((event.flags & LLKHF_ALTDOWN) != 0 || key_down(VK_MENU))
            modifiers |= MOD_ALT;
        if (key_down(VK_CONTROL)) modifiers |= MOD_CONTROL;
        if (key_down(VK_SHIFT)) modifiers |= MOD_SHIFT;
        if (key_down(VK_LWIN) || key_down(VK_RWIN)) modifiers |= MOD_WIN;
        return modifiers;
    }

    static LRESULT CALLBACK hook_proc(int code, WPARAM message, LPARAM data) noexcept {
        Impl* instance = active_instance;
        if (code == HC_ACTION && instance && instance->matcher) {
            const auto& event = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
            const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            const bool released = message == WM_KEYUP || message == WM_SYSKEYUP;
            if (pressed || released) {
                const auto match = instance->matcher->handle_key_event(
                    event.vkCode, pressed, active_modifiers(event));
                if (match && instance->callback) {
                    try {
                        instance->callback(*match);
                    } catch (...) {
                        // Exceptions must never cross the Windows hook boundary.
                    }
                }
            }
        }
        return CallNextHookEx(instance ? instance->hook : nullptr, code, message, data);
    }
};

GlobalHotkeyListener::GlobalHotkeyListener() : impl_(std::make_unique<Impl>()) {}

GlobalHotkeyListener::~GlobalHotkeyListener() { stop(); }

void GlobalHotkeyListener::start(std::span<const Hotkey> hotkeys, Callback callback) {
    validate_unique_hotkeys(hotkeys);
    stop();
    if (Impl::active_instance && Impl::active_instance != impl_.get()) {
        throw std::runtime_error("当前进程中已有一个热键监听器");
    }
    impl_->matcher.emplace(hotkeys);
    impl_->callback = std::move(callback);
    Impl::active_instance = impl_.get();
    impl_->hook = SetWindowsHookExW(WH_KEYBOARD_LL, &Impl::hook_proc,
                                    GetModuleHandleW(nullptr), 0);
    if (!impl_->hook) {
        Impl::active_instance = nullptr;
        impl_->matcher.reset();
        impl_->callback = {};
        throw std::runtime_error("无法启动共享全局热键监听");
    }
}

void GlobalHotkeyListener::stop() noexcept {
    if (impl_->hook) {
        UnhookWindowsHookEx(impl_->hook);
        impl_->hook = nullptr;
    }
    if (Impl::active_instance == impl_.get()) Impl::active_instance = nullptr;
    impl_->matcher.reset();
    impl_->callback = {};
}

bool GlobalHotkeyListener::active() const noexcept { return impl_->hook != nullptr; }

}  // namespace wardogs
