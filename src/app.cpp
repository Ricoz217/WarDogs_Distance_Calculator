#include "wardogs/capture.hpp"
#include "wardogs/core.hpp"
#include "wardogs/hotkeys.hpp"
#include "wardogs/ocr.hpp"
#include "wardogs/settings.hpp"
#include "wardogs/windows_ocr.hpp"

#include <Windows.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <winrt/base.h>

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QStyleFactory>
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

class PinnedResultWindow final : public QWidget {
public:
    explicit PinnedResultWindow(std::function<void()> exit_callback)
        : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint |
                               Qt::WindowStaysOnTopHint),
          exit_callback_(std::move(exit_callback)) {
        setObjectName(QStringLiteral("pinnedWindow"));
        setAttribute(Qt::WA_TranslucentBackground);
        setCursor(Qt::OpenHandCursor);
        setWindowTitle(QStringLiteral("War Dogs 射表结果"));

        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        frame_ = new QFrame;
        frame_->setObjectName(QStringLiteral("pinnedFrame"));
        frame_->setProperty("error", false);
        auto* layout = new QHBoxLayout(frame_);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(8);
        layout->addWidget(result_card(QStringLiteral("射程"), QStringLiteral("#fbbf24"),
                                      distance_), 1);
        layout->addWidget(result_card(QStringLiteral("方位"), QStringLiteral("#67e8f9"),
                                      bearing_), 1);
        outer->addWidget(frame_);

        resize(470, 128);
    }

    void set_values(const QString& distance, const QString& bearing) {
        distance_->setText(distance);
        bearing_->setText(bearing);
    }

    void set_error(bool error) {
        frame_->setProperty("error", error);
        frame_->style()->unpolish(frame_);
        frame_->style()->polish(frame_);
        frame_->update();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            dragging_ = true;
            drag_offset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (dragging_ && (event->buttons() & Qt::LeftButton)) {
            move(event->globalPosition().toPoint() - drag_offset_);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            dragging_ = false;
            setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            dragging_ = false;
            if (exit_callback_) exit_callback_();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    std::function<void()> exit_callback_;
    QFrame* frame_{};
    QLabel *distance_{}, *bearing_{};
    bool dragging_{};
    QPoint drag_offset_{};
};

class SelectionOverlay {
public:
    using Callback = std::function<void(std::optional<wardogs::CaptureRegion>, QString)>;

    ~SelectionOverlay() { if (window_) DestroyWindow(window_); }

    bool begin(Callback callback) {
        if (window_) return false;
        callback_ = std::move(callback);
        ensure_class();
        const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        window_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, class_name,
            L"设置 OCR 区域", WS_POPUP, left, top, width, height, nullptr, nullptr,
            GetModuleHandleW(nullptr), this);
        if (!window_) { callback_ = {}; return false; }
        SetLayeredWindowAttributes(window_, 0, 108, LWA_ALPHA);
        ShowWindow(window_, SW_SHOW);
        SetForegroundWindow(window_);
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        SetCapture(window_);
        return true;
    }

private:
    static constexpr wchar_t class_name[] = L"WarDogsDistanceCalculatorSelectorQt";
    HWND window_{};
    bool selecting_{};
    POINT start_{};
    POINT end_{};
    HMONITOR monitor_{};
    Callback callback_;

    static void ensure_class() {
        static const ATOM atom = [] {
            WNDCLASSEXW value{sizeof(value)};
            value.style = CS_HREDRAW | CS_VREDRAW;
            value.lpfnWndProc = window_proc;
            value.hInstance = GetModuleHandleW(nullptr);
            value.hCursor = LoadCursorW(nullptr, IDC_CROSS);
            value.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            value.lpszClassName = class_name;
            return RegisterClassExW(&value);
        }();
        (void)atom;
    }

    void finish(std::optional<wardogs::CaptureRegion> region, QString error = {}) {
        auto callback = std::move(callback_);
        callback_ = {};
        if (window_) DestroyWindow(window_);
        if (callback) callback(std::move(region), std::move(error));
    }

    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
            case WM_NCHITTEST:
                return HTCLIENT;
            case WM_SETCURSOR:
                SetCursor(LoadCursorW(nullptr, IDC_CROSS));
                return TRUE;
            case WM_LBUTTONDOWN:
                selecting_ = true;
                start_ = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ClientToScreen(window_, &start_);
                end_ = start_;
                monitor_ = MonitorFromPoint(start_, MONITOR_DEFAULTTONEAREST);
                SetCapture(window_);
                return 0;
            case WM_MOUSEMOVE:
                if (selecting_) {
                    POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                    ClientToScreen(window_, &point);
                    MONITORINFO info{sizeof(info)};
                    GetMonitorInfoW(monitor_, &info);
                    point.x = std::clamp(point.x, info.rcMonitor.left, info.rcMonitor.right);
                    point.y = std::clamp(point.y, info.rcMonitor.top, info.rcMonitor.bottom);
                    end_ = point;
                    InvalidateRect(window_, nullptr, TRUE);
                }
                return 0;
            case WM_LBUTTONUP:
                if (selecting_) {
                    selecting_ = false;
                    ReleaseCapture();
                    RECT selected{start_.x, start_.y, end_.x, end_.y};
                    if (std::abs(selected.right - selected.left) < 8 ||
                        std::abs(selected.bottom - selected.top) < 8) {
                        finish(std::nullopt, QStringLiteral("框选区域太小，请重新框选"));
                        return 0;
                    }
                    try { finish(wardogs::make_capture_region(monitor_, selected)); }
                    catch (const std::exception& error) {
                        finish(std::nullopt, error_text(error));
                    }
                }
                return 0;
            case WM_KEYDOWN:
                if (wparam == VK_ESCAPE) { ReleaseCapture(); finish(std::nullopt); }
                return 0;
            case WM_PAINT: {
                PAINTSTRUCT paint{};
                HDC dc = BeginPaint(window_, &paint);
                FillRect(dc, &paint.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                if (selecting_) {
                    RECT rect{start_.x, start_.y, end_.x, end_.y};
                    POINT origin{};
                    ClientToScreen(window_, &origin);
                    OffsetRect(&rect, -origin.x, -origin.y);
                    HPEN pen = CreatePen(PS_SOLID, 4, RGB(34, 211, 153));
                    HGDIOBJ old_pen = SelectObject(dc, pen);
                    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
                    SelectObject(dc, old_brush);
                    SelectObject(dc, old_pen);
                    DeleteObject(pen);
                }
                EndPaint(window_, &paint);
                return 0;
            }
            case WM_DESTROY: window_ = nullptr; return 0;
        }
        return DefWindowProcW(window_, message, wparam, lparam);
    }

    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam,
                                        LPARAM lparam) {
        auto* self = reinterpret_cast<SelectionOverlay*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<SelectionOverlay*>(create->lpCreateParams);
            self->window_ = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->handle(message, wparam, lparam)
                    : DefWindowProcW(window, message, wparam, lparam);
    }
};

