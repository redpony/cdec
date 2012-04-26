#!/bin/sh

EXP=a_simple_test

# change these vars to fit your hadoop installation
HADOOP_HOME=/usr/lib/hadoop-0.20
JAR=contrib/streaming/hadoop-streaming-0.20.2-cdh3u1.jar
HSTREAMING="$HADOOP_HOME/bin/hadoop jar $HADOOP_HOME/$JAR"

 IN=input_on_hdfs
OUT=output_weights_on_hdfs

# you can -reducer to NONE if you want to
# do feature selection/averaging locally (e.g. to
# keep weights of all epochs)
$HSTREAMING \
    -mapper "dtrain.sh" \
    -reducer "ruby lplp.rb l2 select_k 100000" \
    -input $IN \
    -output $OUT \
    -file dtrain.sh \
    -file lplp.rb \
    -file ../dtrain \
    -file dtrain.ini \
    -file cdec.ini \
    -file ../test/example/nc-wmt11.en.srilm.gz \
    -jobconf mapred.reduce.tasks=30 \
    -jobconf mapred.max.map.failures.percent=0 \
    -jobconf mapred.job.name="dtrain $EXP"

