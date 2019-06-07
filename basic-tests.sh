#! /usr/bin/env bash

# Self tests for fastdd
# Author: Sudhi Herle
# License: Public Domain

FASTDD="./fastdd -q"
TESTDIR=/tmp/fastdd$$

# V important for pipelined commands!
set -o pipefail

die() {
    echo "$0: $@" 1>&2
    exit 1
}

mkdir -p $TESTDIR ||  die "Can't make $TESTDIR"
#trap "rm -rf $TESTDIR" EXIT INT QUIT ABRT

uname=$(uname)
case $uname in
    Linux*)
        MD5=md5sum
        filesz() {
            local fn=$1
            local sz=$(stat -c '%s' $fn)
            echo $sz
            return 0
        }
        ;;

    Darwin|OpenBSD)
        MD5='md5 -q'
        filesz() {
            local fn=$1
            eval $(stat -s $fn)
            echo $st_size
            return 0
        }
        ;;

    *) die "Don't know how to do md5 on $uname"
        ;;
esac

FASTDD=
for d in dbg rel; do
    f=$(uname)-$d/fastdd
    if [ -x $f ]; then
        FASTDD=$f
        break
    fi
done

[ -z $FASTDD ] && die "can't find fastdd in $(uname)-rel or $(uname)-dbg"


# verify arguments and such
basictests() {
    local t=$TESTDIR
    local in=$t/in
    local out=$t/out
    local out2=$t/out2

    rdd if=/dev/urandom of=$in bs=1024 count=8 || die "can't dd"

    # This should be first test to verify "nocreat"
    begin "copy +nocreat"
    fdd if=$in of=$out bs=1024 count=6 oflag=nocreat && die "fail nocreat"
    end " OK"

    begin "simple copy"
    fdd if=$in of=$out bs=1024 count=8 || die "fail simple"
    [ -f $out ] || die "fail no outfile"
    xcmp $in $out

    # Keep file from previous run!
    begin "copy +notrunc"
    fdd if=$in of=$out bs=1024 count=2 oflag=notrunc || die "fail notrunc"
    xcmp $in $out

    begin "copy +notrunc +nocreat"
    fdd if=$in of=$out bs=1024 count=1 oflag=notrunc,nocreat || die "fail notrunc"
    xcmp $in $out

    # O_EXCL => don't succeed if file exists
    # Do NOT remove file after previous test; we need it to verify 'excl'
    begin "copy +excl"
    fdd if=$in of=$out bs=1024 count=8 oflag=excl && die "fail exist+excl"
    end " OK"

    rm -f $out
    begin "copy noexist +excl"
    fdd if=$in of=$out bs=1024 count=8 oflag=excl || die "fail noexist+excl"
    xcmp $in $out

    # Do NOT remove file after previous test; we need it to verify 'trunc'
    begin "copy +trunc"
    rdd if=$in of=$in.2 bs=1024 count=2 || die "can't run dd"
    fdd if=$in.2 of=$out bs=1024 count=2 oflag=trunc iosize=1k || die "failed trunc"
    xcmp $in.2 $out
    rm -f $out

    begin "simple opipe"
    (fdd if=$in bs=1024 count=8 iosize=1k | cat - > $out) || die "failed opipe"
    xcmp $in $out
    rm -f $out

    begin "simple ipipe"
    (cat $in | fdd of=$out bs=1024 count=8 iosize=1k) || die "failed ipipe"
    xcmp $in $out
    rm -f $out

    begin "simple dualpipe"
    (cat $in | fdd bs=1024 count=8 iosize=1k | cat - > $out) || die "failed dualpipe"
    xcmp $in $out
    rm -f $out

    begin "seek opipe"
    (fdd if=$in bs=1024 count=8 seek=1 | cat - >$out) && die "fail seek opipe"
    end " OK"

    rm -rf $TESTDIR
}

begin() {
    echo -n "$@"
}

end() {
    echo "$@"
}

xcmp() {
    local inf=$1
    local dec=$2

    local a=`$MD5 $inf| awk '{print $1}'`
    local b=`$MD5 $dec| awk '{print $1}'`
    if [ $a != $b ]; then
        end " Fail"
    else
        end " OK"
    fi
    return 0
}

fdd() {
    $FASTDD -q "$@" 2>/dev/null
    #$DD "$@"
    return $?
}

rdd() {
    dd "$@" >/dev/null 2>&1
    return $?
}




basictests
