#include "pinned_result_window.hpp"
#include "app_icon.hpp"
#include "ghost_reticle_window.hpp"
#include "settings_dialog.hpp"
#include "vehicle_solution_widget.hpp"
#include "window_title_bar.hpp"

#include <Windows.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHoverEvent>
#include <QImage>
#include <QKeySequenceEdit>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMetaObject>
#include <QMouseEvent>
#include <QSlider>
#include <QToolButton>
#include <QPushButton>

#include <array>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <utility>

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
    check(!wardogs_application_icon().isNull(),
          "the application icon is available to windows and title bars");

    wardogs::AppSettings crowded_settings;
    crowded_settings.ghost_reticle.width = 1110;
    SettingsDialog settings_dialog(crowded_settings);
    settings_dialog.show();
    QApplication::processEvents();
    const auto hotkey_editors = settings_dialog.findChildren<QKeySequenceEdit*>();
    check(hotkey_editors.size() == 6,
          "settings exposes all six configurable global hotkeys");
    std::set<int> hotkey_rows;
    for (const auto* editor : hotkey_editors)
        hotkey_rows.insert(editor->mapTo(&settings_dialog, QPoint{}).y());
    check(hotkey_rows.size() == 2,
          "the six hotkey editors are split evenly across two rows");
    const auto* manual_adjust = settings_dialog.findChild<QPushButton*>(
        QStringLiteral("adjustGhostReticle"));
    auto* preset = settings_dialog.findChild<QComboBox*>(
        QStringLiteral("ghostReticlePreset"));
    check(manual_adjust && manual_adjust->text() == QStringLiteral("手动调整大小"),
          "the size button clearly distinguishes manual adjustment");
    check(preset && preset->currentText().startsWith(QStringLiteral("自定义")),
          "a manually changed size is represented as a custom preset");
    const std::array<std::pair<QSize, int>, 4> expected_presets{{
        {{1280, 720}, 480}, {{1600, 900}, 600},
        {{1920, 1080}, 720}, {{2560, 1440}, 960},
    }};
    for (const auto& [resolution, expected_width] : expected_presets) {
        const int index = preset ? preset->findData(resolution) : -1;
        check(index >= 0, "all verified 16:9 resolutions are listed");
        if (preset) preset->setCurrentIndex(index);
        check(settings_dialog.settings().ghost_reticle.width == expected_width,
              "a resolution preset restores its proportional reticle size");
    }
    auto* compensation = settings_dialog.findChild<QDoubleSpinBox*>(
        QStringLiteral("ghostBearingCompensation"));
    auto* decrease_compensation = settings_dialog.findChild<QPushButton*>(
        QStringLiteral("decreaseGhostBearingCompensation"));
    auto* increase_compensation = settings_dialog.findChild<QPushButton*>(
        QStringLiteral("increaseGhostBearingCompensation"));
    check(compensation && decrease_compensation && increase_compensation,
          "settings exposes editable ghost bearing compensation controls");
    if (increase_compensation) increase_compensation->click();
    check(compensation && qAbs(compensation->value() - 0.05) < 1e-9,
          "the right compensation button advances by 0.05 degrees");
    if (decrease_compensation) decrease_compensation->click();
    check(compensation && qAbs(compensation->value()) < 1e-9,
          "the left compensation button decreases by 0.05 degrees");
    if (compensation) compensation->setValue(-0.35);
    check(qAbs(settings_dialog.settings()
                       .ghost_reticle.bearing_compensation_deg +
                   0.35) < 1e-9,
          "manually entered bearing compensation is returned by settings");
    settings_dialog.hide();

    int saved_ghost_width = 0;
    GhostReticleWindow ghost({}, [&saved_ghost_width](int width) {
        saved_ghost_width = width;
    });
    check(ghost.width() * 3 == ghost.height() * 4,
          "ghost reticle always starts at the measured 4:3 aspect ratio");
    check(ghost.testAttribute(Qt::WA_TransparentForMouseEvents) &&
              ghost.windowFlags().testFlag(Qt::WindowTransparentForInput),
          "ordinary ghost mode is completely click-through");
    ghost.set_solution(wardogs::CorrectedSolution{
        wardogs::Arc::low, 188.4, 1248.0, 403.25});
    ghost.set_overlay_enabled(true);
    QApplication::processEvents();
    check(ghost.isVisible(), "enabled ghost reticle appears when a solution exists");
    QImage ghost_render(ghost.size(), QImage::Format_ARGB32_Premultiplied);
    ghost_render.fill(Qt::transparent);
    ghost.render(&ghost_render);
    bool has_ghost_pixels = false;
    for (int y = 0; y < ghost_render.height() && !has_ghost_pixels; ++y) {
        for (int x = 0; x < ghost_render.width(); ++x) {
            if (qAlpha(ghost_render.pixel(x, y)) != 0) {
                has_ghost_pixels = true;
                break;
            }
        }
    }
    check(has_ghost_pixels, "bearing and MIL scales are drawn procedurally");
    ghost.set_mortar_solution(215.0, 500.0);
    QImage mortar_render(ghost.size(), QImage::Format_ARGB32_Premultiplied);
    mortar_render.fill(Qt::transparent);
    ghost.render(&mortar_render);
    bool has_mortar_bearing_pixels = false;
    for (int y = 45; y < 125 && !has_mortar_bearing_pixels; ++y) {
        for (int x = 100; x < mortar_render.width() - 100; ++x) {
            if (qAlpha(mortar_render.pixel(x, y)) != 0) {
                has_mortar_bearing_pixels = true;
                break;
            }
        }
    }
    check(has_mortar_bearing_pixels,
          "mortar ghost reticle keeps the shared top bearing scale");
    ghost.begin_adjustment();
    check(ghost.adjusting() &&
              !ghost.testAttribute(Qt::WA_TransparentForMouseEvents) &&
              !ghost.windowFlags().testFlag(Qt::WindowTransparentForInput),
          "size adjustment temporarily accepts input and exposes the border");
    auto* adjustment_controls = ghost.findChild<QWidget*>(
        QStringLiteral("ghostReticleAdjustmentControls"));
    auto* confirm_ghost = ghost.findChild<QToolButton*>(
        QStringLiteral("ghostReticleConfirm"));
    auto* cancel_ghost = ghost.findChild<QToolButton*>(
        QStringLiteral("ghostReticleCancel"));
    check(adjustment_controls && confirm_ghost && cancel_ghost &&
              confirm_ghost->text().isEmpty() &&
              cancel_ghost->text().isEmpty() &&
              !confirm_ghost->icon().isNull() &&
              !cancel_ghost->icon().isNull(),
          "adjustment mode uses drawn confirm and cancel icons");
    check(adjustment_controls &&
              qAbs(adjustment_controls->geometry().center().x() -
                   ghost.rect().center().x()) <= 1 &&
              ghost.height() - adjustment_controls->geometry().bottom() <= 13,
          "adjustment controls are centered along the bottom edge");
    const int original_ghost_width = ghost.width();
    QMouseEvent hover_ghost_edge(
        QEvent::MouseMove, QPointF(1, ghost.height() / 2.0),
        QPointF(ghost.mapToGlobal(QPoint(1, ghost.height() / 2))),
        Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&ghost, &hover_ghost_edge);
    check(ghost.cursor().shape() == Qt::SizeHorCursor,
          "hovering an adjustment edge exposes the resize cursor");
    ghost.resize(original_ghost_width - 80,
                 qRound((original_ghost_width - 80) * 3.0 / 4.0));
    if (cancel_ghost) cancel_ghost->click();
    check(!ghost.adjusting() && ghost.width() == original_ghost_width &&
              saved_ghost_width == 0,
          "cancel restores the size from before adjustment without saving");
    ghost.begin_adjustment();
    ghost.resize(original_ghost_width - 40,
                 qRound((original_ghost_width - 40) * 3.0 / 4.0));
    QKeyEvent save_ghost_size(QEvent::KeyPress, Qt::Key_Escape,
                              Qt::NoModifier);
    QApplication::sendEvent(&ghost, &save_ghost_size);
    check(!ghost.adjusting() && saved_ghost_width == ghost.width(),
          "Escape saves the adjusted ghost-reticle size");
    ghost.hide();

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
    std::wstring saved_unlock_hotkey;
    PinnedResultWindow pinned(
        [&exits] { ++exits; }, {true, 67},
        [&saved_lock, &saved_opacity, &saved_unlock_hotkey](
            const PinnedResultWindow::Preferences& preferences) {
            saved_lock = preferences.locked;
            saved_opacity = preferences.opacity_percent;
            saved_unlock_hotkey = preferences.unlock_hotkey;
            return true;
        });
    bool ghost_enabled = false;
    int ghost_opacity = 0;
    pinned.configure_ghost_controls(
        false, 74,
        [&ghost_enabled](bool enabled) { ghost_enabled = enabled; },
        [&ghost_opacity](int opacity) { ghost_opacity = opacity; });
    check(pinned.is_locked(), "pinned card restores its locked state");
    check(pinned.opacity_percent() == 67,
          "pinned card restores its opacity setting");
    check(pinned.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus),
          "pinned card cannot steal focus from the game");
    check(pinned.windowType() == Qt::Window,
          "pinned mode keeps a normal taskbar window instead of a tool window");
    check(pinned.testAttribute(Qt::WA_TransparentForMouseEvents),
          "a locked card is completely transparent to mouse input");
    check(pinned.windowFlags().testFlag(Qt::WindowTransparentForInput),
          "the native window system passes locked-card input through");
    pinned.set_values(QStringLiteral("470 m"), QStringLiteral("180.0° S"),
                      QStringLiteral("500 mil"));
    const auto* pinned_mortar_mil = pinned.findChild<QLabel*>(
        QStringLiteral("pinnedMortarMil"));
    check(pinned_mortar_mil &&
              pinned_mortar_mil->text() == QStringLiteral("500 mil"),
          "pinned mortar RNG card keeps MIL as a compact second line");

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
    check(!pinned.testAttribute(Qt::WA_TransparentForMouseEvents),
          "the global unlock action restores mouse interaction");
    check(!pinned.windowFlags().testFlag(Qt::WindowTransparentForInput),
          "unlocking removes native input transparency");
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
    auto* unlock_hotkey = pinned.findChild<QKeySequenceEdit*>(
        QStringLiteral("pinnedUnlockHotkey"));
    auto* ghost_row = pinned.findChild<QWidget*>(
        QStringLiteral("pinnedGhostRow"));
    auto* unlock_row = pinned.findChild<QWidget*>(
        QStringLiteral("pinnedUnlockRow"));
    auto* ghost_button = pinned.findChild<QToolButton*>(
        QStringLiteral("ghostReticleToggle"));
    auto* ghost_slider = pinned.findChild<QSlider*>(
        QStringLiteral("ghostReticleOpacitySlider"));
    check(menu && lock_button && opacity_slider && ghost_row && unlock_row &&
              ghost_button && ghost_slider && unlock_hotkey,
          "right click exposes card, ghost-reticle, and unlock controls");
    check(ghost_row && unlock_row && ghost_row->geometry().top() <
                                      unlock_row->geometry().top(),
          "ghost reticle controls are inserted above the bottom unlock row");
    check(menu && qobject_cast<QMenu*>(menu) == nullptr &&
              menu->windowType() == Qt::Popup,
          "side controls use the same translucent QWidget approach as the card");
    check(lock_button && lock_button->text().isEmpty(),
          "lock control is icon-only");
    check(opacity_slider && opacity_slider->minimum() == 35 &&
              opacity_slider->maximum() == 100,
          "opacity slider keeps the card between 35 and 100 percent visible");
    check(ghost_slider && ghost_slider->minimum() == 20 &&
              ghost_slider->maximum() == 100 && ghost_slider->value() == 74,
          "the middle row has an independent ghost-reticle opacity slider");
    check(lock_button && ghost_button &&
              lock_button->property("pinnedMenuButton").toBool() &&
              ghost_button->property("pinnedMenuButton").toBool() &&
              opacity_slider->property("pinnedMenuSlider").toBool() &&
              ghost_slider->property("pinnedMenuSlider").toBool(),
          "both side-menu rows share the same button and slider styling hooks");
    if (ghost_button) {
        ghost_button->setChecked(true);
        check(ghost_enabled, "the middle-row icon toggles the ghost reticle");
    }
    if (ghost_slider) {
        ghost_slider->setValue(58);
        check(ghost_opacity == 58,
              "the middle-row slider changes only ghost-reticle opacity");
    }
    check(unlock_hotkey &&
              unlock_hotkey->keySequence().toString(QKeySequence::PortableText) ==
                  QStringLiteral("Ctrl+Alt+Q"),
          "the side controls show the default unlock hotkey");
    if (unlock_hotkey) {
        unlock_hotkey->setKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+U")));
        QMetaObject::invokeMethod(unlock_hotkey, "editingFinished",
                                  Qt::DirectConnection);
        check(saved_unlock_hotkey == L"Ctrl+Shift+U",
              "editing the side control immediately persists the unlock hotkey");
    }
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
