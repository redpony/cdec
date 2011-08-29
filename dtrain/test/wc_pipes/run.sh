#!/bin/sh

HADOOP=~/z/hadoop-0.20.2/

$HADOOP/bin/hadoop dfs -put wordcount wordcount

$HADOOP/bin/hadoop pipes -D hadoop.pipes.java.recordreader=true  \
                   -D hadoop.pipes.java.recordwriter=true \
                   -input in/bible.txt   -output out/bible_out  \
                   -program ./wordcount

