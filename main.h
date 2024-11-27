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

#define vrp                  volatile u32*
#define vrg(addr)            (*((volatile u32*)(addr)))
#define uncached(var)        (*((volatile u32*)(0x40000000 | ((u32)&var))))

#define me_section_size ((&__stop__me_section - &__start__me_section + 3) & ~3)
#define _meLoop      vrg((0xbfc00040 + me_section_size))

#define setStatus(status) \
   asm volatile( \
       "mtc0   %0, $12\n" \
       "sync" \
       : \
       : "r" (status) \
   )

static inline void meDCacheWritebackInvalidAll() {
  asm("sync");
  for (int i = 0; i < 8192; i += 64) {
    asm("cache 0x14, 0(%0)" :: "r"(i));
    asm("cache 0x14, 0(%0)" :: "r"(i));
  }
  asm("sync");
}

static volatile u32 _meExit = 0;
static inline void meExit() {
  uncached(_meExit) = 1;
}
