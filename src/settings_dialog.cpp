#include "settings_dialog.hpp"
#include "window_title_bar.hpp"

#include "wardogs/hotkeys.hpp"

#include <QComboBox>
#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <functional>
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

constexpr std::array<const char*, 7> hotkey_action_names{
    "设置区域", "基准点", "目标点", "快速目标", "OCR 落点", "切换弹道",
    "幽灵分划开关"};
constexpr std::array<const char*, 7> hotkey_action_keys{
    "region", "base", "target", "quickTarget", "impact", "ghostArc",
    "ghostToggle"};

QString action_name(std::size_t index) {
    return QString::fromUtf8(hotkey_action_names.at(index));
}

QString action_key(std::size_t index) {
    return QString::fromLatin1(hotkey_action_keys.at(index));
}

std::wstring hotkey_text(const QKeySequenceEdit* editor) {
    return editor->keySequence()
        .toString(QKeySequence::PortableText)
        .toStdWString();
}

QString trigger_label(wardogs::HotkeyTrigger trigger) {
    switch (trigger) {
    case wardogs::HotkeyTrigger::single: return QStringLiteral("单击");
    case wardogs::HotkeyTrigger::double_press: return QStringLiteral("双击");
    case wardogs::HotkeyTrigger::long_press: return QStringLiteral("长按");
    }
    return QStringLiteral("单击");
}

QString trigger_description(wardogs::HotkeyTrigger trigger) {
    switch (trigger) {
    case wardogs::HotkeyTrigger::single:
        return QStringLiteral("独立单击按下即触发；同键组合时等待手势判定");
    case wardogs::HotkeyTrigger::double_press:
        return QStringLiteral("系统双击窗口内按两次（默认约 500 ms）");
    case wardogs::HotkeyTrigger::long_press:
        return QStringLiteral("按住约 400 ms 后触发");
    }
    return {};
}

wardogs::HotkeyTrigger trigger_from_button(const QPushButton* button) {
    return static_cast<wardogs::HotkeyTrigger>(
        button->property("hotkeyTrigger").toInt());
}

void set_trigger_button(QPushButton* button, wardogs::HotkeyTrigger trigger) {
    const QString label = trigger_label(trigger);
    button->setText(label + QStringLiteral("  ▾"));
    button->setProperty("hotkeyTrigger", static_cast<int>(trigger));
    const QString action = button->property("hotkeyActionName").toString();
    button->setAccessibleName(action + QStringLiteral("触发方式：") + label);
}

QString trigger_key(wardogs::HotkeyTrigger trigger) {
    switch (trigger) {
    case wardogs::HotkeyTrigger::single: return QStringLiteral("single");
    case wardogs::HotkeyTrigger::double_press: return QStringLiteral("double");
    case wardogs::HotkeyTrigger::long_press: return QStringLiteral("long");
    }
    return QStringLiteral("single");
}

QFrame* make_separator() {
    auto* line = new QFrame;
    line->setObjectName(QStringLiteral("hotkeyTableSeparator"));
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(1);
    return line;
}

void set_choice_display(QPushButton* button, const QString& text) {
    const QString full = text + QStringLiteral("  ▾");
    const int available = std::max(24, button->width() - 22);
    button->setText(button->fontMetrics().elidedText(
        full, Qt::ElideRight, available));
    button->setToolTip(text);
}

constexpr std::array<int, 5> ghost_line_width_stops{15, 20, 27, 35, 45};
constexpr std::array<int, 5> ghost_outline_width_stops{5, 10, 16, 25, 35};

template <std::size_t Size>
int closest_stop(const std::array<int, Size>& stops, int value) {
    int closest = 0;
    for (int index = 1; index < static_cast<int>(Size); ++index) {
        if (std::abs(stops[index] - value) <
            std::abs(stops[closest] - value)) closest = index;
    }
    return closest;
}

QString pixel_width_text(int tenths) {
    return QStringLiteral("%1 px").arg(tenths / 10.0, 0, 'f', 1);
}

class HslPlane final : public QWidget {
public:
    explicit HslPlane(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(360, 220);
        setCursor(Qt::CrossCursor);
    }

    void set_color(int hue, int saturation, int lightness) {
        hue = std::clamp(hue, 0, 359);
        saturation_ = std::clamp(saturation, 0, 100);
        lightness_ = std::clamp(lightness, 0, 100);
        if (hue_ != hue) {
            hue_ = hue;
            image_ = {};
        }
        update();
    }

    void set_change_handler(std::function<void(int, int)> handler) {
        changed_ = std::move(handler);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (image_.size() != size()) {
            image_ = QImage(size(), QImage::Format_RGB32);
            for (int y = 0; y < image_.height(); ++y) {
                const int lightness = qRound(
                    100.0 * (image_.height() - 1 - y) /
                    std::max(1, image_.height() - 1));
                for (int x = 0; x < image_.width(); ++x) {
                    const int saturation = qRound(
                        100.0 * x / std::max(1, image_.width() - 1));
                    image_.setPixel(x, y,
                        QColor::fromHsl(hue_, qRound(saturation * 2.55),
                                        qRound(lightness * 2.55)).rgb());
                }
            }
        }
        QPainter painter(this);
        painter.drawImage(rect(), image_);
        const QPointF marker(
            saturation_ * (width() - 1) / 100.0,
            (100 - lightness_) * (height() - 1) / 100.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawEllipse(marker, 7, 7);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.drawEllipse(marker, 7, 7);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) update_from(event->position());
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons().testFlag(Qt::LeftButton))
            update_from(event->position());
    }

private:
    void update_from(QPointF position) {
        saturation_ = qRound(100.0 * std::clamp(
                                                position.x(), 0.0,
                                                static_cast<double>(qMax(0, width() - 1))) /
                             std::max(1, width() - 1));
        lightness_ = 100 - qRound(
            100.0 * std::clamp(
                        position.y(), 0.0,
                        static_cast<double>(qMax(0, height() - 1))) /
            std::max(1, height() - 1));
        update();
        if (changed_) changed_(saturation_, lightness_);
    }

    int hue_{};
    int saturation_{};
    int lightness_{50};
    QImage image_;
    std::function<void(int, int)> changed_;
};

