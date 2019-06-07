# What is `fastdd`
`fastdd` is a performance enhanced, simpler implementation of `dd`.
On Linux, it uses `splice(2)` to avoid data-copy to/from user space.
On other platforms, it uses multiple threads and larger block sizes
to speed up I/O.

`fastdd` doesn't try to emulate all the behavior of `dd`; thus, the
notion of "blocks" (and "block size") is used only as a multipler
for the number of bytes to move. The I/O size is picked based on the
source and destination (default is 64k).

# What part of `dd` does it implement?
It only supports a few options of dd:

 * bs=N
 * count=N
 * skip=N     -- skip N input blocks
 * seek=N     -- skip N output blocks before first write
 * if=FILE
 * of=FILE
 * iflag=nonblock
 * oflag=nonblock,excl,sync

 Each of the integer arguments `N` can have an optional suffix of
 `k`, `M`, `G`, `T`, `P` for kilo, Mega, Giga, Tera, Peta byte
 respectively (**multiples of 1024**).

The source and destination can be a file, pipe, character-device or
block-device.

# Building and Installing `fastdd`
`fastdd` currently is designed for Linux, OpenBSD and MacOS;
therefore the makefile only supports those 3 OSes.

You need `GNU make`, `gcc`. There is no `configure` mess, the
code is organized to be generic enough to build on modern POSIX
systems (depends on pthreads):

On Darwin and OpenBSD:

    $ gmake # or gnumake

On Linux:

    $ make

By default, the makefile generates a "release" binary (with full
optimization). The build artifacts produces the `fastdd` binary
in a OS specific directory:

* Linux: `Linux-rel`
* Mac OS: `Darwin-rel`
* OpenBSD: `OpenBSD-rel`

## Installing `fastdd`
Fastdd can be installed in any location:

1. System default: `sudo make install DESTDIR=/usr`
2. Local install: `sudo make install DESTDIR=/usr/local`
3. Home dir: `make install DESTDIR=$HOME`

In each case, the `fastdd` binary goes in `$DESTDIR/bin`.

## Using `fastdd`
Fastdd is written to be usable without having to remember
complicated flags. You only need to know "if=" and "of=".
For the most common use cases of copying from a file to a block
device, you don't need to specify `bs=` or `count=` arguments;
`fastdd` can infer the input size and use a platform optimal block
size for copying.

e.g., if your USB device on Linux was on `/dev/sde`, then you can
turn it into a bootable ISO like so:

    fastdd if=my.iso of=/dev/sde

That's it.  If you do forget what flags to use, try:

    fastdd --help

Unless the `--quiet` option is used, `fastdd` prints a '.' for
every block of data written (here "block" means `iosize` sized
quantity).

# Performance Numbers
Anecdotally, on OpenBSD and Darwin, the multi-threaded version seems
to be faster than the native dd. On Linux, the version with
`splice(2)` seems to be faster than `dd`.

**TODO**: One of these days, I will sit and write a repetable benchmark
script.


# Developer Notes
On Linux, `fastdd` is single-threaded and uses `splice(2)` for
moving data in the kernel avoiding all user-space reads. If
neither source nor destination are pipes (sockets), `fastdd`
creates an intermediate pipe to splice the data.

On other platforms, `fastdd` is multi-threaded and uses a separate
read thread to gather I/O blocks. The reader and writer communicate
via producer-consumer queue.

In both cases, I/O (`splice(2)` or `read(2)`) is done in units of
`iosize` (command line parameter).

## Testing & Test Framework
There are two test harnesses:

1. `basic-tests.sh`: This is simple set of test cases to cover basic
   functionality, command-line flags, `oflag` combinations etc.

2. `tests.sh`: This is a data-file test driver that reads from
   `tests.in` and runs each test in turn. Each test comprises of
   generating some input, using `fastdd` to move the data from source
   to destination, repeating the same data movement using `dd` and
   comparing checksums of the resulting output.

### Using tests.sh
New tests can be added into 'tests.in' or its own input file. The
format of the input file is documented in tests.in.

Running the data-driven tests is simple:

    ./tests.sh tests.inp

## Debug builds
You can build a debug version of the program:

    make DEBUG=1 -j5

And the build artifacts will be in the directory `$OS-dbg`.

## Guide to Source Code

* fastdd.c - `main()` for `fastdd`.

* args.c - `fastdd` command line parsing (key=value). It uses a
  table driven approach to parse the values directly into a struct
  instance (uses `offsetof()`).

* blksize_darwin.c - Implementation of `Blksize()` for Mac OS
  (tested on 10.11 -- 10.14)

* blksize_linux.c - Implementation of `Blksize()` for Linux (tested
  on Linux 4.18)

* blksize_openbsd.c - Implementation of `Blksize()` for OpenBSD
  (tested on 6.5).

* copy_linux.c - Implementation of `Copy()` for Linux using
  `splice(2)`.

* copy_posix.c - Implementation of `Copy()` using pthreads for
  non-Linux platforms (tested only on Darwin and OpenBSD).

* disksize.c - Small test program to call `Blksize()` and print the
  resulting disk size.

* utils.c - I/O utility functions.

* opts.c - Auto-generated file for parsing long and short options;
  the command line options are in opts.in. The code uses standard
  `getopt_long()` - but removes the tedium of having to write the
  option processing by hand.

There is a separate directory `portable/` that is a partial copy of
an [external repository](github.com/opencoff/portable-lib). This
subdirectory only contains the files needed to build `fastdd`. Some
functionality from that lib:

    * Generic (type-safe) lists, producer-consumer queue
    * Implementation of POSIX semaphores for Darwin
    * functions to convert "sizes" to strings and vice versa (a size
      string is a numeric string with a suffix of `[kKMGTPE]`).

## Adding support for other OSes
The easiest way to add support to other POSIX OSes (FreeBSD,
DragonFlyBSD etc.), is:

* Implement `Blksize()` for that OS - follow similar implementations
  as in in *blksize_darwin.c*, *blksize_openbsd.c* etc.

* Make changes to GNUmakefile:

    *  Add the OS specific object files to its var
    *  Add any necessary LD libs to its var

e.g., for some POSIX OS "foo":

    * write code for *blksize_foo.c*
    * *GNUMakefile* changes:
       1. `foo_objs = blksize_foo.o copy_posix.o`
       2. `foo_LIBS =`


This will give you a working version that uses pthreads for I/O. If
your OS supports `splice(2)` like functionality, you have to write
the fast-path code for `Copy()` and put it in *copy_$OS.c*.

## TODO
* Benchmark suite to measure performance on supported platforms
* For non-linux platforms, is `mmap(2)` for source and/or
  destination worth it?
