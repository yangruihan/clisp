#include "cobj.h"

#include <memory.h>

#include "cutils.h"
#include "cvm.h"
#include "cmem.h"

#define CALLOCATE_OBJ(vm, type, objectType) \
    (type*)allocateObject(vm, sizeof(type), objectType)

static Obj* allocateObject(VM* vm, size_t size, ObjType type)
{
    Obj* object = (Obj*)creallocate(vm, NULL, 0, size);
    object->type = type;
    object->isMarked = false;
    object->hash = 0;

    // record objects to global object linked list
    object->next = vm->objs;
    vm->objs = object;

    return object;
}

static StrObj* allocateString(VM* vm, char* chars, int length, uint32_t hash)
{
    StrObj* string = CALLOCATE_OBJ(vm, StrObj, LLO_STRING);
    string->length = length;
    string->chars = chars;
    string->base.hash = hash;

    // insert string to global string table
    table_set(&vm->strings, hash, &string);

    return string;
}

uint32_t obj_hash(Obj* o)
{
    if (o->hash != 0)
        return o->hash;

    uint32_t h;
    switch(o->type)
    {
        case LLO_STRING:
        {
            h = strobj_hash(obj_asStr(o));
            break;
        }

        case LLO_LIST:
        {
            h = HASH((char*)o, sizeof(ListObj));
            break;
        }

        case LLO_SYMBOL:
        {
            h = strobj_hash(obj_asSymbol(o)->symbol);
            h = HASH(&h, sizeof(uint32_t));
            break;
        }

        case LLO_KEYWORD:
        {
            h = HASH((char*)(obj_asKeyword(o)->keyword), sizeof(StrObj));
            break;
        }

        case LLO_VECTOR:
        {
            h = HASH((char*)o, sizeof(VectorObj));
            break;
        }

        case LLO_MAP:
        {
            h = HASH((char*)o, sizeof(MapObj));
            break;
        }

        case LLO_FUNCTION:
        {
            h = HASH((char*)o, sizeof(FuncObj));
            break;
        }

        case LLO_CLOSURE:
        {
            h = HASH((char*)o, sizeof(ClosureObj));
            break;
        }

        case LLO_ATOM:
        {
            h = HASH((char*)o, sizeof(AtomObj));
            break;
        }

        case LLO_EXCEPTION:
        {
            h = HASH((char*)o, sizeof(ExceptionObj));
            break;
        }

        default:
        {
            // TODO throw exception
            h = 0;
            RLOG_ERROR("obj hash: type not supported now! %d", o->type);
            break;
        }
    }

    o->hash = h;
    return h;
}

EscapeData escapeCharDatas[ESCAPE_DATA_LEN] = {
    {'\\', "\\\\"},
    {'"',  "\\\""},
    {'\'', "\\'"},
    {'\a', "\\a"},
    {'\b', "\\b"},
    {'\f', "\\f"},
    {'\n', "\\n"},
    {'\r', "\\r"},
    {'\t', "\\t"},
    {'\v', "\\v"},
    {'\0', "\\0"}
};

