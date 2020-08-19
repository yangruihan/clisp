#include "cvm.h"

#include "creader.h"
#include "cprinter.h"
#include "cvalue.h"
#include "cobj.h"
#include "ccorelib.h"
#include "cmem.h"

#define ENV_SET_FUNC(chars, len, func) \
    do \
    { \
        Value sv = value_symbol(vm, (chars), (len)); \
        VM_PUSHV(sv); \
        envobj_set(vm->env, sv, value_func(vm, &(func))); \
        VM_POPV(sv); \
    } while (false)

#define EXPAND_TO(chars, len, secondValue) \
    Value symbol = value_symbol(vm, (chars), (len)); \
    VM_PUSHV(symbol); \
    Value ret = value_list(vm, 2, symbol, (secondValue)); \
    VM_POPV(symbol)

static Value   READ(VM* vm, const char* input);
static Value   EVAL(VM* vm, Value value, EnvObj* env, ExceptionObj** exception);
static StrObj* PRINT(VM* vm, Value value);

Value evalAst(VM* vm, Value value, EnvObj* env, ExceptionObj** exception)
{
    if (value_isSymbol(value))
    {
        Value ret;
        if (envobj_get(env, value, &ret))
        {
            return ret;
        }
        else
        {
            THROW("RuntimeError: symbol (%s) not found in env",
                  value_asSymbol(value)->symbol->chars);
            return value_none();
        }
    }
    else if (value_isList(value))
    {
        ListObj* listObj = value_asList(value);
        int len = listObj->items.count;

        ListObj* retList = listobj_newWithNil(vm, len);
        VM_PUSH(retList);

        Value temp;
        for (size_t i = 0; i < len; i++)
        {
            listobj_get(listObj, i, &temp);
            listobj_set(retList, i, EVAL(vm, temp, env, exception));
            if (HAS_EXCEPTION())
            {
                VM_POP(retList); // retList
                return value_none();
            }
        }

        VM_POP(retList); // retList
        return value_obj(retList);
    }

    return value;
}

Value READ(VM* vm, const char* input)
{
    return readStr(vm, input);
}

static Value quasiquote(VM* vm, Value listArg)
{
    if (!value_isPair(listArg))
    {
        EXPAND_TO("quote", 5, listArg);
        return ret;
    }
    else
    {
        ValueArray* va = value_listLikeGetArr(listArg);
        VALUE_ARR_GET_CHILD(va, 0, quasiFirstValue);
        if (value_symbolIs(quasiFirstValue, "unquote"))
        {
            VALUE_ARR_GET_CHILD(va, 1, quasiSecondValue);
            return quasiSecondValue;
        }
        else
        {
            do
            {
                if (value_isPair(quasiFirstValue))
                {
                    ValueArray* va2 = value_listLikeGetArr(quasiFirstValue);
                    VALUE_ARR_GET_CHILD(va2, 0, quasiChildFirstValue);

                    if (value_symbolIs(quasiChildFirstValue, "splice-unquote"))
                    {
                        Value sv = value_symbol(vm, "concat", 6);
                        VM_PUSHV(sv);

                        VALUE_ARR_GET_CHILD(va2, 1, quasiSecondValue);

                        Value remainValue = value_listWithArr(vm,
                                                              va->count - 1,
                                                              (Value*)va->data + 1);
                        VM_PUSHV(remainValue);
                        Value handledRemainValue = quasiquote(vm, remainValue);
                        VM_POPV(remainValue);

                        VM_PUSHV(handledRemainValue);

                        VALUE_ARR_GET_CHILD(va2, 1, quasiChildSecondValue);
                        Value ret = value_list(vm, 3, sv, quasiChildSecondValue, handledRemainValue);

                        VM_POPV(handledRemainValue);
                        VM_POPV(sv);

                        return ret;
                    }
                }

                Value sv = value_symbol(vm, "cons", 4);
                VM_PUSHV(sv);

                Value handledQuasiFirstValue = quasiquote(vm, quasiFirstValue);
                VM_PUSHV(handledQuasiFirstValue);

                Value remainValue = value_listWithArr(vm,
                                                      va->count - 1,
                                                      (Value*)va->data + 1);
                VM_PUSHV(remainValue);
                Value handledRemainValue = quasiquote(vm, remainValue);
                VM_POPV(remainValue);

                VM_PUSHV(handledRemainValue);

                Value ret = value_list(vm, 3, sv, handledQuasiFirstValue, handledRemainValue);

                VM_POPV(handledRemainValue);
                VM_POPV(handledQuasiFirstValue);
                VM_POPV(sv);

                return ret;

            } while (false);
        }
    }
}