class HueBar final : public QWidget {
public:
    explicit HueBar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(28);
        setMinimumHeight(220);
        setCursor(Qt::PointingHandCursor);
    }

    void set_hue(int hue) {
        hue_ = std::clamp(hue, 0, 359);
        update();
    }

    void set_change_handler(std::function<void(int)> handler) {
        changed_ = std::move(handler);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        QLinearGradient gradient(0, 0, 0, height());
        for (int index = 0; index <= 6; ++index)
            gradient.setColorAt(index / 6.0,
                                QColor::fromHsl((index * 60) % 360, 255, 128));
        painter.fillRect(rect(), gradient);
        const double y = hue_ * (height() - 1) / 359.0;
        painter.setPen(QPen(Qt::black, 3));
        painter.drawLine(0, qRound(y), width() - 1, qRound(y));
        painter.setPen(QPen(Qt::white, 1));
        painter.drawLine(0, qRound(y), width() - 1, qRound(y));
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) update_from(event->position().y());
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons().testFlag(Qt::LeftButton))
            update_from(event->position().y());
    }

private:
    void update_from(double y) {
        hue_ = qRound(359.0 * std::clamp(
                          y, 0.0,
                          static_cast<double>(qMax(0, height() - 1))) /
                      std::max(1, height() - 1));
        update();
        if (changed_) changed_(hue_);
    }

    int hue_{};
    std::function<void(int)> changed_;
};

class HslColorDialog final : public QDialog {
public:
    HslColorDialog(int hue, int saturation, int lightness, QWidget* parent)
        : QDialog(parent), hue_(hue), saturation_(saturation),
          lightness_(lightness) {
        setWindowTitle(QStringLiteral("幽灵分划颜色"));
        setModal(true);
        resize(500, 390);
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(18, 18, 18, 18);
        root->setSpacing(12);

        auto* picker = new QHBoxLayout;
        picker->setSpacing(10);
        plane_ = new HslPlane;
        hue_bar_ = new HueBar;
        picker->addWidget(plane_, 1);
        picker->addWidget(hue_bar_);
        root->addLayout(picker, 1);

        auto* hex_row = new QHBoxLayout;
        hex_row->addWidget(new QLabel(QStringLiteral("十六进制")));
        hex_edit_ = new QLineEdit;
        hex_edit_->setObjectName(QStringLiteral("ghostColorHex"));
        hex_edit_->setMaxLength(7);
        hex_edit_->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("^#[0-9A-Fa-f]{6}$")),
            hex_edit_));
        hex_row->addWidget(hex_edit_, 1);
        root->addLayout(hex_row);

        auto* presets = new QHBoxLayout;
        presets->setSpacing(7);
        struct Preset { const char* tooltip; int h; int s; int l; };
        const std::array<Preset, 5> values{{
            {"红", 3, 100, 59}, {"绿", 145, 100, 39},
            {"青", 187, 100, 42}, {"黄", 50, 100, 50},
            {"白", 0, 0, 100},
        }};
        for (const auto& preset : values) {
            auto* button = new QPushButton;
            button->setFixedSize(30, 30);
            button->setToolTip(QStringLiteral("%1色预设")
                                   .arg(QString::fromUtf8(preset.tooltip)));
            button->setStyleSheet(
                QStringLiteral(
                    "QPushButton{background:%1;border:1px solid #64748b;"
                    "border-radius:4px;padding:0;min-height:0;}"
                    "QPushButton:hover{border:2px solid #f8fafc;}")
                    .arg(hsl_color_css(preset.h, preset.s, preset.l)));
            connect(button, &QPushButton::clicked, this, [this, preset] {
                set_hsl(preset.h, preset.s, preset.l);
            });
            presets->addWidget(button);
        }
        presets->addStretch();
        root->addLayout(presets);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                              QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("应用"));
        buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);

        plane_->set_change_handler([this](int saturation, int lightness) {
            saturation_ = saturation;
            lightness_ = lightness;
            sync_hex();
        });
        hue_bar_->set_change_handler([this](int hue) {
            hue_ = hue;
            plane_->set_color(hue_, saturation_, lightness_);
            sync_hex();
        });
        connect(hex_edit_, &QLineEdit::editingFinished,
                this, [this] { apply_hex(); });
        set_hsl(hue_, saturation_, lightness_);
    }

    [[nodiscard]] int hue() const { return hue_; }
    [[nodiscard]] int saturation() const { return saturation_; }
    [[nodiscard]] int lightness() const { return lightness_; }

private:
    void set_hsl(int hue, int saturation, int lightness) {
        hue_ = std::clamp(hue, 0, 359);
        saturation_ = std::clamp(saturation, 0, 100);
        lightness_ = std::clamp(lightness, 0, 100);
        plane_->set_color(hue_, saturation_, lightness_);
        hue_bar_->set_hue(hue_);
        sync_hex();
    }

    void sync_hex() {
        hex_edit_->setText(hsl_color_css(hue_, saturation_, lightness_)
                               .toUpper());
    }

    void apply_hex() {
        const QColor color(hex_edit_->text());
        if (!color.isValid()) {
            sync_hex();
            return;
        }
        int hue = 0;
        int saturation = 0;
        int lightness = 0;
        color.getHsl(&hue, &saturation, &lightness);
        set_hsl(hue < 0 ? hue_ : hue,
                qRound(saturation * 100.0 / 255.0),
                qRound(lightness * 100.0 / 255.0));
    }

    int hue_{};
    int saturation_{};
    int lightness_{};
    HslPlane* plane_{};
    HueBar* hue_bar_{};
    QLineEdit* hex_edit_{};
};

struct ChoiceOption {
    QString id;
    QString title;
    QString description;
};

