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
      
      // setup interrupt cleaner
      "la       $k0, %0\n"
      "li       $k1, 0xA0000000\n"
      "or       $k0, $k0, $k1\n"
      
      // make sure $ra has our ret value
      "mfc0     $k1, $14\n"
      "sync\n"
      "move     $ra, $k1\n"
      
      // load new rpc addr
      "mtc0     $k0, $14\n"
      "sync\n"
      
      // restore k0 context
      "lw       $k0, 0($sp)\n"
      "lw       $k1, 4($sp)\n"
      "addi     $sp, $sp, 16\n"
      
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
    "la       $k0, %0             \n"
    "li       $k1, 0xA0000000     \n"
    "or       $k0, $k0, $k1       \n"
    "cache    0x8, 0($k0)         \n"
    "sync                         \n"
    // load handler addr
    "mtc0   $k0, $25              \n"
    "sync                         \n"
    :
    : "i" (interruptHandler)
  );
  // enable Me interrupt on system level
  hw(0xBC300008) = 0x80000000;
  asm volatile("sync");
  // enable external interrupt on cp0 level
  setStatus(0b100 << 8 | 1);
}

int sendSoftInterrupt() {
  asm("sync");
  hw(0xBC100044) = 1;
  asm("sync");
  return 0;
}

volatile u32* mem = nullptr;
#define meCounter       mem[0]
#define meProof         mem[1]
#define meSendInterrupt mem[3]
#define meExit          mem[4]

__attribute__((noinline, aligned(4)))
static void meLoop() {
  // making sure waiting for mem
  do {
    meDcacheWritebackInvalidateAll();
  } while(!mem);
  
  meInitExceptions();
  do {
    if (meSendInterrupt) {
      sendSoftInterrupt();
      meSendInterrupt = 0;
    }
    meProof = hw(0xA0000000);
    meCounter++;
  } while(meExit == 0);
  meExit = 2;
  meHalt();
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section"), noinline, aligned(4)))
int meHandler() {
  hw(0xbc100050) = 0x7f;
  hw(0xbc100004) = 0xffffffff; // clear NMI
  hw(0xbc100040) = 2;          // allow 64MB ram
  asm("sync");
  
  asm volatile(
    "li          $k0, 0x30000000     \n"
    "mtc0        $k0, $12            \n"
    "sync                            \n"
    "la          $k0, %0             \n"
    "li          $k1, 0x80000000     \n"
    "or          $k0, $k0, $k1       \n"
    "cache       0x8, 0($k0)         \n"
    "sync                            \n"
    "jr          $k0                 \n"
    "nop\n"
    :
    : "i" (meLoop)
    : "k0"
  );  
  
  return 0;
}

static int meInit() {
  #define me_section_size (&__stop__me_section - &__start__me_section)
  memcpy((void *)ME_HANDLER_BASE, (void*)&__start__me_section, me_section_size);
  sceKernelDcacheWritebackInvalidateAll();
  hw(0xbc10004c) = 0x04;        // reset enable, just the me
  hw(0xbc10004c) = 0x0;         // disable reset to start the me
  asm volatile("sync");
  return 0;
}

static u32 cpuProof = 0;
__attribute__((noinline, aligned(4)))
int cpuInterruptHandler() {
  cpuProof++;
  return -1;
}

void meWaitExit() {
  meExit = 1;
  do {
    asm volatile("sync");
  } while(meExit < 2);
}

int main() {
  scePowerSetClockFrequency(333, 333, 166);
  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcalli.prx", PSP_MEMORY_PARTITION_KERNEL) < 0) {
    sceKernelExitGame();
    return 0;
  }
  if(kernelRegisterIntrHandler(0x1f, 1, (void*)(CACHED_KERNEL_MASK | (u32)cpuInterruptHandler)) < 0) {
    sceKernelExitGame();
  }
  
  meGetUncached32(&mem, 4);

  kcall(&meInit);
  
  pspDebugScreenInit();
  SceCtrlData ctl;
  do {
    sceKernelDcacheWritebackInvalidateAll();
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
          kcall(&sendSoftInterrupt);
        } else if (ctl.Buttons & PSP_CTRL_SQUARE){
          meSendInterrupt = 1;
        }
        up = false;
      }
    }
    sceDisplayWaitVblankStart();
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  meWaitExit();
  meGetUncached32(&mem, 0);
  
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(500000);
  sceKernelExitGame();
  return 0;
}