StrObj* obj_toStr(VM* vm, Obj* o, bool readably)
{
#define SET_CURRENT_CHAR(char) *currentChar = (char); currentChar++
#define SET_CURRENT_STR(strObj) \
    memcpy(currentChar, (strObj)->chars, (strObj)->length); \
    currentChar += (strObj)->length

    // TODO bugfix if to string length is greater than TO_STR_BUFF_COUNT
    static char s_objStrBuff[TO_STR_BUFF_COUNT];

    StrObj* ret = NULL;

    if (obj_isStr(o))
    {
        if (!readably)
        {
            ret = obj_asStr(o);
        }
        else // convert escape char to readably
        {
            StrObj* strObj = obj_asStr(o);
            char* current = s_objStrBuff;
            char* start = s_objStrBuff;

            // need special buffer
            if (strObj->length > TO_STR_BUFF_COUNT)
            {
                current = CALLOCATE(vm, char, strObj->length * 2);
                start = current;
            }

            *current = '"';
            current++;

            for (size_t i = 0; i < strObj->length; i++)
            {
                bool isEscape = false;
                char c = strObj->chars[i];
                for (size_t j = 0; j < ESCAPE_DATA_LEN; j++)
                {
                    if (c == escapeCharDatas[j].originChar)
                    {
                        isEscape = true;
                        *current = escapeCharDatas[j].tChars[0];
                        current++;
                        *current = escapeCharDatas[j].tChars[1];
                    }
                }

                if (!isEscape)
                    *current = c;
                current++;
            }

            *current = '"';
            current++;

            ret = strobj_copy(vm, start, (int)(current - start));

            // free specail buffer
            if (strObj->length > TO_STR_BUFF_COUNT)
            {
                CFREE_ARRAY(vm, char, start, strObj->length * 2);
            }
        }
    }
    else if (obj_isSymbol(o))
    {
        SymbolObj* symbolObj = obj_asSymbol(o);
        ret = symbolObj->symbol;
    }
    else if (obj_isKeyword(o))
    {
        KeywordObj* keywordObj = obj_asKeyword(o);
        ret = keywordObj->keyword;
    }
    else if (obj_isList(o))
    {
        ListObj* listObj = obj_asList(o);
        const int len = listObj->items.count;
        if (len == 0)
            return strobj_copy(vm, "()", 2);

        StrObj** child = CALLOCATE(vm, StrObj*, len);
        for (int i = 0; i < len; i++)
        {
            Value v;
            array_get(&listObj->items, i, &v);
            child[i] = value_toStr(vm, v, readably);
            VM_PUSH(child[i]);
        }

        char* currentChar = s_objStrBuff;
        SET_CURRENT_CHAR('(');
        for (int i = 0; i < len - 1; i++)
        {
            SET_CURRENT_STR(child[i]);
            SET_CURRENT_CHAR(' ');
        }

        SET_CURRENT_STR(child[len - 1]);
        SET_CURRENT_CHAR(')');

        for (int i = len - 1; i >= 0; i--)
            VM_POP(child[i]); // children

        CFREE_ARRAY(vm, StrObj*, child, len);

        ret = strobj_copy(vm, s_objStrBuff, (int)(currentChar - s_objStrBuff));
    }
    else if (obj_isVector(o))
    {
        VectorObj* vectorObj = obj_asVector(o);
        const int len = vectorObj->items.count;
        if (len == 0)
            return strobj_copy(vm, "[]", 2);

        StrObj** child = CALLOCATE(vm, StrObj*, len);
        for (int i = 0; i < len; i++)
        {
            Value v;
            array_get(&vectorObj->items, i, &v);
            child[i] = value_toStr(vm, v, readably);
            VM_PUSH(child[i]);
        }

        char* currentChar = s_objStrBuff;
        SET_CURRENT_CHAR('[');
        for (int i = 0; i < len - 1; i++)
        {
            SET_CURRENT_STR(child[i]);
            SET_CURRENT_CHAR(',');
            SET_CURRENT_CHAR(' ');
        }

        SET_CURRENT_STR(child[len - 1]);
        SET_CURRENT_CHAR(']');

        for (int i = len - 1; i >= 0; i--)
            VM_POP(child[i]); // children

        CFREE_ARRAY(vm, StrObj*, child, len);

        ret = strobj_copy(vm, s_objStrBuff, (int)(currentChar - s_objStrBuff));
    }
    else if (obj_isMap(o))
    {
        MapObj* mapObj = obj_asMap(o);
        const int len = mapObj->table.count;
        if (len == 0)
            return strobj_copy(vm, "{}", 2);

        StrObj** child = CALLOCATE(vm, StrObj*, len * 2);
        uint32_t* keys = CALLOCATE(vm, uint32_t, len);

        Table* t = &mapObj->table;

        table_keys(t, keys);

        for (int i = 0; i < len; i++)
        {
            MapObjEntry entry;
            table_get(t, keys[i], &entry);
            child[2 * i] = value_toStr(vm, entry.key, readably);
            VM_PUSH(child[2 * i]);

            child[2 * i + 1] = value_toStr(vm, entry.value, readably);
            VM_PUSH(child[2 * i + 1]);
        }

        char* currentChar = s_objStrBuff;
        SET_CURRENT_CHAR('{');
        for (int i = 0; i < len - 1; i++)
        {
            SET_CURRENT_STR(child[2 * i]);
            SET_CURRENT_CHAR(' ');
            SET_CURRENT_STR(child[2 * i + 1]);
            SET_CURRENT_CHAR(',');
            SET_CURRENT_CHAR(' ');
        }

        int index = 2 * (len - 1);
        SET_CURRENT_STR(child[index]);
        SET_CURRENT_CHAR(' ');
        SET_CURRENT_STR(child[index + 1]);
        SET_CURRENT_CHAR('}');

        for (int i = len - 1; i >= 0; i--)
        {
            VM_POP(child[2 * i + 1]); // child
            VM_POP(child[2 * i]); // child
        }

        CFREE_ARRAY(vm, uint32_t, keys, len);
        CFREE_ARRAY(vm, StrObj*, child, len * 2);

        ret = strobj_copy(vm, s_objStrBuff, (int)(currentChar - s_objStrBuff));
    }
    else if (obj_isFunc(o))
    {
        FuncObj* funcObj = obj_asFunc(o);
        size_t len = snprintf(s_objStrBuff, TO_STR_BUFF_COUNT, "<function %p>", funcObj->func);
        ret = strobj_copy(vm, s_objStrBuff, len);
    }
    else if (obj_isClosure(o))
    {
        ClosureObj* cobj = obj_asClosure(o);
        if (cobj->isMacro)
        {
            size_t len = snprintf(s_objStrBuff, TO_STR_BUFF_COUNT, "<macro %p>", o);
            ret = strobj_copy(vm, s_objStrBuff, len);
        }
        else
        {
            size_t len = snprintf(s_objStrBuff, TO_STR_BUFF_COUNT, "<closure %p>", o);
            ret = strobj_copy(vm, s_objStrBuff, len);
        }
    }
    else if (obj_isAtom(o))
    {
        size_t len = snprintf(s_objStrBuff, TO_STR_BUFF_COUNT, "<atom %p>", o);
        ret = strobj_copy(vm, s_objStrBuff, len);
    }
    else if (obj_isException(o))
    {
        ExceptionObj* eobj = obj_asException(o);
        size_t len = snprintf(s_objStrBuff, TO_STR_BUFF_COUNT, "%s", eobj->info->chars);
        ret = strobj_copy(vm, s_objStrBuff, len);
    }
    else if (obj_isEnv(o))
    {
        EnvObj* eobj = obj_asEnv(o);
        size_t len;
        if (eobj->data)
        {
            StrObj* sobj = obj_toStr(vm, (Obj*)eobj->data, readably);
            len = snprintf(s_objStrBuff, TO_STR_BUFF_COUNT, "<Env %p outer:%p data: %s>",
                                  eobj, eobj->outer, sobj->chars);
        }
        else
        {
            len = snprintf(s_objStrBuff, TO_STR_BUFF_COUNT, "<Env %p outer:%p data: NULL>",
                                  eobj, eobj->outer);
        }
        ret = strobj_copy(vm, s_objStrBuff, len);
    }
    else
    {
        RLOG_ERROR("obj toStr: type not supported now! %d", o->type);
    }

#undef SET_CURRENT_STR
#undef SET_CURRENT_CHAR

    return ret;
}

