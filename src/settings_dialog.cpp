#include "settings_dialog.hpp"
#include "window_title_bar.hpp"

#include "wardogs/hotkeys.hpp"

#include <QComboBox>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
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
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <exception>
#include <regex>
#include <utility>

namespace {

QString qtext(const std::wstring& value) { return QString::fromStdWString(value); }
QString error_text(const std::exception& error) {
    return QString::fromUtf8(error.what());
}

QString hsl_color_css(int hue, int saturation, int lightness) {
    return QColor::fromHsl(hue, qRound(saturation * 2.55),
                           qRound(lightness * 2.55)).name();
}

class HslColorDialog final : public QDialog {
public:
    HslColorDialog(int hue, int saturation, int lightness, QWidget* parent)
        : QDialog(parent), hue_(hue), saturation_(saturation),
          lightness_(lightness) {
        setWindowTitle(QStringLiteral("幽灵分划颜色"));
        setModal(true);
        resize(560, 340);
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(18, 18, 18, 18);
        root->setSpacing(12);
        preview_ = new QLabel;
        preview_->setFixedHeight(54);
        preview_->setAlignment(Qt::AlignCenter);
        root->addWidget(preview_);

        hue_slider_ = add_channel(root, QStringLiteral("色相 H"), 0, 359, hue_);
        saturation_slider_ = add_channel(root, QStringLiteral("饱和度 S"), 0, 100,
                                         saturation_);
        lightness_slider_ = add_channel(root, QStringLiteral("亮度 L"), 0, 100,
                                        lightness_);

        auto* presets = new QHBoxLayout;
        presets->setSpacing(8);
        struct Preset { const char* name; int h; int s; int l; };
        const std::array<Preset, 5> values{{
            {"红", 3, 100, 59}, {"绿", 145, 100, 39},
            {"青", 187, 100, 42}, {"黄", 50, 100, 50},
            {"白", 0, 0, 100},
        }};
        for (const auto& preset : values) {
            auto* button = new QPushButton(QString::fromUtf8(preset.name));
            button->setStyleSheet(
                QStringLiteral("QPushButton{border-left:12px solid %1;}")
                    .arg(hsl_color_css(preset.h, preset.s, preset.l)));
            connect(button, &QPushButton::clicked, this, [this, preset] {
                hue_slider_->setValue(preset.h);
                saturation_slider_->setValue(preset.s);
                lightness_slider_->setValue(preset.l);
            });
            presets->addWidget(button);
        }
        root->addLayout(presets);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                              QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("应用"));
        buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);
        update_preview();
    }

    [[nodiscard]] int hue() const { return hue_slider_->value(); }
    [[nodiscard]] int saturation() const { return saturation_slider_->value(); }
    [[nodiscard]] int lightness() const { return lightness_slider_->value(); }

