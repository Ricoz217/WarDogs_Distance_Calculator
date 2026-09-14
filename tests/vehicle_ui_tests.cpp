#include "vehicle_solution_widget.hpp"

#include <QApplication>
#include <QLabel>

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

    std::cout << "All vehicle UI tests passed\n" << std::flush;
    // Qt's Windows offscreen plugin can wait indefinitely during process teardown.
    // All assertions are complete and no persistent resources are owned by this test.
    std::_Exit(0);
}
