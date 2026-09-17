#pragma once

#include <QWidget>
#include <array>
#include <utility>

class MortarReticleWidget final : public QWidget {
public:
    explicit MortarReticleWidget(QWidget* parent = nullptr);

    void set_solution(double distance_m);
    void set_out_of_range(const QString& message);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr std::array<std::pair<double, double>, 17> distance_to_mil_ = {{
        {110, 900}, {132, 850}, {187, 800}, {240, 750}, {290, 700},
        {340, 650}, {385, 600}, {430, 550}, {470, 500}, {510, 450},
        {545, 400}, {578, 350}, {609, 300}, {637, 250}, {661, 200},
        {684, 150}, {710, 100}
    }};

    static constexpr double mil_min_ = 150.0;
    static constexpr double mil_max_ = 900.0;
    static constexpr double mil_step_ = 50.0;
    static constexpr double physical_spacing_cm_ = 2.7;

    double current_distance_m_{-1.0};
    QString out_of_range_message_;
    bool has_solution_{false};
    bool out_of_range_{false};

    [[nodiscard]] double mil_for_distance(double distance_m) const;
    [[nodiscard]] double pixels_per_mil() const;
    [[nodiscard]] double scale_offset(double target_mil) const;
};
