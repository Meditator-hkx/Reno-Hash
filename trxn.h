# pragma once

#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>
#include <mutex>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "storage.h"


#define FLUSH_METHOD 1

//void sfence();
//void mfence();
//void flush(void *addr);


#define sfence()                \
({                      \
    __asm__ __volatile__ ("mfence":::"memory");    \
})

#define flush(addr)                   \
({                              \
    __asm__ __volatile__ ("clflush %0" : : "m"(*addr)); \
})

//inline void sfence() {
//    _mm_sfence();
//}

//inline void mfence() {
//    _mm_mfence();
//}

//inline void flush(void *addr) {
//#if FLUSH_METHOD == 1
//    _mm_clwb(addr);
//#elif FLUSH_METHOD == 2
//    _mm_clflushopt(addr);
//#else
//    _mm_clflush(addr);
//#endif
//}
