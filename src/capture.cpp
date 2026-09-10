#include "wardogs/capture.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace wardogs {
namespace {

MONITORINFOEXW monitor_info(HMONITOR monitor) {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        throw std::runtime_error("cannot read monitor information");
    }
    return info;
}

struct SearchContext {
    const std::wstring* device;
    HMONITOR result{};
};

BOOL CALLBACK find_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto& context = *reinterpret_cast<SearchContext*>(data);
    const auto info = monitor_info(monitor);
    if (*context.device == info.szDevice) {
        context.result = monitor;
        return FALSE;
    }
    return TRUE;
}

}  // namespace

CaptureRegion make_capture_region(HMONITOR monitor, RECT virtual_rect, int padding) {
    const auto info = monitor_info(monitor);
    RECT selected{
        std::min(virtual_rect.left, virtual_rect.right),
        std::min(virtual_rect.top, virtual_rect.bottom),
        std::max(virtual_rect.left, virtual_rect.right),
        std::max(virtual_rect.top, virtual_rect.bottom),
    };
    selected.left = std::max(info.rcMonitor.left, selected.left - padding);
    selected.top = std::max(info.rcMonitor.top, selected.top - padding);
    selected.right = std::min(info.rcMonitor.right, selected.right + padding);
    selected.bottom = std::min(info.rcMonitor.bottom, selected.bottom + padding);
    if (selected.right <= selected.left || selected.bottom <= selected.top) {
        throw std::invalid_argument("capture region is empty");
    }
    return {info.szDevice,
            {selected.left - info.rcMonitor.left, selected.top - info.rcMonitor.top,
             selected.right - info.rcMonitor.left,
             selected.bottom - info.rcMonitor.top}};
}

RECT resolve_capture_region(const CaptureRegion& region) {
    SearchContext context{&region.monitor_device, nullptr};
    EnumDisplayMonitors(nullptr, nullptr, find_monitor,
                        reinterpret_cast<LPARAM>(&context));
    if (!context.result) {
        throw std::runtime_error("the selected monitor is no longer connected");
    }
    const auto info = monitor_info(context.result);
    const RECT& value = region.relative;
    if (value.left < 0 || value.top < 0 || value.right > info.rcMonitor.right - info.rcMonitor.left ||
        value.bottom > info.rcMonitor.bottom - info.rcMonitor.top ||
        value.right <= value.left || value.bottom <= value.top) {
        throw std::runtime_error("capture region no longer fits the selected monitor");
    }
    return {info.rcMonitor.left + value.left, info.rcMonitor.top + value.top,
            info.rcMonitor.left + value.right, info.rcMonitor.top + value.bottom};
}

Image capture_screen(const CaptureRegion& region) {
    const RECT rect = resolve_capture_region(region);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO bitmap_info{};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &bitmap_info, DIB_RGB_COLORS, &pixels,
                                      nullptr, 0);
    if (!screen || !memory || !bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        if (screen) ReleaseDC(nullptr, screen);
        throw std::runtime_error("cannot allocate screen capture surface");
    }
    HGDIOBJ previous = SelectObject(memory, bitmap);
    const BOOL copied = BitBlt(memory, 0, 0, width, height, screen, rect.left, rect.top,
                               SRCCOPY | CAPTUREBLT);
    Image image{width, height, std::vector<std::uint8_t>(
                                   static_cast<std::size_t>(width * height * 3))};
    if (copied) {
        const auto* bgra = static_cast<const std::uint8_t*>(pixels);
        for (std::size_t i = 0, j = 0; j < image.bgr.size(); i += 4, j += 3) {
            image.bgr[j] = bgra[i];
            image.bgr[j + 1] = bgra[i + 1];
            image.bgr[j + 2] = bgra[i + 2];
        }
    }
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    if (!copied) {
        throw std::runtime_error("BitBlt screen capture failed");
    }
    return image;
}

}  // namespace wardogs
