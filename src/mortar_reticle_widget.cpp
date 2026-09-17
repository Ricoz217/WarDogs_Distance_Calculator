#include "mortar_reticle_widget.hpp"

#include <QPainter>
#include <QScreen>
#include <QApplication>
#include <QFontMetrics>

#include <algorithm>
#include <cmath>

MortarReticleWidget::MortarReticleWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(300);
    setMaximumHeight(300);
    setMinimumWidth(400);
}

double MortarReticleWidget::mil_for_distance(double distance_m) const {
    if (distance_m <= distance_to_mil_.front().first)
        return distance_to_mil_.front().second;
    if (distance_m >= distance_to_mil_.back().first)
        return distance_to_mil_.back().second;

    for (size_t i = 0; i + 1 < distance_to_mil_.size(); ++i) {
        const auto [d1, m1] = distance_to_mil_[i];
        const auto [d2, m2] = distance_to_mil_[i + 1];
        if (distance_m >= d1 && distance_m <= d2) {
            const double t = (distance_m - d1) / (d2 - d1);
            return m1 + t * (m2 - m1);
        }
    }
    return distance_to_mil_.front().second;
}

double MortarReticleWidget::pixels_per_mil() const {
    QScreen* screen = QApplication::screenAt(pos());
    if (!screen) screen = QApplication::primaryScreen();
    const double dpi = screen ? screen->logicalDotsPerInch() : 96.0;
    const double cm_per_inch = 2.54;
    return (physical_spacing_cm_ / cm_per_inch) * dpi / mil_step_;
}

double MortarReticleWidget::scale_offset(double target_mil) const {
    const double ppm = pixels_per_mil();
    const double center_y = height() / 2.0;
    return center_y - (target_mil - mil_min_) * ppm;
}

void MortarReticleWidget::set_solution(double distance_m) {
    current_distance_m_ = distance_m;
    out_of_range_ = false;
    out_of_range_message_.clear();
    has_solution_ = true;
    update();
}

void MortarReticleWidget::set_out_of_range(const QString& message) {
    has_solution_ = false;
    out_of_range_ = true;
    out_of_range_message_ = message;
    update();
}

void MortarReticleWidget::clear() {
    has_solution_ = false;
    out_of_range_ = false;
    out_of_range_message_.clear();
    update();
}

void MortarReticleWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const double ppm = pixels_per_mil();
    const double center_y = h / 2.0;

    painter.fillRect(rect(), QColor("#0b1220"));

    const double reticle_w = 80;
    const double reticle_h = 60;
    const double circle_r = 7.5;
    const QColor reticle_color("#475569");

    painter.setPen(QPen(reticle_color, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(w / 2.0 - reticle_w / 2.0, center_y - reticle_h / 2.0,
                            reticle_w, reticle_h));
    painter.drawEllipse(QPointF(w / 2.0, center_y), circle_r, circle_r);

    const double scale_left = 40;
    const double scale_right = w - 40;
    const double scale_width = 60;

    if (out_of_range_) {
        painter.setPen(QColor("#fca5a5"));
        QFont font = painter.font();
        font.setPointSize(11);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter, out_of_range_message_);
        return;
    }

    if (!has_solution_) {
        painter.setPen(QColor("#64748b"));
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("等待计算结果"));
        return;
    }

    const double target_mil = mil_for_distance(current_distance_m_);
    const double offset = scale_offset(target_mil);

    painter.setPen(QPen(QColor("#334155"), 1));
    painter.drawLine(QPointF(scale_left + scale_width, center_y),
                     QPointF(scale_right - scale_width, center_y));

    QFont scale_font = painter.font();
    scale_font.setPointSize(9);
    painter.setFont(scale_font);
    QFontMetrics fm(scale_font);

    for (double mil = mil_min_; mil <= mil_max_; mil += mil_step_) {
        const double y = offset + (mil - mil_min_) * ppm;
        if (y < -20 || y > h + 20) continue;

        painter.setPen(QPen(QColor("#67e8f9"), 1.5));
        painter.drawLine(QPointF(scale_right - scale_width, y),
                         QPointF(scale_right - scale_width + 10, y));

        painter.setPen(QColor("#67e8f9"));
        const QString text = QString::number(static_cast<int>(mil));
        painter.drawText(QPointF(scale_right - scale_width + 14, y + fm.ascent() / 2.0 - 2), text);
    }

    for (const auto [dist, mil] : distance_to_mil_) {
        const double y = offset + (mil - mil_min_) * ppm;
        if (y < -20 || y > h + 20) continue;

        painter.setPen(QPen(QColor("#fbbf24"), 1.5));
        painter.drawLine(QPointF(scale_left + scale_width - 10, y),
                         QPointF(scale_left + scale_width, y));

        painter.setPen(QColor("#fbbf24"));
        const QString text = QString::number(static_cast<int>(dist)) + "m";
        const int text_width = fm.horizontalAdvance(text);
        painter.drawText(QPointF(scale_left + scale_width - 14 - text_width,
                                 y + fm.ascent() / 2.0 - 2), text);
    }

    painter.setPen(QColor("#94a3b8"));
    QFont label_font = painter.font();
    label_font.setBold(true);
    label_font.setPointSize(10);
    painter.setFont(label_font);

    const QFontMetrics label_fm(label_font);
    const QString rng_text = QStringLiteral("RNG");
    const int rng_width = label_fm.horizontalAdvance(rng_text);
    painter.drawText(QPointF(scale_left - rng_width - 5, center_y + label_fm.ascent() / 2.0 - 2), rng_text);

    const QString mil_text = QStringLiteral("MIL");
    painter.drawText(QPointF(scale_right - scale_width + 14 + label_fm.horizontalAdvance(QStringLiteral("900")) + 10,
                             center_y + label_fm.ascent() / 2.0 - 2), mil_text);

    const double indicator_y = center_y;
    painter.setPen(QPen(QColor("#ef4444"), 2));
    painter.drawLine(QPointF(scale_left + scale_width - 8, indicator_y),
                     QPointF(scale_left + scale_width + 8, indicator_y));
    painter.drawLine(QPointF(scale_right - scale_width - 8, indicator_y),
                     QPointF(scale_right - scale_width + 8, indicator_y));
}
