#include "wardogs/terrain_package.hpp"

#include <Windows.h>
#include <bcrypt.h>
#include <zstd.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <list>
#include <map>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace wardogs {
namespace {

constexpr std::array<char, 8> package_magic{'W', 'D', 'T', 'R', 'N', '2', 'M', '1'};
constexpr auto package_format = "wardogs-terrain-pack-v1";

template <typename Value>
Value read_little(std::istream& stream) {
    static_assert(std::is_integral_v<Value>);
    std::array<unsigned char, sizeof(Value)> bytes{};
    stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!stream) throw std::invalid_argument("高度包意外结束");
    std::make_unsigned_t<Value> result{};
    for (std::size_t index = 0; index < bytes.size(); ++index)
        result |= static_cast<std::make_unsigned_t<Value>>(bytes[index]) <<
                  (index * 8);
    return std::bit_cast<Value>(result);
}

std::vector<char> read_exact(std::istream& stream, std::size_t size) {
    std::vector<char> result(size);
    stream.read(result.data(), static_cast<std::streamsize>(size));
    if (!stream) throw std::invalid_argument("高度包意外结束");
    return result;
}

std::uint32_t crc32(std::span<const std::uint16_t> values) {
    std::uint32_t crc = 0xffffffffU;
    const auto* bytes = reinterpret_cast<const unsigned char*>(values.data());
    for (std::size_t index = 0; index < values.size_bytes(); ++index) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return crc ^ 0xffffffffU;
}

std::wstring widen(std::string_view text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return L"?";
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), size);
    return result;
}

std::string json_string(const QJsonObject& object, const char* key) {
    const auto value = object.value(QString::fromLatin1(key));
    if (!value.isString()) throw std::invalid_argument("高度包元数据缺少文本字段");
    const auto utf8 = value.toString().toUtf8();
    return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

double json_number(const QJsonObject& object, const char* key) {
    const auto value = object.value(QString::fromLatin1(key));
    if (!value.isDouble()) throw std::invalid_argument("高度包元数据缺少数值字段");
    return value.toDouble();
}

}  // namespace

struct TerrainPackage::Impl {
    using Key = std::pair<int, int>;
    struct Record {
        std::uint64_t offset{};
        std::uint32_t compressed_size{};
        std::uint32_t raw_size{};
        std::uint32_t checksum{};
    };

    std::filesystem::path path;
    std::ifstream stream;
    std::string map_id;
    std::map<Key, Record> index;
    std::map<Key, std::vector<std::uint16_t>> cache;
    std::list<Key> lru;
    std::size_t cache_limit{};
    int chunk_x_min{}, chunk_x_max{}, chunk_y_min{}, chunk_y_max{};
    int chunk_quads{}, vertices_per_side{};
    double global_quad_offset_x{}, global_quad_offset_y{};
    double factor_x{}, factor_y{};
    int height_base_decimeters{};
    double height_step_meters{};
    double coverage_x_min{}, coverage_x_max{}, coverage_y_min{}, coverage_y_max{};

