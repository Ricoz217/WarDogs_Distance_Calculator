#include "settings_dialog.hpp"
#include "window_title_bar.hpp"

#include "wardogs/hotkeys.hpp"

#include <QComboBox>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
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

}  // namespace

SettingsDialog::SettingsDialog(const wardogs::AppSettings& settings,
                               QWidget* parent)
    : QDialog(parent), capture_region_(settings.capture_region),
      pinned_card_(settings.pinned_card), ghost_reticle_(settings.ghost_reticle) {
    configure_frameless_window(this);
    setWindowTitle(QStringLiteral("设置 · War Dogs 射表计算"));
    setModal(true);
    resize(720, 580);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(new WindowTitleBar(this));
    auto* content = new QWidget;
    auto* root = new QVBoxLayout(content);
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

    auto* hotkeys_first = new QWidget;
    auto* first_row = new QHBoxLayout(hotkeys_first);
    first_row->setContentsMargins(0, 0, 0, 0);
    first_row->setSpacing(10);
    region_key_ = make_hotkey(settings.region_hotkey, first_row,
                              QStringLiteral("设置区域"));
    base_key_ = make_hotkey(settings.base_hotkey, first_row, QStringLiteral("基准"));
    target_key_ = make_hotkey(settings.target_hotkey, first_row, QStringLiteral("目标"));
    form->addRow(QStringLiteral("全局热键"), hotkeys_first);

    auto* hotkeys_second = new QWidget;
    auto* second_row = new QHBoxLayout(hotkeys_second);
    second_row->setContentsMargins(0, 0, 0, 0);
    second_row->setSpacing(10);
    quick_target_key_ = make_hotkey(settings.quick_target_hotkey, second_row,
                                    QStringLiteral("快速目标"));
    impact_key_ = make_hotkey(settings.impact_hotkey, second_row,
                              QStringLiteral("OCR落点"));
    impact_key_->setToolTip(
        QStringLiteral("两发校准及持续校准共用的 OCR 落点热键"));
    ghost_arc_key_ = make_hotkey(settings.ghost_arc_hotkey, second_row,
                                QStringLiteral("切换弹道"));
    ghost_arc_key_->setToolTip(
        QStringLiteral("切换幽灵分划优先显示的低射/高抛解"));
    form->addRow(QString{}, hotkeys_second);

    auto* ghost_row = new QWidget;
    auto* ghost_layout = new QHBoxLayout(ghost_row);
    ghost_layout->setContentsMargins(0, 0, 0, 0);
    ghost_layout->setSpacing(10);
    ghost_preset_ = new QComboBox;
    ghost_preset_->setObjectName(QStringLiteral("ghostReticlePreset"));
    const std::array<QSize, 4> ghost_presets{
        QSize{1280, 720}, QSize{1600, 900},
        QSize{1920, 1080}, QSize{2560, 1440},
    };
    const auto saved_preset_width = wardogs::ghost_preset_width(
        settings.ghost_reticle.preset_screen_width,
        settings.ghost_reticle.preset_screen_height);
    const bool saved_preset_matches =
        saved_preset_width && *saved_preset_width == settings.ghost_reticle.width;
    if (!saved_preset_matches) {
        ghost_preset_->addItem(
            QStringLiteral("自定义 · %1 × %2")
                .arg(settings.ghost_reticle.width)
                .arg(qRound(settings.ghost_reticle.width * 3.0 / 4.0)));
    }
    int selected_preset = saved_preset_matches ? -1 : 0;
    for (const QSize preset : ghost_presets) {
        ghost_preset_->addItem(
            QStringLiteral("%1 × %2").arg(preset.width()).arg(preset.height()),
            preset);
        if (saved_preset_matches &&
            preset.width() == settings.ghost_reticle.preset_screen_width &&
            preset.height() == settings.ghost_reticle.preset_screen_height)
            selected_preset = ghost_preset_->count() - 1;
    }
    if (selected_preset >= 0)
        ghost_preset_->setCurrentIndex(selected_preset);
    ghost_preset_->setToolTip(
        QStringLiteral("16:9 分辨率预设；瞄具外框保持屏幕宽度 37.5%"));
    ghost_layout->addWidget(ghost_preset_, 1);

    auto* decrease_compensation = new QPushButton(QStringLiteral("<"));
    decrease_compensation->setObjectName(
        QStringLiteral("decreaseGhostBearingCompensation"));
    decrease_compensation->setFixedWidth(32);
    decrease_compensation->setAutoRepeat(true);
    decrease_compensation->setToolTip(QStringLiteral("方位补偿减少 0.05°"));
    ghost_layout->addWidget(decrease_compensation);

    ghost_bearing_compensation_ = new QDoubleSpinBox;
    ghost_bearing_compensation_->setObjectName(
        QStringLiteral("ghostBearingCompensation"));
    ghost_bearing_compensation_->setRange(
        wardogs::GhostReticlePreferences::minimum_bearing_compensation_deg,
        wardogs::GhostReticlePreferences::maximum_bearing_compensation_deg);
    ghost_bearing_compensation_->setDecimals(2);
    ghost_bearing_compensation_->setSingleStep(
        wardogs::GhostReticlePreferences::bearing_compensation_step_deg);
    ghost_bearing_compensation_->setValue(
        settings.ghost_reticle.bearing_compensation_deg);
    ghost_bearing_compensation_->setSuffix(QStringLiteral("°"));
    ghost_bearing_compensation_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    ghost_bearing_compensation_->setAlignment(Qt::AlignCenter);
    ghost_bearing_compensation_->setFixedWidth(76);
    ghost_bearing_compensation_->setToolTip(
        QStringLiteral("幽灵方位分划补偿，该角度会加到计算方位上"));
    ghost_layout->addWidget(ghost_bearing_compensation_);

    auto* increase_compensation = new QPushButton(QStringLiteral(">"));
    increase_compensation->setObjectName(
        QStringLiteral("increaseGhostBearingCompensation"));
    increase_compensation->setFixedWidth(32);
    increase_compensation->setAutoRepeat(true);
    increase_compensation->setToolTip(QStringLiteral("方位补偿增加 0.05°"));
    ghost_layout->addWidget(increase_compensation);

    auto* adjust_ghost = new QPushButton(QStringLiteral("手动调整大小"));
    adjust_ghost->setObjectName(QStringLiteral("adjustGhostReticle"));
    adjust_ghost->setToolTip(
        QStringLiteral("进入屏幕居中的 4:3 分划边框调整模式"));
    ghost_layout->addWidget(adjust_ghost);
    form->addRow(QStringLiteral("幽灵分划"), ghost_row);
    connect(adjust_ghost, &QPushButton::clicked,
            this, &SettingsDialog::request_ghost_adjustment);
    connect(decrease_compensation, &QPushButton::clicked, this, [this] {
        ghost_bearing_compensation_->setValue(
            ghost_bearing_compensation_->value() -
            wardogs::GhostReticlePreferences::bearing_compensation_step_deg);
    });
    connect(increase_compensation, &QPushButton::clicked, this, [this] {
        ghost_bearing_compensation_->setValue(
            ghost_bearing_compensation_->value() +
            wardogs::GhostReticlePreferences::bearing_compensation_step_deg);
    });
    connect(ghost_preset_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
                const QSize preset = ghost_preset_->currentData().toSize();
                if (const auto width = wardogs::ghost_preset_width(
                        preset.width(), preset.height())) {
                    ghost_reticle_.preset_screen_width = preset.width();
                    ghost_reticle_.preset_screen_height = preset.height();
                    ghost_reticle_.width = *width;
                }
            });

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
    outer->addWidget(content);
    enable_rounded_window_corners(this);
}

