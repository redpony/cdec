d=$(dirname `readlink -f $0`)/
. $d/decode.sh
in=1dev.ur cfg=cdec-fsa decode
