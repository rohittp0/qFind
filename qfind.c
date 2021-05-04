/* We want POSIX.1-2008 + XSI, i.e. SuSv4, features */
#define _XOPEN_SOURCE 700

/* Added on 2017-06-25:
   If the C library can support 64-bit file sizes
   and offsets, using the standard names,
   these defines tell the C library to do so. */
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ftw.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>

/* POSIX.1 says each process has at least 20 file descriptors.
 * Three of those belong to the standard streams.
 * Here, we use a conservative estimate of 15 available;
 * assuming we use at most two for other uses in this program,
 * we should never run into any problems.
 * Most trees are shallow   er than that, so it is efficient.
 * Deeper trees are traversed fine, just a bit slower.
 * (Linux allows typically hundreds to thousands of open files,
 *  so you'll probably never see any issues even if you used
 *  a much higher value, say a couple of hundred, but
 *  15 is a safe, reasonable value.)
*/
#ifndef USE_FDS
#define USE_FDS 15
#endif

#define PRINT_BUFFER_SIZE 500

int regexflag = 0;
int extraflag = 0;
int followflag = 0;
int nameflag = 0;
int flushflag = 0;

regex_t regex;
char *printBuffer[PRINT_BUFFER_SIZE];

void bprint(const char *text)
{
    if (flushflag >= PRINT_BUFFER_SIZE)
    {
        for (int i = 0; i < flushflag; i++)
        {
            fputs(printBuffer[i], stdout);
            free(printBuffer[i]);
        }
        flushflag = 0;
    }
    else
    {
        printBuffer[flushflag] = (char *)malloc(sizeof(char) * strlen(text) + 1);
        strcpy(printBuffer[flushflag++], text);
    }
}

void print_extra_info(const struct stat *info)
{
    const double bytes = (double)info->st_size; /* Not exact if large! */
    struct tm mtime;

    localtime_r(&(info->st_mtime), &mtime);
    char buffer[21];

    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
            mtime.tm_year + 1900, mtime.tm_mon + 1, mtime.tm_mday,
            mtime.tm_hour, mtime.tm_min, mtime.tm_sec);

    bprint(buffer);

    if (bytes >= 1099511627776.0)
        sprintf(buffer, " %9.3f TiB ", bytes / 1099511627776.0);
    else if (bytes >= 1073741824.0)
        sprintf(buffer, " %9.3f GiB ", bytes / 1073741824.0);
    else if (bytes >= 1048576.0)
        sprintf(buffer, " %9.3f MiB ", bytes / 1048576.0);
    else if (bytes >= 1024.0)
        sprintf(buffer, " %9.3f KiB ", bytes / 1024.0);
    else
        sprintf(buffer, " %9.0f B  ", bytes);

    bprint(buffer);
}

int process_entry(const char *filepath, const struct stat *info,
                  const int typeflag, struct FTW *pathinfo)
{
    const char *const filename = nameflag ? filepath + pathinfo->base : filepath;

    if (regexflag && regexec(&regex, filename, 0, NULL, 0))
        return 0;

    if (extraflag)
        print_extra_info(info);

    if (followflag && typeflag == FTW_SL)
    {
        char *target;
        size_t maxlen = 1023;
        ssize_t len;

        while (1)
        {

            target = malloc(maxlen + 1);
            if (target == NULL)
                return ENOMEM;

            len = readlink(filepath, target, maxlen);
            if (len == (ssize_t)-1)
            {
                const int saved_errno = errno;
                free(target);
                return saved_errno;
            }
            if (len >= (ssize_t)maxlen)
            {
                free(target);
                maxlen += 1024;
                continue;
            }

            target[len] = '\0';
            break;
        }

        bprint(filename);
        bprint(" -> ");
        bprint(target);

        free(target);
        return 0;
    }

    // else if (typeflag == FTW_SLN)
    //     sufix = " (dangling symlink)\n";
    // else if (typeflag == FTW_F)
    //     sufix = "\n";
    // else if (typeflag == FTW_D || typeflag == FTW_DP)
    //     sufix = "/\n";
    // else if (typeflag == FTW_DNR)
    //     sufix = "/ (unreadable)\n";
    // else
    //     sufix = " (unknown)\n";

    bprint(filename);
    bprint("\n");

    return 0;
}

int search_directory_tree(const char *const dirpath)
{
    int result;

    /* Invalid directory path? */
    if (dirpath == NULL || *dirpath == '\0')
        return errno = EINVAL;

    result = nftw(dirpath, process_entry, USE_FDS, FTW_PHYS);

    if (result >= 0)
        errno = result;

    return errno;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {

        if (search_directory_tree("."))
        {
            fprintf(stderr, "%s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    else
    {
        if (regcomp(&regex, argc > 2 ? argv[2] : ".*", 0))
        {
            fprintf(stderr, "Failed to create regex.\n");
            return EXIT_FAILURE;
        }

        regexflag = 1;

        for (int i = 3; i < argc; i++)
            if (strcmp(argv[i], "-e") == 0)
                extraflag = 1;
            else if (strcmp(argv[i], "-l") == 0)
                followflag = 1;
            else if (strcmp(argv[i], "-n") == 0)
                nameflag = 1;

        if (search_directory_tree(argv[1]))
        {
            fprintf(stderr, "%s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < flushflag; i++)
    {
        fputs(printBuffer[i], stdout);
        free(printBuffer[i]);
    }

    return EXIT_SUCCESS;
}