void obj_free(VM* vm, Obj* o)
{
    switch (o->type)
    {
    case LLO_LIST:
    {
        ListObj* lobj = obj_asList(o);
        array_free(&lobj->items);
        CFREE(vm, ListObj, lobj);
        break;
    }

    case LLO_SYMBOL:
    {
        SymbolObj* sobj = obj_asSymbol(o);
        CFREE(vm, SymbolObj, sobj);
        break;
    }

    case LLO_STRING:
    {
        StrObj* sobj = obj_asStr(o);
        CFREE_ARRAY(vm, char, sobj->chars, sobj->length + 1);
        CFREE(vm, StrObj, sobj);
        break;
    }

    case LLO_FUNCTION:
    {
        FuncObj* fobj = obj_asFunc(o);
        CFREE(vm, FuncObj, fobj);
        break;
    }

    case LLO_KEYWORD:
    {
        KeywordObj* kobj = obj_asKeyword(o);
        CFREE(vm, KeywordObj, kobj);
        break;
    }

    case LLO_VECTOR:
    {
        VectorObj* vobj = obj_asVector(o);
        array_free(&vobj->items);
        CFREE(vm, VectorObj, vobj);
        break;
    }

    case LLO_MAP:
    {
        MapObj* mobj = obj_asMap(o);
        table_free(&mobj->table);
        CFREE(vm, MapObj, mobj);
        break;
    }

    case LLO_ATOM:
    {
        CFREE(vm, AtomObj, o);
        break;
    }

    case LLO_EXCEPTION:
    {
        CFREE(vm, ExceptionObj, o);
        break;
    }

    case LLO_ENV:
    {
        EnvObj* eobj = obj_asEnv(o);
        CFREE(vm, EnvObj, eobj);
        break;
    }

    case LLO_CLOSURE:
    {
        CFREE(vm, ClosureObj, o);
        break;
    }

    default:
        break;
    }
}

