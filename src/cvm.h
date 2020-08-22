#ifndef __C_VM_H_
#define __C_VM_H_

#include <string.h>

#include "ccommon.h"
#include "cobj.h"

struct sVM
{
    Table   strings;
    EnvObj* env;
    EnvObj* currentEnv;

    int callDepth;
    Array closureStackIdx;
    Array closureStackCallDepth;
    Array closureStack;

    /* ---- gc ----- */
    Obj*        objs;           // all obj chain
    size_t      bytesAllocated; // current allocated size
    size_t      nextGC;         // next gc size
    ObjPtrArray grayObjArray;   // mark gray obj array
    ObjPtrArray cmBlockArray;   // compile block array
    ObjPtrArray rtblockArray;   // runtime block array
};

VM*         vm_create();
void        vm_free(VM* vm);
const char* vm_rep(VM* vm, const char* input);
Value       vm_eval(VM* vm, Value value, EnvObj* env, ExceptionObj** exception);
void        vm_dofile(VM* vm, const char* filePath, int argc, char** argv);

#define     VM_REGISTER_FUNC(vm, funcName, funcPtr) \
    vm_registerFunc((vm), (funcName), (strlen((funcName))), (funcPtr))
void        vm_registerFunc(VM* vm, const char* funcName, const int nameLen, FuncPtr funcPtr);

/* ---- gc ----- */

void        vm_pushBlockCmObj(VM* vm, Obj* obj);
void        vm_popBlockCmObj(VM* vm);
void        vm_clearBlockCmArr(VM* vm);

void        vm_pushBlockRtObj(VM* vm, Obj* obj);
#ifdef DEBUG_TRACE_GC
void        vm_popBlockRtObj(VM* vm, Obj* obj);
#else
void        vm_popBlockRtObj(VM* vm);
#endif
void        vm_clearBlockRtArr(VM* vm);

#endif // __C_VM_H_