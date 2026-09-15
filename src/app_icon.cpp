#include "app_icon.hpp"

#include <QPainter>
#include <QPixmap>

#include <algorithm>
#include <array>

QIcon wardogs_application_icon() {
    QIcon icon;
    constexpr std::array sizes{16, 20, 24, 32, 48, 64, 128, 256};
    for (const int size : sizes) {
        QPixmap image(size, size);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal scale = size / 18.0;
        const QPointF center(size / 2.0, size / 2.0);
        painter.setPen(QPen(QColor(QStringLiteral("#67e8f9")),
                            std::max(1.2, 1.5 * scale), Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, 5.5 * scale, 5.5 * scale);
        painter.drawEllipse(center, 1.8 * scale, 1.8 * scale);
        painter.drawLine(QPointF(center.x(), 1.0 * scale),
                         QPointF(center.x(), 4.0 * scale));
        painter.drawLine(QPointF(center.x(), 14.0 * scale),
                         QPointF(center.x(), 17.0 * scale));
        painter.drawLine(QPointF(1.0 * scale, center.y()),
                         QPointF(4.0 * scale, center.y()));
        painter.drawLine(QPointF(14.0 * scale, center.y()),
                         QPointF(17.0 * scale, center.y()));
        icon.addPixmap(image);
    }
    return icon;
}
