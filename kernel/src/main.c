#include "kcall.h"
#include <pspkernel.h>
#include <pspsdk.h>

PSP_MODULE_INFO("kcalli", 0x1006, 1, 1);
PSP_NO_CREATE_MAIN_THREAD();

int kcall(FCall const f) {
  return f();
}

int module_start(SceSize args, void *argp) {
  return 0;
}

int module_stop() {
  return 0;
}

int kernelRegisterIntrHandler(int arg0, int arg1, void* arg2) {
  if(sceKernelReleaseIntrHandler(arg0) < 0) {
    return -1;
  }
  if (sceKernelRegisterIntrHandler(arg0, arg1, arg2, NULL, NULL) < 0) {
    return -1;
  }
  if (sceKernelEnableIntr(arg0) < 0) {
    return -1;
  }
  return 0;
}
