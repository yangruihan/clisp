#include "cvalue.h"

#include <memory.h>

#include "cutils.h"
#include "cobj.h"

const Value VAL_NONE  = {LLV_NONE, {.number = 0}};
const Value VAL_NIL   = {LLV_NIL, {.number = 0}};
const Value VAL_TRUE  = {LLV_BOOL, {.boolean = true}};
const Value VAL_FALSE = {LLV_BOOL, {.boolean = false}};

static uint32_t nilHash = -1;
static uint32_t boolTrueHash = -1;
static uint32_t boolFalseHash = -1;

uint32_t value_hash(Value v)
{
    if (value_isNil(v)) // nil
    {
        if (nilHash == -1)
            nilHash = HASH(&VAL_NIL, sizeof(Value));

        return nilHash;
    }
    else if (value_isBool(v)) // bool
    {
        if (v.as.boolean)
        {
            if (boolTrueHash == -1)
                boolTrueHash = HASH(&VAL_TRUE, sizeof(Value));
            
            return boolTrueHash;
        }
        else
        {
            if (boolFalseHash == -1)
                boolFalseHash = HASH(&VAL_FALSE, sizeof(Value));

            return boolFalseHash;
        }
    }
    else if (value_isNum(v)) // number
    {
        return v.as.number;
    }
    else // obj
    {
        return obj_hash(v.as.obj);
    }
}

StrObj* value_toStr(VM* vm, Value v, bool readably)
{
    static char s_valueStrBuff[TO_STR_BUFF_COUNT];

    if (value_isNone(v))
    {
        return strobj_copy(vm, "none", 3);
    }
    else if (value_isNil(v))
    {
        return strobj_copy(vm, "nil", 3);
    }
    else if (value_isBool(v))
    {
        return value_asBool(v) ? 
                    strobj_copy(vm, "true", 4) :
                    strobj_copy(vm, "false", 5);
    }
    else if (value_isNum(v))
    {
        if (value_isInt(v))
        {
            int size = snprintf(s_valueStrBuff, TO_STR_BUFF_COUNT, "%ld", (long)value_asNum(v));
            return strobj_copy(vm, s_valueStrBuff, size);
        }
        else
        {
            int size = snprintf(s_valueStrBuff, TO_STR_BUFF_COUNT, "%lf", value_asNum(v));
            return strobj_copy(vm, s_valueStrBuff, size);
        }
    }
    else
    {
        return obj_toStr(vm, value_asObj(v), readably);
    }
}

bool value_eq(VM* vm, Value a, Value b)
{
    if (a.type != b.type)
        return false;
    
    switch (a.type) {
        case LLV_NONE:
            return false;
            
        case LLV_NIL:
            return value_isNil(b);
            
        case LLV_BOOL:
            return value_asBool(a) == value_asBool(b);
            
        case LLV_NUMBER:
            return value_asNum(a) == value_asNum(b);
            
        case LLV_OBJ:
            return obj_eq(vm, value_asObj(a), value_asObj(b));
    }

    return false;
}

void value_print(Value v)
{
    if (value_isNil(v))
    {
        RLOG_DEBUG("<value nil>");
    }
    else if (value_isBool(v))
    {
        if (value_asBool(v))
        {
            RLOG_DEBUG("<value bool true>");
        }
        else
        {
            RLOG_DEBUG("<value bool false>");
        }
    }
    else if (value_isNum(v))
    {
        if (value_isInt(v))
        {
            RLOG_DEBUG("<value num %ld>", value_asNum(v));
        }
        else
        {
            RLOG_DEBUG("<value num %lf>", value_asNum(v));
        }
    }
    else
    {
        obj_print(value_asObj(v));   
    }
}

bool value_isInt(Value v)
{
    if (!value_isNum(v)) return false;
    double d = value_asNum(v);
    return (int)d == d;
}

bool value_isPair(Value v)
{
    if (value_isListLike(v))
    {
        ValueArray* va = value_listLikeGetArr(v);
        return va && va->count > 0;
    }

    return false;
}

bool value_symbolIs(Value v, const char* symbol)
{
    if (!value_isSymbol(v))
        return false;

    SymbolObj* sobj = value_asSymbol(v);
    return strcmp(sobj->symbol->chars, symbol) == 0;
}
