#include "hash.h"

std::hash<std::string> hasher;

uint32_t server_hash(const char *key) {
    return (uint32_t)hasher(key) % IndexLineNum;
}

uint32_t server_hash_2(const char *key) {
    uint32_t ret = 0;
    ret = (uint32_t)siphash((uint8_t *)key, strlen(key), (uint8_t *)DATA_SEED);
    return (ret % IndexLineNumRehash - 1);
}
