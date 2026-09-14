#include "pinned_result_window.hpp"

#include "vehicle_solution_widget.hpp"

#include <Windows.h>

#include <QEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHoverEvent>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int resize_margin = 8;

QSize minimum_size(bool vehicle) { return vehicle ? QSize{350, 96} : QSize{320, 62}; }
QSize default_size(bool vehicle) { return vehicle ? QSize{420, 116} : QSize{430, 78}; }

class JumpSlider final : public QSlider {
public:
    using QSlider::QSlider;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) {
            QSlider::mousePressEvent(event);
            return;
        }
        QStyleOptionSlider option;
        initStyleOption(&option);
        const QRect handle = style()->subControlRect(
            QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        if (handle.contains(event->position().toPoint())) {
            QSlider::mousePressEvent(event);
            return;
        }
        jump_dragging_ = true;
        set_value_at(event->position().toPoint());
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (jump_dragging_ && (event->buttons() & Qt::LeftButton)) {
            set_value_at(event->position().toPoint());
            event->accept();
            return;
        }
        QSlider::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (jump_dragging_ && event->button() == Qt::LeftButton) {
            set_value_at(event->position().toPoint());
            jump_dragging_ = false;
            event->accept();
            return;
        }
        QSlider::mouseReleaseEvent(event);
    }

private:
    void set_value_at(QPoint position) {
        QStyleOptionSlider option;
        initStyleOption(&option);
        const QRect groove = style()->subControlRect(
            QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
        const QRect handle = style()->subControlRect(
            QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        const int slider_minimum = groove.left();
        const int slider_maximum = groove.right() - handle.width() + 1;
        const int pointer = position.x() - handle.width() / 2;
        setSliderPosition(QStyle::sliderValueFromPosition(
            minimum(), maximum(), pointer - slider_minimum,
            std::max(1, slider_maximum - slider_minimum), option.upsideDown));
    }

    bool jump_dragging_{};
};

class RoundedPopup final : public QWidget {
public:
    explicit RoundedPopup(QWidget* parent)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint |
                              Qt::NoDropShadowWindowHint) {
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void set_effective_opacity(qreal opacity) {
        effective_opacity_ = opacity;
        setWindowOpacity(effective_opacity_);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        (void)event;
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#111827")));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                                10.0, 10.0);
    }

    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        setWindowOpacity(effective_opacity_);
        QTimer::singleShot(0, this, [this] {
            if (isVisible()) setWindowOpacity(effective_opacity_);
        });
    }

private:
    qreal effective_opacity_{1.0};
};

void set_popup_opacity(QWidget* popup, qreal opacity) {
    static_cast<RoundedPopup*>(popup)->set_effective_opacity(opacity);
}

QIcon lock_icon(bool locked) {
    QImage image(24, 24, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color(locked ? QStringLiteral("#67e8f9")
                              : QStringLiteral("#94a3b8"));
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(5.5, 10.0, 13.0, 10.0), 2.0, 2.0);
    QPainterPath shackle;
    if (locked) {
        shackle.moveTo(8.0, 10.0);
        shackle.lineTo(8.0, 7.5);
        shackle.cubicTo(8.0, 2.8, 16.0, 2.8, 16.0, 7.5);
        shackle.lineTo(16.0, 10.0);
    } else {
        shackle.moveTo(10.0, 10.0);
        shackle.lineTo(10.0, 7.5);
        shackle.cubicTo(10.0, 3.0, 17.0, 3.0, 17.0, 7.5);
    }
    painter.drawPath(shackle);
    painter.setBrush(color);
    painter.drawEllipse(QPointF(12.0, 14.4), 1.2, 1.2);
    painter.drawLine(QPointF(12.0, 15.4), QPointF(12.0, 17.3));
    return QIcon(QPixmap::fromImage(image));
}

}  // namespace

PinnedResultWindow::PinnedResultWindow(std::function<void()> exit_callback,
                                       QWidget* parent)
    : PinnedResultWindow(std::move(exit_callback), Preferences{}, {}, parent) {}

