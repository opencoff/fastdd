/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * args.c - dd argument parsing..
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
 * We only support a few options of dd:
 *   bs=N
 *   count=N
 *   skip=N     -- skip N input blocks
 *   seek=N     -- skip N output blocks before first write
 *   if=FILE
 *   of=FILE
 *   iflag=nonblock
 *   oflag=nonblock,excl,sync,nocreat,notrunc,trunc
 *   size=N     -- alias for bs=1, count=N
 *   iosize=N   -- do I/O in chunks of 'iosize' bytes.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "error.h"
#include "utils/utils.h"

#include "args.h"
#include "fastdd.h"


/*
 * intermediate struct to do a table/data driven parse of the
 * command line 'dd' like arguments (bs=N if=F etc.).
 *
 * We use offsetof() to point into struct Args and scribble directly
 * based on the type.
 */
struct arg
{
    const char *str;
    int typ;
    size_t off;
};
typedef struct arg arg;

#define TYP_I       0   // uint64
#define TYP_SZ      1   // uint64 with a size suffix (k,M,G,T,P etc.)
#define TYP_S       2   // string
#define TYP_VA      3   // var arg list (comma separated)
#define TYP_BOOL    4   // boolean (0 or 1)

static const arg Validargs[] =
{
      {"bs",     TYP_SZ,   offsetof(Args, bs)}
    , {"if",     TYP_S,    offsetof(Args, infile)}
    , {"of",     TYP_S,    offsetof(Args, outfile)}
    , {"skip",   TYP_I,    offsetof(Args, skip)}
    , {"seek",   TYP_I,    offsetof(Args, seek)}
    , {"iosize", TYP_SZ,   offsetof(Args, iosize)}
    , {"count",  TYP_SZ,   offsetof(Args, count)}
    , {"iflag",  TYP_VA,   offsetof(Args, iflag)}
    , {"oflag",  TYP_VA,   offsetof(Args, oflag)}

    , {0, 0, 0}
};

struct flag {
    const char *str;
    int val;
};
#define _x(a)   {#a, a}
static const struct flag Flags[] = {
    _x(O_RDONLY),
    _x(O_WRONLY),
    _x(O_RDWR),
    _x(O_NONBLOCK),
    _x(O_APPEND),
    _x(O_EXCL),
    _x(O_CREAT),
    _x(O_TRUNC),
    _x(O_ASYNC),
    _x(O_SYNC),
#ifdef O_DIRECT
    _x(O_DIRECT),
#endif

    // always keep at the end
    {0, 0}
};

static int  parse_flags(Args *aa, size_t off, char *str, char *opt);
static void filldefault(Args *a);
static int  openfile(struct stat *p_st, const char *fn, int flags, mode_t mode);
static void xstat(struct stat *st, int fd);
static const arg* findarg(const char *s);

/*
 * Parse command line args of the form "key=value" and populate
 * 'aa'.
 */
