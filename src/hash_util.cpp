#include "hash_util.h"
#include <botan/hash.h>
#include <botan/hex.h>
#include <memory>

std::string compute_sha256(const std::string& input) {
    auto hash = Botan::HashFunction::create_or_throw("SHA-256");
    hash->update(input);
    return Botan::hex_encode(hash->final());
}


std::string compute_sha256_page_management(const std::vector<uint8_t>& data) {
    // Create a SHA-256 hash object
    std::unique_ptr<Botan::HashFunction> hash(Botan::HashFunction::create("SHA-256"));
    if (!hash) throw std::runtime_error("SHA-256 not available");

    hash->update(data);
    auto digest = hash->final();

    // Optional: return hex string for readability
    return Botan::hex_encode(digest);
}