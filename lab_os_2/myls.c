#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <limits.h>
#include <stdint.h>

#define clrBlue  "\x1b[34m"
#define clrGreen "\x1b[32m"
#define clrCyan  "\x1b[36m"
#define clrReset "\x1b[0m"

typedef struct
{
    char       *name;
    char       *full;
    struct stat st;
    int         ok;
} Item;

int optA = 0, optL = 0, useColor = 0;

char *joinPath(const char *dir, const char *name)
{
    size_t lenDir = strlen(dir), lenName = strlen(name);
    int    slash = (lenDir > 0 && dir[lenDir - 1] != '/');
    char  *path  = malloc(lenDir + slash + lenName + 1);
    if (!path)
        return NULL;

    memcpy(path, dir, lenDir);
    if (slash)
        path[lenDir++] = '/';
    memcpy(path + lenDir, name, lenName);
    path[lenDir + lenName] = '\0';
    return path;
}

int cmpItems(const void *a, const void *b)
{
    const Item *itemA = a, *itemB = b;
    return strcoll(itemA->name, itemB->name);
}

void modeToStr(mode_t mode, char out[11])
{
    out[0] = S_ISDIR(mode)    ? 'd'
             : S_ISLNK(mode)  ? 'l'
             : S_ISCHR(mode)  ? 'c'
             : S_ISBLK(mode)  ? 'b'
             : S_ISSOCK(mode) ? 's'
             : S_ISFIFO(mode) ? 'p'
                              : '-';
    out[1] = (mode & S_IRUSR) ? 'r' : '-';
    out[2] = (mode & S_IWUSR) ? 'w' : '-';
    out[3] = (mode & S_IXUSR) ? 'x' : '-';
    out[4] = (mode & S_IRGRP) ? 'r' : '-';
    out[5] = (mode & S_IWGRP) ? 'w' : '-';
    out[6] = (mode & S_IXGRP) ? 'x' : '-';
    out[7] = (mode & S_IROTH) ? 'r' : '-';
    out[8] = (mode & S_IWOTH) ? 'w' : '-';
    out[9] = (mode & S_IXOTH) ? 'x' : '-';
    if (mode & S_ISUID)
        out[3] = (out[3] == 'x') ? 's' : 'S';
    if (mode & S_ISGID)
        out[6] = (out[6] == 'x') ? 's' : 'S';
    if (mode & S_ISVTX)
        out[9] = (out[9] == 'x') ? 't' : 'T';
    out[10] = '\0';
}

void formatTime(time_t t, char *buf, size_t n)
{
    time_t    now = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    if (llabs((long long)(now - t)) > (long long)60 * 60 * 24 * 365 / 2)
        strftime(buf, n, "%b %e  %Y", &tm);
    else
        strftime(buf, n, "%b %e %H:%M", &tm);
}

