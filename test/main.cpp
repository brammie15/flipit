#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include "bm_utils.h"

#include "stb_image_write.h"

void printUsage() {
    std::cout << "Usage:\n"
              << "  tool bmx2png <input.bmx> [output.png]  - Convert BMX to PNG\n"
              << "  tool png2bmx <input.png> [output.bmx]  - Convert PNG to BMX\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];

    try {
        if (command == "bmx2png") {
            const std::string input_file = argv[2];
            const std::string output_file = (argc > 3) ? argv[3] : std::filesystem::path(input_file).stem().string() + ".png";

            BmxHeader info{};
            const auto bitData = LoadBMX(input_file, info);
            const auto pixels = expandBitData(bitData, info.width, info.height);

            std::cout << "width: " << info.width << " height: " << info.height << std::endl;
            std::cout << "is compressed: " << (info.is_compressed ? "true" : "false") << std::endl;

            if (!stbi_write_png(output_file.c_str(), static_cast<int>(info.width), static_cast<int>(info.height), 1, pixels.data(), info.width)) {
                throw std::runtime_error("Failed to write PNG");
            }

            std::cout << "Saved PNG as " << output_file << "\n";

        } else if (command == "png2bmx") {
            const std::string input_file = argv[2];
            const std::string output_file = (argc > 3) ? argv[3] : std::filesystem::path(input_file).stem().string() + ".bmx";

            if (!convertImageToBM(input_file, output_file)) {
                std::cerr << "Conversion failed\n";
                return 1;
            }

            std::cout << "Converted PNG to BMX: " << output_file << "\n";

        } else {
            printUsage();
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
//
// int main(int argc, char** argv) {
//     std::string input_file = "assets/Lockscreen.bmx";
//     std::string output_file = "lockscreen.png";
//
//     BmxHeader info{};
//     const auto bitData = LoadBMX(input_file, info);
//     const auto pixels = expandBitData(bitData, info.width, info.height);
//
//     std::cout << "Decompressed data size: " << info.compressed_size << " bytes\n";
//
//     if (!stbi_write_png(output_file.c_str(), info.width, info.height, 1, pixels.data(), info.width)) {
//         throw std::runtime_error("Failed to write PNG");
//     }
//
//     std::cout << "Saved PNG as " << output_file << "\n";
//
//     std::string test_output = "recompressed_lockscreen.bmx";
//     if (writeBmx(test_output, pixels.data(), info.width, info.height)) {
//         std::cout << "Recompressed and saved BMX as " << test_output << "\n";
//     } else {
//         std::cerr << "Failed to write BMX file\n";
//     }
//
//     return 0;
// }