class ChoicePopup final : public QWidget {
public:
    ChoicePopup(QString title, std::vector<ChoiceOption> options,
                QString selected_id,
                std::function<void(const QString&)> on_selected,
                QWidget* parent, int width = 280,
                QString option_prefix = QStringLiteral("choiceOption_"))
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint |
                               Qt::NoDropShadowWindowHint),
          selected_id_(std::move(selected_id)),
          on_selected_(std::move(on_selected)),
          option_prefix_(std::move(option_prefix)) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setObjectName(QStringLiteral("choicePickerPopup"));

        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        auto* panel = new QFrame;
        panel->setObjectName(QStringLiteral("triggerPickerPanel"));
        panel->setAttribute(Qt::WA_StyledBackground);
        panel->setFixedWidth(width);
        outer->addWidget(panel);

        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(10, 9, 10, 10);
        layout->setSpacing(3);
        auto* header = new QLabel(std::move(title));
        header->setObjectName(QStringLiteral("triggerPickerHeader"));
        layout->addWidget(header);

        scroll_ = new QScrollArea(panel);
        scroll_->setObjectName(QStringLiteral("choiceScroll"));
        scroll_->setFrameShape(QFrame::NoFrame);
        scroll_->setWidgetResizable(true);
        scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll_->setStyleSheet(QStringLiteral(
            "QScrollArea{background:transparent;border:0;}"
            "QScrollArea>QWidget{background:transparent;}"
            "QScrollArea>QWidget>QWidget{background:transparent;}"));
        auto* content = new QWidget;
        content->setObjectName(QStringLiteral("choiceScrollContent"));
        auto* options_layout = new QVBoxLayout(content);
        options_layout->setContentsMargins(0, 0, 0, 0);
        options_layout->setSpacing(3);
        for (const auto& option : options) {
            add_option(options_layout, option);
        }
        const int content_height =
            static_cast<int>(options.size()) * 52 +
            std::max(0, static_cast<int>(options.size()) - 1) * 3;
        scroll_->setFixedHeight(std::min(300, content_height));
        scroll_->setWidget(content);
        layout->addWidget(scroll_);
        adjustSize();
    }

    void show_at(QPushButton* anchor) {
        adjustSize();
        const QRect anchor_rect(anchor->mapToGlobal(QPoint(0, 0)),
                                anchor->size());
        QScreen* screen = anchor->screen();
        const QRect available = screen ? screen->availableGeometry()
                                       : QRect(QPoint(0, 0), size());
        int x = anchor_rect.right() - width() + 1;
        int y = anchor_rect.bottom() + 6;
        if (y + height() > available.bottom() - 6)
            y = anchor_rect.top() - height() - 6;
        const int min_x = available.left() + 6;
        const int max_x = std::max(min_x, available.right() - width() - 6);
        const int min_y = available.top() + 6;
        const int max_y = std::max(min_y, available.bottom() - height() - 6);
        move(std::clamp(x, min_x, max_x), std::clamp(y, min_y, max_y));
        show();
        raise();
        activateWindow();
        for (auto* option : options_) {
            if (option->isChecked()) {
                option->setFocus();
                if (scroll_) scroll_->ensureWidgetVisible(option);
                break;
            }
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Escape) {
                close();
                return true;
            }
            if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
                int index = 0;
                for (std::size_t i = 0; i < options_.size(); ++i) {
                    if (options_[i]->hasFocus()) index = static_cast<int>(i);
                }
                const int next =
                    (index + (key->key() == Qt::Key_Down ? 1 : -1) +
                     static_cast<int>(options_.size())) %
                    static_cast<int>(options_.size());
                options_[static_cast<std::size_t>(next)]->setFocus();
                if (scroll_) {
                    scroll_->ensureWidgetVisible(
                        options_[static_cast<std::size_t>(next)]);
                }
                return true;
            }
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                for (auto* option : options_) {
                    if (option->hasFocus()) {
                        option->click();
                        return true;
                    }
                }
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    void add_option(QVBoxLayout* layout, const ChoiceOption& option) {
        auto* button = new QPushButton(layout->parentWidget());
        button->setObjectName(option_prefix_ + option.id);
        button->setProperty("triggerOption", true);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedHeight(52);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setAccessibleName(option.title +
                                  QStringLiteral("：") + option.description);
        button->installEventFilter(this);

        auto* row = new QHBoxLayout(button);
        row->setContentsMargins(8, 4, 8, 4);
        row->setSpacing(8);

        auto* check = new QLabel(option.id == selected_id_
                                     ? QStringLiteral("✓")
                                     : QString());
        check->setObjectName(QStringLiteral("triggerOptionCheck"));
        check->setFixedWidth(18);
        check->setAlignment(Qt::AlignCenter);
        check->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* text = new QVBoxLayout;
        text->setContentsMargins(0, 0, 0, 0);
        text->setSpacing(1);
        auto* title = new QLabel(option.title);
        title->setObjectName(QStringLiteral("triggerOptionTitle"));
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* description = new QLabel(option.description);
        description->setObjectName(QStringLiteral("triggerOptionDescription"));
        description->setAttribute(Qt::WA_TransparentForMouseEvents);
        description->setWordWrap(true);
        text->addWidget(title);
        text->addWidget(description);

        row->addWidget(check);
        row->addLayout(text, 1);

        button->setChecked(option.id == selected_id_);
        connect(button, &QPushButton::clicked, this,
                [this, id = option.id] { choose(id); });
        layout->addWidget(button);
        options_.push_back(button);
    }

    void choose(const QString& id) {
        if (on_selected_) on_selected_(id);
        close();
    }

    QString selected_id_;
    std::function<void(const QString&)> on_selected_;
    QString option_prefix_;
    QScrollArea* scroll_{};
    std::vector<QPushButton*> options_;
};

}  // namespace

