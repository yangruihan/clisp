#include "cprinter.h"

StrObj* printStr(VM* vm, Value v, bool printReadably)
{
    if (value_isNone(v))
        return NULL;
    
    return value_toStr(vm, v, printReadably);
}