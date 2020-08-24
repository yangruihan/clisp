#ifndef __C_OBJ_H_
#define __C_OBJ_H_

#include <stdarg.h>

#include "ccommon.h"
#include "cvalue.h"

#define ESCAPE_DATA_LEN 11

typedef struct sEscapeData
{
    char  originChar;
    char* tChars;
} EscapeData;

extern EscapeData escapeCharDatas[ESCAPE_DATA_LEN];

#define obj_asStr(o)       ((StrObj*)o)
#define obj_asList(o)      ((ListObj*)o)
#define obj_asSymbol(o)    ((SymbolObj*)o)
#define obj_asKeyword(o)   ((KeywordObj*)o)
#define obj_asVector(o)    ((VectorObj*)o)
#define obj_asMap(o)       ((MapObj*)o)
#define obj_asFunc(o)      ((FuncObj*)o)
#define obj_asEnv(o)       ((EnvObj*)o)
#define obj_asClosure(o)   ((ClosureObj*)o)
#define obj_asAtom(o)      ((AtomObj*)o)
#define obj_asException(o) ((ExceptionObj*)o)

typedef enum
{
    LLO_STRING    = 3,
    LLO_LIST      = 4,
    LLO_SYMBOL    = 5,
    LLO_KEYWORD   = 6,
    LLO_VECTOR    = 7,
    LLO_MAP       = 8,
    LLO_FUNCTION  = 9,
    LLO_ENV       = 10,
    LLO_CLOSURE   = 11,
    LLO_ATOM      = 12,
    LLO_EXCEPTION = 13,
} ObjType;

struct sObj
{
    ObjType  type;
    bool     isMarked;
    uint32_t hash;

    struct sObj* next;
};

typedef struct sMetaObj
{
    Obj   base;
    Value meta;
} MetaObj;

typedef struct sStrObj
{
    Obj   base;
    int   length;
    char* chars;
} StrObj;

typedef struct sListObj
{
    Obj        base;
    Value      meta;
    ValueArray items;
} ListObj;

typedef struct sSymbolObj
{
    Obj     base;
    StrObj* symbol;
} SymbolObj;

typedef struct sKeywordObj
{
    Obj     base;
    StrObj* keyword;
} KeywordObj;

typedef struct sVectorObj
{
    Obj        base;
    Value      meta;
    ValueArray items;
} VectorObj;

typedef struct sMapObjEntry
{
    Value      key;
    Value      value;
} MapObjEntry;

typedef struct sMapObj
{
    Obj        base;
    Value      meta;
    Table      table;
} MapObj;

typedef struct sFuncObj
{
    Obj        base;
    Value      meta;
    FuncPtr    func;
} FuncObj;

typedef struct sEnvObj
{
    Obj             base;
    struct sEnvObj* outer;
    MapObj*         data;
} EnvObj;

typedef struct sClosureObj
{
    Obj             base;
    Value           meta;
    struct sEnvObj* env;
    Value           params;
    Value           body;
    bool            isMacro;
} ClosureObj;

typedef struct sAtomObj
{
    Obj             base;
    Value           ref;
} AtomObj;

struct sExceptionObj
{
    Obj             base;
    StrObj*         info;
};

#define obj_asStr(o)       ((StrObj*)o)
#define obj_asList(o)      ((ListObj*)o)
#define obj_asSymbol(o)    ((SymbolObj*)o)
#define obj_asKeyword(o)   ((KeywordObj*)o)
#define obj_asVector(o)    ((VectorObj*)o)
#define obj_asMap(o)       ((MapObj*)o)
#define obj_asFunc(o)      ((FuncObj*)o)
#define obj_asEnv(o)       ((EnvObj*)o)
#define obj_asClosure(o)   ((ClosureObj*)o)
#define obj_asAtom(o)      ((AtomObj*)o)
#define obj_asException(o) ((ExceptionObj*)o)

#define _obj_is(o, objType) ((o)->type == (objType))
#define obj_isStr(o)       _obj_is(o, LLO_STRING)
#define obj_isList(o)      _obj_is(o, LLO_LIST)
#define obj_isSymbol(o)    _obj_is(o, LLO_SYMBOL)
#define obj_isKeyword(o)   _obj_is(o, LLO_KEYWORD)
#define obj_isVector(o)    _obj_is(o, LLO_VECTOR)
#define obj_isMap(o)       _obj_is(o, LLO_MAP)
#define obj_isFunc(o)      _obj_is(o, LLO_FUNCTION)
#define obj_isEnv(o)       _obj_is(o, LLO_ENV)
#define obj_isClosure(o)   _obj_is(o, LLO_CLOSURE)
#define obj_isAtom(o)      _obj_is(o, LLO_ATOM)
#define obj_isException(o) _obj_is(o, LLO_EXCEPTION)

