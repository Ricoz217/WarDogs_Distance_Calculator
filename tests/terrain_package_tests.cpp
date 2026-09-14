#include "wardogs/terrain_package.hpp"

#include <Windows.h>

#include <QCoreApplication>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void close(double actual, double expected, const char* message) {
    check(std::abs(actual - expected) < 1e-8, message);
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    const std::filesystem::path fixture{WARDOGS_TERRAIN_TEST_PACKAGE};
    const auto digest = wardogs::sha256_file(fixture);
    check(digest.size() == 64, "SHA-256 is returned as 64 hexadecimal digits");

    wardogs::TerrainPackage terrain(fixture, 1);
    check(terrain.map_id() == "test-map", "package map id is read");
    check(terrain.cached_chunk_count() == 0, "package starts without decoded chunks");
    const auto first = terrain.height_at({0.5, 0.5});
    check(first.has_value(), "first covered point has terrain height");
    close(*first, -8.0, "first chunk is decoded and bilinearly sampled");
    check(terrain.cached_chunk_count() == 1, "only requested chunk is cached");
    const auto second = terrain.height_at({2.5, 0.5});
    check(second.has_value(), "second covered point has terrain height");
    close(*second, 2.0, "second chunk is decoded and bilinearly sampled");
    check(terrain.cached_chunk_count() == 1, "LRU cache honors its chunk limit");
    check(!terrain.height_at({5, 1}), "point outside coverage has no height");

    const auto temporary = std::filesystem::temp_directory_path() /
        (L"wardogs-terrain-test-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(temporary);
    const auto valid_path = temporary / L"valid.wdt";
    const auto damaged_path = temporary / L"damaged.wdt";
    std::filesystem::copy_file(fixture, valid_path,
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(fixture, damaged_path,
                               std::filesystem::copy_options::overwrite_existing);
    std::ofstream(damaged_path, std::ios::binary | std::ios::app).put('x');

    const std::vector specs{
        wardogs::TerrainMapSpec{"test-map", L"有效", L"valid.wdt", digest},
        wardogs::TerrainMapSpec{"test-map", L"损坏", L"damaged.wdt", digest},
        wardogs::TerrainMapSpec{"missing", L"缺失", L"missing.wdt", digest},
    };
    const auto discovery = wardogs::discover_terrain_maps(temporary, specs);
    check(discovery.installed.size() == 1,
          "discovery exposes only verified terrain packages");
    check(discovery.problems.size() == 2,
          "discovery reports damaged and missing packages");
    std::error_code ignored;
    std::filesystem::remove_all(temporary, ignored);

    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All terrain package tests passed\n";
    return 0;
}
