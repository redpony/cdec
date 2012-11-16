This directory contains a number of useful scripts that are helpful for preprocessing parallel and monolingual corpora. They are provided for convenience and may be very useful, but their functionality will often be supplainted by other, more specialized tools.

Many of these scripts assume that the input is [UTF-8 encoded](http://en.wikipedia.org/wiki/UTF-8).

## Paste parallel files together

This script reads one line at a time from a set of files and concatenates them with a triple pipe separator (`|||`) in the output. This is useful for generating parallel corpora files for training or evaluation:

    ./paste-files.pl file.a file.b file.c [...]

## Punctuation Normalization and Tokenization

This script tokenizes text in any language (well, it does a good job in most languages, and in some it will completely go crazy):

    ./tokenize-anything.sh < input.txt > output.txt

It also normalizes a lot of unicode symbols and even corrects some common encoding errors. It can be applied to monolingual and parallel corpora directly.

## Text lowercasing

This script also does what it says, provided your input is in UTF8:

    ./lowercase.pl < input.txt > output.txt

## Length ratio filtering (for parallel corpora)

This script computes statistics about sentence length ratios in a parallel corpus and removes sentences that are statistical outliers. This tends to remove extremely poorly aligned sentence pairs or sentence pairs that would otherwise be difficult to align:

    ./filter-length.pl input.src-trg > output.src-trg

## Add infrequent self-transaltions to a parallel corpus

This script identifies rare words (those that occur less than 2 times in the corpus) and which have the same orthographic form in both the source and target language. Several copies of these words are then inserted at the end of the corpus that is written, which improves alignment quality.

    ./add-self-translations.pl input.src-trg > output.src-trg