private:
    QSlider* add_channel(QVBoxLayout* root, const QString& label,
                         int minimum, int maximum, int value) {
        auto* row = new QHBoxLayout;
        auto* caption = new QLabel(label);
        caption->setFixedWidth(62);
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(minimum, maximum);
        slider->setValue(value);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setTickInterval(maximum == 359 ? 30 : 10);
        auto* number = new QLabel;
        number->setFixedWidth(38);
        number->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        number->setText(QString::number(value));
        connect(slider, &QSlider::valueChanged, this,
                [this, number](int changed) {
                    number->setText(QString::number(changed));
                    update_preview();
                });
        row->addWidget(caption);
        row->addWidget(slider, 1);
        row->addWidget(number);
        root->addLayout(row);
        return slider;
    }

    void update_preview() {
        if (!preview_ || !hue_slider_ || !saturation_slider_ ||
            !lightness_slider_) return;
        const auto color = hsl_color_css(hue_slider_->value(),
                                         saturation_slider_->value(),
                                         lightness_slider_->value());
        preview_->setText(QStringLiteral("H %1  ·  S %2  ·  L %3")
                              .arg(hue_slider_->value())
                              .arg(saturation_slider_->value())
                              .arg(lightness_slider_->value()));
        preview_->setStyleSheet(
            QStringLiteral("background:%1;color:%2;border-radius:7px;"
                           "font-weight:700;")
                .arg(color,
                     lightness_slider_->value() > 65
                         ? QStringLiteral("#0b1018")
                         : QStringLiteral("#f8fafc")));

        const auto slider_style = [](const QString& gradient) {
            return QStringLiteral(
                "QSlider::groove:horizontal{height:8px;border-radius:4px;"
                "background:%1;}"
                "QSlider::handle:horizontal{background:#f8fafc;"
                "border:1px solid #64748b;width:15px;margin:-5px 0;"
                "border-radius:7px;}"
                "QSlider::handle:horizontal:hover{border-color:#38bdf8;}")
                .arg(gradient);
        };
        hue_slider_->setStyleSheet(slider_style(QStringLiteral(
            "qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #ff0000,stop:0.167 #ffff00,stop:0.333 #00ff00,"
            "stop:0.5 #00ffff,stop:0.667 #0000ff,stop:0.833 #ff00ff,"
            "stop:1 #ff0000)")));
        const auto gray = hsl_color_css(hue_slider_->value(), 0,
                                        lightness_slider_->value());
        const auto saturated = hsl_color_css(hue_slider_->value(), 100,
                                             lightness_slider_->value());
        saturation_slider_->setStyleSheet(slider_style(
            QStringLiteral("qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                           "stop:0 %1,stop:1 %2)")
                .arg(gray, saturated)));
        const auto middle = hsl_color_css(hue_slider_->value(),
                                          saturation_slider_->value(), 50);
        lightness_slider_->setStyleSheet(slider_style(
            QStringLiteral("qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                           "stop:0 #000000,stop:0.5 %1,stop:1 #ffffff)")
                .arg(middle)));
    }

    int hue_{};
    int saturation_{};
    int lightness_{};
    QLabel* preview_{};
    QSlider* hue_slider_{};
    QSlider* saturation_slider_{};
    QSlider* lightness_slider_{};
};

}  // namespace

