dtrain
======

Ideas
-----
* *MULTIPARTITE* ranking (108010, 1 vs all, cluster modelscore;score)
* what about RESCORING?
* REMEMBER kbest (merge) weights?
* SELECT iteration with highest (real) BLEU?
* GENERATED data? (multi-task, ability to learn, perfect translation in nbest, at first all modelscore 1)
* CACHING (ngrams for scoring)
* hadoop PIPES imlementation
* SHARED LM (kenlm actually does this!)?
* ITERATION variants
 * once -> average
 * shuffle resulting weights
* weights AVERAGING in reducer (global Ngram counts)
* BATCH implementation (no update after each Kbest list)
* set REFERENCE for cdec (rescoring)?
* MORE THAN ONE reference for BLEU?
* kbest NICER (do not iterate twice)!? -> shared_ptr?
* DO NOT USE Decoder::Decode (input caching as WordID)!?
*  sparse vector instead of vector<double> for weights in Decoder(::SetWeights)?
* reactivate DTEST and tests
* non deterministic, high variance, RANDOM RESTARTS
* use separate TEST SET

Uncertain, known bugs, problems
-------------------------------
* cdec kbest vs 1best (no -k param), rescoring? => ok(?)
* no sparse vector in decoder => ok/fixed
* PhraseModel_* features (0..99 seem to be generated, why 99?)
* flex scanner jams on malicious input, we could skip that

FIXME
-----
* merge with cdec master

Data
----
<pre>
nc-v6.de-en             peg
nc-v6.de-en.loo         peg
nc-v6.de-en.giza.loo    peg
nc-v6.de-en.symgiza.loo pe
nv-v6.de-en.cs          pe
nc-v6.de-en.cs.loo      pe
--
ep-v6.de-en.cs          p
ep-v6.de-en.cs.loo      p

p: prep, e: extract, g: grammar, d: dtrain
</pre>