static bool isMacroCall(VM* vm, Value v, EnvObj* env, /* out */ ClosureObj** cobj)
{
    if (!value_isList(v))
        return false;

    LIST_GET_CHILD(value_asList(v), 0, symbolValue);
    Value funcValue;
    if (!envobj_get(env, symbolValue, &funcValue))
        return false;

    if (!value_isMacro(funcValue))
        return false;

    if (cobj)
        *cobj = value_asClosure(funcValue);

    return true;
}

static bool macroExpand(VM* vm, Value v, EnvObj* env, ExceptionObj** exception,
                        /* out */ Value* out)
{
    Value currentValue = v;
    ClosureObj* cobj;
    bool expandFlag = false;

    while (isMacroCall(vm, currentValue, env, &cobj))
    {
        expandFlag = true;

        ListObj* lobj = value_asList(currentValue);
        VM_PUSH(lobj);

        currentValue = closureobj_invoke(vm, cobj,
                                         lobj->items.count - 1,
                                         (Value*)lobj->items.data + 1,
                                         exception);

        VM_POP(lobj); // lobj

        if (HAS_EXCEPTION())
        {
            *out = value_none();
            return false;
        }
    }

    *out = currentValue;
    return expandFlag;
}

static int s_EvalDepth = 0;

Value EVAL(VM* vm, Value value, EnvObj* env, ExceptionObj** exception)
{
#define SYMBOL_IS(str) (strcmp(sobj->symbol->chars, str) == 0)

#if DEBUG_TRACE_GC
#define RETURN_VALUE(value) \
    do \
    { \
        Value __ret = (value); \
        Obj* o; \
        for (int i = gcTraceArr.count - 1; i >= 0; i--) \
        { \
            /*RLOG_ERROR("EEEE POP------------------------------ %d", s_EvalDepth);*/ \
            array_get(&gcTraceArr, i, &o); \
            VM_POP(o); \
        } \
        /*RLOG_ERROR("EEEE RETURN------------------------------ %d", s_EvalDepth);*/ \
        s_EvalDepth--; \
        array_free(&gcTraceArr); \
        vm->currentEnv = oldEnv; /* restore current env */ \
        vm->callDepth--; \
        for (int i = 0; i < closureNum; i++) \
        { \
            array_pop(&vm->closureStackIdx, NULL); \
            array_pop(&vm->closureStackCallDepth, NULL); \
            array_pop(&vm->closureStack, NULL); \
        } \
        return (__ret); \
    } while (false)
#else
#define RETURN_VALUE(value) \
    do \
    { \
        Value __ret = (value); \
        for (int i = 0; i < blockStackCount; i++) VM_POP(NULL); /* pop value and env */ \
        vm->currentEnv = oldEnv; /* restore current env */ \
        vm->callDepth--; \
        for (int i = 0; i < closureNum; i++) \
        { \
            array_pop(&vm->closureStackIdx, NULL); \
            array_pop(&vm->closureStackCallDepth, NULL); \
            array_pop(&vm->closureStack, NULL); \
        } \
        /*RLOG_ERROR("EEEE RETURN------------------------------ %d", s_EvalDepth);*/ \
        s_EvalDepth--; \
        return (__ret); \
    } while (false)
#endif

    vm->callDepth++;
    int closureNum = 0;

    /* record currentEnv */
    EnvObj* oldEnv = vm->currentEnv;

    int blockStackCount = 0;

#if DEBUG_TRACE_GC
    Array gcTraceArr;
    ARR_INIT(&gcTraceArr, Obj*);
#endif

    s_EvalDepth++;
    // RLOG_ERROR("EEEE INIT------------------------------ %d", s_EvalDepth);

    for(;;)
    {
        if (vm->currentEnv != env)
        {
            vm->currentEnv = env;
            VM_PUSH(env);
            blockStackCount++;

#if DEBUG_TRACE_GC
            array_push(&gcTraceArr, &env);
#endif

            // RLOG_ERROR("EEEE PUSH------------------------------ %d", s_EvalDepth);
        }

        if (value_isObj(value))
        {
            VM_PUSH(value_asObj(value));
            blockStackCount++;

#if DEBUG_TRACE_GC
            array_push(&gcTraceArr, &value_asObj(value));
#endif

            // if (value_isList(value))
            // {
            //     StrObj* s = printRawStr(vm, value);
            //     RLOG_ERROR("EEEE List %s", s->chars);
            // }

            // RLOG_ERROR("EEEE PUSH------------------------------ %d", s_EvalDepth);
        }
        

        if (value_isList(value))
        {
            ListObj* lobj = value_asList(value);

            if (lobj->items.count == 0)
            {
                DTRACE(vm, "EVAL list count is 0, return value");

                RETURN_VALUE(value);
            }
            else
            {
                if (macroExpand(vm, value, env, exception, &value))
                    goto CONTINUE_LOOP;

                if (HAS_EXCEPTION())
                    RETURN_VALUE(value);

                do
                {
                    LIST_GET_CHILD(lobj, 0, firstValue);

                    if (value_isSymbol(firstValue))
                    {
                        SymbolObj* sobj = value_asSymbol(firstValue);

                        if (SYMBOL_IS("def!"))
                        {
                            DTRACE(vm, "EVAL def!");

                            LIST_GET_CHILD(lobj, 1, key);
                            LIST_GET_CHILD(lobj, 2, value);
                            value = EVAL(vm, value, env, exception);

                            if (!HAS_EXCEPTION())
                                envobj_set(env, key, value);

                            RETURN_VALUE(value);
                        }
                        else if (SYMBOL_IS("let*"))
                        {
                            DTRACE(vm, "EVAL let*");

                            EnvObj* newEnv = envobj_new(vm, env);
                            VM_PUSH(newEnv);

                            LIST_GET_CHILD(lobj, 1, bindingList);

                            ValueArray* itemArray = value_listLikeGetArr(bindingList);
                            for (size_t i = 0; i < itemArray->count; i = i + 2)
                            {
                                VALUE_ARR_GET_CHILD(itemArray, i, key);
                                VALUE_ARR_GET_CHILD(itemArray, i + 1, value);
                                value = EVAL(vm, value, newEnv, exception);

                                if (!HAS_EXCEPTION())
                                {
                                    envobj_set(newEnv, key, value);
                                }
                                else
                                {
                                    VM_POP(newEnv); // newEnv
                                    RETURN_VALUE(value_none());
                                }
                            }

                            LIST_GET_CHILD(lobj, 2, statements);
                            value = statements;
                            env = newEnv;
                            VM_POP(newEnv); // newEnv

                            goto CONTINUE_LOOP;
                        }
                        else if (SYMBOL_IS("do"))
                        {
                            DTRACE(vm, "EVAL do");

                            if (lobj->items.count == 1)
                                RETURN_VALUE(value_nil());
                            else
                            {
                                for (int i = 1; i < lobj->items.count - 1; i++)
                                {
                                    LIST_GET_CHILD(lobj, i, child);
                                    EVAL(vm, child, env, exception);

                                    if (HAS_EXCEPTION())
                                        RETURN_VALUE(value_none());
                                }

                                LIST_GET_CHILD(lobj, lobj->items.count - 1, child);
                                value = child;
                                goto CONTINUE_LOOP;
                            }
                        }
                        else if (SYMBOL_IS("if"))
                        {
                            DTRACE(vm, "EVAL if");

                            LIST_GET_CHILD(lobj, 1, condValue);
                            Value condRet = EVAL(vm, condValue, env, exception);

                            if (HAS_EXCEPTION())
                                RETURN_VALUE(value_none());

                            if (value_false(condRet))
                            {
                                if (lobj->items.count == 4)
                                    listobj_get(lobj, 3, &value);
                                else
                                    value = value_nil();
                            }
                            else
                            {
                                listobj_get(lobj, 2, &value);
                            }

                            goto CONTINUE_LOOP;
                        }
                        else if (SYMBOL_IS("fn*"))
                        {
                            DTRACE(vm, "EVAL fn*");

                            LIST_GET_CHILD(lobj, 1, params);
                            LIST_GET_CHILD(lobj, 2, body);
                            RETURN_VALUE(value_closure(vm, env, params, body));
                        }
                        else if (SYMBOL_IS("quote"))
                        {
                            DTRACE(vm, "EVAL quote");

                            LIST_GET_CHILD(lobj, 1, value);
                            RETURN_VALUE(value);
                        }
                        else if (SYMBOL_IS("quasiquote"))
                        {
                            DTRACE(vm, "EVAL quasiquote");

                            LIST_GET_CHILD(lobj, 1, listArg);
                            value = quasiquote(vm, listArg);
                            goto CONTINUE_LOOP;
                        }
                        else if (SYMBOL_IS("defmacro!"))
                        {
                            DTRACE(vm, "EVAL defmacro!");

                            LIST_GET_CHILD(lobj, 2, funcArg);
                            Value evaluatedFunc = EVAL(vm, funcArg, env, exception);

                            if (HAS_EXCEPTION())
                                RETURN_VALUE(value_none());

                            if (!value_isClosure(evaluatedFunc))
                            {
                                THROW("RuntimeError: defmacro! body is not a closure");
                                RETURN_VALUE(value_none());
                            }

                            VM_PUSHV(evaluatedFunc);
                            ClosureObj* funcClone = closureobj_clone(vm,
                                                                     value_asClosure(evaluatedFunc));
                            VM_POPV(evaluatedFunc);

                            funcClone->isMacro = true;

                            Value ret = value_obj(funcClone);
                            LIST_GET_CHILD(lobj, 1, key);
                            envobj_set(env, key, ret);
                            RETURN_VALUE(ret);
                        }
                        else if (SYMBOL_IS("macroexpand"))
                        {
                            DTRACE(vm, "EVAL macroexpand");

                            LIST_GET_CHILD(lobj, 1, macroValue);
                            macroExpand(vm, macroValue, env, exception, &macroValue);
                            RETURN_VALUE(macroValue);
                        }
                        else if (SYMBOL_IS("try*"))
                        {
                            DTRACE(vm, "EVAL try*");

                            LIST_GET_CHILD(lobj, 1, protectedBody);
                            Value ret = EVAL(vm, protectedBody, env, exception);

                            do
                            {
                                if (!HAS_EXCEPTION())
                                    break;

                                if (lobj->items.count < 3)
                                    break;

                                LIST_GET_CHILD(lobj, 2, catchBody);
                                if (!value_isPair(catchBody))
                                    break;

                                ValueArray* catchArr = value_listLikeGetArr(catchBody);
                                VALUE_ARR_GET_CHILD(catchArr, 0, catchSymbol);

                                if (value_symbolIs(catchSymbol, "catch*"))
                                {
                                    DTRACE(vm, "EVAL catch*");

                                    VM_PUSH(*exception);
                                    VM_PUSHV(ret);

                                    EnvObj* newEnv = envobj_new(vm, env);

                                    VM_POPV(ret);
                                    VM_POP(*exception); // *exception
                                    VM_PUSH(newEnv);

                                    VALUE_ARR_GET_CHILD(catchArr, 1, exceptionVar);

                                    envobj_set(newEnv, exceptionVar, value_obj(*exception));

                                    VALUE_ARR_GET_CHILD(catchArr, 2, handleBody);

                                    *exception = NULL;

                                    value = handleBody;
                                    env = newEnv;
                                    VM_POP(newEnv); // newEnv

                                    goto CONTINUE_LOOP;
                                }

                            } while (false);

                            RETURN_VALUE(ret);
                        }
                    }

                    /* ---- func ---- */

#if DEBUG_TRACE
                    if (value_isList(value))
                    {
                        LIST_GET_CHILD(value_asList(value), 0, funcSymbolValue);
                        StrObj* s = printStr(vm, value, true);
                        DTRACE(vm, "EVAL call %s\n", s->chars);
                    }
#endif

                    Value funcListValue = evalAst(vm, value, env, exception);

                    if (HAS_EXCEPTION())
                        RETURN_VALUE(value_none());

                    if (!value_isList(funcListValue))
                    {
                        THROW("RuntimeError: list value exception");
                        RETURN_VALUE(value_none());
                    }

                    VM_PUSHV(funcListValue);

                    ListObj* funcListObj = value_asList(funcListValue);

                    LIST_GET_CHILD(funcListObj, 0, funcValue);

                    if (value_isClosure(funcValue))
                    {
                        ClosureObj* cobj = value_asClosure(funcValue);
                        int len = funcListObj->items.count - 1;
                        Value* args = (Value*)funcListObj->items.data + 1;

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

                        env = newEnv;
                        value = cobj->body;

                        VM_POP(newEnv);
                        VM_POPV(funcListValue);

                        // check tail recur
                        do
                        {
                            if (vm->closureStack.count > 0)
                            {
                                int index = vm->closureStack.count - 1;
                                
                                ClosureObj* topCobj;
                                array_get(&vm->closureStack, index, &topCobj);

                                int callDepth;
                                array_get(&vm->closureStackCallDepth, index, &callDepth);

                                // tail recur
                                if (vm->callDepth == callDepth && topCobj == cobj)
                                {
                                    int stackIndex;
                                    array_get(&vm->closureStackIdx, index, &stackIndex);
                                    
                                    while (vm->rtblockArray.count > stackIndex)
                                    {
                                        VM_POP(NULL);
                                        blockStackCount--;
                                    }

                                    break;
                                }
                            }

                            closureNum++;
                            array_push(&vm->closureStackIdx, &vm->rtblockArray.count);
                            array_push(&vm->closureStackCallDepth, &vm->callDepth);
                            array_push(&vm->closureStack, &cobj);
                        } while (false);

                        goto CONTINUE_LOOP;
                    }
                    else
                    {
                        Value ret = value_invoke(vm, funcValue,
                                                 funcListObj->items.count - 1,
                                                 (Value*)funcListObj->items.data + 1,
                                                 exception);

                        VM_POPV(funcListValue);

                        // has exception
                        if (value_isNone(ret))
                            RETURN_VALUE(ret);

                        RETURN_VALUE(ret);
                    }
                } while (false);
            }
        }
        else if (value_isVector(value))
        {
            VectorObj* vobj = value_asVector(value);
            int len = vobj->items.count;
            VectorObj* ret = vectorobj_newWithNil(vm, len);
            VM_PUSH(ret);

            Value temp;
            for (size_t i = 0; i < len; i++)
            {
                vectorobj_get(vobj, i, &temp);
                vectorobj_set(ret, i, EVAL(vm, temp, env, exception));

                if (HAS_EXCEPTION())
                {
                    VM_POP(ret); // ret
                    RETURN_VALUE(value_none());
                }
            }

            VM_POP(ret); // ret
            RETURN_VALUE(value_obj(ret));
        }
        else if (value_isMap(value))
        {
            MapObj* oldMap = value_asMap(value);

            MapObj* newMap = mapobj_new(vm, 0);
            VM_PUSH(newMap);

            int len = oldMap->table.count;

            uint32_t* keys = CALLOCATE(vm, uint32_t, len);
            table_keys(&oldMap->table, keys);

            MapObjEntry entry;
            for (size_t i = 0; i < len; i++)
            {
                table_get(&oldMap->table, keys[i], &entry);
                Value newValue = EVAL(vm, entry.value, env, exception);

                if (HAS_EXCEPTION())
                {
                    CFREE_ARRAY(vm, uint32_t, keys, len);
                    VM_POP(newMap); // newMap
                    RETURN_VALUE(value_none());
                }

                MapObjEntry newEntry = {entry.key, newValue};
                table_set(&newMap->table, keys[i], &newEntry);
            }

            CFREE_ARRAY(vm, uint32_t, keys, len);
            VM_POP(newMap); // newMap

            RETURN_VALUE(value_obj(newMap));
        }

        RETURN_VALUE(evalAst(vm, value, env, exception));

        CONTINUE_LOOP:;
    }

#undef RETURN_VALUE
#undef SYMBOL_IS
}