bool obj_eq(VM* vm, Obj* a, Obj* b)
{
    if (a == NULL || b == NULL)
        return false;

    if (a == b)
        return true;

    if (a->type != b->type)
        return false;

    switch (a->type)
    {
    case LLO_LIST:
    case LLO_VECTOR:
    {
        ValueArray* aarr = obj_listLikeGetArr(a);
        ValueArray* barr = obj_listLikeGetArr(b);

        if (aarr->count != barr->count)
            return false;

        for (size_t i = 0; i < aarr->count; i++)
        {
            VALUE_ARR_GET_CHILD(aarr, i, avalue);
            VALUE_ARR_GET_CHILD(barr, i, bvalue);

            if (!value_eq(vm, avalue, bvalue))
                return false;
        }

        return true;
    }

    case LLO_SYMBOL:
    {
        return obj_asSymbol(a)->symbol == obj_asSymbol(b)->symbol;
    }

    case LLO_STRING:
    {
        return a == b;
    }

    case LLO_FUNCTION:
    {
        return obj_asFunc(a)->func == obj_asFunc(b)->func;
    }

    case LLO_KEYWORD:
    {
        return obj_asKeyword(a)->keyword == obj_asKeyword(b)->keyword;
    }

    case LLO_MAP:
    {
        MapObj* aobj = obj_asMap(a);
        MapObj* bobj = obj_asMap(b);

        int len = aobj->table.count;

        if (len != bobj->table.count)
            return false;

        uint32_t* keys = CALLOCATE(vm, uint32_t, len);
        table_keys(&aobj->table, keys);

        MapObjEntry aentry;
        MapObjEntry bentry;
        for (size_t i = 0; i < len; i++)
        {
            if (!table_get(&bobj->table, keys[i], &bentry))
                goto MAP_FALSE;

            table_get(&aobj->table, keys[i], &aentry);

            if (!value_eq(vm, aentry.key, bentry.key))
                goto MAP_FALSE;

            if (!value_eq(vm, aentry.value, bentry.value))
                goto MAP_FALSE;
        }

        CFREE_ARRAY(vm, uint32_t, keys, len);
        return true;

        MAP_FALSE:
        CFREE_ARRAY(vm, uint32_t, keys, len);
        return false;
    }

    case LLO_ATOM:
    {
        return a == b;
    }

    case LLO_EXCEPTION:
    {
        return a == b;
    }

    case LLO_ENV:
    {
        EnvObj* aobj = obj_asEnv(a);
        EnvObj* bobj = obj_asEnv(b);

        return aobj->outer == bobj->outer && obj_eq(vm, (Obj*)aobj->data, (Obj*)bobj->data);
    }

    case LLO_CLOSURE:
    {
        ClosureObj* aobj = obj_asClosure(a);
        ClosureObj* bobj = obj_asClosure(b);

        return aobj->env == bobj->env
               && aobj->isMacro == bobj->isMacro
               && value_eq(vm, aobj->params, bobj->params)
               && value_eq(vm, aobj->body, bobj->body);
    }

    default:
        break;
    }

    RLOG_ERROR("ObjEqError: no support type (%d)!", a->type);
    return false;
}

