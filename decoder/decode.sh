d=$(dirname `readlink -f $0`)/
decode() {
if [ "$lm" ] ; then
    lmargs0=-F
    lmargs1="LanguageModel lm.gz -n LM"
fi
set -x
$gdb ${cdec:=$d/cdec} -c $d/${cfg:=cdec-fsa}.ini -i $d/${in:=1dev.ur} $lmargs0 "$lmargs1" --show_features --show_config --show_weights "$@"
set +x
}
