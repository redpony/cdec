#!/bin/bash

pushd . &>/dev/null
cd ..
ID=$(basename $(pwd)) # attempt_...
popd &>/dev/null
./dtrain -c dtrain.ini --hstreaming $ID 

