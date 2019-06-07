/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * blksize_darwin.c - Fetch blocksize for a given block device MacOS edition
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
 *
 */
#include <inttypes.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <sys/disk.h>
#include "fastdd.h"


/*
 * Fetch blocksize in bytes for block device 'fd'.
 * Return 0 on success, -errno on failure.
 */
int
Blksize(uint64_t *p_size, int fd)
{
    uint64_t nblks = 0;
    uint32_t bsiz  = 0;

    if (ioctl(fd, DKIOCGETBLOCKSIZE,  &bsiz)  != 0) return -errno;
    if (ioctl(fd, DKIOCGETBLOCKCOUNT, &nblks) != 0) return -errno;

    // XXX Overflow check?
    *p_size = nblks * bsiz;
    return 0;
}