SettingsDialog::SettingsDialog(const wardogs::AppSettings& settings,
                               GhostPreview ghost_preview, QWidget* parent)
    : QDialog(parent), capture_region_(settings.capture_region),
      pinned_card_(settings.pinned_card), ghost_reticle_(settings.ghost_reticle),
      ghost_preview_(std::move(ghost_preview)) {
    hotkey_profiles_ = wardogs::builtin_hotkey_profiles();
    for (const auto& profile : settings.custom_hotkey_profiles)
        hotkey_profiles_.push_back(profile);
    selected_hotkey_profile_ = settings.hotkey_profile;
    if (!find_hotkey_profile(selected_hotkey_profile_))
        selected_hotkey_profile_ = L"classic";
    configure_frameless_window(this);
    setWindowTitle(QStringLiteral("设置 · War Dogs 射表计算"));
    setModal(true);
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
        QStringLiteral("保存后生效；OCR 区域也会在下次启动时恢复；"
                       "清空某项热键即可停用"));
    subtitle->setObjectName(QStringLiteral("muted"));
    root->addWidget(subtitle);

    auto* panel = new QGroupBox;
    auto* form = new QFormLayout(panel);
    form->setContentsMargins(14, 14, 14, 14);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(
        QStringLiteral("OCR 引擎"),
        build_engine_profile_row(
            settings.backend == wardogs::OcrBackend::windows ? 1 : 0));
    profile_notice_ = new QLabel;
    profile_notice_->setObjectName(QStringLiteral("hotkeyProfileNotice"));
    profile_notice_->setWordWrap(true);
    form->addRow(QString{}, profile_notice_);

    form->addRow(QStringLiteral("全局热键"), build_hotkey_table(settings));
    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget*, QWidget*) { update_active_hotkey_row(); });

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
    ghost_outline_ = new QPushButton(QStringLiteral("描边"));
    ghost_outline_->setObjectName(QStringLiteral("ghostOutlineEnabled"));
    ghost_outline_->setCheckable(true);
    ghost_outline_->setChecked(settings.ghost_reticle.outline_enabled);
    ghost_outline_->setProperty("highlighted",
                                settings.ghost_reticle.outline_enabled);
    style_layout->addWidget(ghost_outline_);

    const auto add_width = [style_layout](const QString& caption,
                                          const QString& object_name,
                                          const std::array<int, 5>& stops,
                                          int value, QSlider*& slider,
                                          QLabel*& label) {
        auto* title = new QLabel(caption);
        title->setObjectName(QStringLiteral("muted"));
        style_layout->addWidget(title);
        slider = new QSlider(Qt::Horizontal);
        slider->setObjectName(object_name);
        slider->setProperty("pinnedMenuSlider", true);
        slider->setRange(0, static_cast<int>(stops.size()) - 1);
        slider->setValue(closest_stop(stops, value));
        slider->setSingleStep(1);
        slider->setPageStep(1);
        slider->setTickInterval(1);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setMinimumWidth(92);
        style_layout->addWidget(slider, 1);
        label = new QLabel(pixel_width_text(stops.at(slider->value())));
        label->setMinimumWidth(43);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        style_layout->addWidget(label);
    };
    add_width(QStringLiteral("线宽"), QStringLiteral("ghostLineWidth"),
              ghost_line_width_stops, settings.ghost_reticle.line_width_tenths,
              ghost_line_width_, ghost_line_width_value_);
    add_width(QStringLiteral("描边线框"), QStringLiteral("ghostOutlineWidth"),
              ghost_outline_width_stops,
              settings.ghost_reticle.outline_width_tenths,
              ghost_outline_width_, ghost_outline_width_value_);
    ghost_color_button_ = new QPushButton(QStringLiteral("颜色"));
    ghost_color_button_->setObjectName(QStringLiteral("ghostColorButton"));
    ghost_color_button_->setToolTip(QStringLiteral("打开 HSL 调色盘或选择预设颜色"));
    style_layout->addWidget(ghost_color_button_);
    form->addRow(QStringLiteral("分划样式"), style_row);
    refresh_ghost_color_button();

    connect(ghost_outline_, &QPushButton::toggled,
            this, [this](bool checked) {
                ghost_outline_->setProperty("highlighted", checked);
                ghost_outline_->style()->unpolish(ghost_outline_);
                ghost_outline_->style()->polish(ghost_outline_);
                notify_ghost_preview();
            });
    connect(ghost_line_width_, &QSlider::valueChanged, this, [this](int stop) {
        ghost_line_width_value_->setText(pixel_width_text(
            ghost_line_width_stops.at(stop)));
        notify_ghost_preview();
    });
    connect(ghost_outline_width_, &QSlider::valueChanged, this, [this](int stop) {
        ghost_outline_width_value_->setText(pixel_width_text(
            ghost_outline_width_stops.at(stop)));
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
    pattern_->setToolTip(QStringLiteral("前两个捕获组依次为 x 和 y"));
    pattern_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pattern_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const auto adjust_pattern_height = [this] {
        if (!pattern_) return;
        const int line = pattern_->fontMetrics().lineSpacing();
        const int chrome = 2 * pattern_->frameWidth() + 12;
        const int content = static_cast<int>(
            std::ceil(pattern_->document()->size().height()));
        pattern_->setFixedHeight(std::clamp(
            content + chrome, 3 * line + chrome, 8 * line + chrome));
    };
    connect(pattern_, &QPlainTextEdit::textChanged, this,
            adjust_pattern_height);
    connect(pattern_->document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged, this,
            [adjust_pattern_height](const QSizeF&) {
                adjust_pattern_height();
            });
    adjust_pattern_height();
    form->addRow(QStringLiteral("坐标正则\n捕获组 x / y"), pattern_);
    for (int row = 0; row < form->rowCount(); ++row) {
        if (auto* item = form->itemAt(row, QFormLayout::LabelRole)) {
            if (auto* label = qobject_cast<QLabel*>(item->widget())) {
                label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
    }
    root->addWidget(panel, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save |
                                          QDialogButtonBox::Cancel);
    auto* save_button = buttons->button(QDialogButtonBox::Save);
    save_button->setText(QStringLiteral("保存设置"));
    save_button->setObjectName(QStringLiteral("saveSettings"));
    save_button->setProperty("primary", true);
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
    update_profile_notice();
    outer->activate();
    const QSize required = minimumSizeHint();
    resize(required.width(), std::max(620, required.height()));
    enable_rounded_window_corners(this);
}

bool SettingsDialog::nativeEvent(const QByteArray& event_type, void* message,
                                 qintptr* result) {
    if (handle_frameless_native_event(this, message, result)) return true;
    return QDialog::nativeEvent(event_type, message, result);
}

wardogs::AppSettings SettingsDialog::settings() const {
    wardogs::AppSettings value;
    value.backend = backend_value() == 1 ? wardogs::OcrBackend::windows
                                         : wardogs::OcrBackend::rapid;
    value.hotkey_profile = selected_hotkey_profile_;
    for (const auto& profile : hotkey_profiles_) {
        if (!profile.builtin) value.custom_hotkey_profiles.push_back(profile);
    }
    value.region_hotkey = hotkey_text(hotkey_rows_[0].editor);
    value.base_hotkey = hotkey_text(hotkey_rows_[1].editor);
    value.target_hotkey = hotkey_text(hotkey_rows_[2].editor);
    value.quick_target_hotkey = hotkey_text(hotkey_rows_[3].editor);
    value.impact_hotkey = hotkey_text(hotkey_rows_[4].editor);
    value.ghost_arc_hotkey = hotkey_text(hotkey_rows_[5].editor);
    value.ghost_toggle_hotkey = hotkey_text(hotkey_rows_[6].editor);
    value.region_trigger = trigger_from_button(hotkey_rows_[0].trigger);
    value.base_trigger = trigger_from_button(hotkey_rows_[1].trigger);
    value.target_trigger = trigger_from_button(hotkey_rows_[2].trigger);
    value.quick_target_trigger =
        trigger_from_button(hotkey_rows_[3].trigger);
    value.impact_trigger = trigger_from_button(hotkey_rows_[4].trigger);
    value.ghost_arc_trigger = trigger_from_button(hotkey_rows_[5].trigger);
    value.ghost_toggle_trigger =
        trigger_from_button(hotkey_rows_[6].trigger);
    value.coordinate_pattern = pattern_->toPlainText().trimmed().toStdWString();
    value.capture_region = capture_region_;
    value.pinned_card = pinned_card_;
    value.ghost_reticle = ghost_reticle_;
    value.ghost_reticle.bearing_compensation_deg =
        ghost_bearing_compensation_->value();
    value.ghost_reticle.outline_enabled = ghost_outline_->isChecked();
    value.ghost_reticle.line_width_tenths =
        ghost_line_width_stops.at(ghost_line_width_->value());
    value.ghost_reticle.outline_width_tenths =
        ghost_outline_width_stops.at(ghost_outline_width_->value());
    if (ghost_preset_->currentData().canConvert<QSize>()) {
        const QSize preset = ghost_preset_->currentData().toSize();
        value.ghost_reticle.preset_screen_width = preset.width();
        value.ghost_reticle.preset_screen_height = preset.height();
    }
    return value;
}

int SettingsDialog::backend_value() const {
    return backend_selector_
               ? backend_selector_->property("backendValue").toInt()
               : 0;
}

QWidget* SettingsDialog::build_engine_profile_row(int backend) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    backend_selector_ = new QPushButton;
    backend_selector_->setObjectName(QStringLiteral("ocrEngineSelector"));
    backend_selector_->setProperty("choiceDisplay", true);
    backend_selector_->setProperty("backendValue", backend);
    backend_selector_->setCursor(Qt::PointingHandCursor);
    backend_selector_->setFixedSize(170, 28);
    connect(backend_selector_, &QPushButton::clicked, this,
            &SettingsDialog::show_backend_picker);
    update_backend_selector();
    layout->addWidget(backend_selector_);
    layout->addSpacing(12);

    auto* profile_label = new QLabel(QStringLiteral("全局热键配置"));
    profile_label->setObjectName(QStringLiteral("muted"));
    layout->addWidget(profile_label);

    profile_selector_ = new QPushButton;
    profile_selector_->setObjectName(QStringLiteral("hotkeyProfileSelector"));
    profile_selector_->setProperty("choiceDisplay", true);
    profile_selector_->setCursor(Qt::PointingHandCursor);
    profile_selector_->setFixedSize(210, 28);
    connect(profile_selector_, &QPushButton::clicked, this,
            &SettingsDialog::show_profile_picker);
    layout->addWidget(profile_selector_);

    auto* manage = new QPushButton(QStringLiteral("管理"));
    manage->setObjectName(QStringLiteral("manageHotkeyProfiles"));
    manage->setCursor(Qt::PointingHandCursor);
    manage->setFixedHeight(28);
    manage->setToolTip(QStringLiteral("新建、重命名或删除自定义热键配置"));
    connect(manage, &QPushButton::clicked, this,
            &SettingsDialog::manage_hotkey_profiles);
    layout->addWidget(manage);
    layout->addStretch();

    update_profile_selector();
    return row;
}

void SettingsDialog::update_backend_selector() {
    if (!backend_selector_) return;
    const QString text = backend_value() == 1
                             ? QStringLiteral("Windows 系统 OCR")
                             : QStringLiteral("RapidOCR");
    set_choice_display(backend_selector_, text);
    backend_selector_->setAccessibleName(
        QStringLiteral("OCR 引擎：") + text + QStringLiteral("。点击选择。"));
}

void SettingsDialog::apply_backend(int backend) {
    if (!backend_selector_) return;
    backend_selector_->setProperty("backendValue", backend == 1 ? 1 : 0);
    update_backend_selector();
}

void SettingsDialog::show_backend_picker() {
    if (!backend_selector_) return;
    const std::vector<ChoiceOption> options{
        ChoiceOption{QStringLiteral("rapid"), QStringLiteral("RapidOCR"),
                     QStringLiteral("CPU 识别，首选")},
        ChoiceOption{QStringLiteral("windows"),
                     QStringLiteral("Windows 系统 OCR"),
                     QStringLiteral("低内存备用")},
    };
    const QString selected =
        backend_value() == 1 ? QStringLiteral("windows")
                             : QStringLiteral("rapid");
    auto* popup = new ChoicePopup(
        QStringLiteral("OCR 引擎"), options, selected,
        [this](const QString& id) {
            apply_backend(id == QStringLiteral("windows") ? 1 : 0);
        },
        backend_selector_, 260);
    popup->show_at(backend_selector_);
}

void SettingsDialog::update_profile_selector() {
    if (!profile_selector_) return;
    const auto* profile = find_hotkey_profile(selected_hotkey_profile_);
    const QString name =
        profile ? qtext(profile->name) : QStringLiteral("未选择");
    set_choice_display(profile_selector_, name);
    QString tooltip = name;
    if (profile && !profile->description.empty()) {
        tooltip += QStringLiteral(" · ") + qtext(profile->description);
    }
    profile_selector_->setToolTip(tooltip);
    profile_selector_->setAccessibleName(
        QStringLiteral("全局热键配置：") + name +
        QStringLiteral("。点击切换。"));
}

void SettingsDialog::show_profile_picker() {
    if (!profile_selector_) return;
    std::vector<ChoiceOption> options;
    options.reserve(hotkey_profiles_.size());
    for (const auto& profile : hotkey_profiles_) {
        QString description = qtext(profile.description);
        if (profile.builtin) {
            description = QStringLiteral("内置 · ") + description;
        } else if (description.isEmpty()) {
            description = QStringLiteral("自定义配置");
        }
        options.push_back(ChoiceOption{
            QString::fromStdWString(profile.id), qtext(profile.name),
            description});
    }
    auto* popup = new ChoicePopup(
        QStringLiteral("全局热键配置"), options,
        QString::fromStdWString(selected_hotkey_profile_),
        [this](const QString& id) {
            apply_profile(id.toStdWString(), true);
        },
        profile_selector_, 340);
    popup->show_at(profile_selector_);
}

void SettingsDialog::apply_hotkeys_to_table(
    const wardogs::HotkeyProfile& profile) {
    for (std::size_t i = 0; i < hotkey_rows_.size(); ++i) {
        QSignalBlocker blocker(hotkey_rows_[i].editor);
        hotkey_rows_[i].editor->setKeySequence(
            QKeySequence(qtext(profile.hotkeys[i])));
        set_trigger_button(hotkey_rows_[i].trigger, profile.triggers[i]);
    }
}

void SettingsDialog::apply_profile(const std::wstring& id, bool announce) {
    const auto* profile = find_hotkey_profile(id);
    if (!profile) return;
    const bool was_dirty = hotkey_profile_dirty_;
    populating_hotkeys_ = true;
    apply_hotkeys_to_table(*profile);
    populating_hotkeys_ = false;
    selected_hotkey_profile_ = id;
    hotkey_profile_dirty_ = false;
    update_profile_selector();
    if (announce && was_dirty) {
        update_profile_notice(
            QStringLiteral("已切换配置；未保存的修改已丢弃。"));
    } else {
        update_profile_notice();
    }
}

std::array<std::wstring, 7> SettingsDialog::current_hotkeys() const {
    std::array<std::wstring, 7> result;
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = hotkey_text(hotkey_rows_[i].editor);
    }
    return result;
}

std::array<wardogs::HotkeyTrigger, 7>
SettingsDialog::current_triggers() const {
    std::array<wardogs::HotkeyTrigger, 7> result;
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = trigger_from_button(hotkey_rows_[i].trigger);
    }
    return result;
}

