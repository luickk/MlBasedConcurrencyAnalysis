#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "QBDIPreload.h"

QBDIPRELOAD_INIT;

VMAction memory_callback(VMInstanceRef vm, GPRState *gprState, FPRState *fprState, void *data) {
  size_t memory_acc_count = 0;
  MemoryAccess *acc = qbdi_getInstMemoryAccess(vm, &memory_acc_count);
  if (memory_acc_count == 0) return QBDI_CONTINUE;
  rword addr  = acc->accessAddress;
  rword value = acc->value;
  rword size  = acc->size;

  // printf("addr: %x, val: %x, size: %llu \n", addr, value, size);

  // Continue this execution
  return QBDI_CONTINUE;
}

VMAction showInstruction(VMInstanceRef vm, GPRState *gprState,
                         FPRState *fprState, void *data) {
  // Obtain an analysis of the instruction from the VM
  const InstAnalysis *instAnalysis = qbdi_getInstAnalysis(
      vm, QBDI_ANALYSIS_INSTRUCTION | QBDI_ANALYSIS_DISASSEMBLY);

  // Printing disassembly
  printf("0x%" PRIRWORD ": %s\n", instAnalysis->address, instAnalysis->disassembly);

  return QBDI_CONTINUE;
}

VMAction countIteration(VMInstanceRef vm, GPRState *gprState,
                        FPRState *fprState, void *data) {
  (*((int *)data))++;

  return QBDI_CONTINUE;
}

static const size_t STACK_SIZE = 0x100000; // 1MB

// int main(int argc, char **argv) {
//   int n = 0;

//   int iterationCount = 0;
//   uint8_t *fakestack;
//   VMInstanceRef vm;
//   GPRState *state;
//   rword retvalue;

//   if (argc >= 2) {
//     n = atoi(argv[1]);
//   }
//   if (n < 1) {
//     n = 1;
//   }

//   // Constructing a new QBDI VM
//   qbdi_initVM(&vm, NULL, NULL, 0);

//   // Get a pointer to the GPR state of the VM
//   state = qbdi_getGPRState(vm);
//   assert(state != NULL);

//   // Setup initial GPR state,
//   qbdi_allocateVirtualStack(state, STACK_SIZE, &fakestack);

//   // Registering showInstruction() callback to print a trace of the execution
//   uint32_t cid = qbdi_addCodeCB(vm, QBDI_PREINST, showInstruction, NULL, 0);
//   assert(cid != QBDI_INVALID_EVENTID);

//   // Registering countIteration() callback
//   qbdi_addMnemonicCB(vm, "CALL*", QBDI_PREINST, countIteration, &iterationCount, 0);

//   qbdi_addMemAccessCB(vm, QBDI_MEMORY_READ_WRITE, memory_callback, NULL, 0);

//   // Setup Instrumentation Range
//   bool res = qbdi_addInstrumentedModuleFromAddr(vm, (rword)&fibonacci);
//   assert(res == true);

//   // Running DBI execution
//   printf("Running fibonacci(%d) ...\n", n);
//   res = qbdi_call(vm, &retvalue, (rword)fibonacci, 1, n);
//   assert(res == true);

//   printf("fibonnaci(%d) returns %llu after %d recursions\n", n, retvalue,
//          iterationCount);

//   // cleanup
//   qbdi_alignedFree(fakestack);
//   qbdi_terminateVM(vm);

//   return 0;
// }


int qbdipreload_on_start(void *main) {
    return QBDIPRELOAD_NOT_HANDLED;
}


int qbdipreload_on_premain(void *gprCtx, void *fpuCtx) {
    return QBDIPRELOAD_NOT_HANDLED;
}


int qbdipreload_on_main(int argc, char** argv) {
    // qbdi_addLogFilter("*", QBDI_DEBUG);
    return QBDIPRELOAD_NOT_HANDLED;
}


int qbdipreload_on_run(VMInstanceRef vm, rword start, rword stop) {
    qbdi_addMemAccessCB(vm, QBDI_MEMORY_READ_WRITE, memory_callback, NULL, 0);
    uint32_t cid = qbdi_addCodeCB(vm, QBDI_PREINST, showInstruction, NULL, 0);
    assert(cid != QBDI_INVALID_EVENTID);
    qbdi_run(vm, start, stop);
    return QBDIPRELOAD_NO_ERROR;
}


int qbdipreload_on_exit(int status) {
    return QBDIPRELOAD_NO_ERROR;
}