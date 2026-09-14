#pragma once

#include "wardogs/settings.hpp"

#include <QDialog>

#include <optional>

class QComboBox;
class QHBoxLayout;
class QKeySequenceEdit;
class QPlainTextEdit;

class SettingsDialog final : public QDialog {
public:
    explicit SettingsDialog(const wardogs::AppSettings& settings,
                            QWidget* parent = nullptr);

    [[nodiscard]] wardogs::AppSettings settings() const;

private:
    static QKeySequenceEdit* make_hotkey(const std::wstring& value,
                                         QHBoxLayout* layout,
                                         const QString& label);
    static std::wstring hotkey_text(const QKeySequenceEdit* editor);
    void accept_if_valid();

    QComboBox* backend_{};
    QKeySequenceEdit* region_key_{};
    QKeySequenceEdit* base_key_{};
    QKeySequenceEdit* target_key_{};
    QKeySequenceEdit* quick_target_key_{};
    QPlainTextEdit* pattern_{};
    std::optional<wardogs::CaptureRegion> capture_region_;
};
