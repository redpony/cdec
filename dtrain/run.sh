#!/bin/sh

rm /tmp/dtrain-*
rm weights_.gz
./dtrain -c data/dtrain.blunsom08.ini 2>/dev/null

