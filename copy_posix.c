/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * copy_posix.c - threaded copy for non-Linux posix
 * systems.
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
 * Notes:
 * ======
 *
 * o  We create a thread for reading from ifd. The main thread
 *    continues writes to ofd.
 *
 * o  For all inputs, we use a pool of buffers for I/O.
 *
 * o  A producer-consumer queue of descriptors synchronizes the read
 *    and write threads.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "error.h"
#include "utils/utils.h"
#include "fast/syncq.h"
#include "fastdd.h"


/*
 * An I/O descriptor. For pipe inputs, buf points to memory from
 * a mempool.
 */
struct desc {
    void*   buf;
    size_t  size;
    size_t  cap;

    /*
     * Errno of the reader; the abs() value of this is errno.
     * Negative values: write errors.
     * Positive values: read  errors
     */
    int     err;
};
typedef struct desc desc;


/* Size of producer-consumer queue */
#define DESC_QSIZE      128

/* producer-consumer queue of free descriptors */
SYNCQ_TYPEDEF(desc_queue, desc*, DESC_QSIZE);

/*
 * Context for buffered I/O read iterator/
 */
struct bufiter {
    int fd;
    uint64_t len;

    int done;
    uint64_t total;
    desc_queue *free;

};
typedef struct bufiter bufiter;


/*
 * Thread context shared between the reader and writer threads.
 */
struct context {
    // free descriptors queue: also producer-consumer (blocking)
    desc_queue  *free;

    // I/O queue: producer-consumer (blocking)
    desc_queue    *io;

    Args *args;

    Acctg *acc;

    bufiter b;
};
typedef struct context context;

static int    bufiter_init(bufiter *ii, int fd, uint64_t len, desc_queue *free);
static desc*  bufiter_start(void *ii);
static desc*  bufiter_next(void *ii);
static uint64_t bufiter_fini(void *ii);

static int    buf_writer(void *v);
static void*  io_reader_thread(void *v);


/*
 * Copy from aa->ifd to aa->ofd
 */
int
Copy(Acctg *g, Args *aa)
{
    ssize_t r;
    desc_queue avail,
               io;

    SYNCQ_INIT(&avail, DESC_QSIZE);
    SYNCQ_INIT(&io,    DESC_QSIZE);

    context c = {
        .free = &avail,
        .io   = &io,
        .args = aa,
        .acc  = g,
    };

    desc    *dpool = NEWZA(desc,   DESC_QSIZE);
    uint8_t *bpool = NEWA(uint8_t, DESC_QSIZE * aa->iosize);

    for (r = 0; r < DESC_QSIZE; r++) {
        desc *d    = &dpool[r];
        uint8_t *b = &bpool[r * aa->iosize];

        d->buf = b;
        d->cap = aa->iosize;

        SYNCQ_ENQ(&avail, d);
    }

    if (aa->skip > 0) {
        r = aa->ipipe ? skip(aa->ifd, aa->skip) : lseek(aa->ifd, aa->skip, SEEK_SET);
        if (r < 0)
            error(1, -r, "%s: can't skip %" PRIu64 "bytes from input", aa->infile, aa->skip);
    }

    if (aa->seek > 0) {
        if (aa->opipe)
            die("can't seek on output pipe %s", aa->outfile);

        r = lseek(aa->ofd, aa->seek, SEEK_SET);
        if (r < 0)
            error(1, -r, "%s: can't seek %" PRIu64 "bytes for output", aa->outfile, aa->seek);
    }

    r = bufiter_init(&c.b, aa->ifd, aa->insize, c.free);
    if (r != 0) error(1, -r, "can't start I/O");

    // spawn new thread to read from ifd.
    pthread_t id;
    r = pthread_create(&id, 0, io_reader_thread, &c);
    if (r != 0) error(1, r, "can't create I/O read thread");

    // We perform writes in this thread. But, process errors later
    // on.
    r = buf_writer(&c);

    pthread_join(id, 0);

    if (r < 0) {
        error(1, -r, "write error on %s", aa->outfile);
    } else if (r > 0) {
        error(1, r, "read error on %s", aa->infile);
    }

    g->nrd = bufiter_fini(&c.b);

    DEL(dpool);
    DEL(bpool);

    SYNCQ_FINI(&avail);
    SYNCQ_FINI(&io);

    return 0;
}


/*
 * Output writer: reads from the prod-cons queue and writes to
 * output-fd. Errors in writing are captured as "negative" errno and
 * sent back as the retval of this function.
 */
static int
buf_writer(void *v)
{
    context *c = v;
    Args *a    = c->args;
    Acctg *g   = c->acc;
    progress p;

    progress_init(&p);
    while (1) {
        desc *d = SYNCQ_DEQ(c->io);

        if (d->size == 0) break;

        if (d->err  != 0) {
            progress_err(&p);
            return d->err;
        }

        int64_t z = fullwrite(a->ofd, d->buf, d->size);
        if (z <= 0) {
            progress_err(&p);
            return -z;
        }

        SYNCQ_ENQ(c->free, d);
        g->nwr += z;
        progress_dot(&p);
    }

    progress_done(&p);
    return 0;
}


/*
 * Read from an I/O iterator and queue to the write thread.
 */
static void *
io_reader_thread(void *v)
{
    context *c = v;
    desc *z;

    for (z = bufiter_start(&c->b); z->size > 0; z = bufiter_next(&c->b)) {
        SYNCQ_ENQ(c->io, z);
    }

    // Last descriptor -- either EOF or an error. In either case, we
    // send it to the writer thread.
    SYNCQ_ENQ(c->io, z);

    return 0;
}



/*
 * Buffer based file iterator methods.
 */


static int
bufiter_init(bufiter *ii, int fd, uint64_t len, desc_queue *free)
{
    memset(ii, 0, sizeof *ii);

    ii->fd   = fd;
    ii->len  = len;
    ii->free = free;
    return 0;
}

static desc*
bufiter_start(void *v)
{
    bufiter *ii = v;
    return bufiter_next(ii);
}

static desc*
bufiter_next(void *v)
{
    bufiter *ii = v;
    desc    *d  = SYNCQ_DEQ(ii->free);

    if (ii->done) {
        d->size = 0;
        d->err = 0;
        return d;
    }

    /*
     * This one test covers three cases:
     *  a) ii->len == 0: read till EOF (i.e., one full I/O block)
     *  b) ii->len > iosize: read one I/O block
     *  c) ii->len < iosize: read remainder.
     */
    uint64_t rem = (ii->len > 0 && ii->len <= d->cap) ? ii->len : d->cap;
    int64_t z    = fullread(ii->fd, d->buf, rem);

    if (z >= 0) {
        ii->total += z;
        d->size = z;
        d->err  = 0;
        if (ii->len > 0) {
            ii->len -= z;
            if (ii->len == 0) ii->done = 1;
        }
    } else {
        // We want to return negative error numbers for read
        // errors.
        d->err  = (int)z;
        d->size = 0;
    }

    return d;
}


static uint64_t
bufiter_fini(void *v)
{
    bufiter *ii = v;

    return ii->total;
}