#define _value_asObjType(v, f) (f((value_asObj((v)))))
#define value_asStr(v)       _value_asObjType(v, obj_asStr)
#define value_asList(v)      _value_asObjType(v, obj_asList)
#define value_asSymbol(v)    _value_asObjType(v, obj_asSymbol)
#define value_asKeyword(v)   _value_asObjType(v, obj_asKeyword)
#define value_asVector(v)    _value_asObjType(v, obj_asVector)
#define value_asMap(v)       _value_asObjType(v, obj_asMap)
#define value_asFunc(v)      _value_asObjType(v, obj_asFunc)
#define value_asClosure(v)   _value_asObjType(v, obj_asClosure)
#define value_asAtom(v)      _value_asObjType(v, obj_asAtom)
#define value_asException(v) _value_asObjType(v, obj_asException)

#define _value_isObjType(v, f) (value_isObj(v) && f(value_asObj(v)))
#define value_isStr(v)       _value_isObjType(v, obj_isStr)
#define value_isList(v)      _value_isObjType(v, obj_isList)
#define value_isSymbol(v)    _value_isObjType(v, obj_isSymbol)
#define value_isKeyword(v)   _value_isObjType(v, obj_isKeyword)
#define value_isVector(v)    _value_isObjType(v, obj_isVector)
#define value_isMap(v)       _value_isObjType(v, obj_isMap)
#define value_isFunc(v)      _value_isObjType(v, obj_isFunc)
#define value_isClosure(v)   _value_isObjType(v, obj_isClosure)
#define value_isAtom(v)      _value_isObjType(v, obj_isAtom)
#define value_isException(v) _value_isObjType(v, obj_isException)

#define obj_hasMeta(o) \
    (obj_isList((o)) || obj_isVector((o)) || obj_isMap((o)) || obj_isFunc((o)) || obj_isClosure((o)))

#define value_isListLike(v) (value_isList(v) || value_isVector(v))
#define value_isMacro(v)    (value_isClosure(v) && (value_asClosure(v)->isMacro))
#define value_isCallable(v) (value_isClosure(v) || value_isFunc(v))
#define value_hasMeta(v)    (value_isObj((v)) && obj_hasMeta((value_asObj((v)))))

uint32_t obj_hash(Obj* o);
StrObj*  obj_toStr(VM* vm, Obj* o, bool readably);
void     obj_free(VM* vm, Obj* o);
bool     obj_eq(VM* vm, Obj* a, Obj* b);

void     obj_print(Obj* o);

/* ----- str ----- */
StrObj*  strobj_new(VM* vm, const char* chars, int length);
StrObj*  strobj_copy(VM* vm, const char* chars, int length);
uint32_t strobj_hash(StrObj* o);
bool     strobj_eq(StrObj* o, const char* chars, int length);

/* ----- list ----- */
ListObj* listobj_new(VM* vm, int len, ...);
ListObj* listobj_newWithNil(VM* vm, int len);
ListObj* listobj_newWithArr(VM* vm, int len, Value* arr);
bool     listobj_set(ListObj* l, int index, Value v);
bool     listobj_get(ListObj* l, int index, Value* v);

/* ----- symbol ----- */
SymbolObj* symbolobj_new(VM* vm, const char* chars, int length);
SymbolObj* symbolobj_newWithStr(VM* vm, StrObj* strObj);

/* ----- keyword ----- */
KeywordObj* keywordobj_new(VM* vm, const char* chars, int length);
KeywordObj* keywordobj_newWithStr(VM* vm, StrObj* strObj);

/* ----- vector ----- */
VectorObj* vectorobj_new(VM* vm, int len, ...);
VectorObj* vectorobj_newWithNil(VM* vm, int len);
VectorObj* vectorobj_newWithArr(VM* vm, int len, Value* arr);
bool       vectorobj_set(VectorObj* vo, int index, Value v);
bool       vectorobj_get(VectorObj* vo, int index, Value* v);

/* ----- map ----- */
MapObj* mapobj_new(VM* vm, int len, ...);
MapObj* mapobj_newWithArr(VM* vm, int len, Value* arr);
MapObj* mapobj_clone(VM* vm, MapObj* other);
bool    mapobj_set(MapObj* m, Value key, Value value);
bool    mapobj_get(MapObj* m, Value key, Value* value);
bool    mapobj_del(MapObj* m, Value key);

