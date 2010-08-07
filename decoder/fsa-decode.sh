d=$(dirname `readlink -f $0`)/
$gdb $d/cdec -c $d/cdec-fsa.ini -i $d/1dev.ur "$@"