bool SettingsDialog::profile_matches_table(
    const wardogs::HotkeyProfile& profile) const {
    const auto hotkeys = current_hotkeys();
    const auto triggers = current_triggers();
    for (std::size_t i = 0; i < hotkeys.size(); ++i) {
        if (profile.hotkeys[i] != hotkeys[i] ||
            profile.triggers[i] != triggers[i]) {
            return false;
        }
    }
    return true;
}

const wardogs::HotkeyProfile* SettingsDialog::find_hotkey_profile(
    const std::wstring& id) const {
    for (const auto& profile : hotkey_profiles_) {
        if (profile.id == id) return &profile;
    }
    return nullptr;
}

std::size_t SettingsDialog::custom_profile_count() const {
    return static_cast<std::size_t>(
        std::count_if(hotkey_profiles_.begin(), hotkey_profiles_.end(),
                      [](const wardogs::HotkeyProfile& profile) {
                          return !profile.builtin;
                      }));
}

std::wstring SettingsDialog::new_custom_profile_id() const {
    for (int number = 1;; ++number) {
        std::wstring id = L"custom-" + std::to_wstring(number);
        if (!find_hotkey_profile(id)) return id;
    }
}

std::wstring SettingsDialog::unique_profile_name(
    const std::wstring& base, const std::wstring& ignore_id) const {
    const auto exists = [&](const std::wstring& name) {
        for (const auto& profile : hotkey_profiles_) {
            if (profile.id != ignore_id && profile.name == name) return true;
        }
        return false;
    };
    if (!exists(base)) return base;
    for (int number = 2;; ++number) {
        std::wstring candidate = base + L" " + std::to_wstring(number);
        if (!exists(candidate)) return candidate;
    }
}

