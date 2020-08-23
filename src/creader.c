#include "creader.h"

#include <setjmp.h>

#include "cvalue.h"
#include "cobj.h"
#include "cvm.h"
#include "cmem.h"

#define reportScannerError(s, errorCode) longjmp(s_jmpBuf, errorCode)

#define reportReaderError(r, token, errorCode) \
    do { r->errorToken = token; longjmp(s_jmpBuf, errorCode); } while(false)

#define VM_CPUSH(obj) vm_pushBlockCmObj(vm, ((Obj*)obj))

#define VM_CPUSHV(val) \
    do { \
        if (!value_isObj((val))) break; \
        vm_pushBlockCmObj(vm, value_asObj((val))); \
    } while (false)

typedef struct
{
    const char* start;
    const char* current;
    int         line;
} Scanner;

static jmp_buf s_jmpBuf;

/* ----- scanner ----- */

static void scanner_init(Scanner* s, const char* source)
{
    s->current = source;
    s->start = source;
    s->line = 1;
}

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool isIllegalSymbol(char c)
{
    return c == ' '  || c == '\r' || c == '\t' ||
           c == '\n' || c == '['  || c == ']'  ||
           c == ']'  || c == '{'  || c == '}'  ||
           c == '('  || c == '\'' || c == '"'  ||
           c == '`'  || c == ','  || c == ';'  ||
           c == ')';
}

static bool scanner_isAtEnd(Scanner* s)
{
    return *s->current == '\0';
}

static char scanner_next(Scanner* s)
{
    s->current++;
    return s->current[-1];
}

static char scanner_peek(Scanner* s)
{
    return *s->current;
}

static char scanner_peekPrev(Scanner* s)
{
    if (s->current == s->start)
        return '\0';
    return s->current[-1];
}

static bool scanner_match(Scanner* s, const char expected)
{
    if (scanner_isAtEnd(s)) return false;
    if (*s->current != expected) return false;
    s->current++;
    return true;
}

static Token scanner_makeToken(Scanner* s)
{
    Token t;
    t.start = s->start;
    t.len = (int)(s->current - s->start);
    t.line = s->line;
    return t;
}

/**
 * Skip whitespaces, commas, comments
 */
static void scanner_skipIgnoredPart(Scanner* s)
{
    for (;;)
    {
        char c = scanner_peek(s);
        switch (c)
        {
            // skip whitespaces
            case ' ':
            case '\r':
            case '\t':
                scanner_next(s);
                break;

            case '\n':
                s->line++;
                scanner_next(s);
                break;

            // skip commas
            case ',':
                scanner_next(s);
                break;

            // skip comments
            case ';':
                while (scanner_peek(s) != '\n' && !scanner_isAtEnd(s))
                    scanner_next(s);
                s->line++;
                break;

            default:
                return;
        }
    }
}

static Token scanner_string(Scanner* s)
{
    char c = scanner_peek(s);
    while (c != '"' && !scanner_isAtEnd(s))
    {
        if (c == '\n')
            s->line++;

        scanner_next(s);
        char nc = scanner_peek(s);

        if (c == '\\' && (nc == '\\' || nc == '"'))
        {
            scanner_next(s);
            nc = scanner_peek(s);
        }

        c = nc;
    }

    if (scanner_isAtEnd(s))
    {
        reportScannerError(s, EC_NoMatchQuote);
    }

    // consume closed "
    scanner_next(s);
    return scanner_makeToken(s);
}

static Token scanner_scanToken(Scanner* s)
{
    scanner_skipIgnoredPart(s);

    s->start = s->current;
    if (scanner_isAtEnd(s)) return scanner_makeToken(s);

    char c = scanner_next(s);

    switch (c)
    {
        case '~':
            // ~@
            if (scanner_match(s, '@'))
                return scanner_makeToken(s);

            // ~
            return scanner_makeToken(s);

        // special single character
        case '[':
        case ']':
        case '{':
        case '}':
        case '(':
        case ')':
        case '\'':
        case '`':
        case '^':
        case '@':
            return scanner_makeToken(s);

        // string
        case '"':
            return scanner_string(s);

        // other symbol, numbers, true, false, nil
        default:
            while (!isIllegalSymbol(scanner_peek(s)) && !scanner_isAtEnd(s))
                scanner_next(s);

            return scanner_makeToken(s);
    }
}

