#include "pinned_result_window.hpp"
#include "vehicle_solution_widget.hpp"
#include "window_title_bar.hpp"

#include <Windows.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QHoverEvent>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QSlider>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    QWidget host;
    host.setWindowTitle(QStringLiteral("测试窗口"));
    configure_frameless_window(&host);
    WindowTitleBar title_bar(&host);
    check(host.windowFlags().testFlag(Qt::FramelessWindowHint),
          "custom title bar removes the native Windows caption");
    const auto* title_text =
        title_bar.findChild<QLabel*>(QStringLiteral("windowTitleText"));
    const auto caption_buttons = title_bar.findChildren<QToolButton*>();
    check(title_text && title_text->text() == QStringLiteral("测试窗口"),
          "custom title bar follows the host window title");
    check(caption_buttons.size() == 3,
          "custom title bar provides minimize, maximize, and close controls");
    for (const auto* button : caption_buttons) {
        check(button->text().isEmpty() && !button->icon().isNull() &&
                  !button->accessibleName().isEmpty(),
              "caption controls are icon-only and remain accessible");
    }
    NCCALCSIZE_PARAMS frame_parameters{};
    MSG frame_message{};
    frame_message.message = WM_NCCALCSIZE;
    frame_message.wParam = TRUE;
    frame_message.lParam = reinterpret_cast<LPARAM>(&frame_parameters);
    qintptr frame_result = -1;
    check(handle_frameless_native_event(&host, &frame_message, &frame_result) &&
              frame_result == 0,
          "custom chrome owns non-client sizing after a window is shown again");

    VehicleSolutionWidget full(wardogs::Arc::low);
    full.set_unavailable(QStringLiteral("3100 m"), QStringLiteral("203.0° SW"));
    check(full.unavailable(), "unavailable result stores warning state");
    check(full.mil_text() == QStringLiteral("无可用分划"),
          "full card explains unavailable reticle");

    full.set_solution({wardogs::Arc::low, 188.4, 1248.0, 33.0});
    check(!full.unavailable(), "valid result clears warning state");
    check(full.distance_text() == QStringLiteral("1248 m"),
          "distance is formatted for the result card");
    check(full.bearing_text().contains(QStringLiteral("188.4°")),
          "bearing keeps degrees and compass direction");
    check(full.mil_text() == QStringLiteral("33 mil"),
          "reticle is prominent and includes its unit");

    VehicleSolutionWidget compact(wardogs::Arc::high, true);
    full.set_unavailable(QStringLiteral("3100 m"), QStringLiteral("203.0° SW"));
    compact.copy_from(full);
    check(compact.mil_text() == QStringLiteral("—"),
          "pinned card uses a dash instead of explanatory text");
    const auto labels = compact.findChildren<QLabel*>();
    for (const auto* label : labels) {
        check(label->text() != QStringLiteral("低射") &&
                  label->text() != QStringLiteral("高抛") &&
                  label->text() != QStringLiteral("射程") &&
                  label->text() != QStringLiteral("方位") &&
                  label->text() != QStringLiteral("分划"),
              "pinned vehicle card contains values only");
    }

    int exits = 0;
    int saved_opacity = 0;
    bool saved_lock = false;
    PinnedResultWindow pinned(
        [&exits] { ++exits; }, {true, 67},
        [&saved_lock, &saved_opacity](PinnedResultWindow::Preferences preferences) {
            saved_lock = preferences.locked;
            saved_opacity = preferences.opacity_percent;
        });
    check(pinned.is_locked(), "pinned card restores its locked state");
    check(pinned.opacity_percent() == 67,
          "pinned card restores its opacity setting");
    check(pinned.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus),
          "pinned card cannot steal focus from the game");

    pinned.move(100, 100);
    pinned.resize(430, 78);
    const QRect locked_geometry = pinned.geometry();
    QMouseEvent locked_press(
        QEvent::MouseButtonPress, QPointF(20, 20), QPointF(120, 120),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&pinned, &locked_press);
    QMouseEvent locked_move(
        QEvent::MouseMove, QPointF(100, 60), QPointF(200, 160),
        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&pinned, &locked_move);
    QMouseEvent locked_release(
        QEvent::MouseButtonRelease, QPointF(100, 60), QPointF(200, 160),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&pinned, &locked_release);
    check(pinned.geometry() == locked_geometry,
          "left-button drag and resize do nothing while the card is locked");

    QMouseEvent locked_double_click(
        QEvent::MouseButtonDblClick, QPointF(20, 20), QPointF(20, 20),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&pinned, &locked_double_click);
    check(exits == 0, "double click cannot exit while the pinned card is locked");

    pinned.set_locked(false);
    const QPointF edge_position(1, pinned.height() / 2.0);
    QHoverEvent edge_hover(QEvent::HoverMove, edge_position,
                           edge_position + QPointF(100, 100), QPointF(20, 20),
                           Qt::NoModifier);
    QApplication::sendEvent(&pinned, &edge_hover);
    check(pinned.cursor().shape() == Qt::SizeHorCursor,
          "an unlocked card shows the horizontal resize cursor at its edge");
    const QPointF center_position(pinned.width() / 2.0,
                                  pinned.height() / 2.0);
    QHoverEvent center_hover(QEvent::HoverMove, center_position,
                             center_position + QPointF(100, 100), edge_position,
                             Qt::NoModifier);
    QApplication::sendEvent(&pinned, &center_hover);
    check(pinned.cursor().shape() == Qt::ArrowCursor,
          "an unlocked card keeps the ordinary cursor in its interior");
    QMouseEvent unlocked_double_click(
        QEvent::MouseButtonDblClick, QPointF(20, 20), QPointF(20, 20),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&pinned, &unlocked_double_click);
    check(exits == 1, "double click exits after the pinned card is unlocked");
    check(!saved_lock && saved_opacity == 67,
          "changing the lock persists both pinned-card preferences");

    pinned.set_opacity_percent(1);
    check(pinned.opacity_percent() == 35,
          "pinned card remains visible at the minimum opacity");
    check(saved_opacity == 35,
          "changing opacity immediately persists the clamped value");

    QContextMenuEvent menu_event(QContextMenuEvent::Mouse, QPoint(10, 10),
                                 QPoint(10, 10));
    QApplication::sendEvent(&pinned, &menu_event);
    auto* menu = pinned.findChild<QWidget*>(QStringLiteral("pinnedContextMenu"));
    const auto* lock_button =
        pinned.findChild<QToolButton*>(QStringLiteral("pinnedLockButton"));
    auto* opacity_slider =
        pinned.findChild<QSlider*>(QStringLiteral("pinnedOpacitySlider"));
    check(menu && lock_button && opacity_slider,
          "right click exposes the lock button and opacity slider");
    check(menu && qobject_cast<QMenu*>(menu) == nullptr &&
              menu->windowType() == Qt::Popup,
          "side controls use the same translucent QWidget approach as the card");
    check(lock_button && lock_button->text().isEmpty(),
          "lock control is icon-only");
    check(opacity_slider && opacity_slider->minimum() == 35 &&
              opacity_slider->maximum() == 100,
          "opacity slider keeps the card between 35 and 100 percent visible");
    check(menu && menu->frameGeometry().left() > pinned.frameGeometry().right(),
          "the context menu is anchored beside the card instead of at the pointer");
    check(menu && menu->frameGeometry().left() - pinned.frameGeometry().right() <= 3,
          "the side menu visually joins the card without a wide gap");
    check(menu && menu->frameGeometry().top() == pinned.frameGeometry().top(),
          "the side menu aligns with the top of the result card");
    check(menu && menu->testAttribute(Qt::WA_TranslucentBackground) &&
              menu->mask().isEmpty(),
          "the menu uses an antialiased translucent edge instead of a binary mask");
    if (menu) {
        QImage rendered(menu->size(), QImage::Format_ARGB32_Premultiplied);
        rendered.fill(Qt::transparent);
        menu->render(&rendered);
        check(qAlpha(rendered.pixel(0, 0)) == 0 &&
                  qAlpha(rendered.pixel(rendered.width() - 1, 0)) == 0,
              "both top menu corners remain transparent");
        bool has_antialiased_edge = false;
        for (int y = 0; y < std::min(12, rendered.height()); ++y) {
            for (int x = 0; x < std::min(12, rendered.width()); ++x) {
                const int alpha = qAlpha(rendered.pixel(x, y));
                has_antialiased_edge = has_antialiased_edge ||
                                       (alpha > 0 && alpha < 255);
            }
        }
        check(has_antialiased_edge,
              "the rounded menu edge contains antialiased transition pixels");
    }
    check(menu && qAbs(menu->windowOpacity() - pinned.windowOpacity()) < 0.001,
          "the context menu follows the card opacity");
    if (opacity_slider) {
        opacity_slider->resize(200, 34);
        opacity_slider->setValue(100);
        QMouseEvent track_click(
            QEvent::MouseButtonPress, QPointF(100, 17), QPointF(100, 17),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(opacity_slider, &track_click);
        check(opacity_slider->value() >= 65 && opacity_slider->value() <= 70,
              "clicking the slider track jumps directly to that position");
    }
    if (menu) {
        menu->hide();
        pinned.set_opacity_percent(42);
        menu->setWindowOpacity(1.0);
        QContextMenuEvent reopened_menu_event(
            QContextMenuEvent::Mouse, QPoint(20, 20), QPoint(20, 20));
        QApplication::sendEvent(&pinned, &reopened_menu_event);
        check(qAbs(menu->windowOpacity() - 0.42) < 0.001,
              "a reopened menu reapplies the current card opacity");
        menu->hide();
    }

    std::cout << "All vehicle UI tests passed\n" << std::flush;
    // Qt's Windows offscreen plugin can wait indefinitely during process teardown.
    // All assertions are complete and no persistent resources are owned by this test.
    std::_Exit(0);
}
