#pragma once

#include "wardogs/ocr.hpp"

#include <Windows.h>

#include <string>

namespace wardogs {

struct CaptureRegion {
    std::wstring monitor_device;
    RECT relative{};
};

CaptureRegion make_capture_region(HMONITOR monitor, RECT virtual_rect, int padding = 0);
RECT resolve_capture_region(const CaptureRegion& region);
Image capture_screen(const CaptureRegion& region);

}  // namespace wardogs
