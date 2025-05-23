#include "hash_util.h"
#include <botan/hash.h>
#include <botan/hex.h>
#include <memory>

std::string compute_sha256(const std::string& input) {
    auto hash = Botan::HashFunction::create_or_throw("SHA-256");
    hash->update(input);
    return Botan::hex_encode(hash->final());
}