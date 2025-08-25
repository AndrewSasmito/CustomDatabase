#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

class ContentHash {
public:
    static std::string computeHash(const std::vector<uint8_t>& data) {
        std::hash<std::string> hasher;
        std::string data_str(data.begin(), data.end());
        return std::to_string(hasher(data_str));
    }
};