void SettingsDialog::mark_hotkeys_edited() {
    if (populating_hotkeys_) return;
    const auto* profile = find_hotkey_profile(selected_hotkey_profile_);
    hotkey_profile_dirty_ = profile && !profile_matches_table(*profile);
    update_profile_notice();
}

void SettingsDialog::update_profile_notice(const QString& extra) {
    if (!profile_notice_) return;
    QString text = extra;
    const auto* profile = find_hotkey_profile(selected_hotkey_profile_);
    if (text.isEmpty()) {
        if (profile && profile->builtin) {
            text = hotkey_profile_dirty_
                       ? QStringLiteral(
                             "内置配置已修改；保存时会另存为自定义配置。")
                       : QStringLiteral(
                             "内置配置；修改后保存会另存为自定义配置。");
        } else {
            text = hotkey_profile_dirty_
                       ? QStringLiteral("自定义配置已修改；保存后写入此配置。")
                       : QStringLiteral("自定义配置；保存后写入此配置。");
        }
    }
    profile_notice_->setText(text);
}

bool SettingsDialog::commit_hotkey_profile() {
    const auto* selected = find_hotkey_profile(selected_hotkey_profile_);
    if (!selected) return true;
    if (profile_matches_table(*selected)) {
        hotkey_profile_dirty_ = false;
        return true;
    }
    if (selected->builtin) {
        if (custom_profile_count() >= wardogs::max_custom_hotkey_profiles) {
            return false;
        }
        const std::wstring base_name = selected->name + L" 副本";
        wardogs::HotkeyProfile copy;
        copy.id = new_custom_profile_id();
        copy.name = unique_profile_name(base_name);
        copy.description = L"自定义配置";
        copy.hotkeys = current_hotkeys();
        copy.triggers = current_triggers();
        hotkey_profiles_.push_back(std::move(copy));
        selected_hotkey_profile_ = hotkey_profiles_.back().id;
    } else {
        const std::wstring id = selected->id;
        for (auto& profile : hotkey_profiles_) {
            if (profile.id == id) {
                profile.hotkeys = current_hotkeys();
                profile.triggers = current_triggers();
                break;
            }
        }
    }
    hotkey_profile_dirty_ = false;
    update_profile_selector();
    update_profile_notice();
    return true;
}

