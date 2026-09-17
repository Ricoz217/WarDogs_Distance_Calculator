#pragma once

#include <QWidget>

class CrosshairOverlay final : public QWidget {
public:
    explicit CrosshairOverlay(QWidget* parent = nullptr);

    void set_gap(int gap_px);
    void set_thickness(int thickness_px);
    void set_length(int length_px);

    [[nodiscard]] int gap() const { return gap_; }
    [[nodiscard]] int thickness() const { return thickness_; }
    [[nodiscard]] int length() const { return length_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    int gap_{100};
    int thickness_{3};
    int length_{60};
};
