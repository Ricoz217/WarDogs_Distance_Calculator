#pragma once

#include "wardogs/settings.hpp"

#include <QDialog>

#include <functional>
#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QHBoxLayout;
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
    static QKeySequenceEdit* make_hotkey(const std::wstring& value,
                                         QHBoxLayout* layout,
                                         const QString& label);
    static std::wstring hotkey_text(const QKeySequenceEdit* editor);
    void accept_if_valid();
    void request_ghost_adjustment();
    void notify_ghost_preview();
    void choose_ghost_color();
    void refresh_ghost_color_button();

    QComboBox* backend_{};
    QKeySequenceEdit* region_key_{};
    QKeySequenceEdit* base_key_{};
    QKeySequenceEdit* target_key_{};
    QKeySequenceEdit* quick_target_key_{};
    QKeySequenceEdit* impact_key_{};
    QKeySequenceEdit* ghost_arc_key_{};
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
