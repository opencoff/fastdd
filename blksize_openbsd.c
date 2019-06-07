/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * blksize_openbsd.c - Fetch blocksize for a given block device (OpenBSD)
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
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <sys/disklabel.h>
#include <sys/dkio.h>
#include "fastdd.h"
#include "utils/utils.h"


/*
 * Fetch blocksize in bytes for block device 'fd'.
 * Return 0 on success, -errno on failure.
 */
int
Blksize(uint64_t *p_size, int fd)
{
    struct disklabel dl;

    if (ioctl(fd, DIOCGPDINFO, &dl) != 0) return -errno;

    uint64_t sz = DL_GETDSIZE(&dl);
    //printf("DL-sz %" PRIu64 " sectors (%d bytes/sector)\n",
    //      sz, dl.d_secsize);

    if (sz > UINT32_MAX) {
        uint64_t spc = _U64(dl.d_ntracks) * _U64(dl.d_nsectors);
        uint64_t cyl = UINT32_MAX / spc;
        sz = cyl * spc;
        //printf("spc %" PRIu64 " cyl %" PRIu64 " sz %" PRIu64 "\n", spc, cyl, sz);
    }

    *p_size = sz * dl.d_secsize;
    return 0;
}
