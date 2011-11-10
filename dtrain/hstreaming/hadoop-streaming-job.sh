#!/bin/bash

ID=
EXP=test

HADOOP_HOME=/usr/lib/hadoop-0.20
JAR=contrib/streaming/hadoop-streaming-0.20.2-cdh3u1.jar
HSTREAMING="$HADOOP_HOME/bin/hadoop jar $HADOOP_HOME/$JAR"

IN=nc-v6.de-en/nc-v6.de-en-dtrain.1500m
OUT=nc-v6.de-en/nc-v6.de-en-dtrain.1500m-weights

$HSTREAMING \
    -mapper "dtrain -c dtrain.ini --hstreaming" \
    -reducer "red-avg.rb" \
    -input $IN \
    -output $OUT \
    -file red-avg.rb \
    -file ../dtrain \
    -file dtrain.ini \
    -file cdec.ini \
    -file nc-wmt11.en.srilm.3.gz \
    -jobconf mapred.reduce.tasks=1 \
    -jobconf mapred.max.map.failures.percent=100 \
    -jobconf mapred.job.name="dtrain $ID $EXP"

