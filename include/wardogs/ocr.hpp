#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace wardogs {

struct Image {
    int width{};
    int height{};
    std::vector<std::uint8_t> bgr;
};

struct OcrResult {
    std::wstring text;
    float confidence{};
};

Image load_image_file(const std::filesystem::path& path);

OcrResult decode_ctc(const std::vector<float>& probabilities,
                     std::size_t time_steps,
                     std::size_t class_count,
                     const std::vector<std::wstring>& characters);

class RapidOcr {
public:
    explicit RapidOcr(const std::filesystem::path& model_path);
    ~RapidOcr();
    RapidOcr(RapidOcr&&) noexcept;
    RapidOcr& operator=(RapidOcr&&) noexcept;
    RapidOcr(const RapidOcr&) = delete;
    RapidOcr& operator=(const RapidOcr&) = delete;

    OcrResult recognize(const Image& image) const;
    [[nodiscard]] std::size_t character_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wardogs
