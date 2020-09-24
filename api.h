#pragma once

#include "storage.h"
#include "hash.h"

int search(char *key, char *value);
int insert(char *key, char *value);
int update(char *key, char *value);
int del(char *key);

// called only by server
int findRelocateSlot(uint32_t off, int llock_off);
int relocate(uint32_t off, uint64_t &index_tmp);
int server_search(char *key, char *value);
int search_old(char *key, uint32_t &iline_off, uint32_t &dline_off);
int server_insert(char *key, char *value);
void server_init(uint32_t index_num);
void index_init();
void server_exit();
void server_recover(uint32_t index_num);

void server_check();
void deal_pending(uint32_t off);

// called only by client
int cache_search(char *key, char *value);   // ignore at first
int client_write(int order, char *key, char *value, int opcode);
int client_search(int order, char *key, char *value);
int client_upsert(int order, char *key, char *value);
int client_del(int order, char *key);
