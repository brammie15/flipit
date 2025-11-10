#include "bm_utils.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

extern "C" {
#include "heatshrink_decoder.h"
#include "heatshrink_encoder.h"
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Failed to open file: " + path);

    const std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!f.read(reinterpret_cast<char*>(buffer.data()), size))
        throw std::runtime_error("Failed to read file: " + path);

    return buffer;
}

std::vector<uint8_t> decompressHeatshrink(const uint8_t* input, const size_t input_size) {
    constexpr uint16_t INPUT_BUFFER_SIZE = 256;

    heatshrink_decoder* dec = heatshrink_decoder_alloc(INPUT_BUFFER_SIZE, WINDOW_BITS, LOOKAHEAD_BITS);
    if (!dec) throw std::runtime_error("Failed to allocate heatshrink decoder");

    std::vector<uint8_t> output;
    output.reserve(input_size);

    size_t offset = 0;
    while (offset < input_size) {
        size_t sunk = 0;
        const HSD_sink_res sres = heatshrink_decoder_sink(dec, const_cast<uint8_t*>(input + offset), input_size - offset, &sunk);

        if (sres == HSDR_SINK_ERROR_NULL) {
            heatshrink_decoder_free(dec);
            throw std::runtime_error("Decoder sink: null parameter");
        }

        if (sres == HSDR_SINK_FULL) {
            while (true) {
                uint8_t buf[256];
                size_t out_size = 0;
                const HSD_poll_res pres = heatshrink_decoder_poll(dec, buf, sizeof(buf), &out_size);
                if (pres == HSDR_POLL_ERROR_NULL) {
                    heatshrink_decoder_free(dec);
                    throw std::runtime_error("Decoder poll: null parameter");
                }
                if (out_size > 0) output.insert(output.end(), buf, buf + out_size);
                if (pres == HSDR_POLL_EMPTY) break;
            }
            continue;
        }

        if (sres != HSDR_SINK_OK) {
            heatshrink_decoder_free(dec);
            throw std::runtime_error("Decoder sink failed");
        }

        offset += sunk;

        while (true) {
            uint8_t buf[256];
            size_t out_size = 0;
            const HSD_poll_res pres = heatshrink_decoder_poll(dec, buf, sizeof(buf), &out_size);
            if (pres == HSDR_POLL_ERROR_NULL) {
                heatshrink_decoder_free(dec);
                throw std::runtime_error("Decoder poll: null parameter");
            }
            if (out_size > 0) output.insert(output.end(), buf, buf + out_size);
            if (pres == HSDR_POLL_EMPTY) break;
        }
    }

    HSD_finish_res fres;
    do {
        fres = heatshrink_decoder_finish(dec);
        if (fres == HSDR_FINISH_ERROR_NULL) {
            heatshrink_decoder_free(dec);
            throw std::runtime_error("Decoder finish: null parameter");
        }

        while (true) {
            uint8_t buf[256];
            size_t out_size = 0;
            const HSD_poll_res pres = heatshrink_decoder_poll(dec, buf, sizeof(buf), &out_size);
            if (pres == HSDR_POLL_ERROR_NULL) {
                heatshrink_decoder_free(dec);
                throw std::runtime_error("Decoder poll (finish): null parameter");
            }
            if (out_size > 0) output.insert(output.end(), buf, buf + out_size);
            if (pres == HSDR_POLL_EMPTY) break;
        }
    } while (fres == HSDR_FINISH_MORE);

    heatshrink_decoder_free(dec);
    return output;
}

std::vector<uint8_t> compressHeatshrink(const uint8_t* input, const size_t input_size) {
    heatshrink_encoder* enc = heatshrink_encoder_alloc(WINDOW_BITS, LOOKAHEAD_BITS);
    if (!enc) throw std::runtime_error("Failed to allocate heatshrink encoder");

    std::vector<uint8_t> output;
    output.reserve(input_size);

    constexpr size_t CHUNK = 256;
    size_t sunk_total = 0;

    while (sunk_total < input_size) {
        size_t sunk_now = 0;
        const size_t remaining = input_size - sunk_total;
        const size_t chunk_size = remaining < CHUNK ? remaining : CHUNK;

        if (const HSE_sink_res sres = heatshrink_encoder_sink(enc, const_cast<uint8_t*>(&input[sunk_total]), chunk_size, &sunk_now); sres != HSER_SINK_OK) throw std::runtime_error("Heatshrink sink error");

        sunk_total += sunk_now;

        while (true) {
            uint8_t buf[256];
            size_t out_size = 0;
            const HSE_poll_res pres = heatshrink_encoder_poll(enc, buf, sizeof(buf), &out_size);

            if (pres < 0) throw std::runtime_error("Heatshrink poll error");
            if (out_size > 0) output.insert(output.end(), buf, buf + out_size);
            if (pres == HSER_POLL_EMPTY) break;
        }
    }

    while (true) {
        const HSE_finish_res fres = heatshrink_encoder_finish(enc);
        if (fres < 0) throw std::runtime_error("Heatshrink finish error");

        uint8_t buf[256];
        size_t out_size = 0;
        const HSE_poll_res pres = heatshrink_encoder_poll(enc, buf, sizeof(buf), &out_size);

        if (pres < 0) throw std::runtime_error("Heatshrink poll error (finish phase)");
        if (out_size > 0) output.insert(output.end(), buf, buf + out_size);
        if (fres == HSER_FINISH_DONE && pres == HSER_POLL_EMPTY) break;
    }

    heatshrink_encoder_free(enc);
    return output;
}

