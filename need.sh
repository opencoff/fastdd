#! /usr/bin/env bash

PORT=../portable-git
HERE=./portable
#e=echo

copy() {
    for f in $*; do
            t=$PORT/$f
            h=$HERE/$f
            if [ -d $t ]; then
                $e mkdir -p $h
                $e cp -pr $t/ $h
                continue
            elif [ ! -f $t ]; then
                    echo "$0: Can't find $f .." 1>&2
                    continue
            fi
            b=$(dirname $h)
            $e mkdir -p $b
            $e cp $t $HERE/$f
    done
}

sync() {
    find $HERE -type f | \
        while read f; do
            b=${f##$HERE/}
            t=$PORT/$b
            [ $t -nt $f ] && cp -v $t $f
        done
}

die() {
    echo "$@" 1>&2
    exit 1
}


if [ "x$1" = "x" ]; then
    die "Usage: $0 copy|sync file|dir [file|dir ...]"
fi

op=$1; shift
case "$op" in
    copy|cp) copy "$@" ;;

    sync|syn) sync ;;

    *)
        echo "Usage: $0 copy|sync file|dir [file|dir ...]"
        exit 1
        ;;
esac
