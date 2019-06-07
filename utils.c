/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * utils.c - misc utils for I/O
 *
 * Copyright (c) 2015 Sudhi Herle <sw at herle.net>
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

#include <unistd.h>
//#include <sys/ioctl.h>
//#include <sys/types.h>
//#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include "fastdd.h"


/*
 * try very hard to read all n bytes of data from fd into buf.
 */
ssize_t
fullread(int fd, void *buf, size_t n)
{
    uint8_t *p = buf;
    size_t   r = n;

    while (r > 0) {
        ssize_t m = read(fd, p, r);
        if (m < 0) {
            int err = errno;
            if (err == EINTR || err == EAGAIN) continue;
            return -err;
        }
        if (m == 0) return n - r;

        p += m;
        r -= m;
    }
    return n;
}


/*
 * Desperately try to write all n bytes of data in buf.
 */
ssize_t
fullwrite(int fd, void *buf, size_t n)
{
    uint8_t *p = buf;
    size_t   r = n;

    while (r > 0) {
        ssize_t m = write(fd, p, r);
        if (m < 0) {
            int err = errno;
            if (err == EINTR || err == EAGAIN) continue;
            return -err;
        }
        if (m == 0) return n - r;

        p += m;
        r -= m;
    }
    return n;
}


/*
 * Skip reading 'n' initial bytes. We can't lseek(2) because fd is a
 * pipe.
 */
ssize_t
skip(int fd, uint64_t n)
{
    uint8_t buf[4096];
    size_t  rem = n;

    while (rem > 0) {
        size_t want  = rem > sizeof buf ? sizeof buf : rem;
        ssize_t have = fullread(fd, buf, want);

        if (have < 0)  return have;
        if (have == 0) return n - rem;

        rem -= have;
    }
    return n;
}

