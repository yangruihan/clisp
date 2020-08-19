#ifndef __C_PRINTER_H_
#define __C_PRINTER_H_

#include "ccommon.h"
#include "cvalue.h"
#include "cobj.h"

#define printRawStr(vm, v) printStr(vm, v, false)
#define printReadablyStr(vm, v) printStr(vm, v, true)

StrObj* printStr(VM* vm, Value v, bool printReadably);

#endif // __C_PRINTER_H_