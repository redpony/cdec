#!/bin/sh

#INI=test/blunsom08.dtrain.ini
#INI=test/nc-wmt11/dtrain.ini
#INI=test/EXAMPLE/dtrain.ini
#INI=test/EXAMPLE/dtrain.ruleids.ini
INI=test/toy.dtrain.ini
#INI=test/EXAMPLE/dtrain.cdecrid.ini

#rm /tmp/dtrain-*
./dtrain -c $INI $1 $2 $3 $4 

