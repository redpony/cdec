#!/bin/bash

HADOOP_HOME=/usr/lib/hadoop-0.20
JAR=contrib/streaming/hadoop-streaming-0.20.2-cdh3u1.jar
HSTREAMING="$HADOOP_HOME/bin/hadoop jar $HADOOP_HOME/$JAR"

IN=in/nc-wmt11-de-en-dyer-cs-joshua.tok.lc.fixamp1.loo.psg.dtrain.1400m
OUT=out/nc-wmt11-de-en-dyer-cs-joshua.tok.lc.fixamp1.loo.psg.dtrain-weights-1400m

$HSTREAMING \
    -mapper "dtrain.sh" \
    -reducer "avgweights.rb" \
    -input $IN \
    -output $OUT \
    -file avgweights.rb \
    -file dtrain.sh \
    -file dtrain \
    -file dtrain.ini \
    -file cdec.ini \
    -file nc-wmt11.en.srilm.3.gz \
    -jobconf mapred.reduce.tasks=1 \
    -jobconf mapred.max.map.failures.percent=100

