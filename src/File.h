//
// Created by capma on 6/26/2025.
//

#ifndef FILE_H
#define FILE_H
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

class File {
public:
    static std::vector<uint32_t> ReadSpirvFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << std::filesystem::absolute(filename) << std::endl;
            return {};
        }

        size_t size = file.tellg();
        if (size % 4 != 0) {
            std::cerr << "Invalid SPIR-V file: " << std::filesystem::absolute(filename) << std::endl;
            return {};
        }

        std::vector<uint32_t> buffer(size / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        file.close();
        std::cout << "Loaded SPIR-V file: " << std::filesystem::absolute(filename) << " (" << size << " bytes)" << std::endl;
        return buffer;
    }
};

#endif //FILE_H
