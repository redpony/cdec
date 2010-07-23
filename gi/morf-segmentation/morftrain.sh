#!/bin/bash

if [[ $# -lt 3 ]]; then
	echo "Trains a morfessor model and places the result in writedir"
	echo
	echo "Usage: `basename $0` corpus_input_file writedir [PPL] [marker] [lines]"
	echo -e "\tcorpus_input_file contains a sentence per line."
	exit 1
fi

MORFESSOR_DIR="/export/ws10smt/software/morfessor_catmap0.9.2"
SCRIPT_DIR=$(dirname `readlink -f $0`)

MORFBINDIR="$MORFESSOR_DIR/bin"
MORFMAKEFILE_TRAIN="$MORFESSOR_DIR/train/Makefile"
VOCABEXT="$SCRIPT_DIR/vocabextractor.sh"

MARKER="#"

if [[ ! -f $VOCABEXT ]]; then
  echo "$VOCABEXT doesn't exist!"
  exit 1
fi
if [[ ! -f $MORFMAKEFILE_TRAIN ]]; then
  echo "$MORFMAKEFILE_TRAIN doesn't exist!"
  exit 1
fi


CORPUS="$1"
WRITETODIR=$2

if [[ ! -f $CORPUS ]]; then
  echo "$CORPUS doesn't exist!"
  exit 1
fi

PPL=10
LINES=0
if [[ $# -gt 2 ]]; then
  PPL=$3
fi
if [[ $# -gt 3 ]]; then
  MARKER="$4"
fi
if [[ $# -gt 4 ]]; then
  LINES=$5
fi

mkdir -p $WRITETODIR

#extract vocabulary to train on
echo "Extracting vocabulary..."
if [[ -f $WRITETODIR/inputvocab.gz ]]; then
  echo " ....$WRITETODIR/inputvocab.gz exists, reusing."
else
  if [[ $LINES -gt 0 ]]; then
    $VOCABEXT $CORPUS $LINES | gzip > $WRITETODIR/inputvocab.gz
  else
    $VOCABEXT $CORPUS | gzip > $WRITETODIR/inputvocab.gz
  fi
fi


#train it
echo "Training morf model..."
if [[ -f $WRITETODIR/segmentation.final.gz ]]; then
  echo " ....$WRITETODIR/segmentation.final.gz exists, reusing.."
else
  OLDPWD=`pwd`
  cd $WRITETODIR
  
  #put the training Makefile in place, with appropriate modifications
  sed -e "s/^GZIPPEDINPUTDATA = .*$/GZIPPEDINPUTDATA = inputvocab.gz/"  \
    -e "s/^PPLTHRESH = .*$/PPLTHRESH = $PPL/" \
    -e "s;^BINDIR = .*$;BINDIR = $MORFBINDIR;" \
    $MORFMAKEFILE_TRAIN > ./Makefile

  date
  make > ./trainmorf.log 2>&1
  cd $OLDPWD
  
  
  echo "Post processing..."
  #remove comments, counts and morph types
  #mark morphs
  
  if [[ ! -f $WRITETODIR/segmentation.final.gz ]]; then
     echo "Failed to learn segmentation model: $WRITETODIR/segmentation.final.gz not written"
     exit 1
  fi

  zcat $WRITETODIR/segmentation.final.gz | \
    awk '$1 !~ /^#/ {print}' | \
    cut -d ' ' --complement -f 1 | \
    sed -e "s/\/...//g" -e "s/ + /$MARKER $MARKER/g" \
    > $WRITETODIR/segmentation.ready

  if [[ ! -f $WRITETODIR/segmentation.ready ]]; then
     echo "Failed to learn segmentation model: $WRITETODIR/segmentation.final.gz not written"
     exit 1
  fi



  echo "Done training."
  date
fi
echo "Segmentation model is $WRITETODIR/segmentation.ready."

