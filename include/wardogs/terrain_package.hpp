#pragma once

#include "wardogs/core.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wardogs {

class TerrainPackage final {
public:
    explicit TerrainPackage(const std::filesystem::path& path,
                            std::size_t cache_chunks = 8);
    ~TerrainPackage();
    TerrainPackage(TerrainPackage&&) noexcept;
    TerrainPackage& operator=(TerrainPackage&&) noexcept;
    TerrainPackage(const TerrainPackage&) = delete;
    TerrainPackage& operator=(const TerrainPackage&) = delete;

    [[nodiscard]] const std::string& map_id() const;
    [[nodiscard]] std::size_t cached_chunk_count() const;
    [[nodiscard]] std::optional<double> height_at(Point point);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct TerrainMapSpec {
    std::string map_id;
    std::wstring display_name;
    std::filesystem::path filename;
    std::string sha256;
};

struct InstalledTerrainMap {
    TerrainMapSpec spec;
    std::filesystem::path path;
};

struct TerrainDiscovery {
    std::vector<InstalledTerrainMap> installed;
    std::vector<std::wstring> problems;
};

[[nodiscard]] std::string sha256_file(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path default_terrain_directory();
[[nodiscard]] const std::vector<TerrainMapSpec>& official_terrain_maps();
[[nodiscard]] TerrainDiscovery discover_terrain_maps(
    const std::filesystem::path& directory,
    const std::vector<TerrainMapSpec>& specs = official_terrain_maps());

}  // namespace wardogs
