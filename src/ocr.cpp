#include "wardogs/ocr.hpp"

#include <onnxruntime_cxx_api.h>

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>

namespace wardogs {
namespace {

std::wstring utf8_to_wide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                          static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) {
        throw std::runtime_error("model metadata is not valid UTF-8");
    }
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), result.data(), count);
    return result;
}

std::vector<std::wstring> split_characters(std::string_view value) {
    std::vector<std::wstring> result;
    std::size_t begin = 0;
    while (begin < value.size()) {
        const auto end = value.find('\n', begin);
        auto line = value.substr(begin, end == std::string_view::npos
                                           ? value.size() - begin
                                           : end - begin);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        result.push_back(utf8_to_wide(line));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    result.push_back(L" ");
    result.insert(result.begin(), L"blank");
    return result;
}

std::vector<float> prepare_image(const Image& image, int& tensor_width,
                                 int crop_top = 0, int crop_bottom = 0) {
    if (image.width <= 0 || image.height <= 0 ||
        image.bgr.size() != static_cast<std::size_t>(image.width * image.height * 3)) {
        throw std::invalid_argument("OCR image must be packed three-channel BGR");
    }
    if (crop_top < 0 || crop_bottom < 0 || crop_top + crop_bottom >= image.height) {
        throw std::invalid_argument("OCR vertical crop is invalid");
    }
    constexpr int target_height = 48;
    constexpr int minimum_width = 320;
    const int source_height = image.height - crop_top - crop_bottom;
    const double ratio = static_cast<double>(image.width) / source_height;
    tensor_width = std::max(minimum_width,
                            static_cast<int>(target_height * std::max(320.0 / 48.0, ratio)));
    const int resized_width = std::min(
        tensor_width, static_cast<int>(std::ceil(target_height * ratio)));
    std::vector<float> tensor(static_cast<std::size_t>(3 * target_height * tensor_width),
                              0.0F);

    const double scale_x = static_cast<double>(image.width) / resized_width;
    const double scale_y = static_cast<double>(source_height) / target_height;
    const std::size_t plane = static_cast<std::size_t>(target_height * tensor_width);
    for (int y = 0; y < target_height; ++y) {
        const double source_y = crop_top + (y + 0.5) * scale_y - 0.5;
        const int y0 = std::clamp(static_cast<int>(std::floor(source_y)), crop_top,
                                  image.height - crop_bottom - 1);
        const int y1 = std::min(y0 + 1, image.height - crop_bottom - 1);
        const double fy = std::clamp(source_y - std::floor(source_y), 0.0, 1.0);
        for (int x = 0; x < resized_width; ++x) {
            const double source_x = (x + 0.5) * scale_x - 0.5;
            const int x0 = std::clamp(static_cast<int>(std::floor(source_x)), 0, image.width - 1);
            const int x1 = std::min(x0 + 1, image.width - 1);
            const double fx = std::clamp(source_x - std::floor(source_x), 0.0, 1.0);
            for (int channel = 0; channel < 3; ++channel) {
                const auto sample = [&](int sx, int sy) {
                    return image.bgr[static_cast<std::size_t>((sy * image.width + sx) * 3 +
                                                              channel)];
                };
                const double top = sample(x0, y0) * (1.0 - fx) + sample(x1, y0) * fx;
                const double bottom = sample(x0, y1) * (1.0 - fx) + sample(x1, y1) * fx;
                const double pixel = top * (1.0 - fy) + bottom * fy;
                tensor[static_cast<std::size_t>(channel) * plane +
                       static_cast<std::size_t>(y * tensor_width + x)] =
                    static_cast<float>(pixel / 127.5 - 1.0);
            }
        }
    }
    return tensor;
}

}  // namespace