/* ----- func ----- */
FuncObj* funcobj_new(VM* vm, FuncPtr func);
FuncObj* funcobj_clone(VM* vm, FuncObj* other);

/* ----- env ----- */
EnvObj* envobj_new(VM* vm, EnvObj* outer);
bool    envobj_set(EnvObj* e, Value key, Value value);
bool    envobj_get(EnvObj* e, Value key, Value* value);

/* ----- closure ----- */
ClosureObj* closureobj_new(VM* vm, EnvObj* outer, Value params, Value body);
Value       closureobj_invoke(VM* vm, ClosureObj* cobj, 
                              int len, Value* args, 
                              ExceptionObj** exception);
ClosureObj* closureobj_clone(VM* vm, ClosureObj* other);

/* ----- atom ----- */
AtomObj* atomobj_new(VM* vm, Value ref);

/* ----- exception ----- */
ExceptionObj* exceptionobj_new(VM* vm, const char* fmt, ...);

ValueArray* obj_listLikeGetArr(Obj* obj);
ValueArray* value_listLikeGetArr(Value v);

Value obj_invoke(VM* vm, Obj* obj, int len, Value* args, ExceptionObj** exception);
Value value_invoke(VM* vm, Value value, int len, Value* args, ExceptionObj** exception);
Value value_meta(Value value);

#define value_str(vm, chars, len)         (value_obj(strobj_copy((vm), (chars), (len))))

#ifdef C_WINDOWS
#define value_list(vm, len, ...)          (value_obj(listobj_new((vm), (len), ##__VA_ARGS__))) 
#else
#define _value_list(vm, len, ...)         (value_obj(listobj_new((vm), (len), ##__VA_ARGS__))) 
#define value_list(vm, len...)            (_value_list(vm, len))
#endif
#define value_listWithNil(vm, len)        (value_obj(listobj_newWithNil((vm), (len))))
#define value_listWithArr(vm, len, arr)   (value_obj(listobj_newWithArr((vm), (len), (arr))))
#define value_listWithEmpty(vm)           (value_obj(listobj_new((vm), 0)))

#define value_symbol(vm, chars, len)      (value_obj(symbolobj_new((vm), (chars), (len))))
#define value_symbolWithStr(vm, strObj)   (value_obj(symbolobj_newWithStr((vm), (strObj))))

#define value_keyword(vm, chars, len)     (value_obj(keywordobj_new((vm), (chars), (len))))
#define value_keywordWithStr(vm, strObj)  (value_obj(keywordobj_newWithStr((vm), (strObj))))

#ifdef C_WINDOWS
#define value_vector(vm, len, ...)        (value_obj(vectorobj_new((vm), (len), ##__VA_ARGS__)))
#else
#define _value_vector(vm, len, ...)       (value_obj(vectorobj_new((vm), (len), ##__VA_ARGS__)))
#define value_vector(vm, len...)          (_value_vector(vm, len))
#endif
#define value_vectorWithNil(vm, len)      (value_obj(vectorobj_newWithArr((vm), (len))))
#define value_vectorWithArr(vm, len, arr) (value_obj(vectorobj_newWithArr((vm), (len), (arr))))
#define value_vectorWithEmpty(vm)         (value_obj(vectorobj_new((vm), 0)))

#ifdef C_WINDOWS
#define value_map(vm, len, ...)           (value_obj(mapobj_new((vm), (len), ##__VA_ARGS__)))
#else
#define _value_map(vm, len, ...)          (value_obj(mapobj_new((vm), (len), ##__VA_ARGS__)))
#define value_map(vm, len...)             (_value_map(vm, len))
#endif
#define value_mapWithArr(vm, len, arr)    (value_obj(mapobj_newWithArr((vm), (len), (arr))))
#define value_mapWithEmpty(vm)            (value_obj(mapobj_new((vm), 0)))

#define value_func(vm, func)              (value_obj(funcobj_new((vm), (func))))

#define value_closure(vm, env, params, body) (value_obj(closureobj_new((vm), (env), (params), (body))))

#define value_atom(vm, ref)               (value_obj(atomobj_new((vm), (ref))))

#define value_exception(vm, fmt, ...)     (value_obj(exceptionobj_new((vm), (fmt), ##__VA_ARGS__)))

#endif // __C_OBJ_H_
