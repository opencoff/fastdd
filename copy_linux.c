/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * copy_linux.c - fast path for linux using copy_file_range()
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
#include <errno.h>
#include <fcntl.h>

#include "error.h"
#include "utils/new.h"
#include "utils/progbar.h"
#include "fast/syncq.h"
#include "fastdd.h"

//static int pipe_splice_threaded(Acctg *g, Args *a);
static int pipe_splice_sequential(Acctg *g, Args *a);

#define progressbar_err(p)  progressbar_finish(p, 0, 1)

int
Copy(Acctg *g, Args *a)
{
    /*
     * If neither source or dest is a pipe, we have to create a pipe
     * and connect the two.
     */
    if (!(a->ipipe || a->opipe)) return pipe_splice_sequential(g, a);


    /*
     * At least one of the fd's is a pipe. So, a single splice()
     * call is sufficient.
     */

    off_t ioff  = a->skip,
          ooff  = a->seek;
    uint64_t n  = a->insize;
    int done    = 0;

    loff_t *p_in  = 0,
           *p_out = 0;

    progress p;

    progressbar_init(&p, Quiet ? -1 : 2, a->insize, P_HUMAN);

    /*
     * Skip initial bytes on the input & output as needed.
     */
    if (ioff > 0) {
        if (a->ipipe) {
            ssize_t r = skip(a->ifd, ioff);
            if (r < 0) error(1, -r, "can't skip %" PRIu64 " bytes from %s", ioff, a->infile);
        } else {
            p_in = &ioff;
        }
    }

    if (ooff > 0) {
        if (a->opipe)
            die("can't seek %" PRIu64 " bytes of output pipe %s", ooff, a->outfile);

        p_out = &ooff;
    }

    while (!done) {
        size_t  m = n > 0 && n <= a->iosize ? n : a->iosize;
        ssize_t r = splice(a->ifd, p_in, a->ofd, p_out, m, SPLICE_F_MOVE|SPLICE_F_MORE);
        if (r < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;

            progressbar_err(&p);
            error(1, errno, "I/O error while splicing around offset %" PRIu64 "", g->nrd);
        }
        if (r == 0) break;

        progressbar_update(&p, r);

        g->nrd += r;
        g->nwr += r;
        if (n > 0) {
            n -= r;
            if (n == 0) done = 1;
        }
    }

    progressbar_finish(&p, 1, 0);
    return 0;
}

/*
 * Splice two non-pipe Fd's by creating an intermediate pipe().
 */
static int
pipe_splice_sequential(Acctg *g, Args *a)
{
    off_t ioff = a->skip,
          ooff = a->seek;
    uint64_t n = a->insize;
    int done   = 0;

    int fd[2];

    progress p;

    if (pipe(fd) < 0) error(1, errno, "can't create pipe for splicing");

    progressbar_init(&p, Quiet ? -1 : 2, a->insize, P_HUMAN);

    while (!done) {
        size_t  m = n > 0 && n <= a->iosize ? n : a->iosize;
        ssize_t r = splice(a->ifd, &ioff, fd[1], 0, m, SPLICE_F_MOVE|SPLICE_F_MORE);
        if (r < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;

            progressbar_err(&p);
            error(1, errno, "I/O read error while splicing around offset %" PRIu64 "", ioff);
        }
        if (r == 0) break;

        g->nrd += r;
        g->nwr += r;
        if (n > 0) {
            n -= r;
            if (n == 0) done = 1;
        }

        while (r > 0) {
            ssize_t s = splice(fd[0], 0, a->ofd, &ooff, r, SPLICE_F_MOVE|SPLICE_F_MORE);
            if (s < 0) {
                if (errno == EAGAIN || errno == EINTR) continue;

                progressbar_err(&p);
                error(1, errno, "I/O write error while splicing around offset %" PRIu64 "", ooff);
            }

            r -= s;
            progressbar_update(&p, s);
        }
    }

    progressbar_finish(&p, 1, 0);

    close(fd[0]);
    close(fd[1]);

    return 0;
}


#if 0

/*
 * Multi-threaded splice.
 *
 * Works - but yields performance degradation compared to single
 * threaded workload.
 */

#define IOQ_SIZE       128

struct io {
    uint64_t size;
    int err;
};
typedef struct io io;

SYNCQ_TYPEDEF(ioq, io, IOQ_SIZE);

struct context {
    int   fd[2];
    Args  *a;
    ioq   *q;
    Acctg *g;
};
typedef struct context context;

static void * reader_thread(void *v);
static int    writer_thread(context *c);



static int
pipe_splice_threaded(Acctg *g, Args *a)
{
    int r;
    ioq q;

    SYNCQ_INIT(&q, IOQ_SIZE);

    context c = {
        .a = a,
        .q = &q,
        .g = g,
    };

    if (pipe(c.fd) < 0) error(1, errno, "can't create pipe for splicing");

    // spawn new thread to read from ifd.
    pthread_t id;
    r = pthread_create(&id, 0, reader_thread, &c);
    if (r != 0) error(1, r, "can't create I/O write thread");

    r = writer_thread(&c);

    void *ret = 0;
    pthread_join(id, &ret);

    close(c.fd[0]);
    close(c.fd[1]);

    SYNCQ_FINI(&q);

    if (r < 0) {
        error(1, -r, "Write error on %s", a->outfile);
    } else if (r > 0) {
        error(1, r, "read error on %s", a->infile);
    }

    return 0;
}

static int
writer_thread(context *c)
{
    Args *a    = c->a;
    Acctg *g   = c->g;
    off_t ooff = a->seek;

    while (1) {
        io o = SYNCQ_DEQ(c->q);
        uint64_t r = o.size;

        if (r == 0)      return 0;
        if (o.err  != 0) return o.err; 

        g->nwr += r;

        while (r > 0) {
            ssize_t s = splice(c->fd[0], 0, a->ofd, &ooff, r, SPLICE_F_MOVE|SPLICE_F_MORE);
            if (s < 0) {
                if (errno == EAGAIN || errno == EINTR) continue;
                return -errno; // negative retval for caller to signify write errors
            }

            r -= s;
        }
    }

    return 0;
}

static void *
reader_thread(void *v)
{
    context *c = v;
    Args *a    = c->a;
    Acctg *g   = c->g;
    off_t ioff = a->skip;
    uint64_t n = a->insize;
    int done   = 0;

    while (!done) {
        size_t  m = n > 0 && n <= a->iosize ? n : a->iosize;
        ssize_t r = splice(a->ifd, &ioff, c->fd[1], 0, m, SPLICE_F_MOVE|SPLICE_F_MORE);
        if (r < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;

            io o = {
                .size = ioff,   // XXX Overloaded semantics for size
                .err  = errno,
            };

            SYNCQ_ENQ(c->q, o);
            return 0;
        }

        if (r == 0) break;

        io o = {
            .size = r,
            .err  = 0,
        };

        SYNCQ_ENQ(c->q, o);

        g->nrd += r;
        if (n > 0) {
            n -= r;
            if (n == 0) done = 1;
        }
    }

    // EOF
    io o = {
        .size = 0,
        .err  = 0,
    };

    SYNCQ_ENQ(c->q, o);

    return 0;
}
#endif // threaded splice