void obj_print(Obj* o)
{
    switch (o->type)
    {
    case LLO_LIST:
    {
        RLOG_DEBUG("<value list %p>", o);
        break;
    }

    case LLO_SYMBOL:
    {
        SymbolObj* sobj = obj_asSymbol(o);
        if (sobj->symbol)
            RLOG_DEBUG("<value symbol %s>", sobj->symbol->chars);
        else
            RLOG_DEBUG("<value symbol none>");

        break;
    }

    case LLO_STRING:
    {
        StrObj* sobj = obj_asStr(o);
        RLOG_DEBUG("<value string \"%s\">", sobj->chars);
        break;
    }

    case LLO_FUNCTION:
    {
        RLOG_DEBUG("<value function %p>", o);
        break;
    }

    case LLO_KEYWORD:
    {
        KeywordObj* kobj = obj_asKeyword(o);
        if (kobj->keyword)
            RLOG_DEBUG("<value keyword %s>", kobj->keyword->chars);
        else
            RLOG_DEBUG("<value keyword none>");

        break;
    }

    case LLO_VECTOR:
    {
        RLOG_DEBUG("<value vector %p>", o);
        break;
    }

    case LLO_MAP:
    {
        RLOG_DEBUG("<value map %p>", o);
        break;
    }

    case LLO_ATOM:
    {
        RLOG_DEBUG("<value atom %p>", o);
        break;
    }

    case LLO_EXCEPTION:
    {
        ExceptionObj* eobj = obj_asException(o);
        if (eobj->info)
            RLOG_DEBUG("<value exception %p %s>", o, eobj->info->chars);
        else
            RLOG_DEBUG("<value exception %p none>");
        break;
    }

    case LLO_ENV:
    {
        RLOG_DEBUG("<value env %p>", o);
        break;
    }

    case LLO_CLOSURE:
    {
        ClosureObj* cobj = obj_asClosure(o);
        if (cobj->isMacro)
            RLOG_DEBUG("<value macro %p>", o);
        else
            RLOG_DEBUG("<value closure %p>", o);
        break;
    }

    default:
        RLOG_ERROR("obj print: type not supported now! %d", o->type);
        break;
    }
}

StrObj* strobj_new(VM* vm, const char *chars, int length)
{
    uint32_t h = HASH(chars, length);
    StrObj* s;

    // try to get string from global string table
    if (table_get(&vm->strings, h, &s))
    {
        CFREE_ARRAY(vm, char, (void*)chars, length + 1);
        return s;
    }

    return allocateString(vm, (char*)chars, length, h);
}

uint32_t strobj_hash(StrObj* o)
{
    return o->base.hash;
}

bool strobj_eq(StrObj* o, const char* chars, int length)
{
    if (o->length != length)
        return false;

    return strcmp(o->chars, chars) == 0;
}

