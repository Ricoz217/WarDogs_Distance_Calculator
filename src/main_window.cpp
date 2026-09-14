#include "main_window.hpp"
#include "selection_overlay.hpp"
#include "settings_dialog.hpp"

#include "wardogs/capture.hpp"
#include "wardogs/core.hpp"
#include "wardogs/hotkeys.hpp"
#include "wardogs/ocr.hpp"
#include "wardogs/settings.hpp"
#include "wardogs/terrain_package.hpp"
#include "wardogs/vehicle_ballistics.hpp"
#include "wardogs/windows_ocr.hpp"

#include "pinned_result_window.hpp"
#include "vehicle_solution_widget.hpp"

#include <Windows.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <winrt/base.h>

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMetaObject>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QStringList>
#include <QStyleFactory>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>

namespace {

QString qtext(const std::wstring& value) { return QString::fromStdWString(value); }
QString error_text(const std::exception& error) { return QString::fromUtf8(error.what()); }

void enable_dark_title_bar(HWND window) {
    const BOOL enabled = TRUE;
    DwmSetWindowAttribute(window, 20, &enabled, sizeof(enabled));
}

std::filesystem::path executable_directory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path{buffer}.parent_path();
}

QWidget* result_card(const QString& caption, const QString& color, QLabel*& value) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("resultCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 9, 10, 11);
    layout->setSpacing(3);
    auto* label = new QLabel(caption);
    label->setObjectName(QStringLiteral("resultCaption"));
    label->setAlignment(Qt::AlignCenter);
    value = new QLabel(QStringLiteral("—"));
    value->setAlignment(Qt::AlignCenter);
    value->setMinimumHeight(58);
    value->setStyleSheet(QStringLiteral(
        "color:%1;font-family:'Bahnschrift';font-size:36px;font-weight:700;").arg(color));
    layout->addWidget(label);
    layout->addWidget(value);
    return card;
}

QIcon pin_icon() {
    QPixmap image(24, 24);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color(QStringLiteral("#e2e8f0"));
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);
    const QPolygonF body{{8.0, 3.0}, {16.0, 3.0}, {14.3, 6.0},
                         {14.3, 11.0}, {17.5, 14.0}, {6.5, 14.0},
                         {9.7, 11.0}, {9.7, 6.0}};
    painter.drawPolygon(body);
    painter.drawLine(QPointF(12.0, 14.0), QPointF(12.0, 21.0));
    return QIcon(image);
}

QIcon weapon_mode_icon(bool vehicle_mode) {
    QPixmap image(24, 24);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color(QStringLiteral("#e2e8f0"));
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin));
    if (vehicle_mode) {
        painter.drawRoundedRect(QRectF(3, 14, 18, 6), 2, 2);
        painter.drawRoundedRect(QRectF(7, 10, 8, 5), 1, 1);
        painter.drawLine(QPointF(12, 10), QPointF(20.5, 4));
        painter.drawEllipse(QPointF(7, 20), 1.5, 1.5);
        painter.drawEllipse(QPointF(17, 20), 1.5, 1.5);
    } else {
        painter.drawLine(QPointF(7, 18), QPointF(15.5, 5));
        painter.drawLine(QPointF(9, 19), QPointF(17.5, 6));
        painter.drawLine(QPointF(5, 20), QPointF(15, 20));
        painter.drawLine(QPointF(10, 19.5), QPointF(14.5, 14));
    }
    return QIcon(image);
}

enum class OcrAction { base, target, calibration_impact };

struct OcrMessage {
    bool success{};
    OcrAction action{OcrAction::target};
    wardogs::Point point{};
    std::wstring text;
    float confidence{};
    QString error;
};

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        try { settings_ = wardogs::load_settings(); } catch (...) { settings_ = {}; }
        bool saved_region_invalid = false;
        if (settings_.capture_region) {
            try {
                (void)wardogs::resolve_capture_region(*settings_.capture_region);
                region_ = settings_.capture_region;
            } catch (...) {
                settings_.capture_region.reset();
                saved_region_invalid = true;
                try { wardogs::save_settings(settings_); } catch (...) {}
            }
        }
        setWindowTitle(QStringLiteral("War Dogs 射表计算"));
        resize(620, 610);
        setMinimumWidth(560);
        terrain_discovery_ = wardogs::discover_terrain_maps(
            wardogs::default_terrain_directory());
        build_ui();
        update_coordinates();
        update_engine_summary();
        update_action_labels();
        update_region_summary();
        if (region_)
            set_status(QStringLiteral("就绪；已恢复保存的 OCR 区域"));
        else if (saved_region_invalid)
            set_status(QStringLiteral("已保存的 OCR 区域不可用，请重新设置"), true);
        enable_dark_title_bar(reinterpret_cast<HWND>(winId()));
        try { register_hotkeys(settings_); }
        catch (const std::exception& error) {
            set_status(QStringLiteral("热键启动失败：") + error_text(error), true);
        }
    }

    ~MainWindow() override {
        unregister_hotkeys();
        if (worker_.joinable()) worker_.join();
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        unregister_hotkeys();
        if (worker_.joinable()) worker_.join();
        event->accept();
    }

