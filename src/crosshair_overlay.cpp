#include "crosshair_overlay.hpp"

#include <QPainter>
#include <QScreen>
#include <QApplication>

#include <Windows.h>
#include <dwmapi.h>

CrosshairOverlay::CrosshairOverlay(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowTitle(QStringLiteral("War Dogs Crosshair"));

    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        const QRect geometry = screen->geometry();
        const int w = 400;
        const int h = 20;
        const int x = geometry.center().x() - w / 2;
        const int y = geometry.center().y() - h / 2;
        setGeometry(x, y, w, h);
    }

    const HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void CrosshairOverlay::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    raise();
    activateWindow();
}

void CrosshairOverlay::set_gap(int gap_px) {
    gap_ = gap_px;
    update();
}

void CrosshairOverlay::set_thickness(int thickness_px) {
    thickness_ = thickness_px;
    setFixedHeight(std::max(thickness_, 4) + 8);
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        const QRect geometry = screen->geometry();
        move(geometry.center().x() - width() / 2,
             geometry.center().y() - height() / 2);
    }
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    update();
}

void CrosshairOverlay::set_length(int length_px) {
    length_ = length_px;
    setFixedWidth(gap_ * 2 + length_ * 2 + 20);
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        const QRect geometry = screen->geometry();
        move(geometry.center().x() - width() / 2,
             geometry.center().y() - height() / 2);
    }
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    update();
}

void CrosshairOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int cx = width() / 2;
    const int cy = height() / 2;
    const QColor color(Qt::red);

    const int left_start = cx - gap_ - length_;
    const int left_end = cx - gap_;
    const int right_start = cx + gap_;
    const int right_end = cx + gap_ + length_;

    painter.setPen(QPen(color, thickness_, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(left_start, cy, left_end, cy);
    painter.drawLine(right_start, cy, right_end, cy);
}