StrObj* PRINT(VM* vm, Value value)
{
    return printReadablyStr(vm, value);
}

VM* vm_create()
{
    VM* vm = ALLOCATE(VM, 1);
    vm->env = NULL;
    vm->currentEnv = NULL;

    vm->callDepth = 0;
    ARR_INIT(&vm->closureStackIdx, int);
    ARR_INIT(&vm->closureStackCallDepth, int);
    ARR_INIT(&vm->closureStack, ClosureObj*);

    /* init gc */
    vm->objs = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = 1024 * 1024;
    ARR_INIT(&vm->grayObjArray, Obj*);
    ARR_INIT(&vm->cmBlockArray, Obj*);
    ARR_INIT(&vm->rtblockArray, Obj*);

    TABLE_INIT(&vm->strings, StrObj*);

    vm->env = envobj_new(vm, NULL);
    vm->currentEnv = vm->env;

    initCoreLib(vm);

    // set *host-language*
    vm_rep(vm, "(def! *host-language* \"c\")");

    // define not
    vm_rep(vm, "(def! not (fn* [a] (if a false true)))");

    // define load file
    vm_rep(vm, "(def! load-file (fn* [f] (eval (read-string (str \"(do \" (slurp f) \"\nnil)\")))))");

    // define cond
    vm_rep(vm, "(defmacro! cond (fn* [& xs] (if (> (count xs) 0) (list 'if (first xs) (if (> (count xs) 1) (nth xs 1) (throw \"odd number of forms to cond\")) (cons 'cond (rest (rest xs)))))))");

    return vm;
}