static Array scanner_tokenize(Scanner* s, const char* source)
{
    Array _;
    Array* tokens = &_;
    ARR_INIT(tokens, Token);

    while (!scanner_isAtEnd(s))
    {
        Token t = scanner_scanToken(s);
        array_push(tokens, &t);
    }

    return *tokens;
}

/* ----- end scanner ----- */

/* ----- token ----- */

void token_print(Token* t)
{
    if (t == NULL)
    {
        RLOG_ERROR("Token is NULL");
        return;
    }

    char* temp = ALLOCATE(char, t->len + 1);
    memcpy(temp, t->start, t->len);
    temp[t->len] = '\0';
    RLOG_DEBUG("<Token %s>", temp);
    FREE_ARRAY(char, temp, t->len + 1);
}

/* ----- end token ----- */

/* ----- reader ----- */

void reader_init(Reader* r, Array tokens)
{
    r->position = 0;
    r->tokens = tokens;
}

void reader_free(Reader* r)
{
    r->position = 0;
    array_free(&r->tokens);
}

Token reader_prev(Reader* r)
{
    Token ret;
    array_get(&r->tokens, r->position - 1, &ret);
    return ret;
}

Token reader_next(Reader* r)
{
    r->position++;
    Token ret;
    array_get(&r->tokens, r->position - 1, &ret);
    return ret;
}

Token reader_peek(Reader* r)
{
    Token ret;
    array_get(&r->tokens, r->position, &ret);
    return ret;
}

bool reader_isAtEnd(Reader* r)
{
    return r->position >= r->tokens.count;
}

/* ----- end reader ----- */

static bool tokenMatch(Token* t, const char* expectedString, const int len)
{
    if (t->len != len)
        return false;

    return memcmp(t->start, expectedString, len) == 0;
}

static Value readList(VM* vm, Reader* r);
static Value readVector(VM* vm, Reader* r);
static Value readMap(VM* vm, Reader* r);
static Value readAtom(VM* vm, Reader* r);
static Value readForm(VM* vm, Reader* r);

static Value readList(VM* vm, Reader* r)
{
    static Value s_listItemBuf[LIST_MAX_ITEM_COUNT];
    static int s_listItemBufIdx = 0;

    // consume '('
    reader_next(r);

    if (reader_isAtEnd(r))
    {
        reportReaderError(r, reader_prev(r), EC_NoMatchRightParen);
    }

    // empty list
    if (*reader_peek(r).start == ')')
    {
        // consume ')'
        reader_next(r);

        Value ret = value_listWithEmpty(vm);
        VM_CPUSHV(ret);
        return ret;
    }

    int listItemBuffStart = s_listItemBufIdx;
    int listItemBuffCurr = listItemBuffStart;

    while (*reader_peek(r).start != ')')
    {
        s_listItemBufIdx = listItemBuffCurr + 1;
        s_listItemBuf[listItemBuffCurr] = readForm(vm, r);
        listItemBuffCurr++;

        if (reader_isAtEnd(r))
        {
            reportReaderError(r, reader_prev(r), EC_NoMatchRightParen);
        }
    }

    s_listItemBufIdx = listItemBuffStart;

    // consume ')'
    reader_next(r);

    Value ret = value_listWithArr(vm,
                                  listItemBuffCurr - listItemBuffStart, 
                                  s_listItemBuf + listItemBuffStart);
    VM_CPUSHV(ret);
    return ret;
}

