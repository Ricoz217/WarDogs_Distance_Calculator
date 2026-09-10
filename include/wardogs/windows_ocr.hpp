#pragma once

#include "wardogs/ocr.hpp"

#include <memory>

namespace wardogs {

class WindowsOcr {
public:
    WindowsOcr();
    ~WindowsOcr();
    WindowsOcr(WindowsOcr&&) noexcept;
    WindowsOcr& operator=(WindowsOcr&&) noexcept;
    WindowsOcr(const WindowsOcr&) = delete;
    WindowsOcr& operator=(const WindowsOcr&) = delete;

    OcrResult recognize(const Image& image) const;
    OcrResult recognize_high_contrast(const Image& image) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wardogs
