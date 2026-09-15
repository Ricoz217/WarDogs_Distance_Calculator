#pragma once

#include <QWidget>

class QLabel;
class QToolButton;

class WindowTitleBar final : public QWidget {
public:
    explicit WindowTitleBar(QWidget* host);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void toggle_maximized();
    void update_maximize_icon();

    QWidget* host_{};
    QLabel* title_{};
    QToolButton* maximize_{};
};

void configure_frameless_window(QWidget* window);
void enable_rounded_window_corners(QWidget* window);
bool handle_frameless_native_event(QWidget* window, void* message,
                                   qintptr* result);
