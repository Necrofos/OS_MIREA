#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s mode file...\n", prog);
    fprintf(stderr, "Mode: [ugoa]*[+-=][rwx]+ or octal number (e.g., 766)\n");
}

int isOctalMode(const char *s)
{
    size_t len = strlen(s);
    if (len != 3)
        return 0;
    for (size_t i = 0; i < len; ++i)
    {
        if (s[i] < '0' || s[i] > '7')
            return 0;
    }
    return 1;
}

mode_t parseOctalMode(const char *s)
{
    int u = s[0] - '0';
    int g = s[1] - '0';
    int o = s[2] - '0';

    mode_t mode = 0;

    if (u & 4)
        mode |= S_IRUSR;
    if (u & 2)
        mode |= S_IWUSR;
    if (u & 1)
        mode |= S_IXUSR;

    if (g & 4)
        mode |= S_IRGRP;
    if (g & 2)
        mode |= S_IWGRP;
    if (g & 1)
        mode |= S_IXGRP;

    if (o & 4)
        mode |= S_IROTH;
    if (o & 2)
        mode |= S_IWOTH;
    if (o & 1)
        mode |= S_IXOTH;

    return mode;
}

int applySymbolicMode(const char *spec, mode_t oldMode, mode_t *newMode)
{
    const char *p = spec;

    int whoU = 0, whoG = 0, whoO = 0;
    int sawWho = 0;

    while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a')
    {
        sawWho = 1;
        if (*p == 'u')
            whoU = 1;
        else if (*p == 'g')
            whoG = 1;
        else if (*p == 'o')
            whoO = 1;
        else if (*p == 'a')
            whoU = whoG = whoO = 1;
        p++;
    }

    if (!sawWho)
    {
        whoU = whoG = whoO = 1;
    }

    char op = *p;
    if (op != '+' && op != '-' && op != '=')
    {
        fprintf(stderr, "mychmod: invalid symbolic mode (expected +, -, =): '%s'\n", spec);
        return -1;
    }
    p++;

    if (*p == '\0')
    {
        fprintf(stderr, "mychmod: missing permission part in mode: '%s'\n", spec);
        return -1;
    }

    mode_t permU = 0, permG = 0, permO = 0;

    for (; *p; ++p)
    {
        char c = *p;
        if (c != 'r' && c != 'w' && c != 'x')
        {
            fprintf(stderr, "mychmod: invalid permission char '%c' in mode '%s'\n", c, spec);
            return -1;
        }

        if (c == 'r')
        {
            if (whoU)
                permU |= S_IRUSR;
            if (whoG)
                permG |= S_IRGRP;
            if (whoO)
                permO |= S_IROTH;
        }
        else if (c == 'w')
        {
            if (whoU)
                permU |= S_IWUSR;
            if (whoG)
                permG |= S_IWGRP;
            if (whoO)
                permO |= S_IWOTH;
        }
        else if (c == 'x')
        {
            if (whoU)
                permU |= S_IXUSR;
            if (whoG)
                permG |= S_IXGRP;
            if (whoO)
                permO |= S_IXOTH;
        }
    }

    mode_t mode = oldMode;

    mode_t maskU = S_IRUSR | S_IWUSR | S_IXUSR;
    mode_t maskG = S_IRGRP | S_IWGRP | S_IXGRP;
    mode_t maskO = S_IROTH | S_IWOTH | S_IXOTH;

    if (op == '+')
    {
        mode |= permU | permG | permO;
    }
    else if (op == '-')
    {
        mode &= ~(permU | permG | permO);
    }
    else if (op == '=')
    {
        if (whoU)
        {
            mode &= ~maskU;
            mode |= permU;
        }
        if (whoG)
        {
            mode &= ~maskG;
            mode |= permG;
        }
        if (whoO)
        {
            mode &= ~maskO;
            mode |= permO;
        }
    }

    *newMode = mode;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *modeStr = argv[1];

    int isOctal = isOctalMode(modeStr);

    for (int i = 2; i < argc; ++i)
    {
        const char *path = argv[i];
        struct stat st;

        if (stat(path, &st) == -1)
        {
            fprintf(stderr, "mychmod: cannot stat '%s': %s\n", path, strerror(errno));
            continue;
        }

        mode_t newMode;

        if (isOctal)
        {
            mode_t perms = parseOctalMode(modeStr);
            newMode      = (st.st_mode & ~0777) | perms;
        }
        else
        {
            if (applySymbolicMode(modeStr, st.st_mode, &newMode) != 0)
            {
                continue;
            }
        }

        if (chmod(path, newMode) == -1)
        {
            fprintf(stderr, "mychmod: cannot chmod '%s': %s\n", path, strerror(errno));
            continue;
        }
    }

    return 0;
}
