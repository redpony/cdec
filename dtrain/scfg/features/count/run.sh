#!/bin/sh

hadoop pipes -conf jobconf.xml -input in/grammar.test -output out/grammar.test.out