StrObj* strobj_copy(VM* vm, const char* chars, int length)
{
    uint32_t h = HASH(chars, length);
    StrObj* s;

    // try to get string from global string table
    if (table_get(&vm->strings, h, &s))
    {
        return s;
    }

    char* heapChars = CALLOCATE(vm, char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(vm, heapChars, length, h);
}

ListObj* listobj_new(VM* vm, int len, ...)
{
    ListObj* listObj = CALLOCATE_OBJ(vm, ListObj, LLO_LIST);
    listObj->meta = value_nil();
    array_init(&listObj->items, len, sizeof(Value));

    va_list args;
    va_start(args, len);

    for (size_t i = 0; i < len; i++)
    {
        Value v = va_arg(args, Value);
        array_push(&listObj->items, &v);
    }

    va_end(args);
    return listObj;
}

ListObj* listobj_newWithNil(VM* vm, int len)
{
    ListObj* listObj = CALLOCATE_OBJ(vm, ListObj, LLO_LIST);
    listObj->meta = value_nil();
    array_init(&listObj->items, len, sizeof(Value));

    for (size_t i = 0; i < len; i++)
        array_push(&listObj->items, (void*)&value_nil());

    return listObj;
}

ListObj* listobj_newWithArr(VM* vm, int len, Value* arr)
{
    ListObj* listObj = CALLOCATE_OBJ(vm, ListObj, LLO_LIST);
    listObj->meta = value_nil();
    array_init(&listObj->items, len, sizeof(Value));

    // TODO OPT direct to copy memory

    for (size_t i = 0; i < len; i++)
        array_push(&listObj->items, (void*)&(arr[i]));

    return listObj;
}

bool listobj_set(ListObj* l, int index, Value v)
{
    return array_set(&l->items, index, &v);
}

bool listobj_get(ListObj* l, int index, Value* v)
{
    return array_get(&l->items, index, v);
}

SymbolObj* symbolobj_new(VM* vm, const char* chars, int length)
{
    SymbolObj* sobj = CALLOCATE_OBJ(vm, SymbolObj, LLO_SYMBOL);
    sobj->symbol = NULL;

    VM_PUSH(sobj);
    sobj->symbol = strobj_copy(vm, chars, length);
    VM_POP(sobj);
    return sobj;
}

SymbolObj* symbolobj_newWithStr(VM* vm, StrObj* strObj)
{
    SymbolObj* sobj = CALLOCATE_OBJ(vm, SymbolObj, LLO_SYMBOL);
    sobj->symbol = strObj;
    return sobj;
}

KeywordObj* keywordobj_new(VM* vm, const char* chars, int length)
{
    KeywordObj* kobj = CALLOCATE_OBJ(vm, KeywordObj, LLO_KEYWORD);
    kobj->keyword = NULL;

    VM_PUSH(kobj);
    kobj->keyword = strobj_copy(vm, chars, length);
    VM_POP(kobj);
    return kobj;
}

KeywordObj* keywordobj_newWithStr(VM* vm, StrObj* strObj)
{
    KeywordObj* kobj = CALLOCATE_OBJ(vm, KeywordObj, LLO_KEYWORD);
    kobj->keyword = strObj;
    return kobj;
}

VectorObj* vectorobj_new(VM* vm, int len, ...)
{
    VectorObj* vectorObj = CALLOCATE_OBJ(vm, VectorObj, LLO_VECTOR);
    vectorObj->meta = value_nil();
    array_init(&vectorObj->items, len, sizeof(Value));

    va_list args;
    va_start(args, len);

    for (size_t i = 0; i < len; i++)
    {
        Value v = va_arg(args, Value);
        array_push(&vectorObj->items, &v);
    }

    va_end(args);
    return vectorObj;
}

VectorObj* vectorobj_newWithNil(VM* vm, int len)
{
    VectorObj* vectorObj = CALLOCATE_OBJ(vm, VectorObj, LLO_VECTOR);
    vectorObj->meta = value_nil();
    array_init(&vectorObj->items, len, sizeof(Value));

    for (size_t i = 0; i < len; i++)
        array_push(&vectorObj->items, (void*)&value_nil());

    return vectorObj;
}

VectorObj* vectorobj_newWithArr(VM* vm, int len, Value* arr)
{
    VectorObj* vectorObj = CALLOCATE_OBJ(vm, VectorObj, LLO_VECTOR);
    vectorObj->meta = value_nil();
    array_init(&vectorObj->items, len, sizeof(Value));

    for (size_t i = 0; i < len; i++)
        array_push(&vectorObj->items, (void*)&(arr[i]));

    return vectorObj;
}

bool vectorobj_set(VectorObj* vo, int index, Value v)
{
    return array_set(&vo->items, index, &v);
}

bool vectorobj_get(VectorObj* vo, int index, Value* v)
{
    return array_get(&vo->items, index, v);
}

MapObj* mapobj_new(VM* vm, int len, ...)
{
    MapObj* mapObj = CALLOCATE_OBJ(vm, MapObj, LLO_MAP);
    mapObj->meta = value_nil();
    TABLE_INIT(&mapObj->table, MapObjEntry);

    va_list args;
    va_start(args, len);

    for (size_t i = 0; i < len; i = i + 2)
    {
        Value key = va_arg(args, Value);
        MapObjEntry entry = {key, va_arg(args, Value)};
        table_set(&mapObj->table, value_hash(key), &entry);
    }

    va_end(args);
    return mapObj;
}

MapObj* mapobj_newWithArr(VM* vm, int len, Value* arr)
{
    MapObj* mapObj = CALLOCATE_OBJ(vm, MapObj, LLO_MAP);
    mapObj->meta = value_nil();
    TABLE_INIT(&mapObj->table, MapObjEntry);

    for (size_t i = 0; i < len; i = i + 2)
    {
        Value key = arr[i];
        Value value = arr[i + 1];
        MapObjEntry entry = {key, value};
        table_set(&mapObj->table, value_hash(key), &entry);
    }

    return mapObj;
}

MapObj* mapobj_clone(VM* vm, MapObj* other)
{
    MapObj* newMap = mapobj_new(vm, 0);
    newMap->meta = other->meta;

    VM_PUSH(newMap);

    int oldMapLen = other->table.count;

    uint32_t* keys = CALLOCATE(vm, uint32_t, oldMapLen);

    table_keys(&other->table, keys);

    MapObjEntry entry;
    for (size_t i = 0; i < oldMapLen; i++)
    {
        table_get(&other->table, keys[i], &entry);
        mapobj_set(newMap, entry.key, entry.value);
    }

    CFREE_ARRAY(vm, uint32_t, keys, oldMapLen);

    VM_POP(newMap); // newMap

    return newMap;
}

bool mapobj_set(MapObj* m, Value key, Value value)
{
    if (m == NULL)
        return false;

    MapObjEntry entry = {key, value};
    return table_set(&m->table, value_hash(key), &entry);
}

bool mapobj_get(MapObj* m, Value key, Value* value)
{
    if (m == NULL)
        return false;

    MapObjEntry entry;
    if (table_get(&m->table, value_hash(key), &entry))
    {
        if (value)
            *value = entry.value;
        return true;
    }

    return false;
}

bool mapobj_del(MapObj* m, Value key)
{
    if (m == NULL)
        return false;

    return table_del(&m->table, value_hash(key));
}

FuncObj* funcobj_new(VM* vm, FuncPtr func)
{
    FuncObj* funcObj = CALLOCATE_OBJ(vm, FuncObj, LLO_FUNCTION);
    funcObj->meta = value_nil();
    funcObj->func = func;
    return funcObj;
}

FuncObj* funcobj_clone(VM* vm, FuncObj* other)
{
    FuncObj* newFunc = funcobj_new(vm, other->func);
    newFunc->meta = other->meta;
    return newFunc;
}

EnvObj* envobj_new(VM* vm, EnvObj* outer)
{
    EnvObj* envObj = CALLOCATE_OBJ(vm, EnvObj, LLO_ENV);
    envObj->data = NULL;
    envObj->outer = NULL;

    VM_PUSH(envObj);
    envObj->data = mapobj_new(vm, 0);
    envObj->outer = outer;
    VM_POP(envObj);

    return envObj;
}

bool envobj_set(EnvObj* e, Value key, Value value)
{
    return mapobj_set(e->data, key, value);
}

bool envobj_get(EnvObj* e, Value key, Value* value)
{
    Value ret;
    EnvObj* current = e;

    while (!mapobj_get(current->data, key, &ret))
    {
        current = current->outer;

        if (current == NULL)
            return false;
    }

    *value = ret;
    return true;
}

ClosureObj* closureobj_new(VM* vm, EnvObj* env, Value params, Value body)
{
    ClosureObj* cobj = CALLOCATE_OBJ(vm, ClosureObj, LLO_CLOSURE);
    cobj->meta = value_nil();
    cobj->env = env;
    cobj->params = params;
    cobj->body = body;
    cobj->isMacro = false;

    return cobj;
}

Value closureobj_invoke(VM* vm, ClosureObj* cobj,
                        int len, Value* args,
                        ExceptionObj** exception)
{
    EnvObj* newEnv = envobj_new(vm, cobj->env);
    VM_PUSH(newEnv);

    // binding args
    ValueArray* paramsArr = value_listLikeGetArr(cobj->params);
    for (size_t i = 0; i < paramsArr->count; i++)
    {
        VALUE_ARR_GET_CHILD(paramsArr, i, paramValue);

        // variable arguments
        if (value_isSymbol(paramValue)
            && strobj_eq(value_asSymbol(paramValue)->symbol, "&", 1))
        {
            array_get(paramsArr, i + 1, &paramValue);

            if (i < len)
            {
                ListObj* varArgObj = listobj_newWithArr(vm,
                                                        len - i,
                                                        args + i);
                envobj_set(newEnv, paramValue, value_obj(varArgObj));
            }
            else
            {
                envobj_set(newEnv, paramValue, value_nil());
            }

            break;
        }

        if (i < len)
        {
            Value argValue = args[i];
            envobj_set(newEnv, paramValue, argValue);
        }
        else
        {
            envobj_set(newEnv, paramValue, value_nil());
        }
    }

    Value ret = vm_eval(vm, cobj->body, newEnv, exception);
    VM_POP(newEnv);

    return ret;
}

ClosureObj* closureobj_clone(VM* vm, ClosureObj* other)
{
    ClosureObj* cobj = closureobj_new(vm, other->env, other->params, other->body);
    cobj->meta = other->meta;
    return cobj;
}

AtomObj* atomobj_new(VM* vm, Value ref)
{
    AtomObj* aobj = CALLOCATE_OBJ(vm, AtomObj, LLO_ATOM);
    aobj->ref = ref;

    return aobj;
}

ExceptionObj* exceptionobj_new(VM* vm, const char* fmt, ...)
{
    char buffer[1024];

    ExceptionObj* eobj = CALLOCATE_OBJ(vm, ExceptionObj, LLO_EXCEPTION);
    eobj->info = NULL;

    VM_PUSH(eobj);
    va_list args;
    va_start(args, fmt);
    int length = vsnprintf(buffer, 1024, fmt, args);
    eobj->info = strobj_copy(vm, buffer, length);
    va_end(args);
    VM_POP(eobj);
    return eobj;
}

ValueArray* obj_listLikeGetArr(Obj* obj)
{
    if (obj_isList(obj))
    {
        return &obj_asList(obj)->items;
    }
    else if (obj_isVector(obj))
    {
        return &obj_asVector(obj)->items;
    }
    else
    {
        // RLOG_ERROR("RuntimeError: get array failed! obj is not a list or vector");
        return NULL;
    }
}

ValueArray* value_listLikeGetArr(Value v)
{
    if (value_isObj(v))
        return obj_listLikeGetArr(value_asObj(v));
    else
        return NULL;
}

Value obj_invoke(VM* vm, Obj* obj, int len, Value* args, ExceptionObj** exception)
{
    EnvObj* currentEnv = vm->currentEnv;
    VM_PUSH(currentEnv);
    Value ret;

    if (obj_isFunc(obj))
    {
        FuncObj* fobj = obj_asFunc(obj);
        ret = fobj->func(vm, len, args, exception);
    }
    else if (obj_isClosure(obj))
    {
        ClosureObj* cobj = obj_asClosure(obj);
        ret = closureobj_invoke(vm, cobj, len, args, exception);
    }
    else
    {
        ret = value_none();
    }

    vm->currentEnv = currentEnv;
    VM_POP(currentEnv);

    return ret;
}

Value value_invoke(VM* vm, Value value, int len, Value* args, ExceptionObj** exception)
{
    if (value_isObj(value))
        return obj_invoke(vm, value_asObj(value), len, args, exception);

    THROW("RuntimeError: value is not callable!");
    return value_none();
}

Value value_meta(Value value)
{
    if (value_hasMeta(value))
    {
        MetaObj* o = (MetaObj*)value_asObj(value);
        return o->meta;
    }

    return value_none();
}
