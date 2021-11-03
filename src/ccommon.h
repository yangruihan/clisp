#ifndef __C_COMMON_H_
#define __C_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

#include "rlib.h"

#include "cconfig.h"

typedef struct sVM VM;
typedef struct sValue Value;
typedef struct sObj Obj;
typedef struct sExceptionObj ExceptionObj;

typedef Array ValueArray;
typedef Array ObjPtrArray;

typedef Value (*FuncPtr)(VM* vm, int len, Value* params, ExceptionObj** exception);

typedef enum ErrorCode
{
    EC_OK = 0,

    // scanner error
    EC_NoMatchQuote = 1,              // missing "
    EC_NoMatchRightParen = 2,         // missing )
    EC_NoMatchRightSquareBracket = 3, // missing ]
    EC_NoMatchRightCurlyBracket = 4,  // missing }
    EC_NoMatchLeftParen = 5,          // missing (
    EC_NoMatchLeftSquareBracket = 6,  // missing [
    EC_NoMatchLeftCurlyBracket = 7,   // missing {

} ErrorCode;

#define LIST_GET_CHILD(lobj, index, varName) \
    Value varName; \
    listobj_get((lobj), (index), &varName)
#define VECTOR_GET_CHILD(vobj, index, varName) \
    Value varName; \
    vectorobj_get((vobj), (index), &varName)
#define VALUE_ARR_GET_CHILD(arr, index, varName) \
    Value varName; \
    array_get((arr), index, &varName);

#define DEF_FUNC(funcName) \
    static Value funcName(VM* vm, int len, Value* params, ExceptionObj** exception)

#define THROW(fmt, ...) \
    *exception = exceptionobj_new(vm, fmt, ##__VA_ARGS__)

#define HAS_EXCEPTION() (*exception != NULL)

#ifdef DEBUG_TRACE_GC
#define VM_PUSH(obj) \
    do { \
        RLOG_DEBUG("+++++ RuntimeBlock Push Obj"); \
        vm_pushBlockRtObj(vm, ((Obj*)obj)); \
    } while (false)
#define VM_PUSHV(val) \
    do { \
        if (!value_isObj(val)) break; \
        RLOG_DEBUG("+++++ RuntimeBlock Push Value"); \
        vm_pushBlockRtObj(vm, value_asObj((val))); \
    } while (false)
#define VM_POP(obj) \
    do { \
        RLOG_DEBUG("+++++ RuntimeBlock Pop "); \
        vm_popBlockRtObj(vm, (Obj*)(obj)); \
    } while (false)
#define VM_POPV(val) \
    do { \
        if (!value_isObj((val))) break; \
        RLOG_DEBUG("+++++ RuntimeBlock Pop "); \
        vm_popBlockRtObj(vm, value_asObj((val))); \
    } while (false)
#else
#define VM_PUSH(obj) vm_pushBlockRtObj(vm, ((Obj*)obj))
#define VM_PUSHV(val) \
    do { \
        if (!value_isObj(val)) break; \
        vm_pushBlockRtObj(vm, value_asObj((val))); \
    } while (false)
#define VM_POP(obj) vm_popBlockRtObj(vm)
#define VM_POPV(val) \
    do { \
        if (!value_isObj((val))) break; \
        vm_popBlockRtObj(vm); \
    } while (false)
#endif

/* ----- Platform ----- */
#if defined(WIN32) || defined(_WIN32)
    #define C_WINDOWS 1
#endif

/* ----- Debug ----- */
#if DEBUG_TRACE
#define DTRACE(vm, fmt, ...) \
    do { \
        RLOG_TRACE(fmt, ##__VA_ARGS__); \
    } while (false)
#else
#define DTRACE(vm, fmt, ...)
#endif

#endif // __C_COMMON_H_
