#!/bin/bash

d=$(dirname `readlink -f $0`)
if [ $# -lt 1 ]; then
	echo "Extracts unique words and their frequencies from a subset of a corpus."
	echo
	echo "Usage: `basename $0` input_file [number_of_lines] > output_file"
	echo -e "\tinput_file contains a sentence per line."
	echo
	echo "Script also removes words from the vocabulary if they contain a digit or a special character. Output is printed to stdout in a format suitable for use with Morfessor."
	echo
	exit
fi

srcname=$1
reallen=0

if [[ $# -gt 1 ]]; then
  reallen=$2
fi

pattern_file=$d/invalid_vocab.patterns

if [[ ! -f $pattern_file ]]; then
  echo "Pattern file missing"
  exit 1 
fi

#this awk strips entries from the vocabulary if they contain invalid characters
#invalid characters are digits and punctuation marks, and words beginning or ending with a dash
#uniq -c extracts the unique words and counts the occurrences

if [[ $reallen -eq 0 ]]; then
	#when a zero is passed, use the whole file
  zcat -f $srcname | sed 's/ /\n/g' | egrep -v -f $pattern_file | sort | uniq -c | sed 's/^  *//' 

else
	zcat -f $srcname | head -n $reallen | sed 's/ /\n/g' | egrep -v -f $pattern_file | sort | uniq -c | sed 's/^  *//'
fi