void vm_free(VM* vm)
{
    table_free(&vm->strings);

    vm->env = NULL;
    vm->currentEnv = NULL;

    vm->callDepth = 0;
    array_free(&vm->closureStackIdx);
    array_free(&vm->closureStackCallDepth);
    array_free(&vm->closureStack);

    array_free(&vm->cmBlockArray);
    array_free(&vm->rtblockArray);
    array_free(&vm->grayObjArray);

    ccollectGarbage(vm);

    vm->objs = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = 0;

    FREE(VM, vm);
}

const char* vm_rep(VM* vm, const char* input)
{
    Value astRoot = READ(vm, input);
    if (value_isNone(astRoot))
        return "";

    // vm_pushAstRoot(vm, astRoot);
    vm_clearBlockCmArr(vm);

    ExceptionObj* exceptionPtr = NULL;
    Value evalRet = EVAL(vm, astRoot, vm->env, &exceptionPtr);
    vm->currentEnv = vm->env;
    // vm_popAstRoot(vm);
    VM_PUSHV(evalRet);

    // TODO handle exception
    if (exceptionPtr)
    {
        RLOG_ERROR("%s", exceptionPtr->info->chars);
    }

    StrObj* ret = PRINT(vm, evalRet);

    VM_POPV(evalRet);

    return ret == NULL ? "" : ret->chars;
}

