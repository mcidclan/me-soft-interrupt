#pragma once
#include <psppower.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <malloc.h>
#include <cstring>
#include "kcall.h"

#define u8  unsigned char
#define u16 unsigned short int
#define u32 unsigned int

#define hwp             volatile u32*
#define hw(addr)        (*((hwp)(addr)))

#define UNCACHED_USER_MASK    0x40000000
#define CACHED_KERNEL_MASK    0x80000000
#define UNCACHED_KERNEL_MASK  0xA0000000

#define ME_HANDLER_BASE       0xbfc00000

#define setStatus(status) \
  asm volatile( \
   "mtc0   %0, $12\n" \
   "sync" \
   : \
   : "r" (status) \
  )

static inline void meDcacheWritebackInvalidateAll() {
  asm("sync");
  for (int i = 0; i < 8192; i += 64) {
    asm("cache 0x14, 0(%0)" :: "r"(i));
    asm("cache 0x14, 0(%0)" :: "r"(i));
  }
  asm("sync");
}

inline void meHalt() {
  asm volatile(".word 0x70000000");
}

inline void meGetUncached32(volatile u32** const mem, const u32 size) {
  static void* _base = nullptr;
  if (!_base) {
    const u32 byteCount = size * 4;
    _base = memalign(16, byteCount);
    memset(_base, 0, byteCount);
    sceKernelDcacheWritebackInvalidateAll();
    *mem = (u32*)(UNCACHED_USER_MASK | (u32)_base);
    __asm__ volatile (
      "cache 0x1b, 0(%0)  \n"
      "sync               \n"
      : : "r" (mem) : "memory"
    );
    return;
  } else if (!size) {
    free(_base);
  }
  *mem = nullptr;
  return;
}