    explicit Impl(const std::filesystem::path& source, std::size_t cache_chunks)
        : path(source), stream(source, std::ios::binary), cache_limit(cache_chunks) {
        if (!stream) throw std::invalid_argument("无法打开高度包");
        if (cache_limit < 1) throw std::invalid_argument("高度块缓存必须至少为 1");
        const auto magic = read_exact(stream, package_magic.size());
        if (!std::equal(magic.begin(), magic.end(), package_magic.begin()))
            throw std::invalid_argument("无法识别高度包文件头");
        const auto metadata_size = read_little<std::uint32_t>(stream);
        const auto chunk_count = read_little<std::uint32_t>(stream);
        const auto metadata_bytes = read_exact(stream, metadata_size);
        QJsonParseError parse_error;
        const auto document = QJsonDocument::fromJson(
            QByteArray(metadata_bytes.data(), static_cast<qsizetype>(metadata_bytes.size())),
            &parse_error);
        if (parse_error.error != QJsonParseError::NoError || !document.isObject())
            throw std::invalid_argument("高度包元数据无效");
        const auto metadata = document.object();
        if (json_string(metadata, "format") != package_format)
            throw std::invalid_argument("高度包格式不受支持");
        if (static_cast<std::uint32_t>(json_number(metadata, "chunkCount")) !=
            chunk_count)
            throw std::invalid_argument("高度包块数量不一致");
        map_id = json_string(metadata, "mapId");
        chunk_x_min = static_cast<int>(json_number(metadata, "chunkXMin"));
        chunk_x_max = static_cast<int>(json_number(metadata, "chunkXMax"));
        chunk_y_min = static_cast<int>(json_number(metadata, "chunkYMin"));
        chunk_y_max = static_cast<int>(json_number(metadata, "chunkYMax"));
        chunk_quads = static_cast<int>(json_number(metadata, "chunkQuads"));
        vertices_per_side = static_cast<int>(json_number(metadata, "verticesPerSide"));
        global_quad_offset_x = json_number(metadata, "globalQuadOffsetX");
        global_quad_offset_y = json_number(metadata, "globalQuadOffsetY");
        const auto shared_factor = metadata.value(QStringLiteral("gameUnitsToLandscapeQuads"));
        factor_x = metadata.value(QStringLiteral("gameUnitsToLandscapeQuadsX"))
                       .toDouble(shared_factor.toDouble());
        factor_y = metadata.value(QStringLiteral("gameUnitsToLandscapeQuadsY"))
                       .toDouble(shared_factor.toDouble());
        height_base_decimeters =
            static_cast<int>(json_number(metadata, "heightBaseDecimeters"));
        height_step_meters = json_number(metadata, "heightStepMeters");
        const auto coverage = metadata.value(QStringLiteral("coverage")).toObject();
        coverage_x_min = json_number(coverage, "gameXMin");
        coverage_x_max = json_number(coverage, "gameXMax");
        coverage_y_min = json_number(coverage, "gameYMin");
        coverage_y_max = json_number(coverage, "gameYMax");

        for (std::uint32_t number = 0; number < chunk_count; ++number) {
            const int x = read_little<std::int16_t>(stream);
            const int y = read_little<std::int16_t>(stream);
            Record record;
            record.compressed_size = read_little<std::uint32_t>(stream);
            record.raw_size = read_little<std::uint32_t>(stream);
            record.checksum = read_little<std::uint32_t>(stream);
            record.offset = static_cast<std::uint64_t>(stream.tellg());
            if (!index.emplace(Key{x, y}, record).second)
                throw std::invalid_argument("高度包包含重复块");
            stream.seekg(record.compressed_size, std::ios::cur);
            if (!stream) throw std::invalid_argument("高度包块数据不完整");
        }
        const auto indexed_end = static_cast<std::uint64_t>(stream.tellg());
        const auto file_size = std::filesystem::file_size(path);
        if (indexed_end != file_size)
            throw std::invalid_argument("高度包存在尾随或截断数据");
    }

    const std::vector<std::uint16_t>& chunk(Key key) {
        if (const auto found = cache.find(key); found != cache.end()) {
            lru.remove(key);
            lru.push_back(key);
            return found->second;
        }
        const auto found = index.find(key);
        if (found == index.end()) throw std::invalid_argument("所需高度块不存在");
        const auto& record = found->second;
        stream.clear();
        stream.seekg(static_cast<std::streamoff>(record.offset));
        const auto compressed = read_exact(stream, record.compressed_size);
        std::vector<std::uint16_t> residual(record.raw_size / 2);
        const std::size_t decoded = ZSTD_decompress(
            residual.data(), record.raw_size, compressed.data(), compressed.size());
        if (ZSTD_isError(decoded) || decoded != record.raw_size)
            throw std::invalid_argument("高度块无法解压");
        std::vector<std::uint16_t> values(residual.size());
        for (int y = 0; y < vertices_per_side; ++y) {
            for (int x = 0; x < vertices_per_side; ++x) {
                const auto offset = static_cast<std::size_t>(y * vertices_per_side + x);
                std::uint32_t restored = residual[offset];
                if (x > 0) restored += values[offset - 1];
                if (y > 0) restored += values[offset - vertices_per_side];
                if (x > 0 && y > 0)
                    restored -= values[offset - vertices_per_side - 1];
                values[offset] = static_cast<std::uint16_t>(restored & 0xffffU);
            }
        }
        if (crc32(values) != record.checksum)
            throw std::invalid_argument("高度块校验失败");
        lru.push_back(key);
        auto [inserted, _] = cache.emplace(key, std::move(values));
        while (cache.size() > cache_limit) {
            cache.erase(lru.front());
            lru.pop_front();
        }
        return inserted->second;
    }
};

TerrainPackage::TerrainPackage(const std::filesystem::path& path,
                               std::size_t cache_chunks)
    : impl_(std::make_unique<Impl>(path, cache_chunks)) {}
TerrainPackage::~TerrainPackage() = default;
TerrainPackage::TerrainPackage(TerrainPackage&&) noexcept = default;
TerrainPackage& TerrainPackage::operator=(TerrainPackage&&) noexcept = default;

const std::string& TerrainPackage::map_id() const { return impl_->map_id; }
std::size_t TerrainPackage::cached_chunk_count() const { return impl_->cache.size(); }

