/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * args.h
 *
 * Copyright (c) 2018 Sudhi Herle <sw at herle.net>
 *
 * Licensing Terms: GPLv2 
 *
 * If you need a commercial license for this work, please contact
 * the author.
 *
 * This software does not come with any express or implied
 * warranty; it is provided "as is". No claim  is made to its
 * suitability for any purpose.
 */

#ifndef ___ARGS_H__PjHOay0xLBQkDp5s___
#define ___ARGS_H__PjHOay0xLBQkDp5s___ 1

    /* Provide C linkage for symbols declared here .. */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * This represents a parsed set of "dd" args.
 *
 * After a successful parse, the file(s) are opened, and the
 * corresponding stat structs are filled-in.
 *
 * For block devices, the stat::st_size is filled in by
 * querying the appropriate blkdev iotcl(2).
 */
struct Args
{
    // Input and output fds
    int ifd,
        ofd;

    int ipipe,      // bool flag: set if ifd is a pipe
        opipe;      // bool flag: set if ofd is a pipe

    uint64_t bs;     // TYP_SZ; block size in bytes
    uint64_t skip;   // TYP_I; in units of blocks
    uint64_t seek;   // TYP_I; in units of blocks
    uint64_t count;  // TYP_SZ; in units of blocks

    uint64_t iosize; // TYP_SZ; if we are doing mmap - then this is the map chunk size

    char infile[PATH_MAX];
    char outfile[PATH_MAX];

    int iflag;     // O_xxx flags
    int oflag;     // O_xxx flags

    // If this is 0, it means read till EOF.
    uint64_t  insize;

    struct stat ist,
                ost;
};
typedef struct Args Args;

// predicate that returns true if an fd is a pipe
// XXX This destroys the current offset!
#define ispipe(fd)   ({\
                        int zr = 0;\
                        if (lseek(fd, 0, SEEK_CUR) != 0) { \
                            if (errno == ESPIPE) zr = 1; \
                        }\
                        zr; \
                    })

/*
 * Parse command line args and populate the Args structure.
 */
int Parse_args(Args *aa, int argc, char * const argv[]);

/*
 * Convert args to a string
 */
char* Args_string(char *buf, size_t sz, const Args *a);

/*
 * Convert generic flags to string.
 */
char * Flag2str(char *buf, size_t bufsz, int flag);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! ___ARGS_H__PjHOay0xLBQkDp5s___ */

/* EOF */
