This is a simple (and parallelizable) tuning method for cdec
which is able to train the weights of very many (sparse) features.
It was used here:
  "Joint Feature Selection in Distributed Stochastic
   Learning for Large-Scale Discriminative Training in
   SMT"
(Simianer, Riezler, Dyer; ACL 2012)


Building
--------
Builds when building cdec, see ../BUILDING .
To build only parts needed for dtrain do
```
  autoreconf -ifv
  ./configure
  cd training/dtrain/; make
```

Running
-------
See directories under test/ .

Legal
-----
Copyright (c) 2012-2013 by Patrick Simianer <p@simianer.de>

See the file LICENSE.txt in the root folder for the licensing terms that this software is
released under.

