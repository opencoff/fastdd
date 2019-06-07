/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * blksize_linux.c - Fetch blocksize for a given block device
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
#include <sys/types.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <errno.h>
#include "fastdd.h"

/*
 * Fetch blocksize in bytes for block device 'fd'.
 * Return 0 on success, -errno on failure.
 */
int
Blksize(uint64_t *p_size, int fd)
{
    uint64_t oct = 0;

    if (ioctl(fd, BLKGETSIZE64, &oct) != 0) return -errno;

    *p_size = oct;
    return 0;
}
