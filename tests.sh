#! /usr/bin/env bash


# Globals
BDEVIN=
BDEVOUT=

Z=$(basename $0)
TESTDIR=/tmp/fastdd
Verbose=


set -o pipefail

die() {
    echo "$Z: $@" 1>&2
    exit 1
}

warn() {
    echo "$Z: $@" 1>&2
}


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
for d in rel dbg; do
    f=$(uname)-$d/fastdd
    if [ -x $f ]; then
        FASTDD=$f
        break
    fi
done

[ -z $FASTDD ] && die "can't find fastdd in $(uname)-rel or $(uname)-dbg"


main() {
    local prev=
    local opt=
    local optarg=
    local args=

    for opt in $*; do
        shift

        if [ -n "$prev" ]; then
            eval "$prev=\$opt"
            prev=
            continue
        fi

        case $opt in
            -*=*) optarg=`echo "$opt" | sed 's/[-_a-zA-Z0-9]*=//'` ;;
            *) optarg= ;;
        esac

        case $opt in
            --help|-h|--hel|--he|--h) usage ;;

            --blockdev-in=*|--in-blockdev=*)   BDEVIN=$optarg  ;;
            --blockdev-out=*|--out-blockdev=*) BDEVOUT=$optarg ;;
            --blockdev-in|--in-blockdev|-b)    prev=BDEVIN     ;;
            --blockdev-out|--out-blockdev|-B)  prev=BDEVOUT    ;;

            --debug|-x)   set -x            ;;
            --verbose|-v) Verbose=1         ;;

            -*) die "Unknown option '$opt'" ;;

            *)
                args="$args $opt"
                #for xx
                #do
                    #args="$args $xx"
                #done
                #break
                ;;
        esac
    done

    [ -z "$args" ] && usage

    mkdir -p $TESTDIR

    for f in $args; do
        readtest $f
    done

    return 0
}

usage() {

    cat <<EOF
$0 - Run dd vs. fastdd tests from a test-description file.

Usage: $0 [options] test-file [test-file ..]

Options:
    -h, --help          Show this help message and quit
    -x                  Run in debug/trace mode [False]
    -v, --verbose       Run in verbose mode [False]
    -b B, --blockdev-in=B  Use 'B' as the input block dev []
    -B B, --blockdev-out=B  Use 'B' as the output block dev []
EOF

    exit 0
}

# generate one test
onetest() {
    local num=$1;   shift       # test number
    local inf=$1;   shift
    local outf=$1;  shift
    local bs=$1;    shift
    local count=$1; shift
    local args="$@"
    local tmp="${TESTDIR}/${num}"
    local src=$(makefile $inf i $tmp)
    local dst=$(makefile $outf o $tmp)
    local ddargs=$(ddargs $args)
    local input=$tmp/input
    local dsta=
    local dstb=
    local ddbs=$(parsesize $bs)
    local ddcount=$(parsesize $count)

    mkdir -p $tmp || die "$num: can't mkdir $tmp"

    args="$args bs=$bs count=$count"
    ddargs="$ddargs bs=$ddbs count=$ddcount"

    # Generate input first.
    geninput $input $bs $count

    #  file output when dest is a pipe
    local pipea=
    local pipeb=

    # command prefixes and suffixes for fastdd & dd
    local prefa=
    local prefb=
    local suffa=
    local suffb=

    # read from file or pipe and setup input command prefix
    case $inf in
        fil*)
            prefa=""
            #prefb="dd        if=$input"
            ;;

        pip*)
            prefa="cat"
            ;;

        block*|bdev)
            input=$BDEVIN
            prefa=""
            ;;

        *) die "$num: Unknown source type '$inf' (not file|pipe|bdev)" ;;

    esac

    case $outf in
        fil*)
            dst=${tmp}/outf${RANDOM}
            dsta=${dst}a
            dstb=${dst}b
            suffa=""
            mda="$dsta"
            mdb="$dstb"
            ;;

        pip*)
            # capture output via pipe into a file
            pipea="$tmp/pipea${RANDOM}"
            pipeb="$tmp/pipeb${RANDOM}"

            suffa="cat"
            mda="$pipea"
            mdb="$pipeb"
            ;;

        block*|bdev)
            # don't write to _two_ block devices!
            dsta=${BDEVOUT}
            dstb=$tmp/block${RANDOM}
            suffa=""
            mda="$dsta"
            mdb="$dstb"
            ;;

        *) die "$num: Unknown dest type '$outf' (not file|pipe|bdev)"
            ;;
    esac

    # Run fastdd & dd; compare $mda vs. $mdb
    #
    # XXX Ugly code ahead; no clean way to express the 4 different
    # combinations. ideally I'd like to have done 'eval "$prefa $args $suffa"'
    case $prefa in
        cat)
            case $suffa in
                cat)
                    #verbose "  cat $input | fastdd $args | cat - > $pipea"
                    #verbose "  cat $input | dd     $ddargs | cat - > $pipeb"
                    (cat $input | $FASTDD -q $args | cat - > $pipea) || die "$num: can't run fastdd"
                    (cat $input | xdd      $ddargs | cat - > $pipeb) || die "$num: can't run dd"
                    ;;
                *)
                    #verbose "  cat $input | fastdd $args of=$dsta"
                    #verbose "  cat $input | dd     $ddargs of=$dstb"
                    (cat $input | $FASTDD -q $args of=$dsta) || die "$num: can't run fastdd"
                    (cat $input | xdd      $ddargs of=$dstb) || die "$num: can't run dd"
                    ;;
            esac
            ;;

        *)
            case $suffa in
                cat)
                    #verbose "  fastdd if=$input $args | cat - > $pipea"
                    #verbose "  dd     if=$input $ddargs | cat - > $pipeb"
                    ($FASTDD -q if=$input $args   | cat - > $pipea) || die "$num: can't run fastdd"
                    (xdd        if=$input $ddargs | cat - > $pipeb) || die "$num: can't run dd"

                    ;;
                *)
                    #verbose "  fastdd if=$input $args of=$dsta"
                    #verbose "  dd     if=$input $ddargs of=$dstb"
                    $FASTDD -q if=$input $args   of=$dsta || die "$num: can't run fastdd"
                    xdd        if=$input $ddargs of=$dstb || die "$num: can't run dd"
                    ;;
            esac
            ;;

    esac

    #verbose "  md5 $mda $mdb"
    local x=$(xcksum $mda)
    local y=$(xcksum $mdb)

    if [ $x != $y ]; then
        die "$num: checksum mismatch $mda vs. $mdb".
    else
        rm -rf $tmp
    fi


    return 0
}


