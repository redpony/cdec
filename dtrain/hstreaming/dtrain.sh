#!/bin/bash

pushd .
cd ..
ID=$(basename $(pwd))
popd
./dtrain -c dtrain.ini --hstreaming $ID 

