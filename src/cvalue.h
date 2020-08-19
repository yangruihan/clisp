#ifndef __C_VALUE_H_
#define __C_VALUE_H_

#include "ccommon.h"

typedef enum
{
    LLV_NONE = -1,
    LLV_NIL = 0,
    LLV_BOOL = 1,
    LLV_NUMBER = 2,
    LLV_OBJ = 3
} ValueType;

struct sValue
{
    ValueType type;
    union {
        bool boolean;
        double number;
        struct sObj* obj;
    } as;
};

#define value_none()    VAL_NONE
#define value_nil()     VAL_NIL
#define value_bool(b)   (b ? VAL_TRUE : VAL_FALSE)
#define value_num(n)    ((Value){LLV_NUMBER, {.number = (n)}})
#define value_obj(o)    ((Value){LLV_OBJ, {.obj = ((Obj*)(o))}})

#define value_isNone(v) ((v).type == LLV_NONE)
#define value_isNil(v)  ((v).type == LLV_NIL)
#define value_isBool(v) ((v).type == LLV_BOOL)
#define value_isNum(v)  ((v).type == LLV_NUMBER)
#define value_isObj(v)  ((v).type == LLV_OBJ)

#define value_asBool(v) ((v).as.boolean)
#define value_asNum(v)  ((v).as.number)
#define value_asObj(v)  ((v).as.obj)

#define value_false(v)  ((value_isNil((v)) || (value_isBool((v)) && !value_asBool((v)))))
#define value_true(v)   (!value_false(v))

uint32_t        value_hash(Value v);
struct sStrObj* value_toStr(VM* vm, Value v, bool readably);
bool            value_eq(VM* vm, Value a, Value b);

void            value_print(Value v);

bool            value_isInt(Value v);
bool            value_isPair(Value v);
bool            value_symbolIs(Value v, const char* symbol);

extern const Value VAL_NONE;
extern const Value VAL_NIL;
extern const Value VAL_TRUE;
extern const Value VAL_FALSE;

#endif // __C_VALUE_H_