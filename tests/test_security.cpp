//
// Created by Neuroil on 2026/8/14.
//

#include "common/security.h"

int main() {
    std::vector<std::byte> passwd, salt, buf;
    passwd.resize(32);
    salt.resize(16);
    buf.resize(EVP_MAX_MD_SIZE);
    security::salt_hash_gen(passwd, salt, buf, 10000);
}