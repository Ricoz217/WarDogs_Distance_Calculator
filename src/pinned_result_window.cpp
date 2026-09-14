#include "pinned_result_window.hpp"

#include "vehicle_solution_widget.hpp"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int resize_margin = 8;

QSize minimum_size(bool vehicle) { return vehicle ? QSize{350, 96} : QSize{320, 62}; }
QSize default_size(bool vehicle) { return vehicle ? QSize{420, 116} : QSize{430, 78}; }

}  // namespace

PinnedResultWindow::PinnedResultWindow(std::function<void()> exit_callback,
                                       QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint |
                          Qt::WindowStaysOnTopHint),
      exit_callback_(std::move(exit_callback)) {
    setObjectName(QStringLiteral("pinnedWindow"));
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setWindowTitle(QStringLiteral("War Dogs 射表结果"));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    frame_ = new QFrame;
    frame_->setObjectName(QStringLiteral("pinnedFrame"));
    frame_->setProperty("error", false);
    frame_->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* layout = new QVBoxLayout(frame_);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    mortar_panel_ = new QWidget;
    auto* mortar_layout = new QHBoxLayout(mortar_panel_);
    mortar_layout->setContentsMargins(0, 0, 0, 0);
    mortar_layout->setSpacing(8);
    mortar_layout->addWidget(result_card(QStringLiteral("#fbbf24"), distance_), 1);
    mortar_layout->addWidget(result_card(QStringLiteral("#67e8f9"), bearing_), 1);
    layout->addWidget(mortar_panel_);

    vehicle_panel_ = new QWidget;
    auto* vehicle_layout = new QVBoxLayout(vehicle_panel_);
    vehicle_layout->setContentsMargins(0, 0, 0, 0);
    vehicle_layout->setSpacing(6);
    low_ = new VehicleSolutionWidget(wardogs::Arc::low, true);
    high_ = new VehicleSolutionWidget(wardogs::Arc::high, true);
    vehicle_layout->addWidget(low_);
    vehicle_layout->addWidget(high_);
    layout->addWidget(vehicle_panel_);
    vehicle_panel_->hide();
    outer->addWidget(frame_);
    setMinimumSize(minimum_size(false));
    resize(default_size(false));
    apply_font_scale();
}

QWidget* PinnedResultWindow::result_card(const QString& color, QLabel*& value) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("resultCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 6, 8, 7);
    value = new QLabel(QStringLiteral("—"));
    value->setAlignment(Qt::AlignCenter);
    value->setStyleSheet(QStringLiteral(
        "color:%1;font-family:'Bahnschrift';font-size:30px;font-weight:700;")
                             .arg(color));
    layout->addWidget(value);
    return card;
}

void PinnedResultWindow::set_mode(bool vehicle_mode) {
    mortar_panel_->setVisible(!vehicle_mode);
    vehicle_panel_->setVisible(vehicle_mode);
    if (vehicle_mode != vehicle_mode_) {
        mode_sizes_[vehicle_mode_] = size();
        vehicle_mode_ = vehicle_mode;
        setMinimumSize(minimum_size(vehicle_mode_));
        resize(mode_sizes_[vehicle_mode_].expandedTo(minimumSize()));
    }
    apply_font_scale();
}

void PinnedResultWindow::set_values(const QString& distance,
                                    const QString& bearing) {
    distance_->setText(distance);
    bearing_->setText(bearing);
}

void PinnedResultWindow::set_vehicle_values(const VehicleSolutionWidget& low,
                                            const VehicleSolutionWidget& high) {
    low_->copy_from(low);
    high_->copy_from(high);
}

void PinnedResultWindow::set_error(bool error) {
    frame_->setProperty("error", error);
    frame_->style()->unpolish(frame_);
    frame_->style()->polish(frame_);
    frame_->update();
}

PinnedResultWindow::Edges PinnedResultWindow::resize_edges_at(QPoint position) const {
    Edges result;
    if (position.x() <= resize_margin)
        result.insert("left");
    else if (position.x() >= width() - resize_margin - 1)
        result.insert("right");
    if (position.y() <= resize_margin)
        result.insert("top");
    else if (position.y() >= height() - resize_margin - 1)
        result.insert("bottom");
    return result;
}

