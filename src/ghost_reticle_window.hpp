#pragma once

#include "wardogs/ghost_reticle.hpp"

#include <QPoint>
#include <QString>
#include <QWidget>

#include <functional>
#include <optional>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QScreen;
class QToolButton;
class QEvent;

class GhostReticleWindow final : public QWidget {
public:
    using Preferences = wardogs::GhostReticlePreferences;
    using SizeSaved = std::function<void(int)>;

    explicit GhostReticleWindow(Preferences preferences,
                                SizeSaved size_saved,
                                QWidget* parent = nullptr);

    void set_solution(std::optional<wardogs::CorrectedSolution> solution);
    void set_mortar_solution(std::optional<double> bearing_degrees,
                             std::optional<double> mil);
    void set_overlay_enabled(bool enabled);
    [[nodiscard]] bool overlay_enabled() const { return overlay_enabled_; }
    void set_opacity_percent(int opacity_percent);
    void set_bearing_compensation(double degrees);
    void set_width(int width);
    void set_target_monitor(const std::wstring& monitor_device);
    void begin_adjustment();
    [[nodiscard]] bool adjusting() const { return adjusting_; }

protected:
    bool nativeEvent(const QByteArray& event_type, void* message,
                     qintptr* result) override;
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void finish_adjustment(bool save);
    void center_on_screen(int width);
    [[nodiscard]] QScreen* target_screen() const;
    void apply_input_mode();
    [[nodiscard]] Qt::CursorShape resize_cursor_at(QPoint position) const;
    void draw_bearing(QPainter& painter, double scale,
                      double bearing_degrees) const;
    void draw_mil(QPainter& painter, double scale) const;
    void draw_mortar_mil(QPainter& painter, double scale) const;
    [[nodiscard]] bool has_reticle_result() const;

    Preferences preferences_;
    SizeSaved size_saved_;
    std::optional<wardogs::CorrectedSolution> solution_;
    std::optional<double> mortar_bearing_;
    std::optional<double> mortar_mil_;
    QWidget* adjustment_controls_{};
    QToolButton* confirm_button_{};
    QToolButton* cancel_button_{};
    bool overlay_enabled_{};
    bool adjusting_{};
    bool resizing_{};
    QPoint resize_center_;
    QString target_monitor_name_;
    int adjustment_original_width_{};
};
