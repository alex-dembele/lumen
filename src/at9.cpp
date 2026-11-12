#include "at9.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>

#include <libatrac9.h>

namespace {

constexpr std::uint16_t kWaveFormatExtensible = 0xFFFE;
constexpr std::size_t kConfigOffsetInFmt = 44; // WAVEFORMATEXTENSIBLE (40) + version (4)

std::uint32_t ReadU32(const std::vector<unsigned char>& d, std::size_t off) {
    return d[off] | d[off + 1] << 8 | d[off + 2] << 16 | static_cast<std::uint32_t>(d[off + 3]) << 24;
}

std::uint16_t ReadU16(const std::vector<unsigned char>& d, std::size_t off) {
    return static_cast<std::uint16_t>(d[off] | d[off + 1] << 8);
}

struct HandleDeleter {
    void operator()(void* h) const { Atrac9ReleaseHandle(h); }
};

} // namespace

bool DecodeAt9(const std::filesystem::path& path, PcmAudio& out, int max_seconds) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    const std::vector<unsigned char> d((std::istreambuf_iterator<char>(file)), {});
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) {
        return false;
    }

    unsigned char config[ATRAC9_CONFIG_DATA_SIZE] = {};
    bool has_config = false;
    std::uint32_t total_samples = 0;
    std::uint32_t delay_samples = 0;
    std::size_t data_off = 0;
    std::size_t data_size = 0;

    for (std::size_t pos = 12; pos + 8 <= d.size();) {
        const std::uint32_t size = ReadU32(d, pos + 4);
        const std::size_t body = pos + 8;
        if (body + size > d.size()) {
            break;
        }
        if (!std::memcmp(d.data() + pos, "fmt ", 4) && size >= kConfigOffsetInFmt + 4 &&
            ReadU16(d, body) == kWaveFormatExtensible) {
            std::memcpy(config, d.data() + body + kConfigOffsetInFmt, sizeof(config));
            has_config = true;
        } else if (!std::memcmp(d.data() + pos, "fact", 4) && size >= 4) {
            total_samples = ReadU32(d, body);
            if (size >= 12) {
                delay_samples = ReadU32(d, body + 8);
            }
        } else if (!std::memcmp(d.data() + pos, "data", 4)) {
            data_off = body;
            data_size = size;
        }
        pos = body + size + (size & 1);
    }
    if (!has_config || data_size == 0) {
        return false;
    }

    std::unique_ptr<void, HandleDeleter> handle(Atrac9GetHandle());
    if (!handle || Atrac9InitDecoder(handle.get(), config) != 0) {
        return false;
    }
    Atrac9CodecInfo info{};
    if (Atrac9GetCodecInfo(handle.get(), &info) != 0 || info.channels <= 0 ||
        info.frameSamples <= 0) {
        return false;
    }

    out.channels = info.channels;
    out.sample_rate = info.samplingRate;
    out.samples.clear();

    const std::size_t max_frames = static_cast<std::size_t>(info.samplingRate) * max_seconds;
    std::vector<std::int16_t> frame(static_cast<std::size_t>(info.frameSamples) * info.channels);
    std::size_t pos = data_off;
    const std::size_t end = data_off + data_size;
    while (pos < end && out.samples.size() / info.channels < max_frames + delay_samples) {
        int used = 0;
        if (Atrac9Decode(handle.get(), d.data() + pos, static_cast<int>(end - pos), frame.data(),
                         &used, 0) != 0 ||
            used <= 0) {
            break;
        }
        out.samples.insert(out.samples.end(), frame.begin(), frame.end());
        pos += static_cast<std::size_t>(used);
    }

    // Retire le délai de l'encodeur au début et le remplissage à la fin.
    const std::size_t skip = std::min<std::size_t>(delay_samples, out.samples.size() / info.channels);
    out.samples.erase(out.samples.begin(), out.samples.begin() + skip * info.channels);
    if (total_samples > 0 && out.samples.size() / info.channels > total_samples) {
        out.samples.resize(static_cast<std::size_t>(total_samples) * info.channels);
    }
    return !out.samples.empty();
}
