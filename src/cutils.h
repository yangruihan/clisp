#ifndef __C_UTILS_H_
#define __C_UTILS_H_

#include "ccommon.h"

#define HASH(bytes, len) hash((char*)((bytes)), len)

uint32_t hash(char* bytes, int length);

bool readFile(const char* path, /* out */ char** content, /* out */ int* fileSize);

#endif // __C_UTILS_H_