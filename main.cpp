#include "main.h"

PSP_MODULE_INFO("me-interrupt", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

__attribute__((noinline, aligned(4)))
void interruptCleaner(void) {
  asm volatile(
    // save k0 context
    "addi       $sp, $sp, -16\n"
    "sw         $k0, 0($sp)\n"
    "sync\n"
    
    // clear Me interrupt flag on system level
    "li         $k0, 0x80000000\n"
    "sw         $k0, 0xbc300000($0)\n"
    "sync\n"
    
    // increment the proof on local edram (0xA0000000 | 0)
    "lw         $k0, 0xA0000000($0)\n"
    "addi       $k0, $k0, 1\n"
    "sw         $k0, 0xA0000000($0)\n"
    "sync\n"
    
    // restore the external interrupt on cp0 level
    "li         $k0, 0x401\n"
    "mtc0       $k0, $12\n"
    "sync\n"

    // restore k0 context
    "lw         $k0, 0($sp)\n"
    "addi       $sp, $sp, 16\n"
    "sync\n"
    
    // jump to inital addr
    "jr         $ra\n"
    "nop"
  );
}

__attribute__((noinline, aligned(4)))
void interruptHandler(void) {
  asm volatile(
      // clean status, disable interrupt
      "mtc0     $0, $12\n"
      "sync\n"

      // save k0 context
      "addi     $sp, $sp, -16\n"
      "sw       $k0, 0($sp)\n"
      "sw       $k1, 4($sp)\n"
      "sync\n"
      
      // setup interrupt cleaner
      "la       $k0, %0\n"
      "li       $k1, 0xA0000000\n"
      "sync\n"
      "or       $k0, $k0, $k1\n"
      "sync\n"
      
      // make sure $ra has our ret value
      "mfc0     $k1, $14\n"
      "sync\n"
      "move     $ra, $k1\n"
      "sync\n"
      
      // load new rpc addr
      "mtc0     $k0, $14\n"
      "sync\n"
      
      // restore k0 context
      "lw       $k0, 0($sp)\n"
      "lw       $k1, 4($sp)\n"
      "addi     $sp, $sp, 16\n"
      "sync\n"
      
      // exit
      "eret\n"
      
      // avoid pipeline issues
      "nop\n"
      "nop\n"
      "nop\n"
      "nop\n"
      "nop\n"
      "nop\n"
      "nop\n"
      :
      : "i" (interruptCleaner)
      : "k0", "k1", "memory"
  );
}

__attribute__((noinline, aligned(4)))
void meInitExceptions() {
  setStatus(0);
  asm volatile(
    // setup interrupt handler
    "la       $k0, %0\n"
    "li       $k1, 0xA0000000\n"
    "or       $k0, $k0, $k1\n"
    // load handler addr
    "sync\n"
    "mtc0   $k0, $25\n"
    "sync"
    :
    : "i" (interruptHandler)
  );
  // enable Me interrupt on system level
  asm("sync");
  vrg(0xBC300008) = 0x80000000;
  asm("sync");
  // enable external interrupt on cp0 level
  setStatus(0b100 << 8 | 1);
}

volatile u32* mem = nullptr;
#define meCounter       mem[0]
#define meProof         mem[1]
#define meSendInterrupt mem[3]

__attribute__((noinline, aligned(4)))
static int meLoop() {
  meDCacheWritebackInvalidAll(); // making sure mem is available
  meInitExceptions();
  do {
    if (meSendInterrupt) {
      vrg(0xBC100044) = 1;
      meSendInterrupt = 0;
    }
    meProof = vrg(0xA0000000);
    meCounter++;
  } while(!uncached(_meExit));
  return uncached(_meExit);
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section"), noinline, aligned(4)))
int meHandler() {
  asm("sync");
  vrg(0xbc100050) = 0x7f;
  vrg(0xbc100004) = 0xffffffff; // clear NMI
  vrg(0xbc100040) = 2; // allow 64MB ram
  asm("sync");
  ((FCall)_meLoop)();
  return 0;
}

static int initMe() {
  memcpy((void *)0xbfc00040, (void*)((u32)&__start__me_section), me_section_size);
  _meLoop = 0xA0000000 | (u32)meLoop;
  meDCacheWritebackInvalidAll();
  // reset and start me
  vrg(0xBC10004C) = 0b100;
  asm("sync");
  vrg(0xBC10004C) = 0x0;
  asm("sync"); 
  return 0;
}

int cpuSendInterrupt() {
  asm("sync");
  vrg(0xBC100044) = 1;
  asm("sync");
  return 0;
}

static u32 cpuProof = 0;
__attribute__((noinline, aligned(4)))
int cpuInterruptHandler() {
  cpuProof++;
  return -1;
}

int main() {
  scePowerSetClockFrequency(333, 333, 166);
  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcalli.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    sceKernelExitGame();
    return 0;
  }
  if(kernelRegisterIntrHandler(0x1f, 1, (void*)(0x80000000 | (u32)cpuInterruptHandler)) < 0) {
    sceKernelExitGame();
  }
  
  void* const _mem = (u32*)memalign(16, sizeof(u32) * 4);
  mem = (u32*)(0x40000000 | (u32)_mem);
  
  kcall(&initMe);
  pspDebugScreenInit();
  SceCtrlData ctl;
  do {
    meDCacheWritebackInvalidAll();
    sceCtrlPeekBufferPositive(&ctl, 1);
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("Me Counter %u          ", meCounter);
    pspDebugScreenSetXY(0, 2);
    pspDebugScreenPrintf("Me Interrupt Proof (Press Delta) %u          ", meProof);
    pspDebugScreenSetXY(0, 3);
    pspDebugScreenPrintf("Cpu Interrupt Proof (Press Square) %u          ", cpuProof);
    {
      static bool up = false;
      if (!ctl.Buttons) {
        up = true;
      }
      if (up && ctl.Buttons) {
        if (ctl.Buttons & PSP_CTRL_TRIANGLE) {
          kcall(&cpuSendInterrupt);
        } else if (ctl.Buttons & PSP_CTRL_SQUARE){
          meSendInterrupt = 1;
        }
        up = false;
      }
    }
    sceDisplayWaitVblankStart();
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  meExit();
  free(_mem);
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(500000);
  sceKernelExitGame();
  return 0;
}