void SettingsDialog::manage_hotkey_profiles() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("管理热键配置"));
    dialog.setModal(true);
    dialog.resize(460, 380);
    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);
    auto* hint = new QLabel(
        QStringLiteral("内置配置不能修改；新建副本会复制所选配置。"));
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    auto* list = new QListWidget;
    list->setObjectName(QStringLiteral("hotkeyProfileList"));
    root->addWidget(list, 1);
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton(QStringLiteral("新建副本"));
    auto* rename = new QPushButton(QStringLiteral("重命名"));
    auto* remove = new QPushButton(QStringLiteral("删除"));
    auto* close = new QPushButton(QStringLiteral("关闭"));
    close->setProperty("primary", true);
    buttons->addWidget(add);
    buttons->addWidget(rename);
    buttons->addWidget(remove);
    buttons->addStretch();
    buttons->addWidget(close);
    root->addLayout(buttons);

    std::wstring selected_id = selected_hotkey_profile_;
    const auto refresh = [&] {
        list->clear();
        for (const auto& profile : hotkey_profiles_) {
            auto* item = new QListWidgetItem(
                qtext(profile.name) +
                (profile.builtin ? QStringLiteral("   · 内置")
                                 : QStringLiteral("   · 自定义")));
            item->setData(Qt::UserRole, QString::fromStdWString(profile.id));
            item->setToolTip(qtext(profile.description));
            list->addItem(item);
            if (profile.id == selected_id) list->setCurrentItem(item);
        }
        const auto* profile = find_hotkey_profile(selected_id);
        const bool custom = profile && !profile->builtin;
        rename->setEnabled(custom);
        remove->setEnabled(custom);
    };
    refresh();

    connect(list, &QListWidget::currentItemChanged, &dialog,
            [&](QListWidgetItem* current, QListWidgetItem*) {
                if (!current) return;
                selected_id =
                    current->data(Qt::UserRole).toString().toStdWString();
                const auto* profile = find_hotkey_profile(selected_id);
                const bool custom = profile && !profile->builtin;
                rename->setEnabled(custom);
                remove->setEnabled(custom);
            });
    connect(add, &QPushButton::clicked, &dialog, [&] {
        if (custom_profile_count() >= wardogs::max_custom_hotkey_profiles) {
            QMessageBox::information(
                &dialog, QStringLiteral("新建热键配置"),
                QStringLiteral("自定义热键配置最多 %1 个，请先删除一个配置。")
                    .arg(static_cast<qulonglong>(
                        wardogs::max_custom_hotkey_profiles)));
            return;
        }
        const auto* source = find_hotkey_profile(selected_id);
        if (!source) source = find_hotkey_profile(L"classic");
        if (!source) return;
        bool ok = false;
        const QString default_name =
            qtext(source->name) + QStringLiteral(" 副本");
        const QString name = QInputDialog::getText(
            &dialog, QStringLiteral("新建热键配置"),
            QStringLiteral("配置名称："), QLineEdit::Normal, default_name,
            &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const auto hotkeys = source->hotkeys;
        const auto triggers = source->triggers;
        wardogs::HotkeyProfile profile;
        profile.id = new_custom_profile_id();
        profile.name = unique_profile_name(name.trimmed().toStdWString());
        profile.description = L"自定义配置";
        profile.hotkeys = hotkeys;
        profile.triggers = triggers;
        hotkey_profiles_.push_back(std::move(profile));
        selected_id = hotkey_profiles_.back().id;
        refresh();
    });
    connect(rename, &QPushButton::clicked, &dialog, [&] {
        const auto* profile = find_hotkey_profile(selected_id);
        if (!profile || profile->builtin) return;
        bool ok = false;
        const QString name = QInputDialog::getText(
            &dialog, QStringLiteral("重命名热键配置"),
            QStringLiteral("配置名称："), QLineEdit::Normal,
            qtext(profile->name), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const std::wstring id = profile->id;
        const std::wstring new_name =
            unique_profile_name(name.trimmed().toStdWString(), id);
        for (auto& item : hotkey_profiles_) {
            if (item.id == id) {
                item.name = new_name;
                break;
            }
        }
        refresh();
    });
    connect(remove, &QPushButton::clicked, &dialog, [&] {
        const auto* profile = find_hotkey_profile(selected_id);
        if (!profile || profile->builtin) return;
        const QString name = qtext(profile->name);
        if (QMessageBox::question(
                &dialog, QStringLiteral("删除热键配置"),
                QStringLiteral("确定删除「%1」？").arg(name),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
        const std::wstring id = profile->id;
        hotkey_profiles_.erase(
            std::remove_if(
                hotkey_profiles_.begin(), hotkey_profiles_.end(),
                [&](const wardogs::HotkeyProfile& item) { return item.id == id; }),
            hotkey_profiles_.end());
        if (selected_id == id) selected_id = L"classic";
        refresh();
    });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
    if (selected_id == selected_hotkey_profile_) {
        // Renames update the selector without overwriting unsaved table edits.
        update_profile_selector();
        return;
    }
    if (find_hotkey_profile(selected_id)) {
        apply_profile(selected_id, true);
    } else {
        apply_profile(L"classic", true);
    }
}

QWidget* SettingsDialog::build_hotkey_table(
    const wardogs::AppSettings& settings) {
    const std::array<std::pair<std::wstring, wardogs::HotkeyTrigger>, 7>
        initial{{
            {settings.region_hotkey, settings.region_trigger},
            {settings.base_hotkey, settings.base_trigger},
            {settings.target_hotkey, settings.target_trigger},
            {settings.quick_target_hotkey, settings.quick_target_trigger},
            {settings.impact_hotkey, settings.impact_trigger},
            {settings.ghost_arc_hotkey, settings.ghost_arc_trigger},
            {settings.ghost_toggle_hotkey, settings.ghost_toggle_trigger},
        }};

    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("hotkeyTableCard"));
    card->setAttribute(Qt::WA_StyledBackground);
    auto* root = new QVBoxLayout(card);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(0);

    constexpr int action_width = 110;
    constexpr int key_width = 200;
    constexpr int trigger_width = 84;
    auto* table_header = new QWidget;
    auto* table_header_layout = new QHBoxLayout(table_header);
    table_header_layout->setContentsMargins(8, 2, 8, 4);
    table_header_layout->setSpacing(10);
    const auto make_header = [](const QString& text, int width) {
        auto* label = new QLabel(text);
        label->setObjectName(QStringLiteral("hotkeyTableHeader"));
        label->setFixedWidth(width);
        label->setAlignment(Qt::AlignCenter);
        return label;
    };
    table_header_layout->addWidget(
        make_header(QStringLiteral("动作"), action_width));
    table_header_layout->addStretch(1);
    table_header_layout->addWidget(
        make_header(QStringLiteral("按键"), key_width));
    table_header_layout->addStretch(1);
    table_header_layout->addWidget(
        make_header(QStringLiteral("触发方式"), trigger_width));
    root->addWidget(table_header);
    root->addWidget(make_separator());

    for (std::size_t i = 0; i < initial.size(); ++i) {
        auto* frame = new QFrame;
        frame->setObjectName(QStringLiteral("hotkeyTableRow"));
        frame->setAttribute(Qt::WA_StyledBackground);
        frame->setProperty("alternate", i % 2 == 1);
        frame->setFixedHeight(40);
        auto* row = new QHBoxLayout(frame);
        row->setContentsMargins(8, 3, 8, 3);
        row->setSpacing(10);

        auto* action = new QLabel(action_name(i));
        action->setObjectName(QStringLiteral("hotkeyTableAction"));
        action->setFixedWidth(action_width);
        action->setAlignment(Qt::AlignCenter);
        row->addWidget(action);
        row->addStretch(1);

        auto* editor =
            new QKeySequenceEdit(QKeySequence(qtext(initial[i].first)));
        editor->setObjectName(QStringLiteral("hotkeyEditor_") +
                              action_key(i));
        editor->setMaximumSequenceLength(1);
        editor->setClearButtonEnabled(true);
        editor->setAccessibleName(action_name(i) + QStringLiteral("热键"));
        editor->setFixedWidth(key_width);
        if (auto* line = editor->findChild<QLineEdit*>())
            line->setPlaceholderText(QStringLiteral("未设置"));
        connect(editor, &QKeySequenceEdit::keySequenceChanged, this,
                [this](const QKeySequence&) { mark_hotkeys_edited(); });
        row->addWidget(editor);
        row->addStretch(1);

        auto* trigger = new QPushButton;
        trigger->setObjectName(QStringLiteral("hotkeyTrigger_") +
                               action_key(i));
        trigger->setProperty("triggerDisplay", true);
        trigger->setProperty("hotkeyActionName", action_name(i));
        trigger->setCursor(Qt::PointingHandCursor);
        trigger->setFixedSize(84, 28);
        trigger->setToolTip(QStringLiteral(
            "独立单击按下即响应；同键配置多种方式时等待手势判定；"
            "双击按系统双击窗口内连按两次；长按约 400 ms 触发。"));
        set_trigger_button(trigger, initial[i].second);
        connect(trigger, &QPushButton::clicked, this, [this, trigger] {
            const std::vector<ChoiceOption> options{
                ChoiceOption{QStringLiteral("single"),
                             trigger_label(wardogs::HotkeyTrigger::single),
                             trigger_description(wardogs::HotkeyTrigger::single)},
                ChoiceOption{
                    QStringLiteral("double"),
                    trigger_label(wardogs::HotkeyTrigger::double_press),
                    trigger_description(wardogs::HotkeyTrigger::double_press)},
                ChoiceOption{
                    QStringLiteral("long"),
                    trigger_label(wardogs::HotkeyTrigger::long_press),
                    trigger_description(wardogs::HotkeyTrigger::long_press)},
            };
            const QString selected =
                trigger_key(trigger_from_button(trigger));
            auto* popup = new ChoicePopup(
                QStringLiteral("触发方式"), options, selected,
                [this, trigger](const QString& id) {
                    set_trigger_button(
                        trigger,
                        wardogs::hotkey_trigger_from_name(
                            id.toStdWString()));
                    mark_hotkeys_edited();
                },
                trigger, 300, QStringLiteral("triggerOption_"));
            popup->show_at(trigger);
        });
        row->addWidget(trigger, 0, Qt::AlignVCenter);

        root->addWidget(frame);
        if (i + 1 < initial.size()) root->addWidget(make_separator());
        hotkey_rows_[i] = HotkeyRow{frame, editor, trigger};
    }
    return card;
}

void SettingsDialog::update_active_hotkey_row() {
    QWidget* focus = QApplication::focusWidget();
    for (auto& row : hotkey_rows_) {
        if (!row.frame) continue;
        const bool active = focus && row.frame->isAncestorOf(focus);
        if (row.frame->property("active").toBool() == active) continue;
        row.frame->setProperty("active", active);
        row.frame->style()->unpolish(row.frame);
        row.frame->style()->polish(row.frame);
        row.frame->update();
    }
}

void SettingsDialog::accept_if_valid() {
    try {
        const auto value = settings();
        const std::array bindings{
            wardogs::HotkeyBinding{wardogs::parse_hotkey(value.region_hotkey),
                                   value.region_trigger},
            wardogs::HotkeyBinding{wardogs::parse_hotkey(value.base_hotkey),
                                   value.base_trigger},
            wardogs::HotkeyBinding{wardogs::parse_hotkey(value.target_hotkey),
                                   value.target_trigger},
            wardogs::HotkeyBinding{
                wardogs::parse_hotkey(value.quick_target_hotkey),
                value.quick_target_trigger},
            wardogs::HotkeyBinding{wardogs::parse_hotkey(value.impact_hotkey),
                                   value.impact_trigger},
            wardogs::HotkeyBinding{wardogs::parse_hotkey(value.ghost_arc_hotkey),
                                   value.ghost_arc_trigger},
            wardogs::HotkeyBinding{
                wardogs::parse_hotkey(value.ghost_toggle_hotkey),
                value.ghost_toggle_trigger},
            wardogs::HotkeyBinding{
                wardogs::parse_hotkey(value.pinned_card.unlock_hotkey),
                wardogs::HotkeyTrigger::single},
        };
        wardogs::validate_unique_bindings(bindings);
        const std::wregex pattern(value.coordinate_pattern,
                                  std::regex_constants::icase);
        if (pattern.mark_count() < 2)
            throw std::invalid_argument("coordinate regex needs x/y capture groups");
        if (!commit_hotkey_profile()) {
            QMessageBox::information(
                this, QStringLiteral("无法保存热键配置"),
                QStringLiteral(
                    "自定义热键配置最多 %1 个，请先在「管理热键配置」中删除一个配置。")
                    .arg(static_cast<qulonglong>(
                        wardogs::max_custom_hotkey_profiles)));
            return;
        }
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
    ghost_color_button_->setText(QStringLiteral("颜色"));
    ghost_color_button_->setStyleSheet(
        QStringLiteral(
            "QPushButton{border:2px solid %1;border-radius:8px;}"
            "QPushButton:hover{border:2px solid %1;background:#23344a;}"
            "QPushButton:pressed{border:2px solid %1;background:#182f55;}")
            .arg(color));
}
