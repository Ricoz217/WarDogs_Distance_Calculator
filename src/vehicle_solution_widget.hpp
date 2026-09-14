#pragma once

#include "wardogs/vehicle_ballistics.hpp"

#include <QFrame>

class QHBoxLayout;
class QLabel;

class VehicleSolutionWidget final : public QFrame {
public:
    explicit VehicleSolutionWidget(wardogs::Arc arc, bool compact = false,
                                   QWidget* parent = nullptr);

    void set_waiting();
    void set_solution(const wardogs::CorrectedSolution& solution);
    void set_unavailable(const QString& distance, const QString& bearing);
    void set_height_unavailable();
    void copy_from(const VehicleSolutionWidget& source);
    void set_compact_scale(double scale);

    [[nodiscard]] QString distance_text() const;
    [[nodiscard]] QString bearing_text() const;
    [[nodiscard]] QString mil_text() const;
    [[nodiscard]] bool unavailable() const;

private:
    QLabel* add_metric(QHBoxLayout* layout, const QString& caption,
                       const QString& object_name, int width);
    void set_unavailable_state(bool unavailable);

    wardogs::Arc arc_;
    bool compact_{};
    double compact_scale_{1.0};
    QLabel *distance_{}, *bearing_{}, *mil_{};
};
