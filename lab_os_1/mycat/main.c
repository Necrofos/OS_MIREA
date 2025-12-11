#include <stdio.h>
#include <string.h>
#include "mycat.h"

int main(int argc, char *argv[])
{
    mycat_run(argc, argv);

    fprintf(
        stderr,
        "Usage:\n"
        "  mycat [-n] [-b] [-E] [files...]\n"
        "  mygrep pattern [file]\n");
    return 0;
}
