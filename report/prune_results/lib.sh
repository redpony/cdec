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

showvars_optional() {
 echo2 RUNNING WITH OPTIONAL VARIABLES:
    local k
 for k in "$@"; do
  if isset $k ; then
   eval local v=\$$k
   echo2 $k=$v
  else
   echo2 UNSET: $k
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

filename_from() {
  perl -e '
sub superchomp {
    my ($ref)=@_;
    if ($$ref) {
        $$ref =~ s|^\s+||;
        $$ref =~ s|\s+$||;
        $$ref =~ s|\s+| |g;
    }
}

sub filename_from {
    my ($fname)=@_;
   &superchomp(\$fname);
   $fname =~ s|[^a-zA-Z0-9_-]+|.|g;
   $fname =~ s|^\.|_|;
    return $fname;
}

$"=" ";print filename_from("@ARGV"),"\n"
' -- "$@"
}

preview() {
 tailn=${tailn:-20}
 head -v -n $tailn "$@"
}

isset() {
  eval local v=\${$1+set}
  [ "$v" = set ]
}
