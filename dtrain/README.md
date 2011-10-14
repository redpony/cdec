dtrain
======

Build & run
-----------
On a hadoop cluster do:
```sh
git clone git://github.com/qlt/cdec-dtrain.git
cd cdec_dtrain
autoreconf -ifv
./configure
make
```
then
```sh
cd dtrain/hstreaming/
(edit ini files)
edit hadoop-streaming-job.sh $IN and $OUT
./hadoop-streaming-job.sh
```


Ideas
-----
* *MULTIPARTITE* ranking (1 vs all, cluster model/score)
* *REMEMBER* sampled translations (merge)
* *SELECT* iteration with highest (_real_) BLEU?
* *GENERATED* data? (perfect translation in kbest)
* *CACHING* (ngrams for scoring)
* hadoop *PIPES* imlementation
* *ITERATION* variants (shuffle resulting weights, re-iterate)
* *MORE THAN ONE* reference for BLEU?
* *RANDOM RESTARTS*
* use separate TEST SET for each shard

Uncertain, known bugs, problems
-------------------------------
* cdec kbest vs 1best (no -k param), rescoring (ref?)? => ok(?)
* no sparse vector in decoder => ok/fixed
* PhraseModel_* features (0..99 seem to be generated, why 99?)
* flex scanner jams on malicious input, we could skip that
* input/grammar caching (strings, files)

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

