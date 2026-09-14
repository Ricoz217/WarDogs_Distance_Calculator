#include "vehicle_solution_widget.hpp"

#include "wardogs/core.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

VehicleSolutionWidget::VehicleSolutionWidget(wardogs::Arc arc, bool compact,
                                             QWidget* parent)
    : QFrame(parent), arc_(arc), compact_(compact) {
    setObjectName(QStringLiteral("vehicleSolutionCard"));
    setProperty("unavailable", false);
    setProperty("compact", compact_);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(compact_ ? 8 : 12, compact_ ? 5 : 9,
                               compact_ ? 8 : 12, compact_ ? 6 : 10);
    layout->setSpacing(compact_ ? 6 : 14);
    if (!compact_) {
        auto* arc_label = new QLabel(
            arc_ == wardogs::Arc::low ? QStringLiteral("低射")
                                      : QStringLiteral("高抛"));
        arc_label->setObjectName(QStringLiteral("solutionArc"));
        arc_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        arc_label->setFixedWidth(46);
        layout->addWidget(arc_label);
        layout->addStretch();
    }
    distance_ = add_metric(layout, QStringLiteral("射程"),
                           QStringLiteral("solutionDistance"), compact_ ? 88 : 128);
    bearing_ = add_metric(layout, QStringLiteral("方位"),
                          QStringLiteral("solutionBearing"), compact_ ? 112 : 158);
    mil_ = add_metric(layout, QStringLiteral("分划"),
                      QStringLiteral("solutionMil"), compact_ ? 105 : 150);
}

QLabel* VehicleSolutionWidget::add_metric(QHBoxLayout* layout,
                                          const QString& caption,
                                          const QString& object_name, int width) {
    auto* block = new QWidget;
    if (compact_)
        block->setMinimumWidth(width);
    else
        block->setFixedWidth(width);
    auto* column = new QVBoxLayout(block);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    if (!compact_) {
        auto* label = new QLabel(caption);
        label->setObjectName(QStringLiteral("solutionMetricCaption"));
        column->addWidget(label);
    }
    auto* value = new QLabel(QStringLiteral("—"));
    value->setObjectName(object_name);
    if (compact_) value->setAlignment(Qt::AlignCenter);
    column->addWidget(value);
    layout->addWidget(block, compact_ ? width : 0);
    return value;
}

void VehicleSolutionWidget::set_waiting() {
    distance_->setText(QStringLiteral("—"));
    bearing_->setText(QStringLiteral("等待目标"));
    mil_->setText(QStringLiteral("—"));
    set_unavailable_state(false);
}

void VehicleSolutionWidget::set_solution(
    const wardogs::CorrectedSolution& solution) {
    distance_->setText(QStringLiteral("%1 m").arg(std::round(solution.reticle_distance_m)));
    bearing_->setText(QString::fromStdWString(wardogs::format_bearing(solution.bearing_deg)));
    mil_->setText(QStringLiteral("%1 mil").arg(std::round(solution.mil)));
    set_unavailable_state(false);
}

void VehicleSolutionWidget::set_unavailable(const QString& distance,
                                             const QString& bearing) {
    distance_->setText(distance);
    bearing_->setText(bearing);
    mil_->setText(QStringLiteral("无可用分划"));
    set_unavailable_state(true);
}

void VehicleSolutionWidget::set_height_unavailable() {
    distance_->setText(QStringLiteral("—"));
    bearing_->setText(QStringLiteral("高度数据不可用"));
    mil_->setText(QStringLiteral("—"));
    set_unavailable_state(true);
}

void VehicleSolutionWidget::copy_from(const VehicleSolutionWidget& source) {
    distance_->setText(source.distance_text());
    const auto source_bearing = source.bearing_text();
    bearing_->setText(source_bearing == QStringLiteral("高度数据不可用") ||
                              source_bearing == QStringLiteral("等待目标")
                          ? QStringLiteral("—")
                          : source_bearing);
    mil_->setText(source.mil_text() == QStringLiteral("无可用分划")
                      ? QStringLiteral("—")
                      : source.mil_text());
    set_unavailable_state(source.unavailable());
}

void VehicleSolutionWidget::set_compact_scale(double scale) {
    if (!compact_) return;
    compact_scale_ = scale;
    const int size = std::max(16, qRound(20 * scale));
    distance_->setStyleSheet(QStringLiteral("font-size:%1px;").arg(size));
    bearing_->setStyleSheet(QStringLiteral("font-size:%1px;").arg(size));
    const auto color = unavailable() ? QStringLiteral("#fca5a5")
                                     : QStringLiteral("#c4b5fd");
    mil_->setStyleSheet(
        QStringLiteral("color:%1;font-size:%2px;").arg(color).arg(size));
}

QString VehicleSolutionWidget::distance_text() const { return distance_->text(); }
QString VehicleSolutionWidget::bearing_text() const { return bearing_->text(); }
QString VehicleSolutionWidget::mil_text() const { return mil_->text(); }
bool VehicleSolutionWidget::unavailable() const {
    return property("unavailable").toBool();
}

void VehicleSolutionWidget::set_unavailable_state(bool unavailable) {
    setProperty("unavailable", unavailable);
    mil_->setProperty("unavailable", unavailable);
    if (compact_) {
        set_compact_scale(compact_scale_);
    } else {
        mil_->setStyleSheet(unavailable
            ? QStringLiteral("color:#fca5a5;font-size:19px;")
            : QStringLiteral("color:#c4b5fd;font-size:25px;"));
    }
    update();
}
