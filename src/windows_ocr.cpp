#include "wardogs/windows_ocr.hpp"

#include <Windows.h>
#include <robuffer.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace wardogs {
namespace {

Image upscale(const Image& source, int minimum_height = 96) {
    if (source.height >= minimum_height) return source;
    const int scale = (minimum_height + source.height - 1) / source.height;
    Image result{source.width * scale, source.height * scale,
                 std::vector<std::uint8_t>(static_cast<std::size_t>(source.width * scale *
                                                                    source.height * scale * 3))};
    for (int y = 0; y < result.height; ++y) {
        for (int x = 0; x < result.width; ++x) {
            const auto source_offset =
                static_cast<std::size_t>(((y / scale) * source.width + x / scale) * 3);
            const auto target_offset = static_cast<std::size_t>((y * result.width + x) * 3);
            std::copy_n(source.bgr.data() + source_offset, 3,
                        result.bgr.data() + target_offset);
        }
    }
    return result;
}

Image color_mask(const Image& source) {
    Image result{source.width, source.height,
                 std::vector<std::uint8_t>(source.bgr.size(), 255)};
    for (std::size_t i = 0; i < source.bgr.size(); i += 3) {
        const int blue = source.bgr[i];
        const int green = source.bgr[i + 1];
        const int red = source.bgr[i + 2];
        const int chroma = std::max({blue, green, red}) - std::min({blue, green, red});
        if (chroma >= 40 && green > red && green > blue) {
            result.bgr[i] = result.bgr[i + 1] = result.bgr[i + 2] = 0;
        }
    }
    return upscale(result);
}

winrt::Windows::Graphics::Imaging::SoftwareBitmap software_bitmap(const Image& image) {
    using namespace winrt::Windows::Graphics::Imaging;
    const auto byte_count = static_cast<std::uint32_t>(image.width * image.height * 4);
    winrt::Windows::Storage::Streams::Buffer buffer(byte_count);
    buffer.Length(byte_count);
    byte* bytes = nullptr;
    winrt::check_hresult(
        buffer.as<::Windows::Storage::Streams::IBufferByteAccess>()->Buffer(&bytes));
    for (std::size_t input = 0, output = 0; input < image.bgr.size();
         input += 3, output += 4) {
        bytes[output] = image.bgr[input];
        bytes[output + 1] = image.bgr[input + 1];
        bytes[output + 2] = image.bgr[input + 2];
        bytes[output + 3] = 255;
    }
    return SoftwareBitmap::CreateCopyFromBuffer(buffer, BitmapPixelFormat::Bgra8,
                                                 image.width, image.height,
                                                 BitmapAlphaMode::Ignore);
}

}  // namespace

struct WindowsOcr::Impl {
    winrt::Windows::Media::Ocr::OcrEngine engine{nullptr};

    Impl() {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        engine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromLanguage(
            winrt::Windows::Globalization::Language(L"en-US"));
        if (!engine) {
            throw std::runtime_error("Windows English OCR language is not installed");
        }
    }

    OcrResult run(const Image& image) const {
        const auto bitmap = software_bitmap(image);
        const auto result = engine.RecognizeAsync(bitmap).get();
        return {result.Text().c_str(), 0.0F};
    }
};

WindowsOcr::WindowsOcr() : impl_(std::make_unique<Impl>()) {}
WindowsOcr::~WindowsOcr() = default;
WindowsOcr::WindowsOcr(WindowsOcr&&) noexcept = default;
WindowsOcr& WindowsOcr::operator=(WindowsOcr&&) noexcept = default;

OcrResult WindowsOcr::recognize(const Image& image) const {
    return impl_->run(upscale(image));
}

OcrResult WindowsOcr::recognize_high_contrast(const Image& image) const {
    return impl_->run(color_mask(image));
}

}  // namespace wardogs
