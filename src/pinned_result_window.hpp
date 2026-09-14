#pragma once

#include "wardogs/pinned_preferences.hpp"

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QWidget>

#include <functional>
#include <map>
#include <set>
#include <string>

class QFrame;
class QContextMenuEvent;
class QLabel;
class QMenu;
class QMouseEvent;
class QResizeEvent;
class QSlider;
class QToolButton;
class VehicleSolutionWidget;

class PinnedResultWindow final : public QWidget {
public:
    using Preferences = wardogs::PinnedCardPreferences;

    explicit PinnedResultWindow(std::function<void()> exit_callback,
                                QWidget* parent = nullptr);
    PinnedResultWindow(std::function<void()> exit_callback,
                       Preferences preferences,
                       std::function<void(Preferences)> preferences_changed,
                       QWidget* parent = nullptr);

    void set_mode(bool vehicle_mode);
    void set_values(const QString& distance, const QString& bearing);
    void set_vehicle_values(const VehicleSolutionWidget& low,
                            const VehicleSolutionWidget& high);
    void set_error(bool error);
    void set_locked(bool locked);
    [[nodiscard]] bool is_locked() const { return preferences_.locked; }
    void set_opacity_percent(int opacity_percent);
    [[nodiscard]] int opacity_percent() const {
        return preferences_.opacity_percent;
    }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    using Edges = std::set<std::string>;

    static QWidget* result_card(const QString& color, QLabel*& value);
    [[nodiscard]] Edges resize_edges_at(QPoint position) const;
    [[nodiscard]] static Qt::CursorShape cursor_for_edges(const Edges& edges);
    void resize_from_pointer(QPoint pointer);
    void apply_font_scale();
    void build_context_menu();
    void update_lock_control();
    void notify_preferences_changed();

    std::function<void()> exit_callback_;
    std::function<void(Preferences)> preferences_changed_;
    Preferences preferences_;
    QFrame* frame_{};
    QMenu* context_menu_{};
    QToolButton* lock_button_{};
    QSlider* opacity_slider_{};
    QWidget *mortar_panel_{}, *vehicle_panel_{};
    QLabel *distance_{}, *bearing_{};
    VehicleSolutionWidget *low_{}, *high_{};
    bool vehicle_mode_{};
    bool dragging_{};
    QPoint drag_offset_{};
    Edges resize_edges_;
    QPoint resize_start_global_{};
    QRect resize_start_geometry_{};
    std::map<bool, QSize> mode_sizes_{{false, {430, 78}}, {true, {420, 116}}};
    double font_scale_{1.0};
    bool applying_font_scale_{};
};
