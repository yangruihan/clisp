#include "cmem.h"

#include "cprinter.h"
#include "cvm.h"

#define GC_HEAP_GROW_FACTOR 2

static void markRoots(VM* vm)
{
    MARK_OBJ(vm, vm->currentEnv);

    Obj* obj;
    for (size_t i = 0; i < vm->cmBlockArray.count; i++)
    {
        array_get(&vm->cmBlockArray, i, &obj);

#ifdef DEBUG_GC_DETAIL
        RLOG_DEBUG("-----B CompileBlock -- %p", obj);
        obj_print(obj);
#endif

        markObj(vm, obj);
    }

    for (size_t i = 0; i < vm->rtblockArray.count; i++)
    {
        array_get(&vm->rtblockArray, i, &obj);

#ifdef DEBUG_GC_DETAIL
        RLOG_DEBUG("-----B RuntimeBlock -- %p", obj);
        obj_print(obj);
#endif

        markObj(vm, obj);
    }
}

static void blackenObj(VM* vm, Obj* obj)
{
#ifdef DEBUG_GC_DETAIL
    RLOG_DEBUG("-----O blacken %p", obj);
    obj_print(obj);
#endif

    switch (obj->type)
    {
    case LLO_LIST:
    {
        ListObj* lobj = obj_asList(obj);
        markValue(vm, lobj->meta);

        Value temp;
        for (size_t i = 0; i < lobj->items.count; i++)
        {
            array_get(&lobj->items, i, &temp);
            markValue(vm, temp);
        }

        break;
    }

    case LLO_SYMBOL:
    {
        SymbolObj* sobj = obj_asSymbol(obj);
        MARK_OBJ(vm, sobj->symbol);
        break;
    }

    case LLO_STRING:
    {
        break;
    }

    case LLO_FUNCTION:
    {
        FuncObj* fobj = obj_asFunc(obj);
        markValue(vm, fobj->meta);
        break;
    }

    case LLO_KEYWORD:
    {
        KeywordObj* kobj = obj_asKeyword(obj);
        MARK_OBJ(vm, kobj->keyword);
        break;
    }

    case LLO_VECTOR:
    {
        VectorObj* vobj = obj_asVector(obj);
        markValue(vm, vobj->meta);

        Value temp;
        for (size_t i = 0; i < vobj->items.count; i++)
        {
            array_get(&vobj->items, i, &temp);
            markValue(vm, temp);
        }

        break;
    }

    case LLO_MAP:
    {
        MapObj* mobj = obj_asMap(obj);
        markValue(vm, mobj->meta);

        int len = mobj->table.count;
        uint32_t* keys = ALLOCATE(uint32_t, len); // use raw allocate function

        table_keys(&mobj->table, keys);

        MapObjEntry entry;
        for (size_t i = 0; i < len; i++)
        {
            table_get(&mobj->table, keys[i], &entry);
            markValue(vm, entry.key);
            markValue(vm, entry.value);
        }

        FREE_ARRAY(uint32_t, keys, len);

        break;
    }

    case LLO_ATOM:
    {
        AtomObj* aobj = obj_asAtom(obj);
        markValue(vm, aobj->ref);
        break;
    }

    case LLO_EXCEPTION:
    {
        ExceptionObj* eobj = obj_asException(obj);
        MARK_OBJ(vm, eobj->info);
        break;
    }

    case LLO_ENV:
    {
        EnvObj* eobj = obj_asEnv(obj);
        while (eobj)
        {
            MARK_OBJ(vm, eobj);
            if (eobj->data)
                MARK_OBJ(vm, eobj->data);

            eobj = eobj->outer;
        }

        break;
    }

    case LLO_CLOSURE:
    {
        ClosureObj* cobj = obj_asClosure(obj);
        markValue(vm, cobj->meta);

        EnvObj* eobj = cobj->env;
        while (eobj)
        {
            MARK_OBJ(vm, eobj);
            if (eobj->data)
                MARK_OBJ(vm, eobj->data);

            eobj = eobj->outer;
        }

        markValue(vm, cobj->params);
        markValue(vm, cobj->body);
        break;
    }

    default:
        break;
    }
}

static void traceReferences(VM* vm)
{
    Obj* obj;
    while (vm->grayObjArray.count > 0)
    {
        array_pop(&vm->grayObjArray, &obj);
        blackenObj(vm, obj);
    }
}

static void globalStringRemoveWhite(VM* vm)
{
    int len = vm->strings.count;

    if (len == 0)
        return;

    uint32_t* keys = ALLOCATE(uint32_t, len);

    table_keys(&vm->strings, keys);

    StrObj* temp;
    for (size_t i = 0; i < len; i++)
    {
        table_get(&vm->strings, keys[i], &temp);
        if (!temp->base.isMarked)
            table_del(&vm->strings, keys[i]);
    }

    FREE_ARRAY(uint32_t, keys, len);
}

static void sweep(VM* vm)
{
    Obj* prev = NULL;
    Obj* obj = vm->objs;

    while (obj)
    {
        if (obj->isMarked)
        {
            obj->isMarked = false;
            prev = obj;
            obj = obj->next;
        }
        else
        {
            Obj* temp = obj;

            obj = obj->next;
            if (prev)
                prev->next = obj;
            else
                vm->objs = obj;

#ifdef DEBUG_GC_DETAIL
            RLOG_DEBUG("-----X free %p", temp);
            obj_print(temp);
#endif

            obj_free(vm, temp);
        }
    }
}

void* creallocate(VM* vm, void* previous, size_t oldSize, size_t newSize)
{
    vm->bytesAllocated += newSize - oldSize;

    if (newSize > oldSize)
    {
#if DEBUG_PRESS_GC
        ccollectGarbage(vm);
#endif

        if (vm->bytesAllocated > vm->nextGC)
            ccollectGarbage(vm);
    }

    return reallocate(previous, oldSize, newSize);
}

void ccollectGarbage(VM* vm)
{
#if DEBUG_GC
    RLOG_DEBUG("--- cmal gc begin\n");
    size_t before = vm->bytesAllocated;
#endif

    markRoots(vm);

    traceReferences(vm);

    globalStringRemoveWhite(vm);
    sweep(vm);

    vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_GC
    RLOG_DEBUG("--- cmal gc end\n");
    RLOG_DEBUG("   collected %ld bytes (from %ld to %ld) next at %ld\n",
           before - vm->bytesAllocated,
           before,
           vm->bytesAllocated,
           vm->nextGC);
#endif
}

void markValue(VM* vm, Value value)
{
    if (!value_isObj(value)) return;
    markObj(vm, value_asObj(value));
}

void markObj(VM* vm, Obj* obj)
{
    if (obj == NULL) return;
    if (obj->isMarked) return;

#ifdef DEBUG_GC_DETAIL
    RLOG_DEBUG("-----* mark %p", obj);
    obj_print(obj);
#endif

    obj->isMarked = true;
    array_push(&vm->grayObjArray, &obj);
}