private:
    wardogs::AppSettings settings_{};
    wardogs::Point base_{};
    std::optional<wardogs::Point> target_;
    wardogs::TerrainDiscovery terrain_discovery_;
    std::unique_ptr<wardogs::TerrainPackage> terrain_;
    std::optional<wardogs::InstalledTerrainMap> terrain_map_;
    std::optional<wardogs::PlatformCalibration> platform_calibration_;
    std::optional<wardogs::CaptureRegion> region_;
    std::unique_ptr<wardogs::RapidOcr> rapid_;
    std::unique_ptr<wardogs::WindowsOcr> windows_;
    wardogs::GlobalHotkeyListener hotkey_listener_;
    std::jthread worker_;
    std::atomic_bool busy_{false};
    SelectionOverlay selector_;
    std::unique_ptr<PinnedResultWindow> pinned_window_;
    bool vehicle_mode_{};
    bool pinned_mode_{};
    bool failure_state_{};
    int next_calibration_shot_{1};
    QFrame* app_frame_{};
    QLabel *base_summary_{}, *target_summary_{}, *distance_{}, *bearing_{};
    QLabel *raw_result_{}, *engine_summary_{}, *region_summary_{}, *ocr_text_{}, *status_{};
    QLabel *terrain_summary_{}, *calibration_summary_{}, *vehicle_note_{};
    QLineEdit *base_input_{}, *target_input_{};
    QLineEdit *first_aim_{}, *first_impact_{}, *second_aim_{}, *second_impact_{};
    QPushButton *region_button_{}, *base_button_{}, *target_button_{},
        *quick_target_button_{};
    QPushButton *mode_button_{}, *pin_button_{}, *first_arc_{}, *second_arc_{},
        *calibration_ocr_{}, *calibration_manual_{};
    QComboBox* terrain_selector_{};
    QGroupBox *terrain_group_{}, *calibration_group_{}, *mortar_result_group_{},
        *vehicle_result_group_{};
    VehicleSolutionWidget *low_solution_{}, *high_solution_{};

    void build_ui() {
        app_frame_ = new QFrame;
        app_frame_->setObjectName(QStringLiteral("appFrame"));
        app_frame_->setProperty("error", false);
        auto* root = new QVBoxLayout(app_frame_);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(10);

        auto* heading = new QHBoxLayout;
        auto* title = new QLabel(QStringLiteral("射表计算"));
        title->setObjectName(QStringLiteral("title"));
        mode_button_ = new QPushButton;
        mode_button_->setObjectName(QStringLiteral("iconButton"));
        mode_button_->setIconSize(QSize(22, 22));
        mode_button_->setFixedSize(38, 34);
        pin_button_ = new QPushButton;
        pin_button_->setObjectName(QStringLiteral("iconButton"));
        pin_button_->setIcon(pin_icon());
        pin_button_->setIconSize(QSize(22, 22));
        pin_button_->setFixedSize(38, 34);
        pin_button_->setToolTip(QStringLiteral("置顶显示射表结果"));
        pin_button_->setAccessibleName(QStringLiteral("进入置顶模式"));
        heading->addWidget(title);
        heading->addStretch();
        heading->addWidget(mode_button_);
        heading->addWidget(pin_button_);
        root->addLayout(heading);
        auto* subtitle = new QLabel(
            QStringLiteral("OCR 热键和手动输入共用同一套计算逻辑"));
        subtitle->setObjectName(QStringLiteral("muted"));
        root->addWidget(subtitle);
        update_mode_button();

        terrain_group_ = new QGroupBox(QStringLiteral("高度模型"));
        auto* terrain_layout = new QVBoxLayout(terrain_group_);
        terrain_selector_ = new QComboBox;
        terrain_selector_->addItem(QStringLiteral("等高假设（无需地图包）"));
        for (const auto& installed : terrain_discovery_.installed)
            terrain_selector_->addItem(
                qtext(installed.spec.display_name) + QStringLiteral(" · 地形高度"));
        terrain_layout->addWidget(terrain_selector_);
        terrain_summary_ = new QLabel;
        terrain_summary_->setObjectName(QStringLiteral("muted"));
        terrain_summary_->setWordWrap(true);
        terrain_layout->addWidget(terrain_summary_);
        terrain_group_->hide();
        root->addWidget(terrain_group_);
        update_terrain_summary();

        auto* coordinates = new QGroupBox(QStringLiteral("坐标"));
        auto* coordinate_layout = new QVBoxLayout(coordinates);
        coordinate_layout->setSpacing(7);
        base_summary_ = new QLabel;
        target_summary_ = new QLabel;
        coordinate_layout->addWidget(base_summary_);
        coordinate_layout->addWidget(target_summary_);
        auto* base_row = new QHBoxLayout;
        base_input_ = new QLineEdit;
        base_input_->setPlaceholderText(
            QStringLiteral("基准点：x12.34, y56.78 或 12.34 56.78"));
        auto* manual_base = new QPushButton(QStringLiteral("手动设为基准点"));
        base_row->addWidget(base_input_, 1);
        base_row->addWidget(manual_base);
        coordinate_layout->addLayout(base_row);
        auto* target_row = new QHBoxLayout;
        target_input_ = new QLineEdit;
        target_input_->setPlaceholderText(QStringLiteral("目标点：输入后立即计算"));
        auto* manual_target = new QPushButton(QStringLiteral("手动计算目标点"));
        target_row->addWidget(target_input_, 1);
        target_row->addWidget(manual_target);
        coordinate_layout->addLayout(target_row);
        root->addWidget(coordinates);

        calibration_group_ = build_calibration_group();
        calibration_group_->hide();
        root->addWidget(calibration_group_);

        mortar_result_group_ = new QGroupBox(QStringLiteral("计算结果"));
        auto* result_layout = new QVBoxLayout(mortar_result_group_);
        auto* cards = new QHBoxLayout;
        cards->setSpacing(12);
        cards->addWidget(result_card(QStringLiteral("射程"),
                                     QStringLiteral("#fbbf24"), distance_), 1);
        cards->addWidget(result_card(QStringLiteral("方位"),
                                     QStringLiteral("#67e8f9"), bearing_), 1);
        result_layout->addLayout(cards);
        raw_result_ = new QLabel(QStringLiteral("等待目标坐标…"));
        raw_result_->setObjectName(QStringLiteral("rawResult"));
        raw_result_->setAlignment(Qt::AlignCenter);
        result_layout->addWidget(raw_result_);
        root->addWidget(mortar_result_group_);

        vehicle_result_group_ = new QGroupBox(QStringLiteral("SPH-2 修正射表"));
        vehicle_result_group_->setObjectName(QStringLiteral("vehicleResultGroup"));
        vehicle_result_group_->setProperty("error", false);
        auto* vehicle_results = new QVBoxLayout(vehicle_result_group_);
        low_solution_ = new VehicleSolutionWidget(wardogs::Arc::low);
        high_solution_ = new VehicleSolutionWidget(wardogs::Arc::high);
        vehicle_results->addWidget(low_solution_);
        vehicle_results->addWidget(high_solution_);
        vehicle_note_ = new QLabel(QStringLiteral("完成两发校准前显示平地参考射表"));
        vehicle_note_->setObjectName(QStringLiteral("rawResult"));
        vehicle_note_->setAlignment(Qt::AlignCenter);
        vehicle_results->addWidget(vehicle_note_);
        vehicle_result_group_->hide();
        root->addWidget(vehicle_result_group_);

        auto* ocr = new QGroupBox(QStringLiteral("OCR 与热键"));
        auto* ocr_layout = new QVBoxLayout(ocr);
        ocr_layout->setSpacing(7);
        engine_summary_ = new QLabel;
        region_summary_ = new QLabel(QStringLiteral("OCR 区域：尚未设置"));
        ocr_layout->addWidget(engine_summary_);
        ocr_layout->addWidget(region_summary_);
        auto* actions = new QHBoxLayout;
        actions->setSpacing(8);
        region_button_ = new QPushButton;
        base_button_ = new QPushButton;
        target_button_ = new QPushButton;
        quick_target_button_ = new QPushButton;
        auto* settings_button = new QPushButton(QStringLiteral("设置"));
        for (auto* button : {region_button_, base_button_, target_button_,
                             quick_target_button_, settings_button})
            actions->addWidget(button, 1);
        ocr_layout->addLayout(actions);
        ocr_text_ = new QLabel(QStringLiteral("OCR 原文：—"));
        ocr_text_->setObjectName(QStringLiteral("muted"));
        ocr_text_->setWordWrap(true);
        ocr_layout->addWidget(ocr_text_);
        root->addWidget(ocr);

        status_ = new QLabel(QStringLiteral("就绪；请先设置游戏聊天框中的坐标区域"));
        status_->setObjectName(QStringLiteral("status"));
        status_->setWordWrap(true);
        root->addWidget(status_);
        setCentralWidget(app_frame_);

        connect(manual_base, &QPushButton::clicked, this, &MainWindow::manual_base);
        connect(base_input_, &QLineEdit::returnPressed, this, &MainWindow::manual_base);
        connect(manual_target, &QPushButton::clicked, this, &MainWindow::manual_target);
        connect(target_input_, &QLineEdit::returnPressed, this, &MainWindow::manual_target);
        connect(mode_button_, &QPushButton::clicked, this, &MainWindow::toggle_mode);
        connect(pin_button_, &QPushButton::clicked, this, &MainWindow::enter_pinned_mode);
        connect(terrain_selector_, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this](int) { on_terrain_changed(); });
        connect(region_button_, &QPushButton::clicked, this, &MainWindow::begin_region_setup);
        connect(base_button_, &QPushButton::clicked, this,
                [this] { start_ocr(OcrAction::base); });
        connect(target_button_, &QPushButton::clicked, this,
                [this] { start_ocr(OcrAction::target); });
        connect(quick_target_button_, &QPushButton::clicked, this,
                &MainWindow::begin_quick_target);
        connect(settings_button, &QPushButton::clicked, this, &MainWindow::edit_settings);
    }

    QGroupBox* build_calibration_group() {
        auto* group = new QGroupBox(QStringLiteral("当前炮位 · 两发校准"));
        auto* layout = new QGridLayout(group);
        layout->addWidget(new QLabel, 0, 0);
        layout->addWidget(new QLabel(QStringLiteral("计划瞄准点")), 0, 1);
        layout->addWidget(new QLabel(QStringLiteral("实际落点")), 0, 2);
        layout->addWidget(new QLabel(QStringLiteral("弹道")), 0, 3);
        first_aim_ = new QLineEdit;
        first_impact_ = new QLineEdit;
        second_aim_ = new QLineEdit;
        second_impact_ = new QLineEdit;
        first_arc_ = make_arc_button();
        second_arc_ = make_arc_button();
        const std::array rows{
            std::tuple{1, QStringLiteral("第一发"), first_aim_, first_impact_, first_arc_},
            std::tuple{2, QStringLiteral("第二发"), second_aim_, second_impact_, second_arc_},
        };
        for (const auto& [row, name, aim, impact, arc] : rows) {
            aim->setPlaceholderText(QStringLiteral("x12.34, y56.78"));
            impact->setPlaceholderText(QStringLiteral("OCR 或手动输入"));
            layout->addWidget(new QLabel(name), row, 0);
            layout->addWidget(aim, row, 1);
            layout->addWidget(impact, row, 2);
            layout->addWidget(arc, row, 3);
        }
        calibration_ocr_ = new QPushButton(QStringLiteral("OCR 录入下一发"));
        calibration_manual_ = new QPushButton(QStringLiteral("手动录入下一发"));
        auto* recalculate = new QPushButton(QStringLiteral("重新计算"));
        auto* clear = new QPushButton(QStringLiteral("清除校准"));
        layout->addWidget(calibration_ocr_, 3, 0, 1, 2);
        layout->addWidget(calibration_manual_, 3, 2, 1, 2);
        layout->addWidget(recalculate, 4, 0, 1, 2);
        layout->addWidget(clear, 4, 2, 1, 2);
        calibration_summary_ = new QLabel(
            QStringLiteral("未校准 · 两发计划方位角差需在 30°～150°之间"));
        calibration_summary_->setObjectName(QStringLiteral("muted"));
        layout->addWidget(calibration_summary_, 5, 0, 1, 4);
        connect(calibration_ocr_, &QPushButton::clicked, this,
                [this] { start_ocr(OcrAction::calibration_impact); });
        connect(calibration_manual_, &QPushButton::clicked, this,
                &MainWindow::record_manual_calibration);
        connect(recalculate, &QPushButton::clicked, this,
                [this] { recalculate_vehicle(); });
        connect(clear, &QPushButton::clicked, this,
                &MainWindow::clear_vehicle_calibration);
        return group;
    }

    QPushButton* make_arc_button() {
        auto* button = new QPushButton;
        button->setObjectName(QStringLiteral("arcToggle"));
        button->setToolTip(QStringLiteral("点击切换低射 / 高抛"));
        set_arc_button(button, wardogs::Arc::low);
        connect(button, &QPushButton::clicked, this, [this, button] {
            set_arc_button(button, arc_from_button(button) == wardogs::Arc::low
                                       ? wardogs::Arc::high
                                       : wardogs::Arc::low);
        });
        return button;
    }

    static wardogs::Arc arc_from_button(const QPushButton* button) {
        return button->property("trajectory").toString() == QStringLiteral("high")
                   ? wardogs::Arc::high
                   : wardogs::Arc::low;
    }

    static void set_arc_button(QPushButton* button, wardogs::Arc arc) {
        const bool high = arc == wardogs::Arc::high;
        button->setText(high ? QStringLiteral("高抛") : QStringLiteral("低射"));
        button->setProperty("trajectory", high ? QStringLiteral("high")
                                                : QStringLiteral("low"));
        button->setProperty("highlighted", high);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }

    void update_mode_button() {
        mode_button_->setIcon(weapon_mode_icon(vehicle_mode_));
        mode_button_->setToolTip(vehicle_mode_
            ? QStringLiteral("切换到迫击炮")
            : QStringLiteral("切换到 SPH-2 两发校准"));
        mode_button_->setAccessibleName(vehicle_mode_
            ? QStringLiteral("当前为 SPH-2；切换到迫击炮")
            : QStringLiteral("当前为迫击炮；切换到 SPH-2"));
    }

    void toggle_mode() {
        vehicle_mode_ = !vehicle_mode_;
        update_mode_button();
        terrain_group_->setVisible(vehicle_mode_);
        calibration_group_->setVisible(vehicle_mode_);
        vehicle_result_group_->setVisible(vehicle_mode_);
        mortar_result_group_->setVisible(!vehicle_mode_);
        if (target_) {
            const bool warning = show_result(*target_);
            if (!warning)
                set_status(vehicle_mode_
                    ? QStringLiteral("SPH-2 模式：选择高度模型并完成两发校准")
                    : QStringLiteral("迫击炮模式：使用原始平面距离与方位模型"));
        } else {
            set_status(vehicle_mode_
                ? QStringLiteral("SPH-2 模式：选择高度模型并完成两发校准")
                : QStringLiteral("迫击炮模式：使用原始平面距离与方位模型"));
        }
        sync_pinned_result();
        fit_window_to_content();
        QTimer::singleShot(0, this, [this] { fit_window_to_content(); });
    }

    void fit_window_to_content() {
        app_frame_->layout()->activate();
        app_frame_->adjustSize();
        adjustSize();
        const auto hint = sizeHint();
        resize(std::max(vehicle_mode_ ? 720 : 620, hint.width()), hint.height());
    }

    void update_terrain_summary(std::optional<double> height_delta = std::nullopt) {
        if (!terrain_map_) {
            const auto count = terrain_discovery_.installed.size();
            terrain_summary_->setText(count
                ? QStringLiteral("当前不计算高差 · 已检测到 %1 张地图").arg(count)
                : QStringLiteral("当前不计算高差 · 地图包目录：") +
                      QString::fromStdWString(
                          wardogs::default_terrain_directory().wstring()));
            return;
        }
        auto text = qtext(terrain_map_->spec.display_name) +
                    QStringLiteral(" · 2 m / 0.1 m 高度数据");
        if (height_delta) {
            const auto value = QString::number(*height_delta, 'f', 1);
            text += QStringLiteral(" · 目标高差 ") +
                    (*height_delta >= 0.0 ? QStringLiteral("+") : QString{}) +
                    value + QStringLiteral(" m");
        }
        terrain_summary_->setText(text);
    }

    std::optional<double> terrain_height(wardogs::Point point) {
        if (!terrain_) return 0.0;
        return terrain_->height_at(point);
    }

    double target_height_delta(wardogs::Point target) {
        const auto base_height = terrain_height(base_);
        const auto target_height = terrain_height(target);
        if (!base_height)
            throw std::invalid_argument("炮位坐标不在所选地图的高度数据范围内");
        if (!target_height)
            throw std::invalid_argument("目标坐标不在所选地图的高度数据范围内");
        return *target_height - *base_height;
    }

    void on_terrain_changed() {
        terrain_.reset();
        terrain_map_.reset();
        const int index = terrain_selector_->currentIndex();
        if (index > 0) {
            try {
                const auto& installed = terrain_discovery_.installed.at(
                    static_cast<std::size_t>(index - 1));
                terrain_ = std::make_unique<wardogs::TerrainPackage>(installed.path);
                terrain_map_ = installed;
            } catch (const std::exception& error) {
                terrain_selector_->blockSignals(true);
                terrain_selector_->setCurrentIndex(0);
                terrain_selector_->blockSignals(false);
                update_terrain_summary();
                set_status(QStringLiteral("地图高度包加载失败：") +
                               error_text(error), true);
                return;
            }
        }
        const bool had_data = has_calibration_data();
        invalidate_platform_calibration();
        if (had_data)
            calibration_summary_->setText(
                QStringLiteral("高度模型已改变，请重新完成两发校准"));
        if (target_ && show_result(*target_)) return;
        update_terrain_summary();
        if (terrain_map_)
            set_status(QStringLiteral("已加载 ") +
                       qtext(terrain_map_->spec.display_name) +
                       QStringLiteral(" 高度数据；校射与目标均应用高差补偿"));
        else
            set_status(QStringLiteral("高度模型已切换为等高假设") +
                       (had_data ? QStringLiteral("；请重新完成两发校准")
                                 : QString{}));
    }

    bool has_calibration_data() const {
        return platform_calibration_.has_value() || !first_aim_->text().isEmpty() ||
               !first_impact_->text().isEmpty() || !second_aim_->text().isEmpty() ||
               !second_impact_->text().isEmpty();
    }

    void record_manual_calibration() {
        if (next_calibration_shot_ > 2) {
            set_status(QStringLiteral("两发校准已经完成；请先点击清除校准"), true);
            return;
        }
        auto* editor = next_calibration_shot_ == 1 ? first_impact_ : second_impact_;
        try {
            record_calibration_impact(
                wardogs::parse_manual_coordinate(editor->text().toStdWString()),
                QStringLiteral("手动"));
        } catch (const std::exception& error) {
            set_status(QStringLiteral("第%1发实际落点：%2")
                           .arg(next_calibration_shot_)
                           .arg(error_text(error)), true);
        }
    }

    void record_calibration_impact(wardogs::Point impact, const QString& source) {
        if (next_calibration_shot_ > 2) {
            set_status(QStringLiteral("两发校准已经完成；请先点击清除校准"), true);
            return;
        }
        if (!target_) {
            set_status(QStringLiteral("请先设置当前目标，再录入实际落点"), true);
            return;
        }
        auto* aim = next_calibration_shot_ == 1 ? first_aim_ : second_aim_;
        auto* actual = next_calibration_shot_ == 1 ? first_impact_ : second_impact_;
        aim->setText(qtext(wardogs::format_point(*target_)));
        actual->setText(qtext(wardogs::format_point(impact)));
        if (next_calibration_shot_ == 1) {
            next_calibration_shot_ = 2;
            calibration_summary_->setText(
                QStringLiteral("已录入第一发 · 请更换方位后录入第二发"));
            set_status(source + QStringLiteral("已录入第一发实际落点 ") +
                       qtext(wardogs::format_point(impact)) +
                       QStringLiteral("；等待第二发"));
            return;
        }
        recalculate_vehicle();
    }

    bool recalculate_vehicle() {
        try {
            const wardogs::CalibrationShot first{
                wardogs::parse_manual_coordinate(first_aim_->text().toStdWString()),
                wardogs::parse_manual_coordinate(first_impact_->text().toStdWString()),
                arc_from_button(first_arc_)};
            const wardogs::CalibrationShot second{
                wardogs::parse_manual_coordinate(second_aim_->text().toStdWString()),
                wardogs::parse_manual_coordinate(second_impact_->text().toStdWString()),
                arc_from_button(second_arc_)};
            wardogs::HeightLookup lookup;
            if (terrain_)
                lookup = [this](wardogs::Point point) { return terrain_height(point); };
            platform_calibration_ = wardogs::calibrate_platform(
                base_, first, second, lookup);
        } catch (const std::exception& error) {
            platform_calibration_.reset();
            calibration_summary_->setText(QStringLiteral("校准失败"));
            set_status(QStringLiteral("校准失败：") + error_text(error), true);
            return false;
        }
        next_calibration_shot_ = 3;
        calibration_ocr_->setEnabled(false);
        calibration_manual_->setEnabled(false);
        calibration_summary_->setText(
            QStringLiteral("校准完成 · 两发夹角不一致残差 %1°")
                .arg(platform_calibration_->pair_angle_residual_deg, 0, 'f', 2));
        if (target_ && show_result(*target_)) return true;
        set_status(QStringLiteral("当前炮位校准完成；车体移动或姿态变化后请重新校准"));
        return true;
    }

    void clear_vehicle_calibration() {
        platform_calibration_.reset();
        next_calibration_shot_ = 1;
        for (auto* editor : {first_aim_, first_impact_, second_aim_, second_impact_})
            editor->clear();
        calibration_ocr_->setEnabled(true);
        calibration_manual_->setEnabled(true);
        calibration_summary_->setText(
            QStringLiteral("未校准 · 两发计划方位角差需在 30°～150°之间"));
        if (target_ && vehicle_mode_ && show_result(*target_)) return;
        set_status(QStringLiteral("已清除当前炮位补偿和两发记录"));
    }

    void invalidate_platform_calibration() {
        if (!has_calibration_data()) return;
        platform_calibration_.reset();
        next_calibration_shot_ = 1;
        for (auto* editor : {first_aim_, first_impact_, second_aim_, second_impact_})
            editor->clear();
        calibration_ocr_->setEnabled(true);
        calibration_manual_->setEnabled(true);
        calibration_summary_->setText(
            QStringLiteral("炮位已改变，请重新完成两发校准"));
    }

    void set_status(const QString& text, bool error = false) {
        set_failure_state(error);
        status_->setProperty("error", error);
        status_->style()->unpolish(status_);
        status_->style()->polish(status_);
        status_->setText(text);
    }

    void set_failure_state(bool failed) {
        failure_state_ = failed;
        if (app_frame_) {
            app_frame_->setProperty("error", failed);
            app_frame_->style()->unpolish(app_frame_);
            app_frame_->style()->polish(app_frame_);
            app_frame_->update();
        }
        if (pinned_window_) pinned_window_->set_error(failed);
    }

    void enter_pinned_mode() {
        if (!pinned_window_) {
            pinned_window_ = std::make_unique<PinnedResultWindow>(
                [this] { exit_pinned_mode(); }, settings_.pinned_card,
                [this](PinnedResultWindow::Preferences preferences) {
                    settings_.pinned_card = preferences;
                    try {
                        wardogs::save_settings(settings_);
                    } catch (...) {
                        // Display preferences remain active for this session.
                    }
                });
        }
        sync_pinned_result();
        pinned_window_->set_error(failure_state_);
        pinned_window_->move(frameGeometry().topLeft());
        pinned_mode_ = true;
        hide();
        pinned_window_->show();
        pinned_window_->raise();
    }

    void exit_pinned_mode() {
        if (!pinned_mode_) return;
        pinned_mode_ = false;
        if (pinned_window_) pinned_window_->hide();
        showNormal();
        raise();
        activateWindow();
    }

    void hide_for_selection() {
        if (pinned_mode_ && pinned_window_) pinned_window_->hide();
        else hide();
    }

    void restore_after_selection() {
        if (pinned_mode_ && pinned_window_) {
            pinned_window_->show();
            pinned_window_->raise();
        } else {
            showNormal();
            raise();
            activateWindow();
        }
    }

    void update_coordinates() {
        base_summary_->setText(QStringLiteral("基准点    ") + qtext(wardogs::format_point(base_)));
        target_summary_->setText(QStringLiteral("目标点    ") +
            (target_ ? qtext(wardogs::format_point(*target_)) : QStringLiteral("—")));
    }

    void update_action_labels() {
        region_button_->setText(qtext(settings_.region_hotkey) +
                                QStringLiteral("  设置区域"));
        base_button_->setText(qtext(settings_.base_hotkey) + QStringLiteral("  基准"));
        target_button_->setText(qtext(settings_.target_hotkey) + QStringLiteral("  目标"));
        quick_target_button_->setText(qtext(settings_.quick_target_hotkey) +
                                      QStringLiteral("  快速目标"));
    }

    void update_engine_summary() {
        engine_summary_->setText(settings_.backend == wardogs::OcrBackend::rapid
            ? QStringLiteral("当前引擎：RapidOCR（首选）")
            : QStringLiteral("当前引擎：Windows 系统 OCR（兼容备用）"));
    }

    void update_region_summary() {
        if (!region_) {
            region_summary_->setText(QStringLiteral("OCR 区域：尚未设置"));
            return;
        }
        const RECT& rect = region_->relative;
        region_summary_->setText(
            QStringLiteral("OCR 区域：%1 · %2×%3 px @ (%4, %5) · 已保存")
                .arg(qtext(region_->monitor_device))
                .arg(rect.right - rect.left).arg(rect.bottom - rect.top)
                .arg(rect.left).arg(rect.top));
    }

    void clear_result(const QString& text) {
        distance_->setText(QStringLiteral("—"));
        bearing_->setText(QStringLiteral("—"));
        raw_result_->setText(text);
        low_solution_->set_waiting();
        high_solution_->set_waiting();
        set_vehicle_result_error(false);
        vehicle_note_->setText(text);
        update_terrain_summary();
        sync_pinned_result();
    }

    bool show_result(wardogs::Point target) {
        const auto result = wardogs::calculate_shot(base_, target);
        if (vehicle_mode_) return show_vehicle_result(result);
        distance_->setText(qtext(wardogs::format_distance_meters(result.distance)));
        bearing_->setText(qtext(wardogs::format_bearing(result.angle)));
        raw_result_->setText(qtext(wardogs::format_raw_distance(result.distance)));
        sync_pinned_result();
        return false;
    }

    bool show_vehicle_result(const wardogs::Shot& result) {
        double height_delta{};
        try {
            height_delta = target_height_delta(result.target);
        } catch (const std::exception& error) {
            low_solution_->set_height_unavailable();
            high_solution_->set_height_unavailable();
            vehicle_note_->setText(QStringLiteral("无法读取炮位或目标点高度"));
            set_vehicle_result_error(true);
            update_terrain_summary();
            sync_pinned_result();
            set_status(QStringLiteral("高度模型失败：") + error_text(error), true);
            return true;
        }
        update_terrain_summary(height_delta);
        const wardogs::PlatformCalibration calibration = platform_calibration_.value_or(
            wardogs::PlatformCalibration{wardogs::identity_rotation(), 0.0});
        int available = 0;
        QStringList warnings;
        const auto raw_distance = qtext(wardogs::format_distance_meters(result.distance));
        const auto raw_bearing = qtext(wardogs::format_bearing(result.angle));
        for (const auto [arc, name, card] : std::array{
                 std::tuple{wardogs::Arc::low, QStringLiteral("低射"), low_solution_},
                 std::tuple{wardogs::Arc::high, QStringLiteral("高抛"), high_solution_}}) {
            try {
                card->set_solution(wardogs::corrected_solution(
                    result.base, result.target, calibration, arc, height_delta));
                ++available;
            } catch (const std::exception& error) {
                card->set_unavailable(raw_distance, raw_bearing);
                warnings.push_back(name + QStringLiteral("：") + error_text(error));
            }
        }
        const bool none_available = available == 0;
        set_vehicle_result_error(none_available);
        sync_pinned_result();
        if (!warnings.isEmpty()) {
            if (available > 0) {
                vehicle_note_->setText(QStringLiteral("部分射表超出范围 · 仍有可用弹道"));
                set_status(warnings.join(QStringLiteral("；")) +
                           QStringLiteral("；另一条弹道仍可使用"));
            } else {
                vehicle_note_->setText(
                    QStringLiteral("超出射程 · 已显示目标距离和方位，无可用分划"));
                set_status(QStringLiteral("超出射程：") +
                           warnings.join(QStringLiteral("；")), true);
            }
            return true;
        }
        if (!platform_calibration_) {
            vehicle_note_->setText(terrain_map_
                ? QStringLiteral("未校准 · 已应用 %1 高差 %2 m")
                      .arg(qtext(terrain_map_->spec.display_name))
                      .arg(height_delta, 0, 'f', 1)
                : QStringLiteral("未校准 · 当前显示平地参考射表"));
        } else {
            vehicle_note_->setText(terrain_map_
                ? QStringLiteral("已应用两发校准和 %1 高差 %2 m")
                      .arg(qtext(terrain_map_->spec.display_name))
                      .arg(height_delta, 0, 'f', 1)
                : QStringLiteral("已应用当前炮位两发校准补偿"));
        }
        return false;
    }

    void set_vehicle_result_error(bool error) {
        vehicle_result_group_->setProperty("error", error);
        vehicle_result_group_->style()->unpolish(vehicle_result_group_);
        vehicle_result_group_->style()->polish(vehicle_result_group_);
        vehicle_result_group_->update();
    }

    void sync_pinned_result() {
        if (!pinned_window_) return;
        pinned_window_->set_mode(vehicle_mode_);
        if (vehicle_mode_)
            pinned_window_->set_vehicle_values(*low_solution_, *high_solution_);
        else
            pinned_window_->set_values(distance_->text(), bearing_->text());
    }

    void manual_base() {
        try {
            base_ = wardogs::parse_manual_coordinate(base_input_->text().toStdWString());
            target_.reset();
            invalidate_platform_calibration();
            base_input_->clear();
            update_coordinates();
            clear_result(QStringLiteral("基准点已更新；等待目标坐标…"));
            set_status(QStringLiteral("基准点已设为 ") + qtext(wardogs::format_point(base_)));
        } catch (const std::exception& error) { set_status(error_text(error), true); }
    }

    void manual_target() {
        try {
            const auto point = wardogs::parse_manual_coordinate(target_input_->text().toStdWString());
            target_ = point;
            target_input_->clear();
            update_coordinates();
            if (!show_result(point)) set_status(QStringLiteral("目标点已计算"));
        } catch (const std::exception& error) { set_status(error_text(error), true); }
    }

    void begin_region_setup() {
        if (busy_) { set_status(QStringLiteral("OCR 正在执行，请稍候")); return; }
        hide_for_selection();
        const bool started = selector_.begin(
            [this](std::optional<wardogs::CaptureRegion> region, QString error) {
                restore_after_selection();
                if (!error.isEmpty()) { set_status(error, true); return; }
                if (!region) { set_status(QStringLiteral("已取消设置区域")); return; }
                region_ = std::move(region);
                settings_.capture_region = region_;
                update_region_summary();
                try {
                    wardogs::save_settings(settings_);
                    set_status(QStringLiteral("OCR 区域已保存；显示器已自动识别"));
                } catch (const std::exception& error) {
                    set_status(QStringLiteral("OCR 区域可在本次运行使用，但保存失败：") +
                                   error_text(error), true);
                }
            });
        if (!started) {
            restore_after_selection();
            set_status(QStringLiteral("无法启动区域设置"), true);
        }
    }

    void begin_quick_target() {
        if (busy_) { set_status(QStringLiteral("OCR 正在执行，请稍候")); return; }
        hide_for_selection();
        const bool started = selector_.begin(
            [this](std::optional<wardogs::CaptureRegion> region, QString error) {
                if (!error.isEmpty()) {
                    restore_after_selection();
                    set_status(error, true);
                    return;
                }
                if (!region) {
                    restore_after_selection();
                    set_status(QStringLiteral("已取消快速目标框选"));
                    return;
                }
                start_ocr(*region, OcrAction::target);
                restore_after_selection();
            });
        if (!started) {
            restore_after_selection();
            set_status(QStringLiteral("无法启动快速目标框选"), true);
        }
    }

    void start_ocr(OcrAction action) {
        if (action == OcrAction::calibration_impact && !target_) {
            set_status(QStringLiteral("请先设置当前目标，再录入实际落点"), true);
            return;
        }
        if (!region_) { begin_region_setup(); return; }
        start_ocr(*region_, action);
    }

    void start_ocr(const wardogs::CaptureRegion& capture_region, OcrAction action) {
        if (busy_.exchange(true)) { set_status(QStringLiteral("OCR 正在执行，请稍候")); return; }
        wardogs::Image image;
        try { image = wardogs::capture_screen(capture_region); }
        catch (const std::exception& error) {
            busy_ = false;
            set_status(QStringLiteral("截图失败：") + error_text(error), true);
            return;
        }
        if (worker_.joinable()) worker_.join();
        const auto backend = settings_.backend;
        const auto pattern = settings_.coordinate_pattern;
        set_status(backend == wardogs::OcrBackend::rapid
                       ? QStringLiteral("RapidOCR 识别中…")
                       : QStringLiteral("Windows OCR 识别中…"));
        QPointer<MainWindow> self(this);
        worker_ = std::jthread([this, self, image = std::move(image), backend,
                                pattern, action](std::stop_token) mutable {
            OcrMessage message;
            message.action = action;
            try {
                wardogs::OcrResult result;
                if (backend == wardogs::OcrBackend::rapid) {
                    if (!rapid_) rapid_ = std::make_unique<wardogs::RapidOcr>(
                        executable_directory() / L"models" / L"PP-OCRv6_rec_small.onnx");
                    result = rapid_->recognize(image);
                    message.text = result.text;
                    message.confidence = result.confidence;
                    message.point = wardogs::parse_ocr_coordinate(result.text, pattern);
                } else {
                    if (!windows_) windows_ = std::make_unique<wardogs::WindowsOcr>();
                    result = windows_->recognize(image);
                    message.text = result.text;
                    try { message.point = wardogs::parse_ocr_coordinate(result.text, pattern); }
                    catch (const std::invalid_argument&) {
                        result = windows_->recognize_high_contrast(image);
                        message.text = result.text;
                    }
                    message.point = wardogs::parse_ocr_coordinate(result.text, pattern);
                    message.confidence = result.confidence;
                }
                message.success = true;
            } catch (const std::exception& error) { message.error = error_text(error); }
            if (self) QMetaObject::invokeMethod(self,
                [self, message = std::move(message)]() mutable {
                    if (self) self->finish_ocr(std::move(message));
                }, Qt::QueuedConnection);
        });
    }

    void finish_ocr(OcrMessage message) {
        busy_ = false;
        if (!message.success) {
            ocr_text_->setText(QStringLiteral("OCR 原文：") +
                (message.text.empty() ? QStringLiteral("（空）") : qtext(message.text)));
            set_status(QStringLiteral("OCR 失败：") + message.error, true);
            return;
        }
        QString confidence;
        if (message.confidence > 0.0F)
            confidence = QStringLiteral(" · 平均置信度 %1%").arg(qRound(message.confidence * 100.0F));
        ocr_text_->setText(QStringLiteral("OCR 原文：") + qtext(message.text) + confidence);
        if (message.action == OcrAction::calibration_impact) {
            record_calibration_impact(message.point, QStringLiteral("OCR"));
            return;
        }
        if (message.action == OcrAction::base) {
            base_ = message.point;
            target_.reset();
            invalidate_platform_calibration();
            clear_result(QStringLiteral("基准点已更新；等待目标坐标…"));
            set_status(QStringLiteral("OCR 基准点：") + qtext(wardogs::format_point(message.point)));
        } else {
            target_ = message.point;
            if (!show_result(message.point))
                set_status(QStringLiteral("OCR 目标点：") +
                           qtext(wardogs::format_point(message.point)) +
                           QStringLiteral("；计算完成"));
        }
        update_coordinates();
    }

    void edit_settings() {
        if (busy_) {
            set_status(QStringLiteral("OCR 正在执行，请稍候再修改设置"));
            return;
        }
        SettingsDialog dialog(settings_, this);
        unregister_hotkeys();
        if (dialog.exec() != QDialog::Accepted) {
            try {
                register_hotkeys(settings_);
            } catch (const std::exception& error) {
                set_status(QStringLiteral("热键恢复失败：") + error_text(error), true);
            }
            return;
        }
        const auto candidate = dialog.settings();
        const auto previous = settings_;
        try {
            register_hotkeys(candidate);
            wardogs::save_settings(candidate);
            settings_ = candidate;
            rapid_.reset(); windows_.reset();
            update_engine_summary(); update_action_labels();
            set_status(QStringLiteral("设置已保存并立即生效"));
        } catch (const std::exception& error) {
            try { register_hotkeys(previous); } catch (...) {}
            set_status(QStringLiteral("设置保存失败：") + error_text(error), true);
        }
    }

    void unregister_hotkeys() {
        hotkey_listener_.stop();
    }

    void register_hotkeys(const wardogs::AppSettings& settings) {
        const std::array values{wardogs::parse_hotkey(settings.region_hotkey),
                                wardogs::parse_hotkey(settings.base_hotkey),
                                wardogs::parse_hotkey(settings.target_hotkey),
                                wardogs::parse_hotkey(settings.quick_target_hotkey)};
        wardogs::validate_unique_hotkeys(values);
        hotkey_listener_.start(values, [this](std::size_t index) {
            QMetaObject::invokeMethod(this, [this, index] {
                if (index == 0) begin_region_setup();
                else if (index == 1) start_ocr(OcrAction::base);
                else if (index == 2) start_ocr(OcrAction::target);
                else if (index == 3) begin_quick_target();
            }, Qt::QueuedConnection);
        });
    }
};

