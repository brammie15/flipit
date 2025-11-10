#pragma once

#include <string>
#include <vector>
#include <cstdint>

#pragma pack(push, 1)
struct BmxHeader {
    uint32_t width;
    uint32_t height;
    bool     is_compressed;
    uint16_t compressed_size;
};
#pragma pack(pop)

#pragma  pack(push, 1)
struct UncompressedBmxHeader {
    uint32_t width;
    uint32_t height;
    bool     is_compressed;
};
#pragma pack(pop)

#pragma  pack(push, 1)
struct CompressedBmxHeader {
    uint32_t width;
    uint32_t height;
    bool     is_compressed;
    uint8_t _pad; //Padding to make it align to 4 bytes prob
    uint16_t compressed_size;
};
#pragma pack(pop)


#pragma pack(push, 1)
struct BmMeta {
    uint32_t width;
    uint32_t height;
    uint32_t frame_rate;
    uint32_t frame_count;
};
#pragma pack(pop)

inline uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

constexpr uint8_t WINDOW_BITS = 8;
constexpr uint8_t LOOKAHEAD_BITS = 4;

std::vector<uint8_t> readFile(const std::string &path);

std::vector<uint8_t> decompressHeatshrink(const uint8_t* input, size_t input_size);
std::vector<uint8_t> compressHeatshrink(const uint8_t* input, size_t input_size);

std::vector<uint8_t> LoadBM(const std::string &path);
std::vector<uint8_t> LoadBMX(const std::string &path, BmxHeader &info);

std::vector<uint8_t> expandBitData(const std::vector<uint8_t>& bitData, uint32_t width, uint32_t height);
std::vector<uint8_t> convertToBitData(const uint8_t* data, uint32_t width, uint32_t height);

bool writeBmx(const std::string &path, const uint8_t* pixels, uint32_t width, uint32_t height);
bool convertImageToBM(const std::string &inputPath, const std::string &outputPath, bool compress);

BmMeta readBmMeta(const std::string &path);