const char *pickColor(const struct stat *st)
{
    if (!useColor)
        return "";
    if (S_ISDIR(st->st_mode))
        return clrBlue;
    if (S_ISLNK(st->st_mode))
        return clrCyan;
    if (S_ISREG(st->st_mode) && (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        return clrGreen;
    return "";
}

void printOneFile(const char *path)
{
    struct stat st;
    if (lstat(path, &st) != 0)
    {
        fprintf(stderr, "myls: cannot access '%s': %s\n", path, strerror(errno));
        return;
    }
    if (!optL)
    {
        const char *c = pickColor(&st);
        const char *r = useColor ? clrReset : "";
        printf("%s%s%s\n", c, path, r);
        return;
    }

    char mode[11];
    modeToStr(st.st_mode, mode);
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    char           tbuf[64];
    formatTime(st.st_mtime, tbuf, sizeof tbuf);

    char sizebuf[64];
    if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
        snprintf(sizebuf, sizeof sizebuf, "%u, %u", major(st.st_rdev), minor(st.st_rdev));
    else
        snprintf(sizebuf, sizeof sizebuf, "%jd", (intmax_t)st.st_size);

    printf(
        "%s %2ju %-8s %-8s %8s %s ",
        mode,
        (uintmax_t)st.st_nlink,
        pw ? pw->pw_name : "-",
        gr ? gr->gr_name : "-",
        sizebuf,
        tbuf);

    const char *c = pickColor(&st);
    const char *r = useColor ? clrReset : "";
    if (S_ISLNK(st.st_mode))
    {
        char    target[PATH_MAX];
        ssize_t k = readlink(path, target, sizeof(target) - 1);
        if (k >= 0)
            target[k] = '\0';
        else
            strcpy(target, "?");
        printf("%s%s%s -> %s\n", c, path, r, target);
    }
    else
    {
        printf("%s%s%s\n", c, path, r);
    }
}

Item *readDirItems(const char *path, size_t *outN)
{
    *outN    = 0;
    DIR *dir = opendir(path);
    if (!dir)
    {
        fprintf(stderr, "myls: cannot open directory '%s': %s\n", path, strerror(errno));
        return NULL;
    }

    size_t cap = 64, n = 0;
    Item  *items = malloc(cap * sizeof(*items));
    if (!items)
    {
        closedir(dir);
        return NULL;
    }

    struct dirent *de;
    while ((de = readdir(dir)) != NULL)
    {
        if (!optA && de->d_name[0] == '.')
            continue;

        if (n == cap)
        {
            cap *= 2;
            Item *tmp = realloc(items, cap * sizeof(*items));
            if (!tmp)
            {
                for (size_t j = 0; j < n; j++)
                {
                    free(items[j].name);
                    free(items[j].full);
                }
                free(items);
                closedir(dir);
                return NULL;
            }
            items = tmp;
        }

        items[n].name = strdup(de->d_name);
        items[n].full = joinPath(path, de->d_name);
        items[n].ok   = 0;
        if (items[n].full && lstat(items[n].full, &items[n].st) == 0)
            items[n].ok = 1;
        n++;
    }

    closedir(dir);
    qsort(items, n, sizeof(*items), cmpItems);
    *outN = n;
    return items;
}

void computeWidths(const Item *items, size_t n, int *wLinks, int *wOwner, int *wGroup, int *wSize)
{
    *wLinks = *wOwner = *wGroup = *wSize = 1;
    for (size_t i = 0; i < n; i++)
    {
        if (!items[i].ok)
            continue;
        struct stat *st = &items[i].st;

        int t = 1;
        for (unsigned long x = st->st_nlink; x >= 10; x /= 10)
            t++;
        if (t > *wLinks)
            *wLinks = t;

        struct passwd *pw = getpwuid(st->st_uid);
        struct group  *gr = getgrgid(st->st_gid);
        int            ow = pw ? (int)strlen(pw->pw_name) : 1;
        int            gw = gr ? (int)strlen(gr->gr_name) : 1;
        if (ow > *wOwner)
            *wOwner = ow;
        if (gw > *wGroup)
            *wGroup = gw;

        char sb[64];
        if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
            snprintf(sb, sizeof sb, "%u, %u", major(st->st_rdev), minor(st->st_rdev));
        else
            snprintf(sb, sizeof sb, "%jd", (intmax_t)st->st_size);

        int sw = (int)strlen(sb);
        if (sw > *wSize)
            *wSize = sw;
    }
}

void printItemsShort(const Item *items, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        if (!items[i].ok)
        {
            printf("%s\n", items[i].name);
            continue;
        }
        const char *c = pickColor(&items[i].st);
        const char *r = (useColor && *c) ? clrReset : "";
        printf("%s%s%s\n", c, items[i].name, r);
    }
}

void printItemsLong(const Item *items, size_t n)
{
    long long blocks = 0;
    for (size_t i = 0; i < n; i++)
        if (items[i].ok)
            blocks += (long long)items[i].st.st_blocks;
    printf("total %lld\n", blocks / 2);

    int wLinks, wOwner, wGroup, wSize;
    computeWidths(items, n, &wLinks, &wOwner, &wGroup, &wSize);

    for (size_t i = 0; i < n; i++)
    {
        if (!items[i].ok)
        {
            fprintf(stderr, "myls: cannot access '%s'\n", items[i].full);
            continue;
        }

        struct stat *st = &items[i].st;
        char         mode[11];
        modeToStr(st->st_mode, mode);

        struct passwd *pw = getpwuid(st->st_uid);
        struct group  *gr = getgrgid(st->st_gid);
        char           tbuf[64];
        formatTime(st->st_mtime, tbuf, sizeof tbuf);

        char sb[64];
        if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
            snprintf(sb, sizeof sb, "%u, %u", major(st->st_rdev), minor(st->st_rdev));
        else
            snprintf(sb, sizeof sb, "%jd", (intmax_t)st->st_size);

        printf(
            "%s %*ju %-*s %-*s %*s %s ",
            mode,
            wLinks,
            (uintmax_t)st->st_nlink,
            wOwner,
            pw ? pw->pw_name : "-",
            wGroup,
            gr ? gr->gr_name : "-",
            wSize,
            sb,
            tbuf);

        const char *c = pickColor(st);
        const char *r = useColor ? clrReset : "";
        if (S_ISLNK(st->st_mode))
        {
            char    target[PATH_MAX];
            ssize_t k = readlink(items[i].full, target, sizeof(target) - 1);
            if (k >= 0)
                target[k] = '\0';
            else
                strcpy(target, "?");
            printf("%s%s%s -> %s\n", c, items[i].name, r, target);
        }
        else
        {
            printf("%s%s%s\n", c, items[i].name, r);
        }
    }
}

void freeItems(Item *items, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        free(items[i].name);
        free(items[i].full);
    }
    free(items);
}

void listDir(const char *path, int printHeader, int multi)
{
    if (multi && printHeader)
        printf("%s:\n", path);

    size_t n     = 0;
    Item  *items = readDirItems(path, &n);
    if (!items)
        return;

    if (!optL)
        printItemsShort(items, n);
    else
        printItemsLong(items, n);

    freeItems(items, n);
}

int pathIsDir(const char *p)
{
    struct stat st;
    if (lstat(p, &st) != 0)
        return 0;
    return S_ISDIR(st.st_mode);
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    useColor = isatty(STDOUT_FILENO);

    int c;
    while ((c = getopt(argc, argv, "la")) != -1)
    {
        if (c == 'l')
            optL = 1;
        else if (c == 'a')
            optA = 1;
        else
        {
            fprintf(stderr, "Usage: %s [-l] [-a] [file...]\n", argv[0]);
            return 1;
        }
    }

    int nArg = argc - optind;
    if (nArg == 0)
    {
        listDir(".", 0, 0);
        return 0;
    }

    int *isDir = calloc(nArg, sizeof(int));
    if (!isDir)
    {
        perror("calloc");
        return 1;
    }

    for (int i = 0; i < nArg; i++)
        isDir[i] = pathIsDir(argv[optind + i]);

    for (int i = 0; i < nArg; i++)
    {
        const char *p = argv[optind + i];
        struct stat st;
        if (lstat(p, &st) == 0 && !S_ISDIR(st.st_mode))
            printOneFile(p);
        else if (lstat(p, &st) != 0)
            fprintf(stderr, "myls: cannot access '%s': %s\n", p, strerror(errno));
    }

    for (int i = 0; i < nArg; i++)
    {
        if (isDir[i])
        {
            listDir(argv[optind + i], 1, nArg > 1);
            if (i != nArg - 1)
                printf("\n");
        }
    }

    free(isDir);
    return 0;
}