constexpr auto style_sheet = R"(
QWidget { color:#e5e7eb; font-family:"Microsoft YaHei UI"; font-size:13px; }
QMainWindow,QDialog { background:#111827; }
QFrame#appFrame { background:#111827; border:3px solid transparent; }
QFrame#appFrame[error="true"] { border-color:#ef4444; }
QFrame#pinnedFrame { background:#0f172a; border:3px solid transparent;
                     border-radius:10px; }
QFrame#pinnedFrame[error="true"] { border-color:#ef4444; }
QMenu#pinnedContextMenu { background:#111827; border:1px solid #475569;
                          border-radius:8px; padding:3px; }
QWidget#pinnedControlPanel { background:#111827; }
QToolButton#pinnedLockButton { background:#0f172a; border:1px solid #475569;
    border-radius:6px; padding:4px; }
QToolButton#pinnedLockButton:hover { background:#1e293b; border-color:#64748b; }
QToolButton#pinnedLockButton:checked { background:#164e63; border-color:#22d3ee; }
QSlider#pinnedOpacitySlider::groove:horizontal { height:5px; background:#334155;
    border-radius:2px; }
QSlider#pinnedOpacitySlider::sub-page:horizontal { background:#38bdf8;
    border-radius:2px; }
QSlider#pinnedOpacitySlider::handle:horizontal { background:#e2e8f0;
    border:1px solid #64748b; width:15px; margin:-6px 0; border-radius:7px; }
