# see do.sh for usage
d=$(dirname `readlink -f $0`)

plboth() {
    local o=$1
    local oarg="-landscape"
    [ "$portrait" ] && oarg=
    shift
    pl -png -o $o.png "$@"
    pl -ps -o $o.ps $oarg "$@"
    ps2pdf $o.ps $o.pdf
}

graph3() {
    local y=$1
    local ylbl="$2"
    local y2=$3
    local ylbl2="$4"
    local y3=$5
    local ylbl3="$6"
    local ylbldistance=${7:-'0.7"'}
    local name=${name:-$data}
    local obase=${obase:-$opre$name.x_`filename_from $xlbl`.$y.`filename_from $ylbl`.$y2.`filename_from $ylbl2`.$y3.`filename_from $ylbl3`}
    local of=$obase.png
    local ops=$obase.ps
        #yrange=0
    #pointsym=none pointsym2=none
    title=${title:-$ylbl $ylbl2 $ylbl3 vs. $xlbl}
    xlbl=${xlbl:=x}
    showvars_required obase xlbl ylbl ylbl2 ylbl3
    require_files $data
    plboth $obase -prefab lines data=$data x=1  y=$y name="$ylbl" y2=$y2 name2="$ylbl2" y3=$y3 name3="$ylbl3" ylbldistance=$ylbldistance xlbl="$xlbl" title="$title" ystubfmt '%4g' ystubdet="size=6" linedet2="style=1" linedet3="style=3" -scale ${scale:-1.4}
    echo $of
}

pre() {
    local pre=$1
    shift
    local f
    for f in "$@"; do
        echo $pre$f
    done
}

main() {
    files=( $(pre "$@") )
    data=${data:-compare.$(filename_from $*)}
    xlbl=${xlbl:-space}
    showvars_required files data xlbl
    require_files ${files[*]}
    $d/merge-columns.pl ${files[*]} > $data
    preview $data
    graph3 2 "${n2:-$2}" 3 "${n3:-$3}" 4 "${n4:-$4}"
    exit
}

echo2() {
 echo "$@" 1>&2
}

errorq() {
 echo2 ERROR: "$@"
}

error() {
 errorq "$@"
 return 2
}

showvars_required() {
 echo2 $0 RUNNING WITH REQUIRED VARIABLES:
    local k
 for k in "$@"; do
  eval local v=\$$k
  echo2 $k=$v
  if [ -z "$v" ] ; then
    errorq "required (environment or shell) variable $k not defined!"
    return 1
  fi
 done
 echo2
}

require_files() {
 local f
 [ "$*" ] || error "require_files called with empty args list"
 for f in "$@"; do
    if ! have_file "$f" ; then
        error "missing required file: $f"
        return 1
    fi
 done
 return 0
}

have_file() {
    [ "$1" -a -f "$1" -a \( -z "$2" -o "$1" -nt "$2" \)  -a \( -z "$require_nonempty" -o -s "$1" \) ]
}

if ! [ "$nomain" ] ; then
    main "$@";exit
fi