static Value readVector(VM* vm, Reader* r)
{
    static Value s_vectorItemBuf[LIST_MAX_ITEM_COUNT];
    static int s_vectorItemBufIdx = 0;

    // consume '['
    reader_next(r);

    if (reader_isAtEnd(r))
    {
        reportReaderError(r, reader_prev(r), EC_NoMatchRightSquareBracket);
    }

    // empty list
    if (*reader_peek(r).start == ']')
    {
        // consume ']'
        reader_next(r);

        Value ret = value_vectorWithEmpty(vm);
        VM_CPUSHV(ret);
        return ret;
    }

    int listItemBuffStart = s_vectorItemBufIdx;
    int listItemBuffCurr = listItemBuffStart;

    while (*reader_peek(r).start != ']')
    {
        s_vectorItemBufIdx = listItemBuffCurr + 1;
        s_vectorItemBuf[listItemBuffCurr] = readForm(vm, r);
        listItemBuffCurr++;

        if (reader_isAtEnd(r))
        {
            reportReaderError(r, reader_prev(r), EC_NoMatchRightSquareBracket);
        }
    }

    s_vectorItemBufIdx = listItemBuffStart;

    // consume ']'
    reader_next(r);

    Value ret = value_vectorWithArr(vm,
                                    listItemBuffCurr - listItemBuffStart, 
                                    s_vectorItemBuf + listItemBuffStart);
    VM_CPUSHV(ret);
    return ret;
}

static Value readMap(VM* vm, Reader* r)
{
    static Value s_mapItemBuf[LIST_MAX_ITEM_COUNT];
    static int s_mapItemBufIdx = 0;

    // consume '{'
    reader_next(r);

    if (reader_isAtEnd(r))
    {
        reportReaderError(r, reader_prev(r), EC_NoMatchRightCurlyBracket);
    }

    // empty list
    if (*reader_peek(r).start == '}')
    {
        // consume '}'
        reader_next(r);

        Value ret = value_mapWithEmpty(vm);
        VM_CPUSHV(ret);
        return ret;
    }

    int listItemBuffStart = s_mapItemBufIdx;
    int listItemBuffCurr = listItemBuffStart;

    while (*reader_peek(r).start != '}')
    {
        s_mapItemBufIdx = listItemBuffCurr + 1;
        s_mapItemBuf[listItemBuffCurr] = readForm(vm, r);
        listItemBuffCurr++;

        if (reader_isAtEnd(r))
        {
            reportReaderError(r, reader_prev(r), EC_NoMatchRightCurlyBracket);
        }
    }

    s_mapItemBufIdx = listItemBuffStart;

    // consume '}'
    reader_next(r);

    Value ret = value_mapWithArr(vm,
                                 listItemBuffCurr - listItemBuffStart, 
                                 s_mapItemBuf + listItemBuffStart);
    VM_CPUSHV(ret);
    return ret;
}

static Value readAtom(VM* vm, Reader* r)
{
    Token _ = reader_next(r);
    Token* t = &_;

    if (tokenMatch(t, "true", 4))
    {
        return value_bool(true);
    }
    else if (tokenMatch(t, "false", 5))
    {
        return value_bool(false);
    }
    else if (tokenMatch(t, "nil", 3))
    {
        return value_nil();
    }
    else if (*t->start == ':') // keyworld
    {
        StrObj* obj = strobj_copy(vm, t->start, t->len);
        VM_CPUSH(obj);
        Value ret = value_keywordWithStr(vm, obj);
        VM_CPUSHV(ret);
        return ret;
    }
    else if (*t->start == '"') // string
    {
        // TODO Opt StringBuilder
        int len = t->len - 2;
        char* chars = CALLOCATE(vm, char, len * 2);
        char* current = chars;

        for (size_t i = 0; i < len;)
        {
            bool flag = false;
            for (size_t j = 0; j < ESCAPE_DATA_LEN; j++)
            {
                if (escapeCharDatas[j].tChars[0] == t->start[i + 1]
                    && escapeCharDatas[j].tChars[1] == t->start[i + 2])
                {
                    *current = escapeCharDatas[j].originChar;
                    current++;
                    flag = true;
                    break;
                }
            }

            if (flag)
            {
                i += 2;
            }
            else
            {
                *current = t->start[i + 1];
                current++;
                i += 1;
            }
        }

        StrObj* obj = strobj_copy(vm, chars, (int)(current - chars));
        VM_CPUSH(obj);
        Value ret = value_obj(obj);

        CFREE_ARRAY(vm, char, chars, len * 2);

        return ret;
    }
    else if (isDigit(*t->start)) // number
    {
        return value_num(strtod(t->start, NULL));
    }

    // symbol
    StrObj* obj = strobj_copy(vm, t->start, t->len);
    VM_CPUSH(obj);
    Value ret = value_symbolWithStr(vm, obj);
    VM_CPUSHV(ret);
    return ret;
}

