#include "settings_dialog.hpp"

#include "wardogs/hotkeys.hpp"

#include <Windows.h>
#include <dwmapi.h>

#include <QComboBox>
#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <array>
#include <exception>
#include <regex>

namespace {

QString qtext(const std::wstring& value) { return QString::fromStdWString(value); }
QString error_text(const std::exception& error) {
    return QString::fromUtf8(error.what());
}

void enable_dark_title_bar(HWND window) {
    const BOOL enabled = TRUE;
    DwmSetWindowAttribute(window, 20, &enabled, sizeof(enabled));
}

}  // namespace

SettingsDialog::SettingsDialog(const wardogs::AppSettings& settings,
                               QWidget* parent)
    : QDialog(parent), capture_region_(settings.capture_region) {
    setWindowTitle(QStringLiteral("设置 · War Dogs 射表计算"));
    setModal(true);
    resize(660, 465);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 20);
    root->setSpacing(12);
    auto* title = new QLabel(QStringLiteral("偏好设置"));
    title->setObjectName(QStringLiteral("dialogTitle"));
    root->addWidget(title);
    auto* subtitle = new QLabel(
        QStringLiteral("修改后立即保存；OCR 区域也会在下次启动时恢复"));
    subtitle->setObjectName(QStringLiteral("muted"));
    root->addWidget(subtitle);

    auto* panel = new QGroupBox;
    auto* form = new QFormLayout(panel);
    form->setContentsMargins(14, 14, 14, 14);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    backend_ = new QComboBox;
    backend_->addItem(QStringLiteral("RapidOCR（首选）"), 0);
    backend_->addItem(QStringLiteral("Windows 系统 OCR（兼容备用）"), 1);
    backend_->setCurrentIndex(settings.backend == wardogs::OcrBackend::windows ? 1 : 0);
    form->addRow(QStringLiteral("OCR 引擎"), backend_);

    auto* hotkeys = new QWidget;
    auto* row = new QHBoxLayout(hotkeys);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    region_key_ = make_hotkey(settings.region_hotkey, row,
                              QStringLiteral("设置区域"));
    base_key_ = make_hotkey(settings.base_hotkey, row, QStringLiteral("基准"));
    target_key_ = make_hotkey(settings.target_hotkey, row, QStringLiteral("目标"));
    quick_target_key_ = make_hotkey(settings.quick_target_hotkey, row,
                                    QStringLiteral("快速目标"));
    form->addRow(QStringLiteral("全局热键"), hotkeys);

    pattern_ = new QPlainTextEdit(qtext(settings.coordinate_pattern));
    pattern_->setMinimumHeight(112);
    pattern_->setToolTip(QStringLiteral("前两个捕获组依次为 x 和 y"));
    form->addRow(QStringLiteral("坐标正则\n捕获组 x / y"), pattern_);
    root->addWidget(panel, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save |
                                          QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存设置"));
    buttons->button(QDialogButtonBox::Save)->setProperty("primary", true);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this,
            &SettingsDialog::accept_if_valid);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* footer = new QHBoxLayout;
    auto* version = new QLabel(
        QStringLiteral("当前版本  v%1").arg(QApplication::applicationVersion()));
    version->setObjectName(QStringLiteral("muted"));
    footer->addWidget(version);
    footer->addStretch();
    footer->addWidget(buttons);
    root->addLayout(footer);
    enable_dark_title_bar(reinterpret_cast<HWND>(winId()));
}

wardogs::AppSettings SettingsDialog::settings() const {
    wardogs::AppSettings value;
    value.backend = backend_->currentData().toInt() == 1
                        ? wardogs::OcrBackend::windows
                        : wardogs::OcrBackend::rapid;
    value.region_hotkey = hotkey_text(region_key_);
    value.base_hotkey = hotkey_text(base_key_);
    value.target_hotkey = hotkey_text(target_key_);
    value.quick_target_hotkey = hotkey_text(quick_target_key_);
    value.coordinate_pattern = pattern_->toPlainText().trimmed().toStdWString();
    value.capture_region = capture_region_;
    return value;
}

QKeySequenceEdit* SettingsDialog::make_hotkey(const std::wstring& value,
                                               QHBoxLayout* layout,
                                               const QString& label) {
    auto* block = new QWidget;
    auto* column = new QVBoxLayout(block);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);
    auto* caption = new QLabel(label);
    caption->setObjectName(QStringLiteral("muted"));
    auto* editor = new QKeySequenceEdit(QKeySequence(qtext(value)));
    editor->setMaximumSequenceLength(1);
    column->addWidget(caption);
    column->addWidget(editor);
    layout->addWidget(block, 1);
    return editor;
}

std::wstring SettingsDialog::hotkey_text(const QKeySequenceEdit* editor) {
    return editor->keySequence().toString(QKeySequence::PortableText).toStdWString();
}

void SettingsDialog::accept_if_valid() {
    try {
        const auto value = settings();
        const std::array hotkeys{
            wardogs::parse_hotkey(value.region_hotkey),
            wardogs::parse_hotkey(value.base_hotkey),
            wardogs::parse_hotkey(value.target_hotkey),
            wardogs::parse_hotkey(value.quick_target_hotkey),
        };
        wardogs::validate_unique_hotkeys(hotkeys);
        const std::wregex pattern(value.coordinate_pattern,
                                  std::regex_constants::icase);
        if (pattern.mark_count() < 2)
            throw std::invalid_argument("coordinate regex needs x/y capture groups");
        QDialog::accept();
    } catch (const std::regex_error&) {
        QMessageBox::warning(this, QStringLiteral("坐标正则不可用"),
                             QStringLiteral("坐标正则语法无效"));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, QStringLiteral("设置不可用"), error_text(error));
    }
}
