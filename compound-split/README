Instructions for running the compound splitter, which is a reimplementation
and extension (more features, larger non-word list) of the model described in

  C. Dyer. (2009)  Using a maximum entropy model to build segmentation
            lattices for MT. In Proceedings of NAACL HLT 2009,
            Boulder, Colorado, June 2009

If you use this software, please cite this paper.


GENERATING 1-BEST SEGMENTATIONS AND LATTICES
------------------------------------------------------------------------------

Here are some sample invokations:

  ./compound-split.pl --output 1best < infile.txt > out.1best.txt
      Segment infile.txt according to the 1-best segmentation file.

  ./compound-split.pl --output plf < infile.txt > out.plf

  ./compound-split.pl --output plf --beam 3.5 < infile.txt > out.plf
      This generates denser lattices than usual (the default beam threshold
      is 2.2, higher numbers do less pruning)


MODEL TRAINING (only for the adventuresome)
------------------------------------------------------------------------------

I've included some training data for training a German language lattice
segmentation model, and if you want to explore, you can or change the data.
If you're especially adventuresome, you can add features to cdec (the current
feature functions are found in ff_csplit.cc).  The training/references are
in the file:

               dev.in-ref

The format is the unsegmented form on the right and the reference lattice on
the left, separated by a triple pipe ( ||| ).  Note that the segmentation
model inserts a # as the first word, so your segmentation references must
include this.

To retrain the model (using MAP estimation of a conditional model), do the
following:

  cd de
  ./TRAIN

Note, the optimization objective is supposed to be non-convex, but i haven't
found much of an effect of where I initialize things.  But I haven't looked
very hard- this might be something to explore.

