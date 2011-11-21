#!/bin/bash

pushd .
cd ..
ID=$(basename $(pwd)) # attempt_...
popd
./dtrain -c dtrain.ini --hstreaming $ID 

