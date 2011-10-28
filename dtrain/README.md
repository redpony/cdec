dtrain
======

Build & run
-----------
build ..
<pre>
git clone git://github.com/qlt/cdec-dtrain.git
cd cdec_dtrain
autoreconf -ifv
./configure
make
</pre>
and run:
<pre>
cd dtrain/hstreaming/
(edit ini files)
edit hadoop-streaming-job.sh $IN and $OUT
./hadoop-streaming-job.sh
</pre>


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
merge dtrain part-* files
mapred count shard sents


Data
----
<pre>
nc-v6.de-en             peg
nc-v6.de-en.loo         peg
nc-v6.de-en.giza.loo    peg
nc-v6.de-en.symgiza.loo peg
nv-v6.de-en.cs          peg
nc-v6.de-en.cs.loo      peg
--
ep-v6.de-en.cs          pe
ep-v6.de-en.cs.loo      p

p: prep, e: extract, g: grammar, d: dtrain
</pre>


Experiments
-----------
features
 TODO

"lm open better than lm closed when tuned"

mira100-10
mira100-17

baselines
 mira
 pro    
 vest   