static Value readForm(VM* vm, Reader* r)
{
#define EXPAND_TO(chars, len) \
    do { \
        reader_next(r); \
        Value symbol = value_symbol(vm, (chars), (len)); \
        VM_CPUSHV(symbol); \
        Value form = readForm(vm, r); \
        Value ret = value_list(vm, 2, symbol, form); \
        VM_CPUSHV(ret); \
        return ret; \
    } while(false)
    
    if (reader_isAtEnd(r))
        return value_none();
    
    Token t = reader_peek(r);

    switch (*t.start)
    {
    case '(':
        return readList(vm, r);

    case '[':
        return readVector(vm, r);

    case '{':
        return readMap(vm, r);

    case '\'':
        EXPAND_TO("quote", 5);

    case '`':
        EXPAND_TO("quasiquote", 10);

    case '~':
    {
        if (t.len == 2)
            EXPAND_TO("splice-unquote", 14);

        EXPAND_TO("unquote", 7);
    }

    case '@':
        EXPAND_TO("deref", 5);

    case '^':
    {
        reader_next(r);
        Value symbol = value_symbol(vm, "with-meta", 9);
        VM_CPUSHV(symbol);
        Value form = readForm(vm, r);
        VM_CPUSHV(form);
        Value form2 = readForm(vm, r);
        VM_CPUSHV(form2);
        Value ret = value_list(vm, 3, symbol, form2, form);
        VM_CPUSHV(ret);
        return ret;
    }

    case ')':
        reportReaderError(r, t, EC_NoMatchLeftParen);
        break;

    case ']':
        reportReaderError(r, t, EC_NoMatchLeftSquareBracket);
        break;

    case '}':
        reportReaderError(r, t, EC_NoMatchLeftCurlyBracket);
        break;

    default:
        return readAtom(vm, r);
    }
}

static void printScannerError(Scanner* s, const char* info)
{
    printf("ParseError: %s (near '%c' at line %d)\n", info, scanner_peekPrev(s), s->line);
}

static void printReaderError(Reader* r, const char* info)
{
    Token errorToken = r->errorToken;
    char* tokenLiteral = ALLOCATE(char, errorToken.len + 1);
    memcpy(tokenLiteral, errorToken.start, errorToken.len);
    tokenLiteral[errorToken.len] = '\0';
    
    printf("ParseError: %s (near '%s' at line %d)\n", info, tokenLiteral, errorToken.line);
    
    FREE_ARRAY(char, tokenLiteral, errorToken.len + 1);
}

Value readStr(VM* vm, const char* source)
{
    Scanner scanner;
    Reader reader;
    Value ret;

    const int errorCode = setjmp(s_jmpBuf);
    if (errorCode == 0)
    {
        scanner_init(&scanner, source);
        const Array tokens = scanner_tokenize(&scanner, source);

#if DEBUG_TOKEN
        RLOG_DEBUG("---- tokens ----\n");
        for (size_t i = 0; i < tokens.count; i++)
        {
            Token t;
            array_get(&tokens, i, &t);
            token_print(&t);
        }
        RLOG_DEBUG("---------------\n\n");
#endif

        reader_init(&reader, tokens);
        ret = readForm(vm, &reader);
        reader_free(&reader);
    }
    else
    {
        switch (errorCode)
        {
            case EC_NoMatchQuote:
                printScannerError(&scanner, "no match '\"' found!");
                break;

            case EC_NoMatchRightParen:
                printReaderError(&reader, "no match ')' found!");
                break;

            case EC_NoMatchRightSquareBracket:
                printReaderError(&reader, "no match ']' found!");
                break;

            case EC_NoMatchRightCurlyBracket:
                printReaderError(&reader, "no match '}' found!");
                break;

            case EC_NoMatchLeftParen:
                printReaderError(&reader, "no match '(' found!");
                break;

            case EC_NoMatchLeftSquareBracket:
                printReaderError(&reader, "no match '[' found!");
                break;

            case EC_NoMatchLeftCurlyBracket:
                printReaderError(&reader, "no match '{' found!");
                break;

            default:
                break;
        }
        
        ret = value_none();
    }

    return ret;
}
