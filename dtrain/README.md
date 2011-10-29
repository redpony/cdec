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
250 forest sampling is real bad, bug?
kenlm not portable (i7-2620M vs Intel(R) Xeon(R) CPU E5620 @ 2.40GHz)
metric reporter of bleu for each shard
mapred chaining? hamake?

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
grammar stats
 oov on dev/devtest/test
 size
 #rules (uniq)
 time for building
  ep: 1.5 days on 278 slots (30 nodes)
  nc: ~2 hours ^^^

lm stats
 oov on dev/devtest/test 
 perplex on train/dev/devtest/test

which word alignment?
 berkeleyaligner
 giza++ as of Sep 24 2011, mgizapp 0.6.3
 symgiza as of Oct 1 2011
 ---
 randomly sample 100 from train.loo
 run mira/dtrain for 50/60 iterations
 w/o lm, wp
 measure ibm_bleu on exact same sents

stability
 mira: 100
 pro: 100
 vest: 100
 dtrain: 100


pro metaparam
 (max) iter
 regularization
 
mira metaparam
 (max) iter: 10 (nc???) vs 15 (ep???)

lm?
 3-4-5
 open
 unk
 nounk (-100 for unk)
 --
 tune or not???
 lm oov weight pos?

features to try
 NgramFeatures -> target side ngrams
 RuleIdentityFeatures
 RuleNgramFeatures -> source side ngrams from rule
 RuleShape -> relative orientation of X's and terminals
 SpanFeatures -> http://www.cs.cmu.edu/~cdyer/wmt11-sysdesc.pdf
 ArityPenalty -> Arity=0 Arity=1 and Arity=2