# Generate an input file of a given size - only if needed
geninput() {
    local fn=$1
    local bs=$(parsesize $2)
    local count=$(parsesize $3)
    local want=$(( $bs * $count ))
    local sz=

    [ -f $fn ] || touch $fn

    sz=$(filesz $fn)

    if [ $sz -lt $want ]; then
        # Make a suitable file
        verbose "  gen input bs=$bs count=$count file=$input"
        xdd if=/dev/urandom of=$input bs=$want count=1 || die "Can't generate $want sized input"
    fi
    return 0
}




# convert fastdd syntax to dd syntax for the optional flags
ddargs() {
    local a=
    local v=
    local s=

    for a in "$@"; do
        case $a in
            iosize=*) ;;

            # ignore this
            iflag*) ;;

            # Traditional dd doesn't handle 'trunc' or 'creat'. Only
            # the "no*" equivalents.
            oflag=*)
                s=${a##oflag=}
                case $s in
                    trunc|creat)                ;;
                    *)           v="$v conv=$s" ;;
                esac
                ;;

            *) v="$v $a" ;;
        esac
    done

    echo $v
    return 0
}

xdd() {
    dd "$@" 2>/dev/null
    return $?
}


# make a filename from a file type. e.g., from "file" - generate a
# random file name. For a blockdev, return the global $BLOCKDEV
makefile() {
    local ty=$1
    local pref=$2
    local tmp=$3
    local fn=
    local dn=

    case $ty in
        file|fil*)
            fn=${tmp}/${pref}f${RANDOM}
            ;;

        pipe|pip*) fn=
            ;;

        block*|bdev)
            case $pref in
                i*) fn=$BDEVIN  ;;
                o*) fn=$BDEVOUT ;;
                *) ;;
            esac
            ;;

        *) die "Unknown type '$ty' (not file|pipe|bdev)" ;;
    esac

    echo $fn
    return 0
}

readtest() {
    local fn=$1; shift

    local n=0
    grep -v '#' $fn | while read ll; do
        n=$(( $n + 1 ))
        [ -z "$ll" ] && continue

        #ll=$(echo $ll| sed -e 's/  */:/')
        #IFS=:
        set -- $ll
        local src=$1; shift
        local dst=$1; shift
        local count=$1; shift
        local bs=$1; shift
        local args="$@"

        if [ -z "$bs" ]; then
            die "$fn: malformed line: $ll"
        fi

        if [ -z "$BDEVIN" ]; then
            case $src in
                bdev|block*)
                    warn "$fn: No blockdev specified; skipping [$ll] .."
                    continue
                    ;;
                *)
                    ;;
            esac
        fi

        if [ -z "$BDEVOUT" ]; then
            case $dst in
                bdev|block*)
                    warn "$fn: No blockdev specified; skipping [$ll] .."
                    continue
                    ;;
                *)
                    ;;
            esac
        fi

        verbose "$fn: $n ($src $dst $bs x $count)"
        onetest $n $src $dst $bs $count $args
    done
}

# calculate md5 and return it
xcksum() {
    local fn=$1

    set -- $($MD5 $fn)
    echo $1
    return 1
}

verbose() {
    [ -n "$Verbose" ] && echo "$@"
}

# Size suffixes
_KB=1024
_MB=1048576
_GB=$(( 1024 * $_MB ))
_TB=$(( 1024 * $_GB ))
_PB=$(( 1024 * $_TB ))

# Parse a size suffix
parsesize() {
    local s=$1

    case $s in
        [0-9]*[kK])
            s=${s%%k}
            s=${s%%K}
            s=$(( $s * _KB ))
            ;;

        [0-9]*M)
            s=${s%%M}
            s=$(( $s * _MB ))
            ;;

        [0-9]*G)
            s=${s%%G}
            s=$(( $s * _GB ))
            ;;

        [0-9]*T)
            s=${s%%T}
            s=$(( $s * _TB ))
            ;;

        [0-9]*P)
            s=${s%%P}
            s=$(( $s * _PB ))
            ;;

        [0-9]*) ;;

        *) die "$s is not a number or size?"
    esac

    echo $s
    return 0
}

main "$@"
