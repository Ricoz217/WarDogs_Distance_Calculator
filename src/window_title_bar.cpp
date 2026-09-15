#include "window_title_bar.hpp"

#include <Windows.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QWindow>

#include <algorithm>

namespace {

enum class CaptionGlyph { minimize, maximize, restore, close, app };

QIcon caption_icon(CaptionGlyph glyph) {
    const QSize size = glyph == CaptionGlyph::app ? QSize(18, 18) : QSize(20, 20);
    QPixmap image(size);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color(glyph == CaptionGlyph::app ? QStringLiteral("#67e8f9")
                                                  : QStringLiteral("#cbd5e1"));
    painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    switch (glyph) {
    case CaptionGlyph::minimize:
        painter.drawLine(QPointF(5, 14), QPointF(15, 14));
        break;
    case CaptionGlyph::maximize:
        painter.drawRoundedRect(QRectF(5.5, 5.5, 9, 9), 0.8, 0.8);
        break;
    case CaptionGlyph::restore:
        painter.drawRoundedRect(QRectF(7.5, 5.5, 8, 8), 0.7, 0.7);
        painter.drawRoundedRect(QRectF(4.5, 8.5, 8, 8), 0.7, 0.7);
        break;
    case CaptionGlyph::close:
        painter.drawLine(QPointF(5.5, 5.5), QPointF(14.5, 14.5));
        painter.drawLine(QPointF(14.5, 5.5), QPointF(5.5, 14.5));
        break;
    case CaptionGlyph::app:
        painter.drawEllipse(QPointF(9, 9), 5.5, 5.5);
        painter.drawEllipse(QPointF(9, 9), 1.8, 1.8);
        painter.drawLine(QPointF(9, 1), QPointF(9, 4));
        painter.drawLine(QPointF(9, 14), QPointF(9, 17));
        painter.drawLine(QPointF(1, 9), QPointF(4, 9));
        painter.drawLine(QPointF(14, 9), QPointF(17, 9));
        break;
    }
    return QIcon(image);
}

QToolButton* caption_button(QWidget* parent, const QString& object_name,
                            CaptionGlyph glyph, const QString& accessible_name) {
    auto* button = new QToolButton(parent);
    button->setObjectName(object_name);
    button->setProperty("windowControl", true);
    button->setIcon(caption_icon(glyph));
    button->setIconSize(QSize(20, 20));
    button->setFixedSize(42, 38);
    button->setToolTip(accessible_name);
    button->setAccessibleName(accessible_name);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

}  // namespace

WindowTitleBar::WindowTitleBar(QWidget* host) : QWidget(host), host_(host) {
    setObjectName(QStringLiteral("windowTitleBar"));
    setFixedHeight(40);
    setAttribute(Qt::WA_StyledBackground);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(13, 1, 5, 1);
    layout->setSpacing(6);

    auto* app_icon = new QLabel;
    app_icon->setPixmap(caption_icon(CaptionGlyph::app).pixmap(18, 18));
    app_icon->setFixedSize(20, 20);
    app_icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(app_icon);

    title_ = new QLabel(host_->windowTitle());
    title_->setObjectName(QStringLiteral("windowTitleText"));
    title_->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(title_);
    layout->addStretch();

    auto* minimize = caption_button(
        this, QStringLiteral("windowMinimizeButton"), CaptionGlyph::minimize,
        QStringLiteral("最小化"));
    maximize_ = caption_button(
        this, QStringLiteral("windowMaximizeButton"), CaptionGlyph::maximize,
        QStringLiteral("最大化"));
    auto* close = caption_button(
        this, QStringLiteral("windowCloseButton"), CaptionGlyph::close,
        QStringLiteral("关闭"));
    close->setProperty("closeControl", true);
    layout->addWidget(minimize);
    layout->addWidget(maximize_);
    layout->addWidget(close);

    connect(minimize, &QToolButton::clicked, host_, &QWidget::showMinimized);
    connect(maximize_, &QToolButton::clicked, this,
            &WindowTitleBar::toggle_maximized);
    connect(close, &QToolButton::clicked, host_, &QWidget::close);
    host_->installEventFilter(this);
}

bool WindowTitleBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == host_) {
        if (event->type() == QEvent::WindowStateChange)
            update_maximize_icon();
        else if (event->type() == QEvent::WindowTitleChange)
            title_->setText(host_->windowTitle());
    }
    return QWidget::eventFilter(watched, event);
}

void WindowTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        toggle_maximized();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void WindowTitleBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && host_->windowHandle()) {
        host_->windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void WindowTitleBar::toggle_maximized() {
    if (host_->isMaximized())
        host_->showNormal();
    else
        host_->showMaximized();
}

void WindowTitleBar::update_maximize_icon() {
    const bool maximized = host_->isMaximized();
    maximize_->setIcon(caption_icon(maximized ? CaptionGlyph::restore
                                              : CaptionGlyph::maximize));
    maximize_->setToolTip(maximized ? QStringLiteral("还原")
                                    : QStringLiteral("最大化"));
    maximize_->setAccessibleName(maximize_->toolTip());
}

void configure_frameless_window(QWidget* window) {
    window->setWindowFlag(Qt::FramelessWindowHint, true);
}

void enable_rounded_window_corners(QWidget* window) {
    constexpr DWORD attribute = 33;  // DWMWA_WINDOW_CORNER_PREFERENCE
    constexpr DWORD round_preference = 2;  // DWMWCP_ROUND
    DwmSetWindowAttribute(reinterpret_cast<HWND>(window->winId()), attribute,
                          &round_preference, sizeof(round_preference));
}

bool handle_frameless_native_event(QWidget* window, void* message,
                                   qintptr* result) {
    auto* native_message = static_cast<MSG*>(message);
    if (!native_message || native_message->message != WM_NCHITTEST ||
        window->isMaximized())
        return false;

    RECT frame{};
    if (!GetWindowRect(reinterpret_cast<HWND>(window->winId()), &frame))
        return false;
    const int border = std::max(6, qRound(7 * window->devicePixelRatioF()));
    const int x = GET_X_LPARAM(native_message->lParam);
    const int y = GET_Y_LPARAM(native_message->lParam);
    const bool left = x >= frame.left && x < frame.left + border;
    const bool right = x < frame.right && x >= frame.right - border;
    const bool top = y >= frame.top && y < frame.top + border;
    const bool bottom = y < frame.bottom && y >= frame.bottom - border;

    if (top && left) *result = HTTOPLEFT;
    else if (top && right) *result = HTTOPRIGHT;
    else if (bottom && left) *result = HTBOTTOMLEFT;
    else if (bottom && right) *result = HTBOTTOMRIGHT;
    else if (left) *result = HTLEFT;
    else if (right) *result = HTRIGHT;
    else if (top) *result = HTTOP;
    else if (bottom) *result = HTBOTTOM;
    else return false;
    return true;
}