int
Parse_args(Args *aa, int argc, char * const argv[])
{
    char tmp[1024];
    char *av[8];
    int i,
        n;

    filldefault(aa);

    for (i = 0; i < argc; i++) {
        strcopy(tmp, sizeof tmp, argv[i]);
        n = strsplit_quick(av, 2, tmp, "=", 0);

        if (n != 2) {
            die("missing '=' in argument %s", argv[i]);
            return -EINVAL;
        }

        char * s = av[0];
        char * v = av[1];
        const arg* a = findarg(s);
        if (!a) {
            die("invalid option %s", s);
            return -EINVAL;
        }

        int r;
        char *z;
        uint64_t u;

        switch (a->typ) {
            case TYP_I:
                r = strtou64(v, 0, 0, &u);
                if (r < 0) {
                    die("argument %s to %s is not an integer", v, s);
                    return -EINVAL;
                }

                *pU64(pU8(aa)+a->off) = u;
                break;

            case TYP_SZ:
                r = strtosize(v, 0, &u);
                if (r < 0) {
                    die("argument %s to %s is not a size", v, s);
                    return -EINVAL;
                }

                *pU64(pU8(aa)+a->off) = u;
                break;

            case TYP_S:
                z = pCHAR(aa)+a->off;
                strcopy(z, PATH_MAX, v);
                break;

            case TYP_VA:
                if (parse_flags(aa, a->off, v, s) < 0) return -EINVAL;
                break;

            case TYP_BOOL:
                if (0 == strcasecmp("true", v) || 0 == strcasecmp("yes", v) || 0 == strcmp("1", v)) {
                    r = 1;
                } else {
                    r = 0;
                }
                *pINT(pU8(aa)+a->off) = r;
                break;

            default:
                die("unknown typ %d; binary corrupted?", a->typ);
        }
    }

    if (aa->bs == 0) die("blocksize can't be zero!");

    // XXX Overflow check?
    aa->insize = aa->bs * aa->count;

    // some flags are useless for iflag
    aa->iflag &= ~(O_EXCL|O_TRUNC|O_WRONLY|O_RDWR);

    if (strlen(aa->infile) > 0 && 0 != strcmp("-", aa->infile)) {
        aa->ifd = openfile(&aa->ist, aa->infile,  aa->iflag, 0);
    } else {
        strcopy(aa->infile, sizeof aa->infile, "<STDIN>");
        aa->ifd = 0;
        xstat(&aa->ist, 0);
    }

    if (strlen(aa->outfile) > 0 && 0 != strcmp("-", aa->outfile)) {
        aa->ofd = openfile(&aa->ost, aa->outfile,  aa->oflag, 0600);
    } else {
        strcopy(aa->outfile, sizeof aa->outfile, "<STDOUT>");
        aa->ofd = 1;
        xstat(&aa->ost, 1);
    }

    aa->ipipe = ispipe(aa->ifd);
    aa->opipe = ispipe(aa->ofd);

    if ((aa->skip * aa->bs) > aa->insize)
        die("%" PRIu64 "input blocks skips over all input", aa->skip);

    if (!aa->ipipe) {
        if (aa->insize == 0) aa->insize = aa->ist.st_size;

        if (S_ISREG(aa->ist.st_mode) || S_ISBLK(aa->ist.st_mode)) {
            if (aa->insize > aa->ist.st_size)
                die("%s: input size is greater than file size %" PRIu64 "",
                        aa->infile, aa->ist.st_size);
        }

        if ((aa->skip * aa->bs) > aa->ist.st_size)
            die("%s: %" PRIu64 "input blocks skips past EOF", aa->infile, aa->skip);
    }


    // Always convert to byte offsets.
    aa->skip *= aa->bs;
    aa->seek *= aa->bs;

    return 0;
}


/*
 * Convert an args instance to printable string.
 */
char *
Args_string(char *buf, size_t sz, const Args *a)
{
    char iflag[256] = { 0 };
    char oflag[256] = { 0 };

    if (a->iflag > 0) {
        char t[128];
        Flag2str(t, sizeof t, a->iflag);
        snprintf(iflag, sizeof iflag, "iflag=%s", t);
    }

    if (a->oflag > 0) {
        char t[128];
        Flag2str(t, sizeof t, a->oflag);
        snprintf(oflag, sizeof oflag, "oflag=%s", t);
    }

    snprintf(buf, sz, "if=%s %s of=%s %s size=%" PRIu64 " %s %s "
                      "(bs=%" PRIu64 " count=%" PRIu64 " iosize=%" PRIu64 ")",
            a->infile,  ispipe(a->ifd) ? "(pipe)" : "",
            a->outfile, ispipe(a->ofd) ? "(pipe)" : "",
            a->insize, iflag, oflag,
            a->bs, a->count, a->iosize);

    return buf;
}

// Convert open(2) flags in 'flag' into a string buffer 'buf'
// bounded by 'bufsz'.
// Returns 'buf'
char *
Flag2str(char *buf, size_t bufsz, int flag)
{
    *buf = 0;
    const struct flag *x = Flags;
    char *s = buf;
    for (;x->str; x++) {
        if (!(flag & x->val)) continue;

        int n = snprintf(s, bufsz, "%s,", x->str);
        s += n;
        bufsz -= n;
    }

    ssize_t n = strlen(buf);
    if (buf[n-1] == ',') {
        buf[--n] = 0;
    }
    return buf;
}