class SettingsDialog final : public QDialog {
public:
    explicit SettingsDialog(const wardogs::AppSettings& settings, QWidget* parent)
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

        auto* panel = new QGroupBox(QStringLiteral("识别与热键"));
        auto* form = new QFormLayout(panel);
        form->setContentsMargins(14, 18, 14, 14);
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
        root->addWidget(buttons);
        enable_dark_title_bar(reinterpret_cast<HWND>(winId()));
    }

    wardogs::AppSettings settings() const {
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

private:
    QComboBox* backend_{};
    QKeySequenceEdit* region_key_{};
    QKeySequenceEdit* base_key_{};
    QKeySequenceEdit* target_key_{};
    QKeySequenceEdit* quick_target_key_{};
    QPlainTextEdit* pattern_{};
    std::optional<wardogs::CaptureRegion> capture_region_;

    static QKeySequenceEdit* make_hotkey(const std::wstring& value,
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

    static std::wstring hotkey_text(const QKeySequenceEdit* editor) {
        return editor->keySequence().toString(QKeySequence::PortableText).toStdWString();
    }

    void accept_if_valid() {
        try {
            const auto value = settings();
            const std::array hotkeys{
                wardogs::parse_hotkey(value.region_hotkey),
                wardogs::parse_hotkey(value.base_hotkey),
                wardogs::parse_hotkey(value.target_hotkey),
                wardogs::parse_hotkey(value.quick_target_hotkey),
            };
            wardogs::validate_unique_hotkeys(hotkeys);
            const std::wregex pattern(value.coordinate_pattern, std::regex_constants::icase);
            if (pattern.mark_count() < 2) {
                throw std::invalid_argument("coordinate regex needs x/y capture groups");
            }
            QDialog::accept();
        } catch (const std::regex_error&) {
            QMessageBox::warning(this, QStringLiteral("坐标正则不可用"),
                                 QStringLiteral("坐标正则语法无效"));
        } catch (const std::exception& error) {
            QMessageBox::warning(this, QStringLiteral("设置不可用"), error_text(error));
        }
    }
};

struct OcrMessage {
    bool success{};
    bool base_action{};
    wardogs::Point point{};
    std::wstring text;
    float confidence{};
    QString error;
};

class MainWindow final : public QMainWindow, public QAbstractNativeEventFilter {
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
        qApp->installNativeEventFilter(this);
        try { register_hotkeys(settings_); }
        catch (const std::exception& error) {
            set_status(QStringLiteral("热键启动失败：") + error_text(error), true);
        }
    }

    ~MainWindow() override {
        qApp->removeNativeEventFilter(this);
        unregister_hotkeys();
        if (worker_.joinable()) worker_.join();
    }

    bool nativeEventFilter(const QByteArray& event_type, void* message,
                           qintptr* result) override {
        if (event_type != "windows_generic_MSG" && event_type != "windows_dispatcher_MSG")
            return false;
        auto* native = static_cast<MSG*>(message);
        if (native->message != WM_HOTKEY) return false;
        if (native->wParam == 1) begin_region_setup();
        else if (native->wParam == 2) start_ocr(true);
        else if (native->wParam == 3) start_ocr(false);
        else if (native->wParam == 4) begin_quick_target();
        if (result) *result = 0;
        return true;
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
    std::optional<wardogs::CaptureRegion> region_;
    std::unique_ptr<wardogs::RapidOcr> rapid_;
    std::unique_ptr<wardogs::WindowsOcr> windows_;
    std::jthread worker_;
    std::atomic_bool busy_{false};
    SelectionOverlay selector_;
    std::unique_ptr<PinnedResultWindow> pinned_window_;
    bool pinned_mode_{};
    bool failure_state_{};
    QFrame* app_frame_{};
    QLabel *base_summary_{}, *target_summary_{}, *distance_{}, *bearing_{};
    QLabel *raw_result_{}, *engine_summary_{}, *region_summary_{}, *ocr_text_{}, *status_{};
    QLineEdit *base_input_{}, *target_input_{};
    QPushButton *region_button_{}, *base_button_{}, *target_button_{},
        *quick_target_button_{};

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
        auto* pin_button = new QPushButton;
        pin_button->setObjectName(QStringLiteral("iconButton"));
        pin_button->setIcon(pin_icon());
        pin_button->setIconSize(QSize(22, 22));
        pin_button->setFixedSize(38, 34);
        pin_button->setToolTip(QStringLiteral("置顶显示射表结果"));
        pin_button->setAccessibleName(QStringLiteral("进入置顶模式"));
        heading->addWidget(title);
        heading->addStretch();
        heading->addWidget(pin_button);
        auto* subtitle = new QLabel(QStringLiteral("OCR 热键和手动输入共用同一套计算逻辑"));
        subtitle->setObjectName(QStringLiteral("muted"));
        root->addLayout(heading);
        root->addWidget(subtitle);

        auto* coordinates = new QGroupBox(QStringLiteral("坐标"));
        auto* coordinate_layout = new QVBoxLayout(coordinates);
        coordinate_layout->setSpacing(7);
        base_summary_ = new QLabel;
        target_summary_ = new QLabel;
        coordinate_layout->addWidget(base_summary_);
        coordinate_layout->addWidget(target_summary_);
        auto* base_row = new QHBoxLayout;
        base_input_ = new QLineEdit;
        base_input_->setPlaceholderText(QStringLiteral("基准点：x12.34, y56.78 或 12.34 56.78"));
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

        auto* results = new QGroupBox(QStringLiteral("计算结果"));
        auto* result_layout = new QVBoxLayout(results);
        auto* cards = new QHBoxLayout;
        cards->setSpacing(12);
        cards->addWidget(result_card(QStringLiteral("射程"), QStringLiteral("#fbbf24"), distance_), 1);
        cards->addWidget(result_card(QStringLiteral("方位"), QStringLiteral("#67e8f9"), bearing_), 1);
        result_layout->addLayout(cards);
        raw_result_ = new QLabel(QStringLiteral("等待目标坐标…"));
        raw_result_->setObjectName(QStringLiteral("rawResult"));
        raw_result_->setAlignment(Qt::AlignCenter);
        result_layout->addWidget(raw_result_);
        root->addWidget(results);

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
        target_button_->setProperty("primary", true);
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
        connect(region_button_, &QPushButton::clicked, this, &MainWindow::begin_region_setup);
        connect(base_button_, &QPushButton::clicked, this, [this] { start_ocr(true); });
        connect(target_button_, &QPushButton::clicked, this, [this] { start_ocr(false); });
        connect(quick_target_button_, &QPushButton::clicked, this,
                &MainWindow::begin_quick_target);
        connect(settings_button, &QPushButton::clicked, this, &MainWindow::edit_settings);
        connect(pin_button, &QPushButton::clicked, this, &MainWindow::enter_pinned_mode);
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
                [this] { exit_pinned_mode(); });
        }
        pinned_window_->set_values(distance_->text(), bearing_->text());
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
        if (pinned_window_)
            pinned_window_->set_values(distance_->text(), bearing_->text());
    }

    void show_result(wardogs::Point target) {
        const auto result = wardogs::calculate_shot(base_, target);
        distance_->setText(qtext(wardogs::format_distance_meters(result.distance)));
        bearing_->setText(qtext(wardogs::format_bearing(result.angle)));
        raw_result_->setText(qtext(wardogs::format_raw_distance(result.distance)));
        if (pinned_window_)
            pinned_window_->set_values(distance_->text(), bearing_->text());
    }

    void manual_base() {
        try {
            base_ = wardogs::parse_manual_coordinate(base_input_->text().toStdWString());
            target_.reset();
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
            show_result(point);
            set_status(QStringLiteral("目标点已计算"));
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
                start_ocr(*region, false);
                restore_after_selection();
            });
        if (!started) {
            restore_after_selection();
            set_status(QStringLiteral("无法启动快速目标框选"), true);
        }
    }

    void start_ocr(bool base_action) {
        if (!region_) { begin_region_setup(); return; }
        start_ocr(*region_, base_action);
    }

    void start_ocr(const wardogs::CaptureRegion& capture_region, bool base_action) {
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
                                pattern, base_action](std::stop_token) mutable {
            OcrMessage message;
            message.base_action = base_action;
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
        if (message.base_action) {
            base_ = message.point;
            target_.reset();
            clear_result(QStringLiteral("基准点已更新；等待目标坐标…"));
            set_status(QStringLiteral("OCR 基准点：") + qtext(wardogs::format_point(message.point)));
        } else {
            target_ = message.point;
            show_result(message.point);
            set_status(QStringLiteral("OCR 目标点：") + qtext(wardogs::format_point(message.point)) +
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
        const HWND window = reinterpret_cast<HWND>(winId());
        for (int id = 1; id <= 4; ++id) UnregisterHotKey(window, id);
    }

    void register_hotkeys(const wardogs::AppSettings& settings) {
        const std::array values{wardogs::parse_hotkey(settings.region_hotkey),
                                wardogs::parse_hotkey(settings.base_hotkey),
                                wardogs::parse_hotkey(settings.target_hotkey),
                                wardogs::parse_hotkey(settings.quick_target_hotkey)};
        wardogs::validate_unique_hotkeys(values);
        unregister_hotkeys();
        const HWND window = reinterpret_cast<HWND>(winId());
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!RegisterHotKey(window, static_cast<int>(i + 1), values[i].modifiers,
                                values[i].virtual_key)) {
                unregister_hotkeys();
                throw std::runtime_error("a hotkey is already in use by another program");
            }
        }
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
QLabel#title,QLabel#dialogTitle { color:#f8fafc; font-size:26px; font-weight:700; }
QLabel#dialogTitle { font-size:23px; }
QLabel#muted { color:#94a3b8; }
QLabel#status { color:#7dd3fc; padding:7px 2px; }
QLabel#status[error="true"] { color:#fca5a5; }
QLabel#resultCaption { color:#94a3b8; font-size:13px; font-weight:600; }
QLabel#rawResult { color:#64748b; font-size:12px; padding:3px; }
QFrame#resultCard { background:#0b1220; border:1px solid #334155; border-radius:8px; }
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

int main(int argc, char* argv[]) {
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
