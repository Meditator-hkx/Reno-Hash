#pragma once

#include <time.h>
#include <stdint.h>

#define KEY_SIZE 16
#define VALUE_SIZE 15
#define DATA_LINE_SIZE 32

#define LINE_NUM_PER_BUCKET 8
#define BUCKET_NUM_PER_GROUP 4

#define BUCKET_SIZE (DATA_LINE_SIZE * LINE_NUM_PER_BUCKET)
#define GROUP_SIZE (BUCKET_SIZE * BUCKET_NUM_PER_GROUP)


#define SUPER_SIZE (sizeof(SuperMeta))
#define PAGE_SIZE 4096
#define MAGIC 12345

#define BIT_FLAG (1 << 7)
#define MIN(a, b) ((a) < (b)?(a):(b))


typedef struct {
    uint64_t magic;
    uint64_t kv_num;
    uint64_t table_size;
    uint32_t iline_num;
    uint32_t dline_num;
    uint64_t mmap_nvm_size;
    uint64_t index_off;

    // only used for rehashing
    uint32_t iline_num_2;
    uint32_t dline_num_2;
    uint64_t index_rehash_off;
    // flag = 0: old hash table
    // flag = 1: new hash table
    // data rehashed from old to new
    int rehash_flag;
    int table_in_use;
} SuperMeta;

typedef struct {
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
} __attribute__((packed)) KeyValue;

// valid state:
// 0 = NONE
// 1 = VALID (server-maintained state after processing)
// 2 = UPSERT (combination of INSERT and UPDATE, similar to SET in Memcached and Redis)
// 3 = DELETE
// 4 = RELOC (server-maintained state for relocation)
typedef struct {
    KeyValue kv;
    uint8_t valid;
} __attribute__((packed)) DataLine;


// bitmap: 1 byte (8 slots)
// pendingmap: 1 byte (8 slots)
// freecount: 1 byte (maximum 8, minimum 1 after relocation)
// local lock: 1 byte (8 slots)
// remote lock: 4 bytes (maximum 2^32-1, overflow problem can be avoided)
typedef uint64_t IndexLine;


extern SuperMeta *Super;
extern IndexLine *ILine;
extern DataLine  *DLine;

extern uint32_t IndexLineNum;
extern uint32_t IndexLineNumRehash;