/*
 * parse iflag=a,b,c or oflag=a,b,c
 */
static int
parse_flags(Args *aa, size_t off, char *str, char *ostr)
{
    char *av[8];
    int r = strsplit_quick(av, 8, str, ",", 1);

    if (r < 0) {
        warn("too many options for %s", ostr);
        return -EINVAL;
    }

    if (r == 0) return 0;

    int i;
    int v  = *pINT(pU8(aa)+off);    // load defaults
    for (i = 0; i < r; i++) {
        char * s = av[i];

        if (0 == strcasecmp("excl", s)) {
            v |= O_EXCL;
            continue;
        }

        if (0 == strcasecmp("trunc", s)) {
            v |= O_TRUNC;
            continue;
        }

        if (0 == strcasecmp("sync", s)) {
            v |= O_SYNC;
            continue;
        }

#ifdef O_DIRECT
        if (0 == strcasecmp("direct", s)) {
            v |= O_DIRECT;
            continue;
        }
#endif

        if (0 == strcasecmp("nonblock", s)) {
            v |= O_NONBLOCK;
            continue;
        }

        if (0 == strcasecmp("notrunc", s)) {
            v &= ~O_TRUNC;
            continue;
        }

        if ((0 == strcasecmp("nocreat", s)) ||
            (0 == strcasecmp("nocreate", s))) {
            v &= ~O_CREAT;
            continue;
        }

        warn("unknown option '%s' for '%s'", s, ostr);
        return -EINVAL;
    }

    if ((v & O_EXCL) && !(v & O_CREAT)) {
        warn("excl with nocreat (or creat) is meaningless");
        v &= ~O_EXCL;
    }

    // We _set_ the new value instead of appending to what's already
    // there. Recall: we started 'v' with defaults from this
    // offset.
    *pINT(pU8(aa)+off) = v;
    return 0;
}


static void
filldefault(Args *a)
{
    memset(a, 0, sizeof *a);
    a->bs = 512;   // same as dd
    a->infile[0]  = 0;  // stdin
    a->outfile[0] = 0;  // stdout
    a->iosize     = 65536; // 64k blocks of I/O

    a->iflag = O_RDONLY;
    a->oflag = O_CREAT | O_WRONLY;
}

static const arg*
findarg(const char *s)
{
    const arg* v = &Validargs[0];

    for(; v->str; v++) {
        if (0 == strcmp(v->str, s)) return v;
    }
    return 0;
}

// Open a file and ensure it is valid
static int
openfile(struct stat *st, const char *fn, int flags, mode_t mode)
{
    const mode_t want = S_IFIFO|S_IFREG|S_IFBLK|S_IFCHR;

    int r = open(fn, flags, mode);
    if (r < 0) error(1, errno, "can't open %s", fn);

    if (fstat(r, st) < 0) error(1, errno, "can't fstat %s", fn);

    if (0 == (st->st_mode & want))
        die("%s is not a file or fifo?", fn);

    // XXX We reuse struct stat members to fill in the blanks
    if (S_ISBLK(st->st_mode)) {
        uint64_t sz;
        int x;

        if ((x = Blksize(&sz, r)) < 0) error(1, -x, "can't get blocksize of %s", fn);

        st->st_size = sz;
    }
    return r;
}


// stat/fstat that dies on error
static void
xstat(struct stat *st, int fd)
{
    const mode_t want = S_IFIFO|S_IFREG|S_IFBLK;
    int r = fstat(fd, st);

    if (r < 0) error(1, errno, "can't stat fd %d", fd);

    if (0 == (st->st_mode & want))
        die("%s is not a file or fifo?", fd == 0 ? "STDIN" : "STDOUT");

    // XXX We reuse struct stat members to fill in the blanks
    if (S_ISBLK(st->st_mode)) {
        uint64_t sz;
        int x;

        if ((x = Blksize(&sz, fd)) < 0) error(1, -x, "can't get blocksize of fd %d", fd);

        st->st_size = sz;
    }
}

// EOF