Value vm_eval(VM* vm, Value value, EnvObj* env, ExceptionObj** exception)
{
    DTRACE(vm, "vm_eval");

    Value ret = EVAL(vm, value, env, exception);
    return ret;
}

void vm_dofile(VM* vm, const char* filePath, int argc, char** argv)
{
    // set *ARGV* variable
    Value argvSymbol = value_symbol(vm, "*ARGV*", 6);
    VM_PUSHV(argvSymbol);

    if (argc > 0)
    {
        ListObj* l = listobj_newWithNil(vm, argc);
        VM_PUSH(l);

        for (size_t i = 0; i < argc; i++)
        {
            int slen = strlen(argv[i]);
            listobj_set(l, i, value_str(vm, argv[i], slen));
        }

        envobj_set(vm->env, argvSymbol, value_obj(l));
        VM_POP(l); // l
    }
    else
    {
        envobj_set(vm->env, argvSymbol, value_nil());
    }

    VM_POPV(argvSymbol);

    int cmdLen = 15 + strlen(filePath);
    char* cmd = CALLOCATE(vm, char, cmdLen);
    sprintf(cmd, "(load-file \"%s\")", filePath);
    cmd[cmdLen - 1] = '\0';

    vm_rep(vm, cmd);

    CFREE_ARRAY(vm, char, cmd, cmdLen);
}

