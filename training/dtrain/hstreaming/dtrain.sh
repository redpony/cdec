#!/bin/bash
# script to run dtrain with a task id

pushd . &>/dev/null
cd ..
ID=$(basename $(pwd)) # attempt_...
popd &>/dev/null
./dtrain -c dtrain.ini --hstreaming $ID

