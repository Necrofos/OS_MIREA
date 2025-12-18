#include <stdio.h>
#include <string.h>
#include "myGrep.h"


int main(int argc, char *argv[])
{
    myGrepRun(argc, argv);

    fprintf(
        stderr,
        "Usage:\n"
        "  mycat [-n] [-b] [-E] [files...]\n"
        "  mygrep pattern [file]\n");
    return 0;
}
