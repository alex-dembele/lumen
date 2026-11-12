#include "sfo.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace {

constexpr std::uint32_t kMagic = 0x46535000; // "\0PSF"
constexpr std::uint16_t kFmtUtf8Special = 0x0004;
constexpr std::uint16_t kFmtUtf8 = 0x0204;
constexpr std::uint16_t kFmtInt32 = 0x0404;

std::uint16_t ReadU16(const std::vector<char>& d, std::size_t off) {
    return static_cast<std::uint16_t>(static_cast<std::uint8_t>(d[off]) |
                                      static_cast<std::uint8_t>(d[off + 1]) << 8);
}

std::uint32_t ReadU32(const std::vector<char>& d, std::size_t off) {
    return ReadU16(d, off) | static_cast<std::uint32_t>(ReadU16(d, off + 2)) << 16;
}

} // namespace

bool Sfo::Load(const std::filesystem::path& path) {
    entries_.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    const std::vector<char> data((std::istreambuf_iterator<char>(file)), {});
    if (data.size() < 20 || ReadU32(data, 0) != kMagic) {
        return false;
    }

    const std::uint32_t key_table = ReadU32(data, 8);
    const std::uint32_t data_table = ReadU32(data, 12);
    const std::uint32_t count = ReadU32(data, 16);

    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t entry = 20 + std::size_t{i} * 16;
        if (entry + 16 > data.size()) {
            return false;
        }
        const std::size_t key_off = key_table + ReadU16(data, entry);
        const std::uint16_t fmt = ReadU16(data, entry + 2);
        const std::uint32_t len = ReadU32(data, entry + 4);
        const std::size_t value_off = data_table + std::size_t{ReadU32(data, entry + 12)};
        if (key_off >= data.size() || value_off + len > data.size()) {
            return false;
        }

        const char* key_ptr = data.data() + key_off;
        const std::string key(key_ptr, strnlen(key_ptr, data.size() - key_off));

        if (fmt == kFmtInt32 && len >= 4) {
            entries_[key] = ReadU32(data, value_off);
        } else if (fmt == kFmtUtf8 || fmt == kFmtUtf8Special) {
            const char* str = data.data() + value_off;
            entries_[key] = std::string(str, strnlen(str, len));
        }
    }
    return true;
}

std::optional<std::string> Sfo::GetString(const std::string& key) const {
    const auto it = entries_.find(key);
    if (it == entries_.end() || !std::holds_alternative<std::string>(it->second)) {
        return std::nullopt;
    }
    return std::get<std::string>(it->second);
}

std::optional<std::uint32_t> Sfo::GetInt(const std::string& key) const {
    const auto it = entries_.find(key);
    if (it == entries_.end() || !std::holds_alternative<std::uint32_t>(it->second)) {
        return std::nullopt;
    }
    return std::get<std::uint32_t>(it->second);
}