bool SettingsDialog::nativeEvent(const QByteArray& event_type, void* message,
                                 qintptr* result) {
    if (handle_frameless_native_event(this, message, result)) return true;
    return QDialog::nativeEvent(event_type, message, result);
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
    value.impact_hotkey = hotkey_text(impact_key_);
    value.ghost_arc_hotkey = hotkey_text(ghost_arc_key_);
    value.coordinate_pattern = pattern_->toPlainText().trimmed().toStdWString();
    value.capture_region = capture_region_;
    value.pinned_card = pinned_card_;
    value.ghost_reticle = ghost_reticle_;
    value.ghost_reticle.bearing_compensation_deg =
        ghost_bearing_compensation_->value();
    if (ghost_preset_->currentData().canConvert<QSize>()) {
        const QSize preset = ghost_preset_->currentData().toSize();
        value.ghost_reticle.preset_screen_width = preset.width();
        value.ghost_reticle.preset_screen_height = preset.height();
    }
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
            wardogs::parse_hotkey(value.impact_hotkey),
            wardogs::parse_hotkey(value.ghost_arc_hotkey),
            wardogs::parse_hotkey(value.pinned_card.unlock_hotkey),
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

void SettingsDialog::request_ghost_adjustment() {
    adjust_ghost_requested_ = true;
    accept_if_valid();
    if (result() != QDialog::Accepted) adjust_ghost_requested_ = false;
}
