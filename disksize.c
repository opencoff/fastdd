#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils/utils.h"
#include "fastdd.h"

/*
 * Print disksize of every arg on the command line
 */
int
main(int argc, char *argv[])
{
    int i;
    char buf[256];

    for (i = 1; i < argc; i++) {
        const char *fn = argv[i];
        int fd = open(fn, O_RDONLY);
        if (fd < 0) {
            printf("%s: %s\n", fn, strerror(errno));
            continue;
        }

        uint64_t sz = 0;
        int r = Blksize(&sz, fd);
        if (r < 0) {
            printf("%s: blksize %s\n", fn, strerror(-r));
        } else {
            humanize_size(buf, sizeof buf, sz);
            printf("%s: %s (%" PRIu64 ")\n", fn, buf, sz);
        }
        close(fd);
    }
    return 0;
}
