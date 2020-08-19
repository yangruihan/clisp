#ifndef __C_READER_H_
#define __C_READER_H_

#include "ccommon.h"
#include "cvalue.h"

typedef struct sToken
{
    const char* start;
    int         len;
    int         line;
} Token;

typedef struct sReader
{
    Array tokens;
    int   position;
    Token errorToken;
} Reader;

void  token_print(Token* t);

void  reader_init(Reader* r, Array tokens);
void  reader_free(Reader* r);

Token reader_next(Reader* r);
Token reader_peek(Reader* r);

Value readStr(VM* vm, const char* source);

#endif // __C_READER_H_