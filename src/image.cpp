#include "wardogs/ocr.hpp"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <stdexcept>

namespace wardogs {

Image load_image_file(const std::filesystem::path& path) {
    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE) {
        throw std::runtime_error("cannot initialize COM");
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(result)) {
        throw std::runtime_error("cannot create WIC imaging factory");
    }
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    result = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(result)) {
        throw std::runtime_error("cannot decode image file");
    }
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, &frame);
    if (FAILED(result)) {
        throw std::runtime_error("cannot read image frame");
    }
    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) {
        result = converter->Initialize(frame.Get(), GUID_WICPixelFormat24bppBGR,
                                       WICBitmapDitherTypeNone, nullptr, 0,
                                       WICBitmapPaletteTypeCustom);
    }
    if (FAILED(result)) {
        throw std::runtime_error("cannot convert image to BGR");
    }

    Image image{static_cast<int>(width), static_cast<int>(height), {}};
    const UINT stride = width * 3;
    image.bgr.resize(static_cast<std::size_t>(stride) * height);
    result = converter->CopyPixels(nullptr, stride,
                                   static_cast<UINT>(image.bgr.size()), image.bgr.data());
    if (FAILED(result)) {
        throw std::runtime_error("cannot copy image pixels");
    }
    return image;
}

}  // namespace wardogs