SettingsDialog::SettingsDialog(const wardogs::AppSettings& settings,
                               GhostPreview ghost_preview, QWidget* parent)
    : QDialog(parent), capture_region_(settings.capture_region),
      pinned_card_(settings.pinned_card), ghost_reticle_(settings.ghost_reticle),
      ghost_preview_(std::move(ghost_preview)) {
    configure_frameless_window(this);
    setWindowTitle(QStringLiteral("设置 · War Dogs 射表计算"));
    setModal(true);
    resize(800, 620);
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

    auto* style_row = new QWidget;
    auto* style_layout = new QHBoxLayout(style_row);
    style_layout->setContentsMargins(0, 0, 0, 0);
    style_layout->setSpacing(8);
    ghost_outline_ = new QCheckBox(QStringLiteral("描边"));
    ghost_outline_->setObjectName(QStringLiteral("ghostOutlineEnabled"));
    ghost_outline_->setChecked(settings.ghost_reticle.outline_enabled);
    style_layout->addWidget(ghost_outline_);

    const auto add_width = [style_layout](const QString& caption,
                                          const QString& object_name,
                                          int minimum, int maximum, int value,
                                          QSlider*& slider, QLabel*& label) {
        auto* title = new QLabel(caption);
        title->setObjectName(QStringLiteral("muted"));
        style_layout->addWidget(title);
        slider = new QSlider(Qt::Horizontal);
        slider->setObjectName(object_name);
        slider->setProperty("pinnedMenuSlider", true);
        slider->setRange(minimum, maximum);
        slider->setValue(value);
        slider->setSingleStep(1);
        slider->setPageStep(5);
        slider->setTickInterval(5);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setMinimumWidth(92);
        style_layout->addWidget(slider, 1);
        label = new QLabel(QStringLiteral("%1 px").arg(value / 10.0, 0, 'f', 1));
        label->setMinimumWidth(43);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        style_layout->addWidget(label);
    };
    add_width(QStringLiteral("线宽"), QStringLiteral("ghostLineWidth"),
              wardogs::GhostReticlePreferences::minimum_line_width_tenths,
              wardogs::GhostReticlePreferences::maximum_line_width_tenths,
              settings.ghost_reticle.line_width_tenths,
              ghost_line_width_, ghost_line_width_value_);
    add_width(QStringLiteral("描边线框"), QStringLiteral("ghostOutlineWidth"),
              wardogs::GhostReticlePreferences::minimum_outline_width_tenths,
              wardogs::GhostReticlePreferences::maximum_outline_width_tenths,
              settings.ghost_reticle.outline_width_tenths,
              ghost_outline_width_, ghost_outline_width_value_);
    ghost_color_button_ = new QPushButton(QStringLiteral("颜色"));
    ghost_color_button_->setObjectName(QStringLiteral("ghostColorButton"));
    ghost_color_button_->setToolTip(QStringLiteral("打开 HSL 调色盘或选择预设颜色"));
    style_layout->addWidget(ghost_color_button_);
    form->addRow(QStringLiteral("分划样式"), style_row);
    refresh_ghost_color_button();

    connect(ghost_outline_, &QCheckBox::toggled,
            this, [this](bool) { notify_ghost_preview(); });
    connect(ghost_line_width_, &QSlider::valueChanged, this, [this](int value) {
        ghost_line_width_value_->setText(
            QStringLiteral("%1 px").arg(value / 10.0, 0, 'f', 1));
        notify_ghost_preview();
    });
    connect(ghost_outline_width_, &QSlider::valueChanged, this, [this](int value) {
        ghost_outline_width_value_->setText(
            QStringLiteral("%1 px").arg(value / 10.0, 0, 'f', 1));
        notify_ghost_preview();
    });
    connect(ghost_color_button_, &QPushButton::clicked,
            this, &SettingsDialog::choose_ghost_color);

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
    connect(ghost_bearing_compensation_,
            qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double) { notify_ghost_preview(); });
    connect(ghost_preset_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
                const QSize preset = ghost_preset_->currentData().toSize();
                if (const auto width = wardogs::ghost_preset_width(
                        preset.width(), preset.height())) {
                    ghost_reticle_.preset_screen_width = preset.width();
                    ghost_reticle_.preset_screen_height = preset.height();
                    ghost_reticle_.width = *width;
                }
                notify_ghost_preview();
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
    value.ghost_reticle.outline_enabled = ghost_outline_->isChecked();
    value.ghost_reticle.line_width_tenths = ghost_line_width_->value();
    value.ghost_reticle.outline_width_tenths =
        ghost_outline_width_->value();
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

// Preview only: the dialog never persists anything, MainWindow owns saving.
void SettingsDialog::notify_ghost_preview() {
    if (!ghost_preview_) return;
    ghost_preview_(settings().ghost_reticle);
}

void SettingsDialog::choose_ghost_color() {
    HslColorDialog dialog(ghost_reticle_.color_hue,
                          ghost_reticle_.color_saturation,
                          ghost_reticle_.color_lightness, this);
    if (dialog.exec() != QDialog::Accepted) return;
    ghost_reticle_.color_hue = dialog.hue();
    ghost_reticle_.color_saturation = dialog.saturation();
    ghost_reticle_.color_lightness = dialog.lightness();
    refresh_ghost_color_button();
    notify_ghost_preview();
}

void SettingsDialog::refresh_ghost_color_button() {
    if (!ghost_color_button_) return;
    const auto color = hsl_color_css(ghost_reticle_.color_hue,
                                     ghost_reticle_.color_saturation,
                                     ghost_reticle_.color_lightness);
    ghost_color_button_->setText(QStringLiteral("颜色  %1").arg(color.toUpper()));
    ghost_color_button_->setStyleSheet(
        QStringLiteral("QPushButton{border-left:14px solid %1;}").arg(color));
}
