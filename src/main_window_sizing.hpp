#pragma once

#include <QSize>

#include <algorithm>
#include <array>

class MainWindowSizing final {
public:
    static QSize minimum_for_content(const QSize& content,
                                     const QSize& title_bar,
                                     int frame_inset = 6) {
        return {std::max(content.width(), title_bar.width()) + frame_inset,
                content.height() + title_bar.height() + frame_inset};
    }

    void remember(bool vehicle_mode, const QSize& size) {
        sizes_[index(vehicle_mode)] = size;
    }

    [[nodiscard]] QSize preferred(bool vehicle_mode,
                                  const QSize& minimum) const {
        const QSize saved = sizes_[index(vehicle_mode)];
        return (saved.isValid() ? saved : minimum).expandedTo(minimum);
    }

private:
    static constexpr std::size_t index(bool vehicle_mode) {
        return vehicle_mode ? 1U : 0U;
    }

    std::array<QSize, 2> sizes_{};
};