PinnedResultWindow::PinnedResultWindow(
    std::function<void()> exit_callback, Preferences preferences,
    std::function<void(Preferences)> preferences_changed, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint |
                          Qt::WindowStaysOnTopHint |
                          Qt::WindowDoesNotAcceptFocus),
      exit_callback_(std::move(exit_callback)),
      preferences_changed_(std::move(preferences_changed)),
      preferences_(preferences) {
    preferences_.opacity_percent = std::clamp(
        preferences_.opacity_percent, Preferences::minimum_opacity_percent,
        Preferences::maximum_opacity_percent);
    setObjectName(QStringLiteral("pinnedWindow"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    setWindowOpacity(preferences_.opacity_percent / 100.0);
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
    build_context_menu();
    apply_font_scale();
}

void PinnedResultWindow::build_context_menu() {
    context_menu_ = new RoundedPopup(this);
    context_menu_->setObjectName(QStringLiteral("pinnedContextMenu"));
    set_popup_opacity(context_menu_, preferences_.opacity_percent / 100.0);

    auto* layout = new QHBoxLayout(context_menu_);
    layout->setContentsMargins(1, 1, 8, 1);
    layout->setSpacing(8);

    lock_button_ = new QToolButton(context_menu_);
    lock_button_->setObjectName(QStringLiteral("pinnedLockButton"));
    lock_button_->setCheckable(true);
    lock_button_->setAutoRaise(false);
    lock_button_->setFixedSize(42, 40);
    lock_button_->setIconSize(QSize(22, 22));
    layout->addWidget(lock_button_);

    opacity_slider_ = new JumpSlider(Qt::Horizontal, context_menu_);
    opacity_slider_->setObjectName(QStringLiteral("pinnedOpacitySlider"));
    opacity_slider_->setRange(Preferences::minimum_opacity_percent,
                              Preferences::maximum_opacity_percent);
    opacity_slider_->setValue(preferences_.opacity_percent);
    opacity_slider_->setMinimumWidth(150);
    opacity_slider_->setToolTip(
        QStringLiteral("卡片透明度：%1%").arg(preferences_.opacity_percent));
    opacity_slider_->setAccessibleName(QStringLiteral("结果卡片透明度"));
    layout->addWidget(opacity_slider_);

    connect(lock_button_, &QToolButton::toggled, this,
            [this](bool locked) { set_locked(locked); });
    connect(opacity_slider_, &QSlider::valueChanged, this,
            [this](int value) { set_opacity_percent(value); });
    update_lock_control();
}

void PinnedResultWindow::update_lock_control() {
    if (!lock_button_) return;
    const QSignalBlocker blocker(lock_button_);
    lock_button_->setChecked(preferences_.locked);
    lock_button_->setIcon(lock_icon(preferences_.locked));
    lock_button_->setToolTip(preferences_.locked
                                 ? QStringLiteral("解除固定")
                                 : QStringLiteral("固定结果卡片"));
    lock_button_->setAccessibleName(lock_button_->toolTip());
}

void PinnedResultWindow::notify_preferences_changed() {
    if (preferences_changed_) preferences_changed_(preferences_);
}

void PinnedResultWindow::set_locked(bool locked) {
    if (preferences_.locked == locked) return;
    preferences_.locked = locked;
    dragging_ = false;
    resize_edges_.clear();
    setCursor(Qt::ArrowCursor);
    update_lock_control();
    notify_preferences_changed();
}

void PinnedResultWindow::set_opacity_percent(int opacity_percent) {
    const int clamped = std::clamp(
        opacity_percent, Preferences::minimum_opacity_percent,
        Preferences::maximum_opacity_percent);
    if (preferences_.opacity_percent == clamped) {
        if (opacity_slider_)
            opacity_slider_->setToolTip(
                QStringLiteral("卡片透明度：%1%").arg(clamped));
        return;
    }
    preferences_.opacity_percent = clamped;
    setWindowOpacity(clamped / 100.0);
    if (context_menu_) set_popup_opacity(context_menu_, clamped / 100.0);
    if (opacity_slider_) {
        const QSignalBlocker blocker(opacity_slider_);
        opacity_slider_->setValue(clamped);
        opacity_slider_->setToolTip(
            QStringLiteral("卡片透明度：%1%").arg(clamped));
    }
    notify_preferences_changed();
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

bool PinnedResultWindow::event(QEvent* event) {
    if (event->type() == QEvent::HoverMove && !dragging_ && resize_edges_.empty()) {
        const auto* hover = static_cast<QHoverEvent*>(event);
        setCursor(preferences_.locked
                      ? Qt::ArrowCursor
                      : cursor_for_edges(
                            resize_edges_at(hover->position().toPoint())));
    }
    return QWidget::event(event);
}

bool PinnedResultWindow::nativeEvent(const QByteArray& event_type, void* message,
                                     qintptr* result) {
    auto* native_message = static_cast<MSG*>(message);
    if (native_message && native_message->message == WM_SETCURSOR) {
        LPCWSTR cursor_id = IDC_ARROW;
        if (!preferences_.locked) {
            const auto edges = resize_edges_at(mapFromGlobal(QCursor::pos()));
            if (edges == Edges{"left", "top"} ||
                edges == Edges{"bottom", "right"})
                cursor_id = IDC_SIZENWSE;
            else if (edges == Edges{"right", "top"} ||
                     edges == Edges{"bottom", "left"})
                cursor_id = IDC_SIZENESW;
            else if (edges.contains("left") || edges.contains("right"))
                cursor_id = IDC_SIZEWE;
            else if (edges.contains("top") || edges.contains("bottom"))
                cursor_id = IDC_SIZENS;
        }
        ::SetCursor(::LoadCursorW(nullptr, cursor_id));
        *result = TRUE;
        return true;
    }
    return QWidget::nativeEvent(event_type, message, result);
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
    return Qt::ArrowCursor;
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
        if (preferences_.locked) {
            event->accept();
            return;
        }
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
    if (preferences_.locked) {
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
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
        if (preferences_.locked) {
            event->accept();
            return;
        }
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
        if (preferences_.locked) {
            event->accept();
            return;
        }
        dragging_ = false;
        resize_edges_.clear();
        if (exit_callback_) exit_callback_();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PinnedResultWindow::leaveEvent(QEvent* event) {
    if (!dragging_ && resize_edges_.empty())
        setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(event);
}

void PinnedResultWindow::contextMenuEvent(QContextMenuEvent* event) {
    if (!context_menu_) build_context_menu();
    update_lock_control();
    const qreal opacity = preferences_.opacity_percent / 100.0;
    set_popup_opacity(context_menu_, opacity);
    context_menu_->move(context_menu_position());
    context_menu_->show();
    context_menu_->raise();
    set_popup_opacity(context_menu_, opacity);
    event->accept();
}

QPoint PinnedResultWindow::context_menu_position() const {
    context_menu_->ensurePolished();
    context_menu_->adjustSize();
    constexpr int gap = 2;
    const QSize menu_size = context_menu_->sizeHint().expandedTo(context_menu_->size());
    QPoint position = mapToGlobal(QPoint(width() + gap, 0));
    QScreen* screen = QGuiApplication::screenAt(mapToGlobal(rect().center()));
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return position;
    const QRect available = screen->availableGeometry();
    if (position.x() + menu_size.width() > available.right() + 1)
        position.setX(mapToGlobal(QPoint(-menu_size.width() - gap, 0)).x());
    const int maximum_x = std::max(
        available.left(), available.right() - menu_size.width() + 1);
    const int maximum_y = std::max(
        available.top(), available.bottom() - menu_size.height() + 1);
    position.setX(std::clamp(position.x(), available.left(), maximum_x));
    position.setY(std::clamp(position.y(), available.top(), maximum_y));
    return position;
}
