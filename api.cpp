
#include "config.h"
#include "api.h"
#include "storage.h"
#include "ib.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

void server_init(uint32_t index_num) {
    // mmap super metadata
    int fd = open(PM_PATH, O_RDWR | O_CREAT);
    fd = -1;
    if (fd == -1) {
        Super = (SuperMeta *)mmap(0, SUPER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    }
    else
    {
        Super = (SuperMeta *)mmap(0, SUPER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
    
    goto debug;

    // normal recover
    if (Super->magic == MAGIC && Super->iline_num == index_num) {
        if (fd == -1)
            ILine = (IndexLine *)mmap(0, Super->table_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, Super->index_off);
        else
            ILine = (IndexLine *)mmap(0, Super->table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, Super->index_off);
        DLine = (DataLine *)(ILine + index_num);
        Super->magic = MAGIC - 1;
    }
    // for evaluation, this will never be called
    // crash recovery
    else if (Super->magic == MAGIC - 1 && Super->iline_num == index_num) {
        Super->magic = 0;
        if (fd == -1)
            ILine = (IndexLine *)mmap(0, Super->table_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, Super->index_off);
        else
            ILine = (IndexLine *)mmap(0, Super->table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, Super->index_off);
        DLine = (DataLine *)(ILine + index_num);
        server_recover(index_num);
        Super->magic = MAGIC - 1;
    }
    // allocate space for the first time
    else {
debug:
        Super->table_size = (index_num + BUCKET_NUM_PER_GROUP - 1) * 8;
        Super->table_size += (index_num + BUCKET_NUM_PER_GROUP - 1) * LINE_NUM_PER_BUCKET * DATA_LINE_SIZE;
        if (fd == -1)
            ib_res.buf = (char *)mmap(0, Super->table_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, PAGE_SIZE);
        else
            ib_res.buf = (char *)mmap(0, Super->table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PAGE_SIZE);
        setup_ib();
        ILine = (IndexLine *)ib_res.buf;
        DLine = (DataLine *)(ILine + index_num + BUCKET_NUM_PER_GROUP - 1);
        Super->iline_num = index_num;
        IndexLineNum = index_num;
        Super->dline_num = Super->iline_num * LINE_NUM_PER_BUCKET;
        Super->index_off = PAGE_SIZE;
        Super->kv_num = 0;
        Super->table_in_use = 0;
        Super->rehash_flag = 0;
        
        index_init();

        Super->magic = MAGIC - 1;
    }
}

void index_init() {
    IndexLine *iLine = ILine;
    uint8_t *free_count;
    for (uint32_t i = 0;i < Super->iline_num + BUCKET_NUM_PER_GROUP - 1;i++) {
        *iLine = 0;
        free_count = ((uint8_t *)iLine + 5);
        *free_count = LINE_NUM_PER_BUCKET;
        iLine++;
    }
}

void server_exit() {
    Super->magic = MAGIC;
    munmap(Super, SUPER_SIZE);
    munmap(ILine, Super->table_size);
}

void server_recover(uint32_t index_num) {
    for (uint32_t i = 0;i < index_num;i++) {
        deal_pending(i);
    }
}

int server_search(char *key, char *value) {
    // 1. hash to corresponding data bucket
    uint32_t bucket_off = server_hash(key);
    DataLine *dLine = DLine + bucket_off * LINE_NUM_PER_BUCKET;
    int exist_flag = 0;

    // search in the first bucket
    // (it is special because it is the primary hashed bucket for clients)
    for (int i = 0;i < LINE_NUM_PER_BUCKET;i++) {
        if (strcmp(dLine->kv.key, key) == 0) {
            if (dLine->valid == 3) {
                exist_flag = 3;
            }
            else if (dLine->valid == 2) {
                strcpy(value, dLine->kv.value);
                exist_flag = 2;
            }
            else if (dLine->valid == 1) {
                if (exist_flag < 1) {
                    strcpy(value, dLine->kv.value);
                    exist_flag = 1;
                }
            }
        }
        dLine++;
    }

    if (exist_flag == 3) {
        return -1;
    }
    else if (exist_flag >= 1) {
        return 0;
    }

    // if exist_flag == 0, continue
    for (int i = 1;i < BUCKET_NUM_PER_GROUP;i++) {
        for (int j = 0;j < LINE_NUM_PER_BUCKET;j++) {
            if (strcmp(dLine->kv.key, key) == 0 && dLine->valid == 1) {
                strcpy(value, dLine->kv.value);
                return 0;
            }
            dLine++;
        }
    }

    // no key-value information in the entire group
    return -1;
}

int search_old(char *key, uint32_t &iline_off, uint32_t &dline_off) {
    // hash to corresponding data bucket
    uint32_t bucket_off = server_hash(key);
    DataLine *dLine = DLine + bucket_off * LINE_NUM_PER_BUCKET;

    for (int i = 0;i < BUCKET_NUM_PER_GROUP;i++) {
        for (int j = 0;j < LINE_NUM_PER_BUCKET;j++) {
            if ((strcmp(dLine->kv.key, key) == 0) && dLine->valid == 1) {
                iline_off = bucket_off + i;
                dline_off = j;
                return 0;
            }
            dLine++;
        }
    }
    return -1;
}

int server_insert(char *key, char *value) {
    // 1. hash to corresponding data bucket
    uint32_t bucket_off = server_hash(key);
    IndexLine *iLine = ILine + bucket_off;
    DataLine *dLine = DLine + bucket_off * LINE_NUM_PER_BUCKET;


    uint8_t *bitmap;
    uint8_t *free_count;
    uint8_t *llock;
    uint32_t *rlock;

    // lock corresponding bucket (or its neighbour bucket when specified bucket is full)
    for (int i = 0;i < BUCKET_NUM_PER_GROUP;i++) {
        bitmap = (uint8_t *)iLine + 7;
        free_count = (uint8_t *)iLine + 5;
        llock = (uint8_t *)iLine + 4;
        rlock = (uint32_t *)iLine;
    
        // if (free_count > llock + rlock)
        if (*free_count > *llock + *rlock) {
            // update index bucket atomically
            *llock += 1;
            // add data to the locked bucket
            // step 1. make sure which slot is able to use
            int target_slot = -1;
            int count_llock = 0;
            for (int j = LINE_NUM_PER_BUCKET - 1; j >= 0;j--) {
                if ((*bitmap & (BIT_FLAG >> j)) == 0) {
                    count_llock++;
                    if (count_llock == *llock) {
                        target_slot = j;
                        break;
                    }
                }
            }

            if (target_slot == -1) {
                *llock -= 1;
                return -1;
            }

            // step 3. write key-value data to that slot
            dLine += target_slot;
            strcpy(dLine->kv.key, key);
            strcpy(dLine->kv.value, value);
            dLine->valid = 1; // insert operation

            // step 4. if there is need to deal with the whole bucket
            if (*free_count <= *llock + *rlock) {
                deal_pending(bucket_off);
            }

            return 0;
        }

        else {
            iLine++;
            dLine += LINE_NUM_PER_BUCKET;
        }
    }

    return -1;
}


void server_check() {
    // propose a recv request
    int ret;
    for (int i = 0;i < NODE_NUM;i++) {
        ret = rdma_srq_recv();
        if (ret == -1) {
            exit(-1);
        }
    }

    while(1) {
        // post recv request for immediate
        int flag = 0;
        uint32_t imm_data;
        ret = post_cq(flag, imm_data);
        if (ret == -1) {
            printf("fail to poll recv.\n");
            exit(-1);
        }
        else {
            // supplement a recv request
            ret = rdma_srq_recv();
            if (ret == -1) {
                printf("fail to poll recv.\n");
                exit(-1);
            }
            deal_pending(imm_data);
        }
    }
}


int findRelocateSlot(uint32_t off, int llock_off) {
    IndexLine *iLine = ILine + off;
    DataLine *dLine = DLine + off * LINE_NUM_PER_BUCKET;

    uint8_t *bitmap = (uint8_t *)iLine + 7;

    for (int i = LINE_NUM_PER_BUCKET-1;i >= 0;i--) {
        if ((*bitmap & (BIT_FLAG >> i)) == 0) {
            // only insert (relocate) will occur
            llock_off--;
            if (llock_off == 0) {
                return i;
            }
        }
    }

    return -1;
}

int relocate(uint32_t off, uint64_t &index_tmp) {
    DataLine *dLine = DLine + off * LINE_NUM_PER_BUCKET;
    IndexLine *tIndex;
    DataLine *tLine;
    uint32_t hash_off;

    uint8_t *bitmap = (uint8_t *)&index_tmp + 7;
    uint8_t *pendingmap = (uint8_t *)&index_tmp + 6;
    uint8_t *freecount = (uint8_t *)&index_tmp + 5;
    uint8_t *llock = (uint8_t *)&index_tmp + 4;
    uint32_t *rlock = (uint32_t *)&index_tmp;

    // count how many keys should be migrated: 4 - free_count
    int migrate_count = 2;
    for (int i = 0;i < LINE_NUM_PER_BUCKET;i++) {
        // choose a key and its corresponding bucket & neighbour
        if (dLine->valid == 1) {
            hash_off = server_hash(dLine->kv.key);
            for (int j = 0;j < BUCKET_NUM_PER_GROUP;j++) {
                if (hash_off + j == off)
                    continue;
                // check the index of its neighbour and compute free slots
                tIndex = ILine + hash_off + j;
                bitmap = (uint8_t *)tIndex + 7;
                freecount = (uint8_t *)tIndex + 5;
                llock = (uint8_t *)tIndex + 4;
                rlock = (uint32_t *)tIndex;

                if (*freecount - *llock - *rlock > 2) {
                    // if possible, lock one free slot using llock
                    *llock += 1;
                    int target_off = findRelocateSlot(hash_off + j, *llock);
                    if (target_off == -1) {
                        *llock -= 1;
                        return -1;
                    }

                    tLine = DLine + (hash_off + j) * LINE_NUM_PER_BUCKET + target_off;
                    // migrate data to corresponding data slot
                    memcpy(&tLine->kv, &dLine->kv, sizeof(KeyValue));
                    // set the valid bit to be 1
                    tLine->valid = 1;
                    // delete in local bucket
                    dLine->valid = 0;
                    // reset index slot
                    *((uint8_t *)&index_tmp + 7) &= (~BIT_FLAG >> i);   // bitmap 1->0
                    *((uint8_t *)&index_tmp + 5) += 1;                  // free_count++

                    migrate_count--;
                    break;
                }
            }
        }

        // loop for next key to migrate
        if (migrate_count == 0) {
            return 0;
        }
        dLine++;
    }

    return -1;
}

void deal_pending(uint32_t off) {
    IndexLine *iLine = ILine + off;
    DataLine *dLine;

    uint8_t *bitmap = (uint8_t *)iLine + 7;
    uint8_t *pendingmap = (uint8_t *)iLine + 6;
    uint8_t *freecount = (uint8_t *)iLine + 5;
    uint8_t *llock = (uint8_t *)iLine + 4;
    uint32_t *rlock = (uint32_t *)iLine;
    uint32_t rlock_real = MIN(*freecount-*llock, *rlock);
    uint32_t llock_real = *llock;
    IndexLine tmp = *iLine;

    // step 1: deal with remote locks
    // ordered lookup for free bits
    for (int i = 0;i < LINE_NUM_PER_BUCKET;i++) {
        if (rlock_real <= 0) {
            break;
        }

        if ((*bitmap & (BIT_FLAG >> i)) == 0) {
            dLine = DLine + off * LINE_NUM_PER_BUCKET + i;
            // 1. UPSERT
            if (dLine->valid == 2) {
                uint32_t iline_off, dline_off;
                int ret = search_old(dLine->kv.key, iline_off, dline_off);
                // UPSERT is an INSERT
                if (ret == -1) {
                    dLine->valid = 1;
                    *((uint8_t *)&tmp + 7) = *((uint8_t *)&tmp + 7) | (BIT_FLAG >> i);
                    *((uint8_t *)&tmp + 5) = *((uint8_t *)&tmp + 5) - 1;
                }
                // UPSERT is an UPDATE
                // old and new in the same bucket
                else if (iline_off == off) {
                    DataLine *oldLine = DLine + iline_off * LINE_NUM_PER_BUCKET + dline_off;
                    oldLine->valid = 0;
                    dLine->valid = 1;
                    // bitmap 0->1
                    // bitmap 1->0
                    // freecount--
                    *((uint8_t *)&tmp + 7) = *((uint8_t *)&tmp + 7) | (BIT_FLAG >> i);
                    *((uint8_t *)&tmp + 5) = *((uint8_t *)&tmp + 5) - 1;
                }
                // UPSERT is an UPDATE
                // old and new in different buckets
                else {
                    DataLine *oldLine = DLine + iline_off * LINE_NUM_PER_BUCKET + dline_off;
                    oldLine->valid = 0;
                    dLine->valid = 1;
                    *((uint8_t *)&tmp + 7) = *((uint8_t *)&tmp + 7) | (BIT_FLAG >> i);
                    *((uint8_t *)&tmp + 5) = *((uint8_t *)&tmp + 5) - 1;
                }
            }
            // 2. DELETE
            else if (dLine->valid == 3) {
                uint32_t iline_off, dline_off;
                int ret = search_old(dLine->kv.key, iline_off, dline_off);
                if (ret == -1) {
                    // ignore
                    dLine->valid = 0;
                }
                // old and new in the same bucket
                else if (iline_off == off) {
                    DataLine *oldLine = DLine + iline_off * LINE_NUM_PER_BUCKET + dline_off;
                    oldLine->valid = 0;
                    dLine->valid = 0;
                    // bitmap 1->0
                    // freecount++;
                    *((uint8_t *)&tmp + 7) = *((uint8_t *)&tmp + 7) & (~BIT_FLAG >> dline_off);
                    *((uint8_t *)&tmp + 5) = *((uint8_t *)&tmp + 5) - 1;
                }
                // old and new in different buckets
                else {
                    DataLine *oldLine = DLine + iline_off * LINE_NUM_PER_BUCKET + dline_off;
                    oldLine->valid = 0;
                    dLine->valid = 0;
                }
            }
            rlock_real--;
        }
    }

    // step 2: deal with local locks
    // notice that local lock will only be used when relocation is processed
    // reverse lookup for free bits
    for (int i = LINE_NUM_PER_BUCKET-1;i >= 0;i--) {
        if (llock_real <= 0) {
            break;
        }
        dLine = DLine + off * LINE_NUM_PER_BUCKET + i;
        if ((*bitmap & (BIT_FLAG >> i)) == 0) {
            // only insert (relocate) will occur
            if (dLine->valid == 1) {
                // bitmap 0->1
                // freecount--
                *((uint8_t *)&tmp + 7) = *((uint8_t *)&tmp + 7) | (BIT_FLAG >> i);
                *((uint8_t *)&tmp + 5) = *((uint8_t *)&tmp + 5) - 1;
            }
            llock_real--;
        }
    }

    // step 3. scan all inconsistent data slots
    // in bitmap is 1 but its valid is 0
    for (int i = 0;i < LINE_NUM_PER_BUCKET;i++) {
        if ((*bitmap & (BIT_FLAG >> i)) > 0) {
            dLine = DLine + off * LINE_NUM_PER_BUCKET + i;
            if (dLine->valid == 0) {
                // bitmap 1->0
                // freecount++;
                *((uint8_t *)&tmp + 7) = *((uint8_t *)&tmp + 7) & (~BIT_FLAG >> i);
                *((uint8_t *)&tmp + 5) = *((uint8_t *)&tmp + 5) + 1;
            }
        }
    }

    // step 3.5 relocate data slots if current bucket is heavy-loaded (such as 6/8)
    int free_count = *((uint8_t *)&tmp + 5);
    if (free_count < 2) {
        // relocate some key-value items
        relocate(off, tmp);
    }

    // step 4. finally, atomically update index slot
    *((uint8_t *)&tmp + 4) = 0;
    *(uint32_t *)&tmp = 0;
    *iLine = tmp;
}

int client_search(int order, char *key, char *value) {
    int ret;

    // 2.1 prepare read request
    uint32_t bucket_off = server_hash(key);
    char *remote_addr = (char *)(DLine + bucket_off * LINE_NUM_PER_BUCKET);

    // 2.4 post the read request (read->write)
    ret = rdma_read(order, remote_addr, GROUP_SIZE, ib_res.read_buf + GROUP_SIZE * order);
    if (ret != 0) {
        printf("fail to post read request.\n");
        return -1;
    }

    // 2.5 poll for read signal success
    int flag = 1;   // poll send
    uint32_t imm_data;
    ret = post_cq(flag, imm_data);
    if (ret == -1) {
        printf("fail to poll wc.\n");
        return -1;
    }

    // 2.6 read through the read_buffer for the specified key
    DataLine *dLine = (DataLine *)(ib_res.read_buf + GROUP_SIZE * order);
    // EXIST_FLAG =
    int exist_flag = 0;

    // search in the first bucket (it is special because it is the only remote bucket for clients)
    for (int i = 0;i < LINE_NUM_PER_BUCKET;i++) {
        if (strcmp(dLine->kv.key, key) == 0) {
            switch(dLine->valid) {
                // VALID or RELOC
                case 1:
                case 4:
                    if (exist_flag == 0) {
                        strcpy(value, dLine->kv.value);
                        exist_flag = 1;
                    }
                    break;
                // UPSERT
                case 2:
                    strcpy(value, dLine->kv.value);
                    exist_flag = 2;
                    break;
                // DELETE
                case 3:
                    exist_flag = 3;
                    break;
                // NONE
                default:
                    break;
            }
        }
        dLine++;
    }

    if (exist_flag == 3) {
        return -1;
    }
    else if (exist_flag >= 1) {
        return 0;
    }

    // if exist_flag == 0, continue
    for (int i = 1;i < BUCKET_NUM_PER_GROUP;i++) {
        for (int j = 0;j < LINE_NUM_PER_BUCKET;j++) {
            if (strcmp(dLine->kv.key, key) == 0 && dLine->valid == 1) {
                strcpy(value, dLine->kv.value);
                return 0;
            }
            dLine++;
        }
    }

    // no key-value information in the entire group
    return -1;
}

// no INSERT operation now
// opcode = 2: UPSERT
// opcode = 3: DELETE
int client_write(int order, char *key, char *value, int opcode) {
    int ret;
    // 1. hash to remote bucket
    uint32_t bucket_off = server_hash(key);

    // 2. lock remote bucket
    // 2.1 post atomic faa request
    char *remote_addr = (char *)(ILine + bucket_off);
    uint32_t imm_data = 0;
    ret = rdma_write(order, remote_addr, 8, ib_res.recv_buf + BUCKET_SIZE * order, 2, imm_data);
    if (ret == -1) {
        printf("fail to post atomic request.\n");
        return -1;
    }

    // 2.2 poll for atomic request signal success
    int flag = 1;
    ret = post_cq(flag, imm_data);
    if (ret == -1) {
        printf("fail to poll lock wc.\n");
        return -1;
    }

    // 3. check metadata information
    // 3.1 index division
    uint64_t *meta = (uint64_t *)ib_res.recv_buf + BUCKET_SIZE * order;
    uint8_t *bitmap = (uint8_t *)meta + 7;
    uint8_t *pendingmap = (uint8_t *)meta + 6;
    uint8_t *freecount = (uint8_t *)meta + 5;
    uint8_t *llock = (uint8_t *)meta + 4;
    uint32_t *rlock = (uint32_t *)meta;

    if (*freecount <= *llock + *rlock) {
        printf("no free slot in this bucket, please retry later.\n");
        return -1;
    }

    // 3.2 find the kth free bit in current bucket, k = rlock + 1
    DataLine *dLine = DLine + LINE_NUM_PER_BUCKET * bucket_off;
    int skipcount = *rlock;
    int target_slot = -1;
    for (int i = 0;i < LINE_NUM_PER_BUCKET;i++) {
        if ((*bitmap & (BIT_FLAG >> i)) == 0) {
            skipcount--;
            if (skipcount < 0) {
                target_slot = i;
                break;
            }
        }
    }
    if (target_slot == -1) {
        printf("cannot find a free slot, metadata error!\n");
        return -1;
    }

    // 4. write to remote data slot
    remote_addr = (char *)(dLine + target_slot);

    // 4.1 set attributes of write work request
    DataLine *sendline = (DataLine *)malloc(DATA_LINE_SIZE);
    strcpy(sendline->kv.key, key);
    strcpy(sendline->kv.value, value);
    sendline->valid = (uint8_t)opcode;

    // 4.2 write
    flag = 0;
    if (*freecount - *llock - *rlock <= 1) {
        flag = 1;
    } 
    ret = rdma_write(order, remote_addr, DATA_LINE_SIZE, (char *)sendline, flag, bucket_off);
    if (ret != 0) {
        printf("cannot post write request.");
        return -1;
    }

    // 4.3 poll for write signal success
    flag = 1;     // poll send
    ret = post_cq(flag, imm_data);
    if (ret == -1) {
        printf("fail to poll write wc.\n");
        return -1;
    }

    return 0;
}

int client_insert(int order, char *key, char *value) {
    int opcode = 1;
    return client_write(order, key, value, opcode);
}

int client_update(int order, char *key, char *value) {
    int opcode = 2;
    return client_write(order, key, value, opcode);
}

int client_upsert(int order, char *key, char *value) {
    int opcode = 2;
    return client_write(order, key, value, opcode);
}

int client_del(int order, char *key) {
    int opcode = 3;
    char *value = (char *)"null";
    return client_write(order, key, value, opcode);
}
