#!/bin/sh

INI=test/blunsom08.dtrain.ini
#INI=test/nc-wmt11/nc-wmt11.loo.dtrain.ini

rm /tmp/dtrain-*
./dtrain -c $INI $1 $2 $3 $4 2>/dev/null

