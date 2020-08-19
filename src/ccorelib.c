#include "ccorelib.h"

#include <time.h>

#include "creader.h"
#include "cprinter.h"
#include "cmem.h"
#include "cvalue.h"
#include "cobj.h"
#include "cutils.h"

#define THROW_EXCEPTION(fmt, ...) \
    do { \
        THROW(fmt, ##__VA_ARGS__); \
        return value_none(); \
    } while (false)

#define ASSERT(cond, fmt, ...) \
    do { \
        if (!(cond)) \
            THROW_EXCEPTION(fmt, ##__VA_ARGS__); \
    } while (false)

#define ASSERT_ONE_PARAM(funcName) \
    ASSERT(len == 1, "RuntimeError: %s only needs one argument", (funcName))

#define ENV_SET_FUNC(chars, len, func) \
    do \
    { \
        Value sv = value_symbol(vm, (chars), (len)); \
        VM_PUSHV(sv); \
        envobj_set(vm->env, sv, value_func(vm, &(func))); \
        VM_POPV(sv); \
    } while (false)

#define FIRST_VAL params[0]
#define SECOND_VAL params[1]

/* ----- arithmatic ----- */

DEF_FUNC(plusFunc)
{
    double ret = value_asNum(FIRST_VAL);
    for (size_t i = 1; i < len; i++)
        ret += value_asNum(params[i]);

    return value_num(ret);
}

DEF_FUNC(subFunc)
{
    double ret = value_asNum(FIRST_VAL);
    for (size_t i = 1; i < len; i++)
        ret -= value_asNum(params[i]);

    return value_num(ret);
}

DEF_FUNC(mulFunc)
{
    double ret = value_asNum(FIRST_VAL);
    for (size_t i = 1; i < len; i++)
        ret *= value_asNum(params[i]);

    return value_num(ret);
}

DEF_FUNC(divFunc)
{
    double ret = value_asNum(FIRST_VAL);
    for (size_t i = 1; i < len; i++)
        ret /= value_asNum(params[i]);

    return value_num(ret);
}

DEF_FUNC(lessFunc)
{
    if (!value_isNum(FIRST_VAL))
    {
        // TODO report error
    }

    double val = value_asNum(FIRST_VAL);

    for (size_t i = 1; i < len; i++)
    {
        if (!value_isNum(params[i]))
        {
            // TODO report error
        }

        double val2 = value_asNum(params[i]);
        if (val >= val2)
            return VAL_FALSE;

        val = val2;
    }

    return VAL_TRUE;
}

DEF_FUNC(lessEqFunc)
{
    if (!value_isNum(FIRST_VAL))
    {
        // TODO report error
    }

    double val = value_asNum(FIRST_VAL);

    for (size_t i = 1; i < len; i++)
    {
        if (!value_isNum(params[i]))
        {
            // TODO report error
        }

        double val2 = value_asNum(params[i]);
        if (val > val2)
            return VAL_FALSE;

        val = val2;
    }

    return VAL_TRUE;
}

DEF_FUNC(greatFunc)
{
    if (!value_isNum(FIRST_VAL))
    {
        // TODO report error
    }

    double val = value_asNum(FIRST_VAL);

    for (size_t i = 1; i < len; i++)
    {
        if (!value_isNum(params[i]))
        {
            // TODO report error
        }

        double val2 = value_asNum(params[i]);
        if (val <= val2)
            return VAL_FALSE;

        val = val2;
    }

    return VAL_TRUE;
}

DEF_FUNC(greatEqFunc)
{
    if (!value_isNum(FIRST_VAL))
    {
        // TODO report error
    }

    double val = value_asNum(FIRST_VAL);

    for (size_t i = 1; i < len; i++)
    {
        if (!value_isNum(params[i]))
        {
            // TODO report error
        }

        double val2 = value_asNum(params[i]);
        if (val < val2)
            return VAL_FALSE;

        val = val2;
    }

    return VAL_TRUE;
}

/* ----- string ----- */
DEF_FUNC(prStrFunc)
{
    // TODO opt Array to StringBuilder
    Array ca;
    ARR_INIT_CAP(&ca, char, 256);

    StrObj* s = NULL;

    char whitespace = ' ';
    for (int i = 0; i < len - 1; i++)
    {
        s = printReadablyStr(vm, params[i]);
        if (s)
        {
            for (size_t j = 0; j < s->length; j++)
            {
                array_push(&ca, &s->chars[j]);
            }
        }

        array_push(&ca, &whitespace);
    }

    s = printReadablyStr(vm, params[len - 1]);
    if (s)
    {
        for (size_t j = 0; j < s->length; j++)
        {
            array_push(&ca, &s->chars[j]);
        }
    }

    Value ret = value_str(vm, (char*)ca.data, ca.count);

    array_free(&ca);

    return ret;
}

DEF_FUNC(strFunc)
{
    // TODO opt Array to StringBuilder
    Array ca;
    ARR_INIT_CAP(&ca, char, 256);

    StrObj* s = NULL;

    for (int i = 0; i < len; i++)
    {
        s = printRawStr(vm, params[i]);
        if (s)
        {
            for (size_t j = 0; j < s->length; j++)
            {
                array_push(&ca, &s->chars[j]);
            }
        }
    }

    Value ret = value_str(vm, (char*)ca.data, ca.count);

    array_free(&ca);

    return ret;
}

DEF_FUNC(prnFunc)
{
    StrObj* s = NULL;

    for (int i = 0; i < len - 1; i++)
    {
        s = printReadablyStr(vm, params[i]);
        printf("%s ", s->chars);
    }

    s = printReadablyStr(vm, params[len - 1]);
    printf("%s\n", s->chars);

    return value_nil();
}

DEF_FUNC(printlnFunc)
{
    StrObj* s = NULL;

    for (int i = 0; i < len - 1; i++)
    {
        s = printRawStr(vm, params[i]);
        printf("%s ", s->chars);
    }

    s = printRawStr(vm, params[len - 1]);
    printf("%s\n", s->chars);

    return value_nil();
}

/* ----- list ----- */

DEF_FUNC(listFunc)
{
    return value_listWithArr(vm, len, params);
}

DEF_FUNC(listCheckFunc)
{
    ASSERT_ONE_PARAM("list?");

    return value_bool(value_isList(FIRST_VAL));
}

DEF_FUNC(emptyCheckFunc)
{
    ASSERT_ONE_PARAM("empty?");

    ValueArray* array = value_listLikeGetArr(FIRST_VAL);
    if (array)
        return value_bool(array->count == 0);
    else
        return VAL_TRUE;
}

DEF_FUNC(countCheckFunc)
{
    ASSERT_ONE_PARAM("count");

    ValueArray* array = value_listLikeGetArr(FIRST_VAL);
    if (array)
        return value_num(array->count);
    else
        return value_num(0);
}

/* ----- atom ----- */

DEF_FUNC(atomFunc)
{
    ASSERT_ONE_PARAM("atom");

    return value_atom(vm, FIRST_VAL);
}

DEF_FUNC(atomCheckFunc)
{
    ASSERT_ONE_PARAM("atom?");

    return value_bool(value_isAtom(FIRST_VAL));
}

DEF_FUNC(derefFunc)
{
    ASSERT_ONE_PARAM("deref");

    ASSERT(value_isAtom(FIRST_VAL), "RuntimeError: deref arg is not a atom");

    return value_asAtom(FIRST_VAL)->ref;
}

DEF_FUNC(resetFunc)
{
    ASSERT(value_isAtom(FIRST_VAL), "RuntimeError: reset arg is not a atom");

    value_asAtom(FIRST_VAL)->ref = SECOND_VAL;
    return SECOND_VAL;
}

DEF_FUNC(swapFunc)
{
    ASSERT(value_isAtom(FIRST_VAL), "RuntimeError: swap arg is not a atom");
    ASSERT(value_isCallable(SECOND_VAL), "RuntimeError: swap 2rd arg is not callable");

    ListObj* lobj = listobj_newWithNil(vm, len - 1);
    VM_PUSH(lobj);

    AtomObj* aobj = value_asAtom(FIRST_VAL);

    listobj_set(lobj, 0, aobj->ref);
    for (size_t i = 2; i < len; i++)
        listobj_set(lobj, i - 1, params[i]);

    Value ret = value_invoke(vm, SECOND_VAL, len - 1, (Value*)lobj->items.data, exception);

    VM_POP(lobj);

    if (!HAS_EXCEPTION())
        aobj->ref = ret;

    return ret;
}

/* ----- other ----- */

DEF_FUNC(equalFunc)
{
    for (size_t i = 1; i < len; i++)
    {
        if (!value_eq(vm, params[i - 1], params[i]))
            return VAL_FALSE;
    }

    return VAL_TRUE;
}

DEF_FUNC(readStringFunc)
{
    ASSERT(value_isStr(FIRST_VAL), "RuntimeError: read-string arg is not string");

    Value ret = readStr(vm, value_asStr(FIRST_VAL)->chars);
    vm_clearBlockCmArr(vm);
    return ret;
}

DEF_FUNC(slurpFunc)
{
    ASSERT(value_isStr(FIRST_VAL), "RuntimeError: slurp arg is not string");

    int fileSize;
    char* fileContent;

    if (!readFile(value_asStr(FIRST_VAL)->chars, &fileContent, &fileSize))
    {
        return value_nil();
    }

    Value ret = value_str(vm, fileContent, fileSize);
    FREE_ARRAY(char, fileContent, fileSize + 1);

    return ret;
}

DEF_FUNC(evalFunc)
{
    return vm_eval(vm, FIRST_VAL, vm->env, exception);
}

DEF_FUNC(consFunc)
{
    ASSERT(value_isList(SECOND_VAL), "RuntimeError: cons 2rd arg is not a list");

    ListObj* lobj = value_asList(SECOND_VAL);
    ListObj* nobj = listobj_newWithNil(vm, lobj->items.count + 1);

    listobj_set(nobj, 0, FIRST_VAL);

    for (size_t i = 1; i < lobj->items.count + 1; i++)
    {
        LIST_GET_CHILD(lobj, i - 1, value);
        listobj_set(nobj, i, value);
    }

    return value_obj(nobj);
}

static Value concat(VM* vm, int len, Value* arr)
{
    ValueArray va;
    ARR_INIT(&va, Value);

    for (size_t i = 0; i < len; i++)
    {
        Value v = arr[i];
        if (value_isList(v) || value_isVector(v))
        {
            ValueArray* l = value_listLikeGetArr(v);

            Value lv;
            for (size_t j = 0; j < l->count; j++)
            {
                array_get(l, j, &lv);
                array_push(&va, &lv);
            }
        }
        else if (value_isMap(v))
        {
            array_push(&va, &v);
        }
        else if (value_isNil(v))
        {
            continue;
        }
        else
        {
            array_push(&va, &v);
        }
    }

    Value ret = value_listWithArr(vm, va.count, (Value*)va.data);
    array_free(&va);

    return ret;
}

DEF_FUNC(concatFunc)
{
    return concat(vm, len, params);
}

DEF_FUNC(nthFunc)
{
    ASSERT(value_isListLike(FIRST_VAL), "RuntimeError: nth arg is not listlike");

    ValueArray* va = value_listLikeGetArr(FIRST_VAL);
    int index = value_asNum(SECOND_VAL);

    if (index < 0 || index >= va->count)
        THROW_EXCEPTION("nth out of range (%d/%d)", index, va->count);

    VALUE_ARR_GET_CHILD(va, index, ret);
    return ret;
}

DEF_FUNC(firstFunc)
{
    ASSERT_ONE_PARAM("first");

    if (value_isNil(FIRST_VAL))
        return value_nil();

    ASSERT(value_isListLike(FIRST_VAL), "RuntimeError: first arg is not listlike");

    ValueArray* va = value_listLikeGetArr(FIRST_VAL);
    if (va->count == 0)
        return value_nil();

    VALUE_ARR_GET_CHILD(va, 0, ret);
    return ret;
}

DEF_FUNC(restFunc)
{
    ASSERT_ONE_PARAM("rest");

    if (value_isNil(FIRST_VAL))
        return value_listWithEmpty(vm);

    ASSERT(value_isListLike(FIRST_VAL), "RuntimeError: rest arg is not listlike");

    ValueArray* va = value_listLikeGetArr(FIRST_VAL);
    if (va->count <= 1)
        return value_listWithEmpty(vm);

    Value ret = value_listWithArr(vm, va->count - 1, (Value*)va->data + 1);
    return ret;
}

DEF_FUNC(throwFunc)
{
    ASSERT_ONE_PARAM("throw");

    THROW_EXCEPTION("%s", value_asStr(FIRST_VAL)->chars);
}

DEF_FUNC(applyFunc)
{
    ASSERT(value_isCallable(FIRST_VAL), "RuntimeError: apply arg is not callable");

    Value args = concat(vm, len - 1, (Value*)params + 1);
    Value funcValue = FIRST_VAL;

    VM_PUSHV(args);

    ListObj* lobj = value_asList(args);
    Value ret = value_invoke(vm, funcValue,
                             lobj->items.count, (Value*)lobj->items.data, exception);

    VM_POPV(args);

    return ret;
}

DEF_FUNC(mapFunc)
{
    ASSERT(value_isCallable(FIRST_VAL), "RuntimeError: map arg is not callable");

    Value args = concat(vm, len - 1, (Value*)params + 1);
    Value funcValue = FIRST_VAL;

    VM_PUSHV(args);

    ListObj* lobj = value_asList(args);

    ListObj* ret = listobj_newWithNil(vm, lobj->items.count);
    VM_PUSH(ret);

    for (size_t i = 0; i < lobj->items.count; i++)
    {
        LIST_GET_CHILD(lobj, i, item);
        Value itemRet = value_invoke(vm, funcValue, 1, &item, exception);

        if (HAS_EXCEPTION())
        {
            VM_POP(ret);
            VM_POPV(args);
            return value_none();
        }
        else
        {
            listobj_set(ret, i, itemRet);
        }
    }

    VM_POP(ret);
    VM_POPV(args);

    return value_obj(ret);
}

DEF_FUNC(nilCheckFunc)
{
    ASSERT_ONE_PARAM("nil?");

    return value_bool(value_isNil(FIRST_VAL));
}

DEF_FUNC(trueCheckFunc)
{
    ASSERT_ONE_PARAM("true?");

    return value_bool(value_isBool(FIRST_VAL) && value_asBool(FIRST_VAL));
}

DEF_FUNC(falseCheckFunc)
{
    ASSERT_ONE_PARAM("false?");

    return value_bool(value_isBool(FIRST_VAL) && !value_asBool(FIRST_VAL));
}

DEF_FUNC(symbolCheckFunc)
{
    ASSERT_ONE_PARAM("symbol?");

    return value_bool(value_isSymbol(FIRST_VAL));
}

DEF_FUNC(symbolFunc)
{
    ASSERT_ONE_PARAM("symbol");

    ASSERT(value_isStr(FIRST_VAL), "RuntimeError: symbol arg is not a string");

    StrObj* sobj = value_asStr(FIRST_VAL);
    return value_symbolWithStr(vm, sobj);
}

DEF_FUNC(keywordFunc)
{
    ASSERT_ONE_PARAM("keyword");

    ASSERT(value_isStr(FIRST_VAL), "RuntimeError: keyword arg is not a string");

    StrObj* sobj = value_asStr(FIRST_VAL);
    return value_keywordWithStr(vm, sobj);
}

DEF_FUNC(keywordCheckFunc)
{
    ASSERT_ONE_PARAM("keyword?");

    return value_bool(value_isKeyword(FIRST_VAL));
}

DEF_FUNC(vectorFunc)
{
    return value_vectorWithArr(vm, len, params);
}

DEF_FUNC(vectorCheckFunc)
{
    ASSERT_ONE_PARAM("vector?");

    return value_bool(value_isVector(FIRST_VAL));
}

DEF_FUNC(sequentialCheckFunc)
{
    ASSERT_ONE_PARAM("sequential?");

    return value_bool(value_isPair(FIRST_VAL));
}

DEF_FUNC(hashMapFunc)
{
    return value_mapWithArr(vm, len, params);
}

DEF_FUNC(mapCheckFunc)
{
    ASSERT_ONE_PARAM("map?");

    return value_bool(value_isMap(FIRST_VAL));
}

DEF_FUNC(assocFunc)
{
    ASSERT(value_isMap(FIRST_VAL), "RuntimeError: assoc arg is not a map");
    ASSERT(len > 1 && (len - 1) % 2 == 0, "RuntimeError: assoc need even change args");

    MapObj* oldMap = value_asMap(FIRST_VAL);

    MapObj* newMap = mapobj_newWithArr(vm, len - 1, &SECOND_VAL);

    VM_PUSH(newMap);

    int oldMapLen = oldMap->table.count;

    uint32_t* keys = CALLOCATE(vm, uint32_t, oldMapLen);
    table_keys(&oldMap->table, keys);

    MapObjEntry entry;
    for (size_t i = 0; i < oldMapLen; i++)
    {
        table_get(&oldMap->table, keys[i], &entry);

        if (!mapobj_get(newMap, entry.key, NULL))
            mapobj_set(newMap, entry.key, entry.value);
    }

    CFREE_ARRAY(vm, uint32_t, keys, oldMapLen);

    VM_POP(newMap);

    return value_obj(newMap);
}

DEF_FUNC(dissocFunc)
{
    ASSERT(value_isMap(FIRST_VAL), "RuntimeError: dissoc arg is not a map");

    MapObj* oldMap = value_asMap(FIRST_VAL);

    MapObj* newMap = mapobj_clone(vm, oldMap);

    for (size_t i = 1; i < len; i++)
        mapobj_del(newMap, params[i]);

    return value_obj(newMap);
}

DEF_FUNC(getFunc)
{
    ASSERT(value_isMap(FIRST_VAL), "RuntimeError: get arg is not a map");

    MapObj* m = value_asMap(FIRST_VAL);
    Value ret;
    if (mapobj_get(m, SECOND_VAL, &ret))
        return ret;
    return value_nil();
}

DEF_FUNC(containsCheckFunc)
{
    ASSERT(value_isMap(FIRST_VAL), "RuntimeError: contains? arg is not a map");

    MapObj* m = value_asMap(FIRST_VAL);
    if (mapobj_get(m, SECOND_VAL, NULL))
        return VAL_TRUE;
    return VAL_FALSE;
}

DEF_FUNC(keysFunc)
{
    ASSERT_ONE_PARAM("keys");

    ASSERT(value_isMap(FIRST_VAL), "RuntimeError: keys arg is not a map");

    MapObj* m = value_asMap(FIRST_VAL);

    int mlen = m->table.count;

    ListObj* lobj = listobj_newWithNil(vm, mlen);
    VM_PUSH(lobj);

    uint32_t* keys = CALLOCATE(vm, uint32_t, mlen);
    table_keys(&m->table, keys);

    MapObjEntry entry;
    for (size_t i = 0; i < mlen; i++)
    {
        table_get(&m->table, keys[i], &entry);
        listobj_set(lobj, i, entry.key);
    }

    CFREE_ARRAY(vm, uint32_t, keys, mlen);

    VM_POP(lobj);

    return value_obj(lobj);
}

DEF_FUNC(valsFunc)
{
    ASSERT_ONE_PARAM("vals");

    ASSERT(value_isMap(FIRST_VAL), "RuntimeError: vals arg is not a map");

    MapObj* m = value_asMap(FIRST_VAL);

    int mlen = m->table.count;

    ListObj* lobj = listobj_newWithNil(vm, mlen);
    VM_PUSH(lobj);

    uint32_t* keys = CALLOCATE(vm, uint32_t, mlen);
    table_keys(&m->table, keys);

    MapObjEntry entry;
    for (size_t i = 0; i < mlen; i++)
    {
        table_get(&m->table, keys[i], &entry);
        listobj_set(lobj, i, entry.value);
    }

    CFREE_ARRAY(vm, uint32_t, keys, mlen);

    VM_POP(lobj);

    return value_obj(lobj);
}

DEF_FUNC(readlineFunc)
{
    ASSERT(value_isStr(FIRST_VAL), "RuntimeError: readline arg is not a string");

    StrObj* sobj = value_asStr(FIRST_VAL);
    printf("%s", sobj->chars);

    char input[256];
    if (fgets(input, 256, stdin))
    {
        int slen = strlen(input);
        return value_str(vm, input, slen);
    }
    else
    {
        return value_nil();
    }
}

DEF_FUNC(timeMsFunc)
{
    return value_num(time(NULL) * 1000);
}

DEF_FUNC(metaFunc)
{
    ASSERT_ONE_PARAM("meta");

    Value ret = value_meta(FIRST_VAL);
    if (value_isNone(ret))
        THROW_EXCEPTION("RuntimeError: base type doesn't have meta value");

    return ret;
}

DEF_FUNC(withMetaFunc)
{
    ASSERT(len == 2, "RuntimeError: meta needs two argument");

    if (value_isFunc(FIRST_VAL))
    {
        FuncObj* newFunc = funcobj_clone(vm, value_asFunc(FIRST_VAL));
        newFunc->meta = SECOND_VAL;
        return value_obj(newFunc);
    }
    else if (value_isClosure(FIRST_VAL))
    {
        ClosureObj* newClosure = closureobj_clone(vm, value_asClosure(FIRST_VAL));
        newClosure->meta = SECOND_VAL;
        return value_obj(newClosure);
    }

    THROW_EXCEPTION("RuntimeError: only function can set meta");
}

DEF_FUNC(fnCheckFunc)
{
    ASSERT_ONE_PARAM("fn?");

    return value_bool(value_isFunc(FIRST_VAL) || value_isClosure(FIRST_VAL));
}

DEF_FUNC(macroCheckFunc)
{
    ASSERT_ONE_PARAM("macro?");

    return value_bool(value_isMacro(FIRST_VAL));
}

DEF_FUNC(stringCheckFunc)
{
    ASSERT_ONE_PARAM("string?");

    return value_bool(value_isStr(FIRST_VAL));
}

DEF_FUNC(numberCheckFunc)
{
    ASSERT_ONE_PARAM("number?");

    return value_bool(value_isNum(FIRST_VAL));
}

DEF_FUNC(seqFunc)
{
    ASSERT_ONE_PARAM("seq");

    if (value_isNil(FIRST_VAL))
    {
        return FIRST_VAL;
    }
    else if (value_isList(FIRST_VAL))
    {
        if (value_asList(FIRST_VAL)->items.count == 0)
            return value_nil();
        else
            return FIRST_VAL;
    }
    else if (value_isVector(FIRST_VAL))
    {
        VectorObj* vobj = value_asVector(FIRST_VAL);
        if (vobj->items.count == 0)
            return value_nil();
        else
            return value_listWithArr(vm, vobj->items.count, vobj->items.data);
    }
    else if (value_isStr(FIRST_VAL))
    {
        StrObj* sobj = value_asStr(FIRST_VAL);
        if (sobj->length == 0)
            return value_nil();
        else
        {
            ListObj* lobj = listobj_newWithNil(vm, sobj->length);
            VM_PUSH(lobj);

            for (int i = 0; i < sobj->length; i++)
                listobj_set(lobj, i, value_str(vm, &sobj->chars[i], 1));

            VM_POP(lobj);

            return value_obj(lobj);
        }
    }

    ASSERT(false, "RuntimeError: seq type not support");
}

DEF_FUNC(conjFunc)
{
    ASSERT(value_isPair(FIRST_VAL), "RuntimeError: first argument must be pair");

    if (value_isList(FIRST_VAL))
    {
        ListObj* rawList = value_asList(FIRST_VAL);
        ListObj* newList = listobj_newWithNil(vm, rawList->items.count + len - 1);
        for (int i = len - 1; i > 0; i--)
        {
            listobj_set(newList, len - 1 - i, params[i]);
        }

        for (int i = len - 1; i < len + rawList->items.count - 1; i++)
        {
            LIST_GET_CHILD(rawList, i - len + 1, tempValue);
            listobj_set(newList, i, tempValue);
        }

        return value_obj(newList);
    }
    else
    {
        VectorObj* rawVector = value_asVector(FIRST_VAL);
        VectorObj* newVector = vectorobj_newWithNil(vm, rawVector->items.count + len - 1);
        for (int i = 0; i < rawVector->items.count; i++)
        {
            VECTOR_GET_CHILD(rawVector, i, tempValue);
            vectorobj_set(newVector, i, tempValue);
        }

        for (int i = rawVector->items.count; i < rawVector->items.count + len - 1; i++)
        {
            vectorobj_set(newVector, i, params[i - rawVector->items.count + 1]);
        }

        return value_obj(newVector);
    }
}

DEF_FUNC(gcFunc)
{
    ccollectGarbage(vm);
    return value_nil();
}

void initCoreLib(VM* vm)
{
    if (vm == NULL)
        return;

    ENV_SET_FUNC("+", 1, plusFunc);
    ENV_SET_FUNC("-", 1, subFunc);
    ENV_SET_FUNC("*", 1, mulFunc);
    ENV_SET_FUNC("/", 1, divFunc);
    ENV_SET_FUNC("<", 1, lessFunc);
    ENV_SET_FUNC("<=", 2, lessEqFunc);
    ENV_SET_FUNC(">", 1, greatFunc);
    ENV_SET_FUNC(">=", 2, greatEqFunc);

    ENV_SET_FUNC("=", 1, equalFunc);

    ENV_SET_FUNC("pr-str", 6, prStrFunc);
    ENV_SET_FUNC("str", 3, strFunc);
    ENV_SET_FUNC("prn", 3, prnFunc);
    ENV_SET_FUNC("println", 7, printlnFunc);

    ENV_SET_FUNC("list", 4, listFunc);
    ENV_SET_FUNC("list?", 5, listCheckFunc);

    ENV_SET_FUNC("empty?", 6, emptyCheckFunc);
    ENV_SET_FUNC("count", 5, countCheckFunc);

    ENV_SET_FUNC("read-string", 11, readStringFunc);
    ENV_SET_FUNC("slurp", 5, slurpFunc);
    ENV_SET_FUNC("eval", 4, evalFunc);

    ENV_SET_FUNC("atom", 4, atomFunc);
    ENV_SET_FUNC("atom?", 5, atomCheckFunc);
    ENV_SET_FUNC("deref", 5, derefFunc);
    ENV_SET_FUNC("reset!", 6, resetFunc);
    ENV_SET_FUNC("swap!", 5, swapFunc);

    ENV_SET_FUNC("cons", 4, consFunc);
    ENV_SET_FUNC("concat", 6, concatFunc);

    ENV_SET_FUNC("nth", 3, nthFunc);
    ENV_SET_FUNC("first", 5, firstFunc);
    ENV_SET_FUNC("rest", 4, restFunc);

    ENV_SET_FUNC("throw", 5, throwFunc);

    ENV_SET_FUNC("apply", 5, applyFunc);
    ENV_SET_FUNC("map", 3, mapFunc);

    ENV_SET_FUNC("nil?", 4, nilCheckFunc);
    ENV_SET_FUNC("true?", 5, trueCheckFunc);
    ENV_SET_FUNC("false?", 6, falseCheckFunc);
    ENV_SET_FUNC("symbol?", 7, symbolCheckFunc);
    ENV_SET_FUNC("symbol", 6, symbolFunc);
    ENV_SET_FUNC("keyword", 7, keywordFunc);
    ENV_SET_FUNC("keyword?", 8, keywordCheckFunc);
    ENV_SET_FUNC("vector", 6, vectorFunc);
    ENV_SET_FUNC("vector?", 7, vectorCheckFunc);
    ENV_SET_FUNC("sequential?", 11, sequentialCheckFunc);
    ENV_SET_FUNC("hash-map", 8, hashMapFunc);
    ENV_SET_FUNC("map?", 4, mapCheckFunc);

    ENV_SET_FUNC("assoc", 5, assocFunc);
    ENV_SET_FUNC("dissoc", 6, dissocFunc);
    ENV_SET_FUNC("get", 3, getFunc);
    ENV_SET_FUNC("contains?", 9, containsCheckFunc);
    ENV_SET_FUNC("keys", 4, keysFunc);
    ENV_SET_FUNC("vals", 4, valsFunc);

    ENV_SET_FUNC("readline", 8, readlineFunc);
    ENV_SET_FUNC("time-ms", 7, timeMsFunc);
    ENV_SET_FUNC("meta", 4, metaFunc);
    ENV_SET_FUNC("with-meta", 9, withMetaFunc);
    ENV_SET_FUNC("fn?", 3, fnCheckFunc);
    ENV_SET_FUNC("macro?", 6, macroCheckFunc);
    ENV_SET_FUNC("string?", 7, stringCheckFunc);
    ENV_SET_FUNC("number?", 7, numberCheckFunc);
    ENV_SET_FUNC("seq", 3, seqFunc);
    ENV_SET_FUNC("conj", 4, conjFunc);

    ENV_SET_FUNC("gc", 2, gcFunc);
}
