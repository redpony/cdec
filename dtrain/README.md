dtrain
======

Build & run
-----------
build ..
<pre>
git clone git://github.com/qlt/cdec-dtrain.git
cd cdec-dtrain
autoreconf -if[v]
./configure [--disable-gtest]
make
</pre>
and run:
<pre>
cd dtrain/hstreaming/
(edit ini files)
edit the vars in hadoop-streaming-job.sh ($ID, $IN and $OUT)
./hadoop-streaming-job.sh
</pre>

Ideas
-----
* *MULTIPARTITE* ranking (1 vs rest, cluster model/score)
* *REMEMBER* sampled translations (merge kbest lists)
* *SELECT* iteration with highest real BLEU on devtest?
* *GENERATED* data? (perfect translation always in kbest)
* *CACHE* ngrams for scoring
* hadoop *PIPES* imlementation
* *ITERATION* variants (shuffle resulting weights, re-iterate)
* *MORE THAN ONE* reference for BLEU?
* *RANDOM RESTARTS* or directions
* use separate *TEST SET* for each shard
* *REDUCE* training set (50k?)
* *SYNTAX* features (CD)

Uncertain, known bugs, problems
-------------------------------
* cdec kbest vs 1best (no -k param), rescoring (ref?)? => ok(?)
* no sparse vector in decoder => ok/fixed
* PhraseModel features, mapping?
* flex scanner jams on bad input, we could skip that
* input/grammar caching (strings -> WordIDs)
* look at forest sampling...
* devtest loo or not? why loo grammars larger? (sort psgs | uniq -> grammar)

FIXME, todo
-----------
* merge dtrain part-X files, for better blocks
* mapred count shard sents
* 250 forest sampling is real bad, bug?
* metric reporter of bleu for each shard
* kenlm not portable (i7-2620M vs Intel(R) Xeon(R) CPU E5620 @ 2.40GHz)
* mapred chaining? hamake?

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

a: alignment:, p: prep, e: extract,
g: grammar, d: dtrain
</pre>

Experiments
-----------
[grammar stats
  oov on dev/devtest/test
  size
  #rules (uniq)
  time for building
   ep: 1.5 days on 278 slots (30 nodes)
   nc: ~2 hours ^^^

 lm stats
  oov on dev/devtest/test 
  perplex on train/dev/devtest/test]

[0]
which word alignment?
 berkeleyaligner
 giza++ as of Sep 24 2011, mgizapp 0.6.3
 --symgiza as of Oct 1 2011--
 ---
 NON LOO
 (symgiza unreliable)
 randomly sample 100 from train with loo
 run dtrain for 100 iterations
 w/o all other feats (lm, wp, ...) +Glue
 measure ibm bleu on exact same sents

[1]
lm?
 3-4-5
 open
 unk
 nounk (-100 for unk)
 --
 lm oov weight pos?
 no tuning, -100 prob for unk EXPECT: nounk
 tuning with dtrain EXPECT: open

[2]
cs?
 'default' weights

[3]
loo vs non-loo
 'jackknifing'
 generalization (determ.!) on dev, test on devtest

[4]
stability
 all with default params
 mira: 100
 pro: 100
 vest: 100
 dtrain: 100

[undecided]
pro metaparam
 (max) iter
 regularization
 ???
 
mira metaparam
 (max) iter: 10 (nc???) vs 15 (ep???)

features to try
 NgramFeatures -> target side ngrams
 RuleIdentityFeatures
 RuleNgramFeatures -> source side ngrams from rule
 RuleShape -> relative orientation of X's and terminals
 SpanFeatures -> http://www.cs.cmu.edu/~cdyer/wmt11-sysdesc.pdf
 ArityPenalty -> Arity=0 Arity=1 and Arity=2


