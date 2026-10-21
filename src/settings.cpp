#include "wardogs/settings.hpp"

#include <Windows.h>
#include <ShlObj.h>
#include <wrl/client.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

namespace wardogs {
namespace {

std::wstring read_value(const std::filesystem::path& path,
                        const wchar_t* section, const wchar_t* key,
                        std::wstring_view fallback) {
    std::array<wchar_t, 8192> buffer{};
    GetPrivateProfileStringW(section, key, std::wstring{fallback}.c_str(),
                             buffer.data(), static_cast<DWORD>(buffer.size()),
                             path.c_str());
    return buffer.data();
}

std::wstring read_value(const std::filesystem::path& path, const wchar_t* key,
                        std::wstring_view fallback) {
    return read_value(path, L"settings", key, fallback);
}

void write_value(const std::filesystem::path& path, const wchar_t* section,
                 const wchar_t* key, const std::wstring& value) {
    if (!WritePrivateProfileStringW(section, key, value.c_str(),
                                    path.c_str())) {
        throw std::runtime_error("cannot save settings");
    }
}

void write_value(const std::filesystem::path& path, const wchar_t* key,
                 const std::wstring& value) {
    write_value(path, L"settings", key, value);
}

std::optional<long> read_integer(const std::filesystem::path& path,
                                 const wchar_t* section,
                                 const wchar_t* key) {
    const std::wstring text = read_value(path, section, key, L"");
    if (text.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        const long value = std::stol(text, &consumed);
        if (consumed != text.size()) return std::nullopt;
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<long> read_integer(const std::filesystem::path& path,
                                 const wchar_t* key) {
    return read_integer(path, L"settings", key);
}

std::optional<double> read_double(const std::filesystem::path& path,
                                  const wchar_t* key) {
    const std::wstring text = read_value(path, key, L"");
    if (text.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (consumed != text.size() || !std::isfinite(value))
            return std::nullopt;
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void remove_value(const std::filesystem::path& path, const wchar_t* key) {
    if (!WritePrivateProfileStringW(L"settings", key, nullptr, path.c_str())) {
        throw std::runtime_error("cannot remove settings value");
    }
}

constexpr std::array<const wchar_t*, 7> profile_hotkey_keys{
    L"region", L"base", L"target", L"quick_target", L"impact",
    L"ghost_arc", L"ghost_toggle"};

std::wstring encode_profile_name(const std::wstring& name) {
    static constexpr wchar_t digits[] = L"0123456789ABCDEF";
    std::wstring encoded = L"hex:";
    encoded.reserve(4 + name.size() * 4);
    for (const wchar_t character : name) {
        const unsigned value = static_cast<unsigned>(character);
        encoded.push_back(digits[(value >> 12) & 0xF]);
        encoded.push_back(digits[(value >> 8) & 0xF]);
        encoded.push_back(digits[(value >> 4) & 0xF]);
        encoded.push_back(digits[value & 0xF]);
    }
    return encoded;
}

std::wstring decode_profile_name(const std::wstring& value) {
    if (value.rfind(L"hex:", 0) != 0) return value;
    std::wstring decoded;
    for (std::size_t i = 4; i + 3 < value.size(); i += 4) {
        unsigned character = 0;
        for (std::size_t digit = 0; digit < 4; ++digit) {
            const wchar_t symbol = value[i + digit];
            unsigned nibble = 0;
            if (symbol >= L'0' && symbol <= L'9')
                nibble = static_cast<unsigned>(symbol - L'0');
            else if (symbol >= L'A' && symbol <= L'F')
                nibble = static_cast<unsigned>(symbol - L'A' + 10);
            else if (symbol >= L'a' && symbol <= L'f')
                nibble = static_cast<unsigned>(symbol - L'a' + 10);
            else
                return {};
            character = (character << 4) | nibble;
        }
        decoded.push_back(static_cast<wchar_t>(character));
    }
    return decoded;
}

std::array<std::wstring, 7> active_hotkeys(const AppSettings& settings) {
    return {settings.region_hotkey, settings.base_hotkey,
            settings.target_hotkey, settings.quick_target_hotkey,
            settings.impact_hotkey, settings.ghost_arc_hotkey,
            settings.ghost_toggle_hotkey};
}

std::array<HotkeyTrigger, 7> active_triggers(const AppSettings& settings) {
    return {settings.region_trigger, settings.base_trigger,
            settings.target_trigger, settings.quick_target_trigger,
            settings.impact_trigger, settings.ghost_arc_trigger,
            settings.ghost_toggle_trigger};
}

bool profile_matches_settings(const HotkeyProfile& profile,
                              const AppSettings& settings) {
    const auto hotkeys = active_hotkeys(settings);
    const auto triggers = active_triggers(settings);
    for (std::size_t i = 0; i < hotkeys.size(); ++i) {
        if (profile.hotkeys[i] != hotkeys[i] ||
            profile.triggers[i] != triggers[i]) {
            return false;
        }
    }
    return true;
}

HotkeyProfile custom_profile_from_settings(const AppSettings& settings,
                                           std::wstring id,
                                           std::wstring name) {
    HotkeyProfile profile;
    profile.id = std::move(id);
    profile.name = std::move(name);
    profile.description = L"自定义配置";
    profile.hotkeys = active_hotkeys(settings);
    profile.triggers = active_triggers(settings);
    return profile;
}

std::wstring unique_custom_profile_id(const AppSettings& settings) {
    for (int number = 1;; ++number) {
        std::wstring id = L"custom-" + std::to_wstring(number);
        const bool taken = std::any_of(
            settings.custom_hotkey_profiles.begin(),
            settings.custom_hotkey_profiles.end(),
            [&](const HotkeyProfile& profile) { return profile.id == id; });
        if (!taken) return id;
    }
}

}  // namespace

const std::vector<HotkeyProfile>& builtin_hotkey_profiles() {
    static const std::vector<HotkeyProfile> profiles = [] {
        HotkeyProfile classic;
        classic.id = L"classic";
        classic.name = L"默认配置1";
        classic.description = L"全部单击";
        classic.builtin = true;
        classic.hotkeys = {L"F8", L"F9", L"F10", L"F11", L"F12", L"F4", L""};

        HotkeyProfile advanced;
        advanced.id = L"advanced";
        advanced.name = L"默认配置2";
        advanced.description = L"单击 / 双击 / 长按";
        advanced.builtin = true;
        advanced.hotkeys = {L"F8", L"F8", L"F9", L"F9", L"F10",
                            L"F10", L"F10"};
        advanced.triggers = {
            HotkeyTrigger::long_press, HotkeyTrigger::single,
            HotkeyTrigger::single, HotkeyTrigger::double_press,
            HotkeyTrigger::single, HotkeyTrigger::double_press,
            HotkeyTrigger::long_press};

        return std::vector<HotkeyProfile>{classic, advanced};
    }();
    return profiles;
}

std::filesystem::path settings_path() {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr,
                                    &raw))) {
        throw std::runtime_error("cannot locate LocalAppData");
    }
    const std::filesystem::path path =
        std::filesystem::path{raw} / L"WarDogsDistanceCalculatorCpp" / L"settings.ini";
    CoTaskMemFree(raw);
    return path;
}

AppSettings load_settings_from(const std::filesystem::path& path) {
    AppSettings settings;
    settings.region_hotkey = read_value(path, L"region_hotkey", settings.region_hotkey);
    settings.base_hotkey = read_value(path, L"base_hotkey", settings.base_hotkey);
    settings.target_hotkey = read_value(path, L"target_hotkey", settings.target_hotkey);
    settings.quick_target_hotkey =
        read_value(path, L"quick_target_hotkey", settings.quick_target_hotkey);
    settings.impact_hotkey = read_value(path, L"impact_hotkey", settings.impact_hotkey);
    settings.ghost_arc_hotkey =
        read_value(path, L"ghost_arc_hotkey", settings.ghost_arc_hotkey);
    settings.ghost_toggle_hotkey =
        read_value(path, L"ghost_toggle_hotkey", settings.ghost_toggle_hotkey);
    settings.region_trigger = hotkey_trigger_from_name(
        read_value(path, L"region_hotkey_trigger", L"single"));
    settings.base_trigger = hotkey_trigger_from_name(
        read_value(path, L"base_hotkey_trigger", L"single"));
    settings.target_trigger = hotkey_trigger_from_name(
        read_value(path, L"target_hotkey_trigger", L"single"));
    settings.quick_target_trigger = hotkey_trigger_from_name(
        read_value(path, L"quick_target_hotkey_trigger", L"single"));
    settings.impact_trigger = hotkey_trigger_from_name(
        read_value(path, L"impact_hotkey_trigger", L"single"));
    settings.ghost_arc_trigger = hotkey_trigger_from_name(
        read_value(path, L"ghost_arc_hotkey_trigger", L"single"));
    settings.ghost_toggle_trigger = hotkey_trigger_from_name(
        read_value(path, L"ghost_toggle_hotkey_trigger", L"single"));
    settings.coordinate_pattern =
        read_value(path, L"coordinate_pattern", settings.coordinate_pattern);
    settings.backend = read_value(path, L"ocr_backend", L"rapid") == L"windows"
                           ? OcrBackend::windows
                           : OcrBackend::rapid;
    settings.pinned_card.locked =
        read_value(path, L"pinned_card_locked", L"0") == L"1";
    settings.pinned_card.unlock_hotkey = read_value(
        path, L"pinned_card_unlock_hotkey",
        settings.pinned_card.unlock_hotkey);
    if (const auto opacity = read_integer(path, L"pinned_card_opacity_percent")) {
        settings.pinned_card.opacity_percent = static_cast<int>(std::clamp(
            *opacity,
            static_cast<long>(PinnedCardPreferences::minimum_opacity_percent),
            static_cast<long>(PinnedCardPreferences::maximum_opacity_percent)));
    }
    if (const auto opacity = read_integer(path, L"ghost_reticle_opacity_percent")) {
        settings.ghost_reticle.opacity_percent = static_cast<int>(std::clamp(
            *opacity,
            static_cast<long>(GhostReticlePreferences::minimum_opacity_percent),
            static_cast<long>(GhostReticlePreferences::maximum_opacity_percent)));
    }
    if (const auto width = read_integer(path, L"ghost_reticle_width")) {
        settings.ghost_reticle.width = static_cast<int>(std::clamp(
            *width, static_cast<long>(GhostReticlePreferences::minimum_width),
            static_cast<long>(GhostReticlePreferences::maximum_width)));
    }
    if (const auto compensation = read_double(
            path, L"ghost_reticle_bearing_compensation_deg")) {
        settings.ghost_reticle.bearing_compensation_deg = std::clamp(
            *compensation,
            GhostReticlePreferences::minimum_bearing_compensation_deg,
            GhostReticlePreferences::maximum_bearing_compensation_deg);
    }
    if (const auto width = read_integer(path, L"ghost_preset_screen_width"))
        settings.ghost_reticle.preset_screen_width = static_cast<int>(*width);
    if (const auto height = read_integer(path, L"ghost_preset_screen_height"))
        settings.ghost_reticle.preset_screen_height = static_cast<int>(*height);
    settings.ghost_reticle.preferred_arc =
        read_value(path, L"ghost_reticle_preferred_arc", L"low") == L"high"
            ? Arc::high : Arc::low;
    if (const auto width = read_integer(path, L"ghost_reticle_line_width_tenths")) {
        settings.ghost_reticle.line_width_tenths = static_cast<int>(std::clamp(
            *width,
            static_cast<long>(GhostReticlePreferences::minimum_line_width_tenths),
            static_cast<long>(GhostReticlePreferences::maximum_line_width_tenths)));
    } else if (const auto legacy = read_integer(
                   path, L"ghost_reticle_stroke_scale_percent")) {
        settings.ghost_reticle.line_width_tenths = static_cast<int>(std::clamp(
            std::lround(27.0 * *legacy / 100.0),
            static_cast<long>(GhostReticlePreferences::minimum_line_width_tenths),
            static_cast<long>(GhostReticlePreferences::maximum_line_width_tenths)));
    }
    if (const auto width = read_integer(path, L"ghost_reticle_outline_width_tenths"))
        settings.ghost_reticle.outline_width_tenths = static_cast<int>(std::clamp(
            *width,
            static_cast<long>(GhostReticlePreferences::minimum_outline_width_tenths),
            static_cast<long>(GhostReticlePreferences::maximum_outline_width_tenths)));
    const auto outline = read_value(path, L"ghost_reticle_outline_enabled", L"");
    settings.ghost_reticle.outline_enabled = outline.empty()
        ? read_value(path, L"ghost_reticle_readability", L"0") == L"1"
        : outline == L"1";
    if (const auto hue = read_integer(path, L"ghost_reticle_color_hue"))
        settings.ghost_reticle.color_hue = static_cast<int>(std::clamp(
            *hue, static_cast<long>(GhostReticlePreferences::minimum_hue),
            static_cast<long>(GhostReticlePreferences::maximum_hue)));
    if (const auto saturation = read_integer(path, L"ghost_reticle_color_saturation"))
        settings.ghost_reticle.color_saturation = static_cast<int>(std::clamp(
            *saturation,
            static_cast<long>(GhostReticlePreferences::minimum_color_percent),
            static_cast<long>(GhostReticlePreferences::maximum_color_percent)));
    if (const auto lightness = read_integer(path, L"ghost_reticle_color_lightness"))
        settings.ghost_reticle.color_lightness = static_cast<int>(std::clamp(
            *lightness,
            static_cast<long>(GhostReticlePreferences::minimum_color_percent),
            static_cast<long>(GhostReticlePreferences::maximum_color_percent)));
    if (!read_integer(path, L"ghost_reticle_color_hue")) {
        const auto legacy = read_value(path, L"ghost_reticle_color_preset", L"red");
        if (legacy == L"green" || legacy == L"violet") {
            settings.ghost_reticle.color_hue = 145;
            settings.ghost_reticle.color_saturation = 100;
            settings.ghost_reticle.color_lightness = 39;
        } else if (legacy == L"cyan") {
            settings.ghost_reticle.color_hue = 187;
            settings.ghost_reticle.color_saturation = 100;
            settings.ghost_reticle.color_lightness = 42;
        } else if (legacy == L"yellow" || legacy == L"amber") {
            settings.ghost_reticle.color_hue = 50;
            settings.ghost_reticle.color_saturation = 100;
            settings.ghost_reticle.color_lightness = 50;
        } else if (legacy == L"white") {
            settings.ghost_reticle.color_hue = 0;
            settings.ghost_reticle.color_saturation = 0;
            settings.ghost_reticle.color_lightness = 100;
        }
    }
    const std::wstring monitor = read_value(path, L"capture_monitor", L"");
    const auto left = read_integer(path, L"capture_left");
    const auto top = read_integer(path, L"capture_top");
    const auto right = read_integer(path, L"capture_right");
    const auto bottom = read_integer(path, L"capture_bottom");
    if (!monitor.empty() && left && top && right && bottom && *left >= 0 &&
        *top >= 0 && *right > *left && *bottom > *top) {
        settings.capture_region = CaptureRegion{
            monitor, {static_cast<LONG>(*left), static_cast<LONG>(*top),
                      static_cast<LONG>(*right), static_cast<LONG>(*bottom)}};
    }

    const long custom_count =
        read_integer(path, L"hotkey_profiles", L"count").value_or(0);
    for (long index = 0;
         index < custom_count &&
         index < static_cast<long>(max_custom_hotkey_profiles);
         ++index) {
        const std::wstring prefix =
            L"profile" + std::to_wstring(index) + L"_";
        HotkeyProfile profile;
        profile.id = read_value(path, L"hotkey_profiles",
                                (prefix + L"id").c_str(), L"");
        profile.name = decode_profile_name(read_value(
            path, L"hotkey_profiles", (prefix + L"name").c_str(), L""));
        if (profile.id.empty() || profile.name.empty()) continue;
        profile.description = L"自定义配置";
        for (std::size_t action = 0; action < profile_hotkey_keys.size();
             ++action) {
            const std::wstring key = prefix + profile_hotkey_keys[action];
            profile.hotkeys[action] =
                read_value(path, L"hotkey_profiles", key.c_str(), L"");
            const std::wstring trigger_key = key + L"_trigger";
            profile.triggers[action] = hotkey_trigger_from_name(
                read_value(path, L"hotkey_profiles", trigger_key.c_str(),
                           L"single"));
        }
        settings.custom_hotkey_profiles.push_back(std::move(profile));
    }

    settings.hotkey_profile =
        read_value(path, L"hotkey_profile", L"classic");
    const auto find_profile = [&](const std::wstring& id) {
        for (const auto& profile : builtin_hotkey_profiles()) {
            if (profile.id == id) return &profile;
        }
        for (const auto& profile : settings.custom_hotkey_profiles) {
            if (profile.id == id) return &profile;
        }
        return static_cast<const HotkeyProfile*>(nullptr);
    };
    const HotkeyProfile* selected = find_profile(settings.hotkey_profile);
    if (!selected) {
        const auto& builtins = builtin_hotkey_profiles();
        const auto match = std::find_if(
            builtins.begin(), builtins.end(),
            [&](const HotkeyProfile& profile) {
                return profile_matches_settings(profile, settings);
            });
        if (match != builtins.end()) {
            settings.hotkey_profile = match->id;
        } else {
            HotkeyProfile profile = custom_profile_from_settings(
                settings, unique_custom_profile_id(settings), L"我的配置");
            settings.hotkey_profile = profile.id;
            settings.custom_hotkey_profiles.push_back(std::move(profile));
        }
    } else if (selected->builtin) {
        if (!profile_matches_settings(*selected, settings)) {
            HotkeyProfile profile = custom_profile_from_settings(
                settings, unique_custom_profile_id(settings), L"我的配置");
            settings.hotkey_profile = profile.id;
            settings.custom_hotkey_profiles.push_back(std::move(profile));
        }
    } else {
        for (auto& profile : settings.custom_hotkey_profiles) {
            if (profile.id == settings.hotkey_profile) {
                profile.hotkeys = active_hotkeys(settings);
                profile.triggers = active_triggers(settings);
                break;
            }
        }
    }
    return settings;
}

void save_settings_to(const std::filesystem::path& path,
                      const AppSettings& settings) {
    std::filesystem::create_directories(path.parent_path());
    write_value(path, L"region_hotkey", settings.region_hotkey);
    write_value(path, L"base_hotkey", settings.base_hotkey);
    write_value(path, L"target_hotkey", settings.target_hotkey);
    write_value(path, L"quick_target_hotkey", settings.quick_target_hotkey);
    write_value(path, L"impact_hotkey", settings.impact_hotkey);
    write_value(path, L"ghost_arc_hotkey", settings.ghost_arc_hotkey);
    write_value(path, L"ghost_toggle_hotkey", settings.ghost_toggle_hotkey);
    write_value(path, L"region_hotkey_trigger",
                std::wstring{hotkey_trigger_name(settings.region_trigger)});
    write_value(path, L"base_hotkey_trigger",
                std::wstring{hotkey_trigger_name(settings.base_trigger)});
    write_value(path, L"target_hotkey_trigger",
                std::wstring{hotkey_trigger_name(settings.target_trigger)});
    write_value(path, L"quick_target_hotkey_trigger",
                std::wstring{hotkey_trigger_name(settings.quick_target_trigger)});
    write_value(path, L"impact_hotkey_trigger",
                std::wstring{hotkey_trigger_name(settings.impact_trigger)});
    write_value(path, L"ghost_arc_hotkey_trigger",
                std::wstring{hotkey_trigger_name(settings.ghost_arc_trigger)});
    write_value(path, L"ghost_toggle_hotkey_trigger",
                std::wstring{hotkey_trigger_name(settings.ghost_toggle_trigger)});
    write_value(path, L"hotkey_profile", settings.hotkey_profile);
    write_value(path, L"coordinate_pattern", settings.coordinate_pattern);
    write_value(path, L"ocr_backend",
                settings.backend == OcrBackend::rapid ? L"rapid" : L"windows");
    write_value(path, L"pinned_card_locked",
                settings.pinned_card.locked ? L"1" : L"0");
    write_value(path, L"pinned_card_unlock_hotkey",
                settings.pinned_card.unlock_hotkey);
    write_value(path, L"pinned_card_opacity_percent",
                std::to_wstring(std::clamp(
                    settings.pinned_card.opacity_percent,
                    PinnedCardPreferences::minimum_opacity_percent,
                    PinnedCardPreferences::maximum_opacity_percent)));
    write_value(path, L"ghost_reticle_opacity_percent",
                std::to_wstring(std::clamp(
                    settings.ghost_reticle.opacity_percent,
                    GhostReticlePreferences::minimum_opacity_percent,
                    GhostReticlePreferences::maximum_opacity_percent)));
    write_value(path, L"ghost_reticle_width",
                std::to_wstring(std::clamp(
                    settings.ghost_reticle.width,
                    GhostReticlePreferences::minimum_width,
                    GhostReticlePreferences::maximum_width)));
    write_value(
        path, L"ghost_reticle_bearing_compensation_deg",
        std::to_wstring(std::clamp(
            settings.ghost_reticle.bearing_compensation_deg,
            GhostReticlePreferences::minimum_bearing_compensation_deg,
            GhostReticlePreferences::maximum_bearing_compensation_deg)));
    write_value(path, L"ghost_preset_screen_width",
                std::to_wstring(settings.ghost_reticle.preset_screen_width));
    write_value(path, L"ghost_preset_screen_height",
                std::to_wstring(settings.ghost_reticle.preset_screen_height));
    write_value(path, L"ghost_reticle_preferred_arc",
                settings.ghost_reticle.preferred_arc == Arc::high
                    ? L"high" : L"low");
    write_value(path, L"ghost_reticle_line_width_tenths",
                std::to_wstring(std::clamp(
                    settings.ghost_reticle.line_width_tenths,
                    GhostReticlePreferences::minimum_line_width_tenths,
                    GhostReticlePreferences::maximum_line_width_tenths)));
    write_value(path, L"ghost_reticle_outline_width_tenths",
                std::to_wstring(std::clamp(
                    settings.ghost_reticle.outline_width_tenths,
                    GhostReticlePreferences::minimum_outline_width_tenths,
                    GhostReticlePreferences::maximum_outline_width_tenths)));
    write_value(path, L"ghost_reticle_outline_enabled",
                settings.ghost_reticle.outline_enabled ? L"1" : L"0");
    write_value(path, L"ghost_reticle_color_hue",
                std::to_wstring(std::clamp(
                    settings.ghost_reticle.color_hue,
                    GhostReticlePreferences::minimum_hue,
                    GhostReticlePreferences::maximum_hue)));
    write_value(path, L"ghost_reticle_color_saturation",
                std::to_wstring(std::clamp(
                    settings.ghost_reticle.color_saturation,
                    GhostReticlePreferences::minimum_color_percent,
                    GhostReticlePreferences::maximum_color_percent)));
    write_value(path, L"ghost_reticle_color_lightness",
                std::to_wstring(std::clamp(
                    settings.ghost_reticle.color_lightness,
                    GhostReticlePreferences::minimum_color_percent,
                    GhostReticlePreferences::maximum_color_percent)));
    if (settings.capture_region) {
        const auto& region = *settings.capture_region;
        write_value(path, L"capture_monitor", region.monitor_device);
        write_value(path, L"capture_left", std::to_wstring(region.relative.left));
        write_value(path, L"capture_top", std::to_wstring(region.relative.top));
        write_value(path, L"capture_right", std::to_wstring(region.relative.right));
        write_value(path, L"capture_bottom", std::to_wstring(region.relative.bottom));
    } else {
        for (const wchar_t* key : {L"capture_monitor", L"capture_left", L"capture_top",
                                   L"capture_right", L"capture_bottom"}) {
            remove_value(path, key);
        }
    }

    WritePrivateProfileStringW(L"hotkey_profiles", nullptr, nullptr,
                               path.c_str());
    std::vector<const HotkeyProfile*> custom_profiles;
    for (const auto& profile : settings.custom_hotkey_profiles) {
        if (!profile.builtin) custom_profiles.push_back(&profile);
    }
    write_value(path, L"hotkey_profiles", L"count",
                std::to_wstring(custom_profiles.size()));
    for (std::size_t index = 0; index < custom_profiles.size(); ++index) {
        const auto& profile = *custom_profiles[index];
        const std::wstring prefix =
            L"profile" + std::to_wstring(index) + L"_";
        write_value(path, L"hotkey_profiles", (prefix + L"id").c_str(),
                    profile.id);
        write_value(path, L"hotkey_profiles", (prefix + L"name").c_str(),
                    encode_profile_name(profile.name));
        for (std::size_t action = 0; action < profile_hotkey_keys.size();
             ++action) {
            const std::wstring key = prefix + profile_hotkey_keys[action];
            write_value(path, L"hotkey_profiles", key.c_str(),
                        profile.hotkeys[action]);
            write_value(path, L"hotkey_profiles",
                        (key + L"_trigger").c_str(),
                        std::wstring{hotkey_trigger_name(
                            profile.triggers[action])});
        }
    }
}

AppSettings load_settings() { return load_settings_from(settings_path()); }

void save_settings(const AppSettings& settings) {
    save_settings_to(settings_path(), settings);
}

}  // namespace wardogs
