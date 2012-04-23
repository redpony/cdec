This is a simple (but parallelizable) tuning method for cdec, as used here:
  "Joint Feature Selection in Distributed Stochastic
   Learning for Large-Scale Discriminative Training in
   SMT" Simianer, Riezler, Dyer
   ACL 2012


Building
--------
builds when building cdec, see ../BUILDING

Running
-------
To run this on a dev set locally:
```
    #define DTRAIN_LOCAL
```
otherwise remove that line or undef. You need a single grammar file
or per-sentence-grammars (psg) as you would use with cdec.
Additionally you need to give dtrain a file with
references (--refs).

The input for use with hadoop streaming looks like this:
```
    <sid>\t<source>\t<ref>\t<grammar rules separated by \t>
```
To convert a psg to this format you need to replace all "\n"
by "\t". Make sure there are no tabs in your data.

For an example of local usage (with 'distributed' format)
the see test/example/ . This expects dtrain to be built without
DTRAIN_LOCAL.

Legal stuff
-----------
Copyright (c) 2012 by Patrick Simianer <p@simianer.de>

See the file ../LICENSE.txt for the licensing terms that this software is
released under.

