This is a simple (and parallelizable) tuning method for cdec
which enables training weights of very many (sparse) features
on the full training set.

Please cite as:
>  "Joint Feature Selection in Distributed Stochastic
>   Learning for Large-Scale Discriminative Training in
>   SMT" (Simianer, Riezler, Dyer; ACL 2012)
>

Building
--------
Builds when building cdec, see ../../BUILDING .

Running
-------
Download examples for all use cases from [1] and extract here.

TODO
----
 * "stop_after" stop after X inputs
 * "select_weights" average, best, last
 * "rescale" rescale weight vector
 * implement SVM objective?
 * other variants of l1 regularization?
 * l2 regularization?
 * l1/l2 regularization?
 * scale updates by bleu difference
 * AdaGrad, per-coordinate learning rates
 * batch update
 * "repeat" iterate over k-best lists
 * show k-best loss improvement
 * "quiet"
 * "verbose"
 * fix output

Legal
-----
Copyright (c) 2012-2016 by Patrick Simianer <p@simianer.de>

See the file LICENSE.txt in the root folder for the licensing terms that this
software is released under.


[1] http://simianer.de/dtrain-example.tar.xz

