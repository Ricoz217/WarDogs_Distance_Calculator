#include "selection_overlay.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdlib>
#include <exception>

namespace {

QString error_text(const std::exception& error) {
    return QString::fromUtf8(error.what());
}

}  // namespace

SelectionOverlay::~SelectionOverlay() {
    if (window_) DestroyWindow(window_);
}

bool SelectionOverlay::begin(Callback callback) {
    if (window_) return false;
    callback_ = std::move(callback);
    ensure_class();
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    window_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, class_name,
        L"设置 OCR 区域", WS_POPUP, left, top, width, height, nullptr, nullptr,
        GetModuleHandleW(nullptr), this);
    if (!window_) {
        callback_ = {};
        return false;
    }
    SetLayeredWindowAttributes(window_, 0, 108, LWA_ALPHA);
    ShowWindow(window_, SW_SHOW);
    SetForegroundWindow(window_);
    SetCursor(LoadCursorW(nullptr, IDC_CROSS));
    SetCapture(window_);
    return true;
}

void SelectionOverlay::ensure_class() {
    static const ATOM atom = [] {
        WNDCLASSEXW value{sizeof(value)};
        value.style = CS_HREDRAW | CS_VREDRAW;
        value.lpfnWndProc = window_proc;
        value.hInstance = GetModuleHandleW(nullptr);
        value.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        value.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        value.lpszClassName = class_name;
        return RegisterClassExW(&value);
    }();
    (void)atom;
}

void SelectionOverlay::finish(std::optional<wardogs::CaptureRegion> region,
                              QString error) {
    auto callback = std::move(callback_);
    callback_ = {};
    if (window_) DestroyWindow(window_);
    if (callback) callback(std::move(region), std::move(error));
}

LRESULT SelectionOverlay::handle(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            return TRUE;
        case WM_LBUTTONDOWN:
            selecting_ = true;
            start_ = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ClientToScreen(window_, &start_);
            end_ = start_;
            monitor_ = MonitorFromPoint(start_, MONITOR_DEFAULTTONEAREST);
            SetCapture(window_);
            return 0;
        case WM_MOUSEMOVE:
            if (selecting_) {
                POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ClientToScreen(window_, &point);
                MONITORINFO info{sizeof(info)};
                GetMonitorInfoW(monitor_, &info);
                point.x = std::clamp(point.x, info.rcMonitor.left,
                                     info.rcMonitor.right);
                point.y = std::clamp(point.y, info.rcMonitor.top,
                                     info.rcMonitor.bottom);
                end_ = point;
                InvalidateRect(window_, nullptr, TRUE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (selecting_) {
                selecting_ = false;
                ReleaseCapture();
                RECT selected{start_.x, start_.y, end_.x, end_.y};
                if (std::abs(selected.right - selected.left) < 8 ||
                    std::abs(selected.bottom - selected.top) < 8) {
                    finish(std::nullopt,
                           QStringLiteral("框选区域太小，请重新框选"));
                    return 0;
                }
                try {
                    finish(wardogs::make_capture_region(monitor_, selected));
                } catch (const std::exception& error) {
                    finish(std::nullopt, error_text(error));
                }
            }
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                ReleaseCapture();
                finish(std::nullopt);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window_, &paint);
            FillRect(dc, &paint.rcPaint,
                     static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            if (selecting_) {
                RECT rect{start_.x, start_.y, end_.x, end_.y};
                POINT origin{};
                ClientToScreen(window_, &origin);
                OffsetRect(&rect, -origin.x, -origin.y);
                HPEN pen = CreatePen(PS_SOLID, 4, RGB(34, 211, 153));
                HGDIOBJ old_pen = SelectObject(dc, pen);
                HGDIOBJ old_brush =
                    SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
                SelectObject(dc, old_brush);
                SelectObject(dc, old_pen);
                DeleteObject(pen);
            }
            EndPaint(window_, &paint);
            return 0;
        }
        case WM_DESTROY:
            window_ = nullptr;
            return 0;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

LRESULT CALLBACK SelectionOverlay::window_proc(HWND window, UINT message,
                                                WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<SelectionOverlay*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<SelectionOverlay*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->handle(message, wparam, lparam)
                : DefWindowProcW(window, message, wparam, lparam);
}