std::vector<uint8_t> LoadBM(const std::string& path) {
    auto file = readFile(path);
    if (file.empty()) throw std::runtime_error("Empty BM file");

    const uint8_t flag = file[0];
    std::vector<uint8_t> result;

    if (flag == 0x00) {
        std::cout << "Uncompressed BM data\n";
        result.assign(file.begin() + 1, file.end());
    } else if (flag == 0x01) {
        std::cout << "Compressed BM data\n";
        if (file.size() < 5) throw std::runtime_error("Truncated compressed length field");
        const uint32_t comp_len = (static_cast<uint32_t>(file[1]) << 8) | static_cast<uint32_t>(file[2]) << 0;
        if (1 + comp_len > file.size()) throw std::runtime_error("Compressed data overflow");
        result = decompressHeatshrink(&file[4], comp_len);
    } else {
        throw std::runtime_error("Unknown compression flag");
    }

    return result;
}

std::vector<uint8_t> LoadBMX(const std::string& path, BmxHeader& header) {
    auto file = readFile(path);
    if (file.size() < sizeof(UncompressedBmxHeader))
        throw std::runtime_error("File too small for header");

    UncompressedBmxHeader base{};
    std::memcpy(&base, file.data(), sizeof(base));

    header.width = base.width;
    header.height = base.height;
    header.is_compressed = base.is_compressed;
    header.compressed_size = 0;

    std::vector<uint8_t> result;

    if (header.is_compressed) {
        if (file.size() < sizeof(CompressedBmxHeader))
            throw std::runtime_error("Truncated compressed header");

        CompressedBmxHeader ch{};
        std::memcpy(&ch, file.data(), sizeof(ch));
        header.compressed_size = ch.compressed_size;

        const size_t offset = sizeof(CompressedBmxHeader);
        if (offset + header.compressed_size > file.size())
            throw std::runtime_error("Compressed data overflow");

        result = decompressHeatshrink(file.data() + offset, header.compressed_size);
    } else {
        constexpr size_t offset = sizeof(UncompressedBmxHeader);
        result.assign(file.begin() + offset, file.end());
    }

    return result;
}

std::vector<uint8_t> expandBitData(const std::vector<uint8_t>& bitData, const uint32_t width, const uint32_t height) {
    std::vector<uint8_t> pixels(width * height);
    const size_t bytes_per_row = (width + 7) / 8;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t byteIndex = y * bytes_per_row + (x / 8);
            const uint8_t mask = 1 << (x % 8);
            if (byteIndex >= bitData.size()) {
                pixels[y * width + x] = 255;
                continue;
            }
            const bool bit = (bitData[byteIndex] & mask) != 0;
            pixels[y * width + x] = bit ? 0 : 255;
        }
    }
    return pixels;
}

std::vector<uint8_t> convertToBitData(const uint8_t* data, const uint32_t width, const uint32_t height) {
    const size_t bytes_per_row = (width + 7) / 8;
    std::vector<uint8_t> bitData(bytes_per_row * height, 0);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t pixel = data[y * width + x];
            const bool is_black = pixel < 128;
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
    const bool should_compress = compressedData.size() < bitData.size();

    if (should_compress) {
        CompressedBmxHeader header{};
        header.width = width;
        header.height = height;
        header.is_compressed = true;
        header._pad = 0;
        header.compressed_size = static_cast<uint16_t>(compressedData.size());

        f.write(reinterpret_cast<const char*>(&header), sizeof(header));
        f.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
    } else {
        UncompressedBmxHeader header{};
        header.width = width;
        header.height = height;
        header.is_compressed = false;

        f.write(reinterpret_cast<const char*>(&header), sizeof(header));
        f.write(reinterpret_cast<const char*>(bitData.data()), bitData.size());
    }

    return true;
}

bool convertImageToBM(const std::string& inputPath, const std::string& outputPath) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load(inputPath.c_str(), &width, &height, &channels, 1);
    if (!pixels) {
        std::cerr << "Failed to load image: " << inputPath << "\n";
        return false;
    }

    const bool ok = writeBmx(outputPath, pixels, width, height);
    stbi_image_free(pixels);
    return ok;
}

BmMeta readBmMeta(const std::string& path) {
    const auto file = readFile(path);
    if (file.size() < sizeof(BmMeta)) throw std::runtime_error("File too small for BmMeta");

    BmMeta meta{};
    std::memcpy(&meta, file.data(), sizeof(BmMeta));
    return meta;
}