void vm_pushBlockCmObj(VM* vm, Obj* obj)
{
    array_push(&vm->cmBlockArray, &obj);
}

void vm_popBlockCmObj(VM* vm)
{
    array_pop(&vm->cmBlockArray, NULL);
}

void vm_clearBlockCmArr(VM* vm)
{
    array_clear(&vm->cmBlockArray);
}

void vm_pushBlockRtObj(VM* vm, Obj* obj)
{
#ifdef DEBUG_TRACE_GC
    RLOG_DEBUG("++++++ RuntimeBlock Push Obj %p", obj);
    obj_print(obj);
#endif

    array_push(&vm->rtblockArray, &obj);
}

#ifdef DEBUG_TRACE_GC
void vm_popBlockRtObj(VM* vm, Obj* obj)
#else
void vm_popBlockRtObj(VM* vm)
#endif
{
#ifdef DEBUG_TRACE_GC
    Obj* outObj;
    array_pop(&vm->rtblockArray, &outObj);
    RLOG_DEBUG("++++++ RuntimeBlock Pop Obj %p", outObj);
    obj_print(outObj);

    if (!obj_eq(vm, outObj, obj))
    {
        RLOG_ERROR("XXXXXX RuntimeBlock Pop Error , obj is not at stack top!!!");
        obj_print(obj);
    }
#else
    array_pop(&vm->rtblockArray, NULL);
#endif
}

void vm_clearBlockRtArr(VM* vm)
{
    array_clear(&vm->rtblockArray);
}
