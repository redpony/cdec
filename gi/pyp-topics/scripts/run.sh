#!/bin/sh


./simple-extract-context.sh ~/workspace/clsp2010/jhuws2010/data/btec/split.zh-en.al 1 | ~/workspace/pyp-topics/scripts/contexts2documents.py > split.zh-en.data

~/workspace/pyp-topics/bin/pyp-topics-train -d split.zh-en.data -t 50 -s 100 -o split.zh-en.documents.gz -w split.zh-en.topics.gz
gunzip split.zh-en.documents.gz

~/workspace/cdec/extools/extractor -i ../jhuws2010/data/btec/split.zh-en.al -S 1 -c 500000 -L 12 --base_phrase_spans | ~/workspace/pyp-topics/scripts/spans2labels.py split.zh-en.phrases split.zh-en.contexts split.zh-en.documents > corpus.zh-en.labelled_spans

paste -d " " ~/workspace/clsp2010/jhuws2010/data/btec/split.zh-en.al corpus.labelled_spans > split.zh-en.labelled_spans

./simple-extract.sh ~/workspace/clsp2010/scratch/split.zh-en.labelled_spans
