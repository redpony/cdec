#!/bin/bash

EXP=test

HADOOP_HOME=/usr/lib/hadoop-0.20
JAR=contrib/streaming/hadoop-streaming-0.20.2-cdh3u1.jar
HSTREAMING="$HADOOP_HOME/bin/hadoop jar $HADOOP_HOME/$JAR"

 IN=nc-v6.de-en.cs.giza.loo/nc-v6.de-en.cs.giza.loo-dtrain1.sz2
OUT=out/$EXP-weights

$HSTREAMING \
    -mapper "dtrain.sh" \
    -reducer "red-avg.rb" \
    -input $IN \
    -output $OUT \
    -file dtrain.sh \
    -file red-avg.rb \
    -file ~/exp/cdec-dtrain-ro/dtrain/dtrain \
    -file dtrain.ini \
    -file cdec.ini \
    -file ~/exp/data/nc-v6.en.3.unk.probing.kenv5 \
    -jobconf mapred.reduce.tasks=1 \
    -jobconf mapred.max.map.failures.percent=0 \
    -jobconf mapred.job.name="dtrain $EXP"