std::optional<double> TerrainPackage::height_at(Point point) {
    auto& value = *impl_;
    if (point.x < value.coverage_x_min || point.x > value.coverage_x_max ||
        point.y < value.coverage_y_min || point.y > value.coverage_y_max)
        return std::nullopt;
    const double quad_x = value.global_quad_offset_x + point.x * value.factor_x;
    const double quad_y = value.global_quad_offset_y + point.y * value.factor_y;
    const int chunk_x = std::clamp(
        static_cast<int>(std::floor(quad_x / value.chunk_quads)), value.chunk_x_min,
        value.chunk_x_max);
    const int chunk_y = std::clamp(
        static_cast<int>(std::floor(quad_y / value.chunk_quads)), value.chunk_y_min,
        value.chunk_y_max);
    const double local_x = std::clamp(quad_x - chunk_x * value.chunk_quads, 0.0,
                                      static_cast<double>(value.chunk_quads));
    const double local_y = std::clamp(quad_y - chunk_y * value.chunk_quads, 0.0,
                                      static_cast<double>(value.chunk_quads));
    const auto& heights = value.chunk({chunk_x, chunk_y});
    const int maximum_vertex = value.vertices_per_side - 1;
    const int x0 = std::min(maximum_vertex, static_cast<int>(std::floor(local_x)));
    const int y0 = std::min(maximum_vertex, static_cast<int>(std::floor(local_y)));
    const int x1 = std::min(maximum_vertex, x0 + 1);
    const int y1 = std::min(maximum_vertex, y0 + 1);
    const double fx = local_x - x0;
    const double fy = local_y - y0;
    const auto at = [&](int x, int y) {
        return static_cast<double>(heights[static_cast<std::size_t>(
            y * value.vertices_per_side + x)]);
    };
    const double top = at(x0, y0) * (1 - fx) + at(x1, y0) * fx;
    const double bottom = at(x0, y1) * (1 - fx) + at(x1, y1) * fx;
    const double quantized = top * (1 - fy) + bottom * fy;
    return (value.height_base_decimeters + quantized) * value.height_step_meters;
}

std::string sha256_file(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD object_size{};
    DWORD result_size{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                          &result_size, 0) < 0) {
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("无法初始化 SHA-256");
    }
    std::vector<unsigned char> object(object_size);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("无法创建 SHA-256");
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("无法打开高度包进行校验");
    }
    std::vector<char> buffer(1024 * 1024);
    while (stream) {
        stream.read(buffer.data(), buffer.size());
        const auto count = stream.gcount();
        if (count > 0 && BCryptHashData(
                             hash, reinterpret_cast<PUCHAR>(buffer.data()),
                             static_cast<ULONG>(count), 0) < 0) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            throw std::runtime_error("SHA-256 计算失败");
        }
    }
    std::array<unsigned char, 32> digest{};
    const auto status = BCryptFinishHash(
        hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) throw std::runtime_error("SHA-256 计算失败");
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : digest) output << std::setw(2) << static_cast<int>(byte);
    return output.str();
}

std::filesystem::path default_terrain_directory() {
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(),
                                            static_cast<DWORD>(executable.size()));
    executable.resize(length);
    return std::filesystem::path(executable).parent_path() / L"terrain-packs";
}

const std::vector<TerrainMapSpec>& official_terrain_maps() {
    static const std::vector<TerrainMapSpec> maps{
        {"bakurani", L"Bakurani", L"bakurani.wdt",
         "9c79af2f69df5023f6e2e944329ae80981432e7115832ae801da067bd79bfdde"},
        {"ozeti", L"Ozeti", L"ozeti.wdt",
         "f636225e89ffe19111da58466d4db60e4ccff285c3cfa11d8c30b959f544c400"},
        {"zestafona", L"Zestafona", L"zestafona.wdt",
         "e60f95a6e23791164342fe51b465ac04238f338f1f4293aef664b61513ecc048"},
    };
    return maps;
}

TerrainDiscovery discover_terrain_maps(const std::filesystem::path& directory,
                                        const std::vector<TerrainMapSpec>& specs) {
    TerrainDiscovery result;
    for (const auto& spec : specs) {
        const auto path = directory / spec.filename;
        if (!std::filesystem::is_regular_file(path)) {
            result.problems.push_back(spec.display_name + L"：未安装");
            continue;
        }
        try {
            if (sha256_file(path) != spec.sha256)
                throw std::invalid_argument("SHA-256 校验失败");
            TerrainPackage package(path);
            if (package.map_id() != spec.map_id)
                throw std::invalid_argument("地图标识不匹配");
            result.installed.push_back({spec, path});
        } catch (const std::exception& error) {
            result.problems.push_back(spec.display_name + L"：校验失败（" +
                                      widen(error.what()) + L"）");
        }
    }
    return result;
}

}  // namespace wardogs
