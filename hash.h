#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "storage.h"

#define DATA_SEED "dataseed"

uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
uint64_t siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k);


uint32_t server_hash(const char *key);
uint32_t server_hash_2(const char *key);
