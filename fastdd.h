/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * fastdd.h - common functions and types
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

#ifndef ___FASTDD_H__uT79yi1XEBN8awV4___
#define ___FASTDD_H__uT79yi1XEBN8awV4___ 1

    /* Provide C linkage for symbols declared here .. */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>

#include "args.h"

struct Acctg {
    uint64_t nrd,
             nwr;

    uint64_t elapsed_us;
};
typedef struct Acctg Acctg;


/*
 * Perform a copy operation for arguments in 'a' and write stats
 * into 'g'.
 */
extern int Copy(Acctg *g, Args *a);

/*
 * Return blocksize of device in 'fd'.
 */
extern int Blksize(uint64_t *p_size, int fd);


// -- Internal functions --
ssize_t fullread(int fd, void *buf, size_t n);
ssize_t fullwrite(int fd, void *buf, size_t n);
ssize_t skip(int fd, uint64_t n);

extern int Quiet;

#define Verbose(fmt,...) do { \
                            if (!Quiet) fprintf(stderr, fmt,##__VA_ARGS__);\
                         } while(0)

struct progress {
    int dots;
    int maxdots;
};
typedef struct progress progress;

extern void progress_init(progress *p);
extern void progress_dot(progress *p);
extern void progress_done(progress *p);
extern void progress_err(progress *p);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! ___FASTDD_H__uT79yi1XEBN8awV4___ */

/* EOF */
