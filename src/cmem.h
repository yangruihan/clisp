#ifndef __C_MEM_H_
#define __C_MEM_H_

#include "ccommon.h"

#define CALLOCATE(vm, type, count) \
    (type*)creallocate(vm, NULL, 0, sizeof(type) * (count))
#define CFREE(vm, type, p) \
    creallocate(vm, p, sizeof(type), 0)
#define CGROW_ARRAY(vm, prev, type, oldCnt, newCnt) \
    (type*)creallocate(vm, prev, sizeof(type) * (oldCnt), sizeof(type) * (newCnt))
#define CFREE_ARRAY(vm, type, p, oldCnt) \
    creallocate(vm, p, sizeof(type) * (oldCnt), 0)
#define MARK_OBJ(vm, obj) markObj((vm), (Obj*)(obj))

void* creallocate(VM* vm, void* previous, size_t oldSize, size_t newSize);
void  ccollectGarbage(VM* vm);
void  markValue(VM* vm, Value value);
void  markObj(VM* vm, Obj* obj);

#endif // __C_MEN_H_