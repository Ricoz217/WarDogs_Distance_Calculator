#include "wardogs/core.hpp"
#include "wardogs/ocr.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}  // namespace

int main() {
    using namespace wardogs;

    // blank, a, b, space. Consecutive equal classes collapse; blank separates.
    const std::vector<std::wstring> characters{L"blank", L"a", L"b", L" "};
    const std::vector<std::size_t> winners{1, 1, 0, 1, 2, 2, 0};
    std::vector<float> probabilities(winners.size() * characters.size(), 0.01F);
    for (std::size_t t = 0; t < winners.size(); ++t) {
        probabilities[t * characters.size() + winners[t]] = 0.97F;
    }
    const auto decoded = decode_ctc(probabilities, winners.size(), characters.size(),
                                    characters);
    check(decoded.text == L"aab", "CTC removes duplicates and blank tokens");
    check(std::abs(decoded.confidence - 0.97F) < 0.001F,
          "CTC confidence averages selected character scores");

    const auto image = load_image_file(WARDOGS_TEST_IMAGE);
    check(image.width == 226 && image.height == 47,
          "WIC loads a golden screenshot in physical pixels");
    check(image.bgr.size() == static_cast<std::size_t>(image.width * image.height * 3),
          "decoded screenshot is packed BGR");

    RapidOcr ocr(WARDOGS_TEST_MODEL);
    check(ocr.character_count() == 18710,
          "model metadata supplies the full CTC character table");
    const auto result = ocr.recognize(image);
    check(result.text == L"x114.51, y191.81",
          "recognition-only model reads the game coordinate line");
    check(result.confidence > 0.95F, "golden screenshot confidence stays high");
    check(parse_ocr_coordinate(result.text) == Point{114.51, 191.81},
          "OCR result feeds the coordinate parser");

    const auto extra_text_image = load_image_file(WARDOGS_TEST_EXTRA_TEXT_IMAGE);
    check(extra_text_image.width == 390 && extra_text_image.height == 58,
          "extra-text regression screenshot loads at its original size");
    const auto extra_text = ocr.recognize(extra_text_image);
    try {
        const auto extra_point = parse_ocr_coordinate(extra_text.text);
        check(std::abs(extra_point.x - 120.32) < 0.001 &&
                  std::abs(extra_point.y - 84.21) < 0.001,
              "extra multilingual text does not change the coordinate values");
    } catch (const std::invalid_argument&) {
        check(false, "direct OCR output keeps an embedded coordinate parseable");
    }

    const auto repeated_image = load_image_file(WARDOGS_TEST_REPEATED_IMAGE);
    check(repeated_image.width == 150 && repeated_image.height == 43,
          "repeated-digit regression keeps the loose vertical crop");
    const auto repeated = ocr.recognize(repeated_image);
    check(repeated.text.find(L"x99.67") != std::wstring::npos,
          "loose vertical crop preserves the x coordinate");
    check(repeated.text.find(L"y111.06") != std::wstring::npos,
          "recognition preserves three repeated 1 digits");
    try {
        check(parse_ocr_coordinate(repeated.text) == Point{99.67, 111.06},
              "repeated-digit OCR output remains parseable");
    } catch (const std::invalid_argument&) {
        check(false, "repeated-digit OCR output contains a complete coordinate pair");
    }

    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All OCR tests passed\n";
    return 0;
}
