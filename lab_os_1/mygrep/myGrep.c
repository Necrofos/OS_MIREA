#include "myGrep.h"
#include <stdio.h>
#include <string.h>

int myGrepRun(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: mygrep pattern [file]\n");
        return 1;
    }

    const char *pattern = argv[1];
    FILE       *file    = stdin;

    if (argc > 2)
    {
        file = fopen(argv[2], "r");
        if (!file)
        {
            perror(argv[2]);
            return 1;
        }
    }

    char line[4096];
    while (fgets(line, sizeof(line), file))
    {
        if (strstr(line, pattern))
        {
            fputs(line, stdout);
        }
    }

    if (file != stdin)
        fclose(file);
    return 0;
}
