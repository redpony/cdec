#!/usr/bin/env bash

scriptDir=$(cd $(dirname $0); pwd)

set -u
autoreconf -ifv
./configure --with-boost=$HOME/prefix --disable-gtest
make clean
make -j32

set +eo pipefail
make -j32

echo >&2 "============================="
echo >&2 "TESTING: $(git log | head -n1 | cut -f2 -d' ')"
echo >&2 "============================="
zcat $scriptDir/mapoutput.abj.gz \
  | $scriptDir/cdec/pro-train/mr_pro_reduce --weights $scriptDir/weights.0 -C 500 -y 5000 --interpolate_with_weights 1
echo >&2 "============================="

sleep 5
