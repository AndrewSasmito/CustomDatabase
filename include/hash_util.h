#pragma once
#include <botan/hash.h>
#include <botan/hex.h>
#include <memory>
#include <string>

// Computes SHA-256 hash of the input data
std::string compute_sha256(const std::string& input);
std::string compute_sha256_page_management(const std::vector<uint8_t>& data);
