#include "flipit.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern "C" {
#include "heatshrink_decoder.h"
#include "heatshrink_encoder.h"
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Failed to open file: " + path);

    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!f.read(reinterpret_cast<char*>(buffer.data()), size))
        throw std::runtime_error("Failed to read file: " + path);

    return buffer;
}

std::vector<uint8_t> decompressHeatshrink(const uint8_t* input, size_t input_size) {
    constexpr uint16_t INPUT_BUFFER_SIZE = 256;

    heatshrink_decoder* dec = heatshrink_decoder_alloc(INPUT_BUFFER_SIZE, WINDOW_BITS, LOOKAHEAD_BITS);
    if (!dec) throw std::runtime_error("Failed to allocate heatshrink decoder");

    std::vector<uint8_t> output;
    output.reserve(input_size);

    size_t offset = 0;
    while (offset < input_size) {
        size_t sunk = 0;
        const HSD_sink_res sres = heatshrink_decoder_sink(dec, const_cast<uint8_t*>(input + offset), input_size - offset, &sunk);
        if (sres != HSDR_SINK_OK && sres != HSDR_SINK_FULL)
            throw std::runtime_error("Heatshrink sink failed");

        offset += sunk;

        uint8_t buf[256];
        size_t out_size = 0;
        while (heatshrink_decoder_poll(dec, buf, sizeof(buf), &out_size) == HSDR_POLL_MORE) {
            output.insert(output.end(), buf, buf + out_size);
        }
    }

    heatshrink_decoder_finish(dec);
    heatshrink_decoder_free(dec);
    return output;
}

std::vector<uint8_t> compressHeatshrink(const uint8_t* input, const size_t input_size) {
    heatshrink_encoder* enc = heatshrink_encoder_alloc(WINDOW_BITS, LOOKAHEAD_BITS);
    if (!enc) throw std::runtime_error("Failed to allocate heatshrink encoder");

    std::vector<uint8_t> output;
    output.reserve(input_size);

    size_t offset = 0;
    while (offset < input_size) {
        size_t sunk = 0;
        const size_t chunk = std::min<size_t>(256, input_size - offset);

        heatshrink_encoder_sink(enc, const_cast<uint8_t*>(input + offset), chunk, &sunk);
        offset += sunk;

        uint8_t buf[256];
        size_t out_size = 0;
        while (heatshrink_encoder_poll(enc, buf, sizeof(buf), &out_size) == HSER_POLL_MORE) {
            output.insert(output.end(), buf, buf + out_size);
        }
    }

    heatshrink_encoder_finish(enc);
    heatshrink_encoder_free(enc);
    return output;
}

std::vector<uint8_t> LoadBM(const std::string& path) {
    auto file = readFile(path);
    if (file.empty()) throw std::runtime_error("Empty BM file");

    const uint8_t flag = file[0];
    std::vector<uint8_t> result;

    if (flag == 0x00) {
        result.assign(file.begin() + 1, file.end());
    } else if (flag == 0x01) {
        if (file.size() < 5) throw std::runtime_error("Truncated compressed length field");
        const uint32_t comp_len = ((uint32_t)file[1] << 8) | ((uint32_t)file[2] << 0);
        if (1 + comp_len > file.size()) throw std::runtime_error("Compressed data overflow");
        result = decompressHeatshrink(&file[4], comp_len);
    } else {
        throw std::runtime_error("Unknown compression flag");
    }

    return result;
}

std::vector<uint8_t> LoadBMX(const std::string& path, BmxHeader& header) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Failed to open file");

    f.read(reinterpret_cast<char*>(&header), sizeof(BmxHeader));
    if (!f) throw std::runtime_error("Failed to read header");

    std::vector<uint8_t> data{};
    if (header.is_compressed) {
        data.resize(header.compressed_size);
        f.read(reinterpret_cast<char*>(data.data()), header.compressed_size);
        data = decompressHeatshrink(data.data(), data.size());
    } else {
        data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    return data;
}

std::vector<uint8_t> expandBitData(const std::vector<uint8_t>& bitData, uint32_t width, uint32_t height) {
    std::vector<uint8_t> pixels(width * height);
    size_t bytes_per_row = (width + 7) / 8;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t byteIndex = y * bytes_per_row + (x / 8);
            uint8_t mask = 1 << (x % 8);
            bool bit = (bitData[byteIndex] & mask) != 0;
            pixels[y * width + x] = bit ? 0 : 255;
        }
    }
    return pixels;
}

std::vector<uint8_t> convertToBitData(const uint8_t* data, uint32_t width, uint32_t height) {
    const size_t bytes_per_row = (width + 7) / 8;
    std::vector<uint8_t> bitData(bytes_per_row * height, 0);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t pixel = data[y * width + x];
            bool is_black = pixel < 128;
            if (is_black)
                bitData[y * bytes_per_row + (x / 8)] |= (1 << (x % 8));
        }
    }
    return bitData;
}

bool writeBmx(const std::string& path, const uint8_t* pixels, uint32_t width, uint32_t height) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    std::vector<uint8_t> bitData = convertToBitData(pixels, width, height);
    std::vector<uint8_t> compressedData = compressHeatshrink(bitData.data(), bitData.size());

    bool should_compress = compressedData.size() < bitData.size();

    BmxHeader header{};
    header.width = width;
    header.height = height;
    header.is_compressed = should_compress;
    header.compressed_size = static_cast<uint16_t>(
        should_compress ? compressedData.size() : bitData.size());

    f.write(reinterpret_cast<const char*>(&header), sizeof(BmxHeader));
    const auto& dataToWrite = should_compress ? compressedData : bitData;
    f.write(reinterpret_cast<const char*>(dataToWrite.data()), dataToWrite.size());
    return true;
}

bool convertImageToBM(const std::string& inputPath, const std::string& outputPath, bool compress) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load(inputPath.c_str(), &width, &height, &channels, 1);
    if (!pixels) {
        std::cerr << "Failed to load image: " << inputPath << "\n";
        return false;
    }

    bool ok = writeBmx(outputPath, pixels, width, height);
    stbi_image_free(pixels);
    return ok;
}

BmMeta readBmMeta(const std::string& path) {
    const auto file = readFile(path);
    if (file.size() < sizeof(BmMeta))
        throw std::runtime_error("File too small for BmMeta");

    BmMeta meta{};
    std::memcpy(&meta, file.data(), sizeof(BmMeta));
    return meta;
}
