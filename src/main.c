#include <stdio.h>

#include "cmal.h"

static void repl(VM* vm)
{
    char input[256];

    printf("user> ");
    while(fgets(input, 256, stdin))
    {
        if (*input != '\n')
        {
            const char* ret = vm_rep(vm, input);
            if (*ret != '\0')
                printf("%s\n", ret);
        }

        printf("user> ");
    }
}

static void dofile(VM* vm, const char* path, int argc, char** argv)
{
    vm_dofile(vm, path, argc, argv);
}

int main(int argc, char** argv)
{
    // init log
#if DEBUG
    rlog_setLevel(RLOG_TRACE);
#else
    rlog_setLevel(RLOG_ERROR);
#endif

#if ENABLE_LOG_FILE
    FILE* file = fopen("./log.log", "w");
    rlog_addFp(file, RLOG_TRACE);
#endif

    VM* vm = vm_create();

    if (argc == 1)
        repl(vm);
    else
    {
        if (argc > 2)
            dofile(vm, argv[1], argc - 2, argv + 2);
        else
            dofile(vm, argv[1], 0, NULL);
    }

    vm_free(vm);

#if ENABLE_LOG_FILE
    fclose(file);
#endif

    return 0;
}