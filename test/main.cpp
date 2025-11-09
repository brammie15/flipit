#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <chrono>
#include <cstring>
#include <fstream>

#include "../src/stb_image_write.h"
#include "../src/bm_utils.h"
#include <iostream>
#include <stdexcept>

#include "../src/gif.h"

void testHeatshrinkCompression() {
    // read textfile
    std::ifstream file("assets/bee_movie.txt");
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open test file");
    }

    std::string originalText((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());


    const std::vector<uint8_t> inputData(originalText.begin(), originalText.end());

    const std::vector<uint8_t> compressedData = compressHeatshrink(inputData.data(), inputData.size());
    std::cout << "Original size: " << inputData.size() << ", Compressed size: " << compressedData.size() << "\n";

    const std::vector<uint8_t> decompressedData = decompressHeatshrink(compressedData.data(), compressedData.size());
    std::cout << "Decompressed size: " << decompressedData.size() << "\n";

    assert(decompressedData.size() == inputData.size());
    assert(std::memcmp(decompressedData.data(), inputData.data(), inputData.size()) == 0);

    std::cout << "Heatshrink compression/decompression test passed!\n";
}

// int main(int argc, char** argv) {
//     //timer
//     std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
//     testHeatshrinkCompression();
//     std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
//
//     std::cout << "Time taken: "
//               << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
//               << " ms\n";
//
//     std::string input_file = (argc > 1) ? argv[1] : "assets/NFC_dolphin_emulation_51x64.bmx";
//     std::string output_file = (argc > 2) ? argv[2] : "output10.png";
//
//     BmxHeader info{};
//     try {
//         const auto bitData = LoadBMX(input_file, info);
//         const auto pixels = expandBitData(bitData, info.width, info.height);
//
//         std::cout << "Decompressed data size: " << info.compressed_size << " bytes\n";
//
//         if (!stbi_write_png("out.png", info.width, info.height, 1, pixels.data(), info.width)) {
//             throw std::runtime_error("Failed to write PNG");
//         }
//
//         std::cout << "Saved PNG as out.png\n";
//     } catch (const std::exception &e) {
//         std::cerr << "Error: " << e.what() << "\n";
//         return 1;
//     }
// }

// int main() {
//     try {
//         if (convertImageToBM("out.png", "output.bmx", true))
//             std::cout << "Conversion successful!\n";
//         else
//             std::cerr << "Conversion failed.\n";
//     } catch (const std::exception &e) {
//         std::cerr << "Error: " << e.what() << "\n";
//     }
// }

// int main(int argc, char** argv) {
//     //timer
//
//     std::string input_file = (argc > 1) ? argv[1] : "assets/frame_00.bm";
//     std::string output_file = (argc > 2) ? argv[2] : "output11.png";
//
//     std::string meta_file = "assets/Loading_24/meta";
//     const BmMeta meta = readBmMeta(meta_file);
//     std::cout << "BM Meta - Width: " << meta.width << ", Height: " << meta.height
//               << ", Frame Rate: " << meta.frame_rate << ", Frame Count: " << meta.frame_count << "\n";
//
//     std::vector<std::string> bm_files;
//     for (int i = 0; i < meta.frame_count; ++i) {
//         char filename[256];
//         std::snprintf(filename, sizeof(filename), "assets/Loading_24/frame_%02d.bm", i);
//         bm_files.push_back(filename);
//     }
//
//     for (int index{ 0 }; index < bm_files.size(); ++index) {
//         const std::vector<uint8_t> bitData = LoadBM(bm_files[index]);
//         const int width = static_cast<int>(meta.width);
//         const int height = static_cast<int>(meta.height);
//         const auto pixels = expandBitData(bitData, width, height);
//         char output_filename[256];
//         std::snprintf(output_filename, sizeof(output_filename), "output_frames/frame_%02d.png", index);
//         if (!stbi_write_png(output_filename, width, height, 1, pixels.data(), width)) {
//             throw std::runtime_error("Failed to write PNG");
//         }
//         std::cout << "Saved PNG as " << output_filename << "\n";
//     }
//
//     return 0;
// }

int main(int argc, char** argv) {
    std::string input_file = (argc > 1) ? argv[1] : "assets/Lockscreen.bmx";
    std::string output_file = (argc > 2) ? argv[2] : "lockscreen.png";

    BmxHeader info{};
    const auto bitData = LoadBMX(input_file, info);
    const auto pixels = expandBitData(bitData, info.width, info.height);

    std::cout << "Decompressed data size: " << info.compressed_size << " bytes\n";

    if (!stbi_write_png(output_file.c_str(), info.width, info.height, 1, pixels.data(), info.width)) {
        throw std::runtime_error("Failed to write PNG");
    }

    std::cout << "Saved PNG as " << output_file << "\n";


    std::string test_output = "recompressed_lockscreen.bmx";
    if (writeBmx(test_output, pixels.data(), info.width, info.height)) {
        std::cout << "Recompressed and saved BMX as " << test_output << "\n";
    } else {
        std::cerr << "Failed to write BMX file\n";
    }

    return 0;
}
