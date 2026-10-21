#pragma once

#include "wardogs/settings.hpp"

#include <QDialog>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QKeySequenceEdit;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSlider;

class SettingsDialog final : public QDialog {
public:
    using GhostPreview =
        std::function<void(const wardogs::GhostReticlePreferences&)>;

    explicit SettingsDialog(const wardogs::AppSettings& settings,
                            GhostPreview ghost_preview = {},
                            QWidget* parent = nullptr);

    [[nodiscard]] wardogs::AppSettings settings() const;
    [[nodiscard]] bool adjust_ghost_requested() const {
        return adjust_ghost_requested_;
    }

protected:
    bool nativeEvent(const QByteArray& event_type, void* message,
                     qintptr* result) override;

private:
    struct HotkeyRow {
        QFrame* frame{};
        QKeySequenceEdit* editor{};
        QPushButton* trigger{};
    };

    QWidget* build_engine_profile_row(int backend);
    QWidget* build_hotkey_table(const wardogs::AppSettings& settings);
    void update_backend_selector();
    void show_backend_picker();
    void apply_backend(int backend);
    void update_profile_selector();
    void show_profile_picker();
    void apply_profile(const std::wstring& id, bool announce);
    void mark_hotkeys_edited();
    void update_profile_notice(const QString& extra = {});
    void manage_hotkey_profiles();
    void update_active_hotkey_row();
    void accept_if_valid();
    void request_ghost_adjustment();
    void notify_ghost_preview();
    void choose_ghost_color();
    void refresh_ghost_color_button();

    [[nodiscard]] int backend_value() const;
    [[nodiscard]] std::array<std::wstring, 7> current_hotkeys() const;
    [[nodiscard]] std::array<wardogs::HotkeyTrigger, 7> current_triggers() const;
    [[nodiscard]] bool profile_matches_table(
        const wardogs::HotkeyProfile& profile) const;
    [[nodiscard]] const wardogs::HotkeyProfile* find_hotkey_profile(
        const std::wstring& id) const;
    [[nodiscard]] std::size_t custom_profile_count() const;
    [[nodiscard]] std::wstring new_custom_profile_id() const;
    [[nodiscard]] std::wstring unique_profile_name(
        const std::wstring& base, const std::wstring& ignore_id = {}) const;
    [[nodiscard]] bool commit_hotkey_profile();
    void apply_hotkeys_to_table(const wardogs::HotkeyProfile& profile);

    QPushButton* backend_selector_{};
    QPushButton* profile_selector_{};
    QLabel* profile_notice_{};
    std::vector<wardogs::HotkeyProfile> hotkey_profiles_;
    std::wstring selected_hotkey_profile_;
    bool populating_hotkeys_{};
    bool hotkey_profile_dirty_{};
    std::array<HotkeyRow, 7> hotkey_rows_{};
    QComboBox* ghost_preset_{};
    QPushButton* ghost_outline_{};
    QSlider* ghost_line_width_{};
    QSlider* ghost_outline_width_{};
    QLabel* ghost_line_width_value_{};
    QLabel* ghost_outline_width_value_{};
    QPushButton* ghost_color_button_{};
    QDoubleSpinBox* ghost_bearing_compensation_{};
    QPlainTextEdit* pattern_{};
    std::optional<wardogs::CaptureRegion> capture_region_;
    wardogs::PinnedCardPreferences pinned_card_;
    wardogs::GhostReticlePreferences ghost_reticle_;
    GhostPreview ghost_preview_;
    bool adjust_ghost_requested_{};
};