Qt::CursorShape PinnedResultWindow::cursor_for_edges(const Edges& edges) {
    if (edges == Edges{"left", "top"} || edges == Edges{"bottom", "right"})
        return Qt::SizeFDiagCursor;
    if (edges == Edges{"right", "top"} || edges == Edges{"bottom", "left"})
        return Qt::SizeBDiagCursor;
    if (edges.contains("left") || edges.contains("right"))
        return Qt::SizeHorCursor;
    if (edges.contains("top") || edges.contains("bottom"))
        return Qt::SizeVerCursor;
    return Qt::OpenHandCursor;
}

void PinnedResultWindow::resize_from_pointer(QPoint pointer) {
    const auto delta = pointer - resize_start_global_;
    auto resized = resize_start_geometry_;
    if (resize_edges_.contains("left"))
        resized.setLeft(std::min(resize_start_geometry_.left() + delta.x(),
                                 resize_start_geometry_.right() - minimumWidth() + 1));
    if (resize_edges_.contains("right"))
        resized.setRight(std::max(resize_start_geometry_.right() + delta.x(),
                                  resize_start_geometry_.left() + minimumWidth() - 1));
    if (resize_edges_.contains("top"))
        resized.setTop(std::min(resize_start_geometry_.top() + delta.y(),
                                resize_start_geometry_.bottom() - minimumHeight() + 1));
    if (resize_edges_.contains("bottom"))
        resized.setBottom(std::max(
            resize_start_geometry_.bottom() + delta.y(),
            resize_start_geometry_.top() + minimumHeight() - 1));
    setGeometry(resized);
}

void PinnedResultWindow::apply_font_scale() {
    if (applying_font_scale_) return;
    applying_font_scale_ = true;
    const auto base = default_size(vehicle_mode_);
    font_scale_ = std::clamp(
        std::min(width() / static_cast<double>(base.width()),
                 height() / static_cast<double>(base.height())),
        0.78, 2.5);
    if (vehicle_mode_) {
        low_->set_compact_scale(font_scale_);
        high_->set_compact_scale(font_scale_);
        applying_font_scale_ = false;
        return;
    }
    const int size = std::max(22, qRound(30 * font_scale_));
    distance_->setStyleSheet(QStringLiteral(
        "color:#fbbf24;font-family:'Bahnschrift';font-size:%1px;font-weight:700;")
                                 .arg(size));
    bearing_->setStyleSheet(QStringLiteral(
        "color:#67e8f9;font-family:'Bahnschrift';font-size:%1px;font-weight:700;")
                                .arg(size));
    applying_font_scale_ = false;
}

void PinnedResultWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    mode_sizes_[vehicle_mode_] = event->size();
    if (low_ && !applying_font_scale_) apply_font_scale();
}

void PinnedResultWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const auto edges = resize_edges_at(event->position().toPoint());
        if (!edges.empty()) {
            resize_edges_ = edges;
            resize_start_global_ = event->globalPosition().toPoint();
            resize_start_geometry_ = geometry();
            setCursor(cursor_for_edges(edges));
            event->accept();
            return;
        }
        dragging_ = true;
        drag_offset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void PinnedResultWindow::mouseMoveEvent(QMouseEvent* event) {
    if (!resize_edges_.empty() && (event->buttons() & Qt::LeftButton)) {
        resize_from_pointer(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - drag_offset_);
        event->accept();
        return;
    }
    setCursor(cursor_for_edges(resize_edges_at(event->position().toPoint())));
    QWidget::mouseMoveEvent(event);
}

void PinnedResultWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        resize_edges_.clear();
        dragging_ = false;
        setCursor(cursor_for_edges(resize_edges_at(event->position().toPoint())));
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PinnedResultWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
        resize_edges_.clear();
        if (exit_callback_) exit_callback_();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PinnedResultWindow::leaveEvent(QEvent* event) {
    if (!dragging_ && resize_edges_.empty()) setCursor(Qt::OpenHandCursor);
    QWidget::leaveEvent(event);
}