QSlider#pinnedOpacitySlider::handle:horizontal:hover { background:#f8fafc;
    border-color:#38bdf8; }
QLabel#title,QLabel#dialogTitle { color:#f8fafc; font-size:26px; font-weight:700; }
QLabel#dialogTitle { font-size:23px; }
QLabel#muted { color:#94a3b8; }
QLabel#status { color:#7dd3fc; padding:7px 2px; }
QLabel#status[error="true"] { color:#fca5a5; }
QLabel#resultCaption { color:#94a3b8; font-size:13px; font-weight:600; }
QLabel#rawResult { color:#64748b; font-size:12px; padding:3px; }
QFrame#resultCard { background:#0b1220; border:1px solid #334155; border-radius:8px; }
QFrame#vehicleSolutionCard { background:#0b1220; border:1px solid #334155;
                             border-radius:8px; }
QFrame#vehicleSolutionCard[unavailable="true"] { border-color:#64748b; }
QLabel#solutionArc { color:#94a3b8; font-size:13px; font-weight:600; }
QLabel#solutionMetricCaption { color:#64748b; font-size:11px; }
QLabel#solutionDistance,QLabel#solutionBearing,QLabel#solutionMil {
    font-family:"Bahnschrift"; font-size:25px; font-weight:700; }
QLabel#solutionDistance { color:#fbbf24; }
QLabel#solutionBearing { color:#67e8f9; }
QLabel#solutionMil { color:#c4b5fd; }
QLabel#solutionMil[unavailable="true"] { color:#fca5a5; font-size:19px; }
QGroupBox#vehicleResultGroup[error="true"] { border:2px solid #ef4444; }
QGroupBox { background:#0f172a; border:1px solid #334155; border-radius:8px;
            margin-top:9px; padding-top:10px; font-weight:600; }
QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 5px; color:#f1f5f9; }
QLineEdit,QPlainTextEdit,QKeySequenceEdit,QComboBox { background:#0b1220;
    border:1px solid #475569; border-radius:5px; padding:7px; color:#f8fafc;
    selection-background-color:#2563eb; }
QLineEdit:focus,QPlainTextEdit:focus,QKeySequenceEdit:focus,QComboBox:focus { border-color:#3b82f6; }
QComboBox::drop-down { border:0; width:28px; }
QComboBox QAbstractItemView { background:#0f172a; border:1px solid #475569;
    color:#f8fafc; selection-background-color:#1d4ed8; padding:4px; }
QPushButton { background:#1e3a5f; border:1px solid #2563eb; border-radius:5px;
              padding:7px 10px; min-height:18px; }
QPushButton:hover { background:#1d4ed8; }
QPushButton:pressed { background:#1e40af; }
QPushButton[primary="true"] { background:#1d4ed8; }
QPushButton[primary="true"]:hover { background:#2563eb; }
QPushButton#arcToggle { min-width:58px; padding-left:8px; padding-right:8px;
    background:#0f172a; color:#cbd5e1; border-color:#475569; }
QPushButton#arcToggle:hover { background:#1e3a5f; }
QPushButton#arcToggle[highlighted="true"] { background:#1d4ed8; color:#f8fafc;
    border-color:#60a5fa; font-weight:700; }
QPushButton#arcToggle[highlighted="true"]:hover {
    background:#2563eb; border-color:#93c5fd; }
QPushButton#arcToggle[highlighted="true"]:pressed { background:#1e40af; }
QPushButton#iconButton { background:#0f172a; border:1px solid #334155;
                         border-radius:6px; padding:5px; min-height:0; }
QPushButton#iconButton:hover { background:#1e3a5f; border-color:#3b82f6; }
QPushButton#iconButton:pressed { background:#1e40af; }
QPushButton:disabled { color:#94a3b8; background:#334155; border-color:#475569; }
QScrollBar:vertical { background:#0b1220; width:10px; margin:0; }
QScrollBar::handle:vertical { background:#475569; border-radius:4px; min-height:24px; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
)";

}  // namespace

int run_application(int argc, char* argv[]) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setApplicationVersion(QStringLiteral(WARDOGS_VERSION));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(QString::fromUtf8(style_sheet));
    MainWindow window;
    window.show();
    return app.exec();
}