OcrResult decode_ctc(const std::vector<float>& probabilities,
                     std::size_t time_steps,
                     std::size_t class_count,
                     const std::vector<std::wstring>& characters) {
    if (class_count == 0 || characters.size() != class_count ||
        probabilities.size() != time_steps * class_count) {
        throw std::invalid_argument("invalid CTC tensor dimensions");
    }
    OcrResult result;
    float confidence_sum = 0.0F;
    std::size_t selected = 0;
    std::size_t previous = class_count;
    for (std::size_t step = 0; step < time_steps; ++step) {
        const auto begin = probabilities.begin() +
                           static_cast<std::ptrdiff_t>(step * class_count);
        const auto winner = std::max_element(begin, begin +
                                                       static_cast<std::ptrdiff_t>(class_count));
        const auto index = static_cast<std::size_t>(winner - begin);
        if (index != previous && index != 0) {
            result.text += characters[index];
            confidence_sum += *winner;
            ++selected;
        }
        previous = index;
    }
    result.confidence = selected ? confidence_sum / static_cast<float>(selected) : 0.0F;
    return result;
}

struct RapidOcr::Impl {
    Ort::Env environment{ORT_LOGGING_LEVEL_WARNING, "wardogs"};
    Ort::SessionOptions options;
    Ort::Session session{nullptr};
    std::vector<std::wstring> characters;

    explicit Impl(const std::filesystem::path& model_path) {
        options.SetIntraOpNumThreads(1);
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session = Ort::Session(environment, model_path.c_str(), options);
        Ort::AllocatorWithDefaultOptions allocator;
        auto metadata = session.GetModelMetadata();
        auto encoded = metadata.LookupCustomMetadataMapAllocated("character", allocator);
        if (!encoded) {
            throw std::runtime_error("OCR model does not contain character metadata");
        }
        characters = split_characters(encoded.get());
    }
};

RapidOcr::RapidOcr(const std::filesystem::path& model_path)
    : impl_(std::make_unique<Impl>(model_path)) {}

RapidOcr::~RapidOcr() = default;
RapidOcr::RapidOcr(RapidOcr&&) noexcept = default;
RapidOcr& RapidOcr::operator=(RapidOcr&&) noexcept = default;

std::size_t RapidOcr::character_count() const { return impl_->characters.size(); }

OcrResult RapidOcr::recognize(const Image& image) const {
    const auto infer = [this, &image](int crop_top, int crop_bottom) {
        int width = 0;
        auto input = prepare_image(image, width, crop_top, crop_bottom);
        const std::array<std::int64_t, 4> shape{1, 3, 48, width};
        auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto tensor = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(),
                                                       shape.data(), shape.size());
        constexpr std::array<const char*, 1> input_names{"x"};
        constexpr std::array<const char*, 1> output_names{"fetch_name_0"};
        auto outputs = impl_->session.Run(Ort::RunOptions{nullptr}, input_names.data(),
                                          &tensor, 1, output_names.data(), 1);
        const auto output_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        if (output_shape.size() != 3 || output_shape[0] != 1 || output_shape[2] <= 0) {
            throw std::runtime_error("OCR model returned an unexpected tensor shape");
        }
        const auto steps = static_cast<std::size_t>(output_shape[1]);
        const auto classes = static_cast<std::size_t>(output_shape[2]);
        const float* values = outputs[0].GetTensorData<float>();
        return decode_ctc(std::vector<float>(values, values + steps * classes), steps,
                          classes, impl_->characters);
    };

    OcrResult original = infer(0, 0);
    if (image.height < 20) return original;

    // Recognition-only models expect a tightly cropped text line. Preserve the
    // user's exact region, but also try removing a small amount of vertical
    // margin so repeated narrow glyphs receive enough horizontal time steps.
    const int trim = std::clamp(static_cast<int>(std::lround(image.height * 0.10)),
                                1, (image.height - 12) / 2);
    OcrResult tightened = infer(trim, trim);
    return tightened.confidence > original.confidence ? tightened : original;
}

}  // namespace wardogs
