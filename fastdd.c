/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * fastdd.c - Fast dd for linux
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
 * Notes
 * =====
 * A fast version of dd that uses copy_file_range(2) to speed up
 * disk-to-disk copies.
 *
 * We only support a few options of dd:
 *   bs=N
 *   count=N
 *   skip=N     -- skip N input blocks
 *   seek=N     -- skip N output blocks before first write
 *   if=FILE
 *   of=FILE
 *   iflag=nonblock
 *   oflag=nonblock,excl,sync
 *
 * default bs=PAGESIZE
 * default count=filesize/bs
 * default of=STDOUT
 * default if=STDIN
 *
 * splice(2) needs one of the descriptors to be a pipe or socket.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "error.h"
#include "fastdd.h"
#include "utils/utils.h"

// Auto-generated headerfile
#include "opts.h"

int Quiet = 0;

int
main(int argc, char * const *argv)
{
    opt_option opt;

    program_name = argv[0];

    if (opt_parse(&opt, argc, argv) != 0)
        exit(1);

    if (opt.argv_count <= 0)
        error(1, 0, "Usage: %s [options] [arguments]", program_name);

    // global state
    Quiet = opt.quiet;

    Args a;
    Acctg g;

    memset(&g, 0, sizeof g);
    memset(&a, 0, sizeof a);

    if (Parse_args(&a, opt.argv_count, opt.argv_inputs) < 0) {
        return 1;
    }


    uint64_t st = timenow();

    Copy(&g, &a);

    if (a.ifd > 0) close(a.ifd);
    if (a.ofd > 0) close(a.ofd);

    g.elapsed_us = (timenow() - st) / 1000;

#define d(x)  ((double)(x))
    double   wrspeed = d(g.nwr) / d(g.elapsed_us);

    // wrspeed comes out to be MB/s for the following reason:
    //    nwr / 1e6     --> MiB
    //    elapsed / 1e6 --> seconds

    char sz[128];
    humanize_size(sz, sizeof sz, g.nwr);

    double secs  = d(g.elapsed_us)/1.0e6;

    // final results - we always print em.
    fprintf(stderr, "%s (%" PRIu64 " bytes) copied in %4.6f secs (%4.2f MB/s)\n",
                sz, g.nwr, secs, wrspeed);
    return 0;
}



const char*
opt_usage()
{
    static char msg[1024];
    snprintf(msg, sizeof msg, "Usage: %s [options] [arguments]\n"
            "\n"
            "Arguments:\n"
            "    if=FILE   Read input from FILE [STDIN]\n"
            "    of=FILE   Write output to FILE [STDOUT]\n"
            "    bs=N      Use N as the input/output blocksize [512]\n"
            "    count=N   Copy N bytes from infile to outfile [Till EOF]\n"
            "    skip=N    Skip first N bytes of the input [0]\n"
            "    seek=N    Seek to offset N before first write to output [0]\n"
            "    iosize=N  Do I/O in chunks of N bytes [64kB]\n"
#ifdef O_DIRECT
            "    iflag=IF  One or more flags for input file I/O (nonblock,direct) []\n"
            "    oflag=OF  One or more flags for output file I/O (nonblock,direct,excl,sync,trunc,creat) []\n"
#else
            "    iflag=IF  One or more input flags for I/O (nonblock) []\n"
            "    oflag=OF  One or more flags for output file I/O (nonblock,excl,sync,trunc,creat) []\n"
#endif

            "\n"
            "Note: The flags direct, excl, sync, trunc, creat also have their corresponding\n"
            "      negative version prefixed with 'no' (e.g., nodirect, notrunc, nocreat etc.)\n"
            , program_name);

    return msg;
}



/* EOF */
