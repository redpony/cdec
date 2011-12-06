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
* *SELECT* iteration with highest _real_ BLEU on devtest?
* *SYNTHETIC* data? (perfect translation always in kbest)
* *CACHE* ngrams for scoring
* hadoop *PIPES* implementation
* *ITERATION* variants (shuffle resulting weights, re-iterate)
* *MORE THAN ONE* reference for BLEU, paraphrases?
* *RANDOM RESTARTS* or random directions
* use separate *TEST SET* for each shard
* *REDUCE* training set (50k?)
* *SYNTAX* features (CD)
* distribute *DEV* set to all nodes, avg

Notes
-------------------------------
* cdec kbest vs 1best (no -k param), rescoring (ref?)? => ok(?)
* no sparse vector in decoder => fixed/'ok'
* PhraseModel features 0..99, mapping?
* flex scanner jams on bad input, we could skip that
* input/grammar caching (vector<string> -> vector<WordID>)
* why loo grammars larger? are they? (sort psgs | uniq -> grammar)
* lower beam size to be faster?
* why is <unk> -100 in lm so good?
* noise helps for discriminative training?
* what does srilm do with -unk but nothing mapped to unk (<unk> unigram)?
  => this: http://www-speech.sri.com/pipermail/srilm-user/2007q4/000543.html
* does AER correlate with BLEU? paper?
* learning rate tuned with perceptron?
* dtrain (perceptron) used for some tests because no optimizer instability
* http://www.ark.cs.cmu.edu/cdyer/dtrain/
* repeat as often as max needed by any learner!
* don't compare lms (perplex.) with diff vocab (see stupid backoff paper)
* what does mira/pro optimize exactly?
* early stopping (epsilon, no change in kbest list)
* 10-20k rules per sent are normal
* giza vs. berkeleyaligner: giza more/less noise?
* compound splitting -> more rules?
* loo (jackknifing) => ref can't be reached?
* prune singletons -> less noise? (do I do this?)
* random sample: take fixed X at random
* scale of features/weights?

Features
--------
* baseline features (take whatever cdec implements for VEST)
* rule identifiers (feature name = rule as string)
* rule discounts (taken from frequency i or frequency interval [i,j] of rule in extraction from parallel training data) bins
  => from PRO
* target ngrams (from nonterminals in rule rhs), with gaps?
* source-target unigrams (from word alignments used in rule extraction, if they are?)
* lhs, rhs, rule length features
* all other features depend on syntax annotation. 
* word alignment

Todo
-----------
* merge dtrain part-X files, for better blocks (how to do this with 4.5tb ep)
* mapred count shard sents
* mapred stats for learning curve (output weights per iter for eval on devtest)
* 250 forest sampling is real bad, bug?
* metric reporter of bleu for each shard (reporters, status?)
  to draw learning curves for all shards in 1 plot
* kenlm not portable (i7-2620M vs Intel(R) Xeon(R) CPU E5620 @ 2.40GHz)
* mapred chaining? hamake?
* make our sigtest work with cdec
* l1l2 red (tsuroke)?
* epsilon stopping criterion
* normalize weight vector to get proper model scores for forest sampling
* 108010 with gap(s), and/or fix (same score in diff groups)
* 108010: combine model score + bleu
* visualize weight vector
* *100 runs stats
* correlation of *_bleu to ibm_bleu
* ep: open lm, cutoff @1
* tune regs
* 3x3 4x4 5x5 .. 10x10 until standard dev ok, moving avg
* avg weight vector for dtrain? (mira non-avg)
* repeat lm choose with mira/pro
* shuffle training data
* learning rate dynamic (Duh? Tsuroka?)
* divide updates by ?
* mira: 5/10/15, pro: (5)/10/20/30 (on devtest!)
* sample pairs like in pro
* mira forest sampling
* platform specific (108010!)

Data
----
<pre>
nc-v6.de-en             apegd
nc-v6.de-en.loo         apegd
nc-v6.de-en.giza        apegd
nc-v6.de-en.giza.loo    apegd
nc-v6.de-en.cs.giza     apegd
nc-v6.de-en.cs.giza.loo apegd
nv-v6.de-en.cs          apegd
nc-v6.de-en.cs.loo      apegd
--
ep-v6.de-en.cs          apegd
ep-v6.de-en.cs.loo      apegd

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
  perplex on train/dev/devtest/test?]

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
 ep -> berkeleyaligner ??? (mb per sent, rules per sent)

*100 -> triples, quadruples

[1]
lm?
 3-4-5
 open
 unk
 nounk (-100 for unk)
 --
 lm oov weight pos? -100
 no tuning, -100 prob for unk EXPECT: nounk
 tuning with dtrain EXPECT: open
 =>
  lmtest on cs.giza.loo???

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
do we even need loo for ep?
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

---
shard size: 500-2k
iterations, re-iterate (shuffle w): 10
gamma, eta
SVM, perceptron
reducer: avg (feats/shard), l1l2, active on all shards
sentence sampling: forest
pair sampling: all, rand, 108010 (sort), PRO
out of domain test?

---
variables to control

[alignment]

[lm]

[vest]

[mira]

[dtrain]

[pro]


--------
In PRO, a continually growing list of candidates is maintained for
each sentence by concatenating k-best lists from each decoding run,
and the training pairs are sampled from them. This is done to ensure
that the optimizer doesn't forget about bad places in the parameter
space that it visited previously (since some training samples will be
selected from that space). Something like your approach should work
well though, provided you don't overfit to the sentence pair you're
looking at in each iteration. So I guess the question is: what are you
doing in step 2 exactly? A complete optimization? Taking one step? The
other thing is, do you maintain n-best hypotheses from previous
iterations?

--------
good grammar? => ability to overfit
 berkeley vs giza
 not LOO
 NO optimizer instability
 20+ iterations
 approx_bleu-4
 train on dev => test on dev
 train on devtest => test on devtest
 dev on dev better?
 devtest on devtest better?
 (train/test on loo? => lower!)
 (test on others => real bad)


loo vs non-loo? => generalization
 (cs vs non-cs?)
 giza||berkeley
 LOO + non LOO
 2 fold cross validation 
 train on dev, test on devtest
 train on devtest, test on dev
 as above ^^^


 ---

as PRO
 - UPDATES:       perceptron
 - LEARNING RATE: 0.0005
 - GAMMA:         -
 - #ITERATIONS:   30
 - SCORER:        stupid_bleu@4
 - K:             100, 1500?(top X pairs)
 - SAMPLE:        kbest uniq, kbest no
 - PAIR SAMPLING: all, PRO?TODO
 - SELECT:        best
 - FEATURES:      baseline, RuleShape+SpanFeatures
 ---
 - Note: no weight interpolation
         no early stopping based on kbest lists (epsilon?TODO)

dtrain tune reg 
 - updates: SVM
 - pair sampling important!
 - learning_rate= 100 50 10 5 1 0.5 0.1 0.05 0.01 0.005 0.001 0.0005 0.0001 0.00005 0.00001 0.000005 0.000001 0.0000005 0.0000001 0.0000000001
   
 - gamma=
   
 - scorer: stupid_bleu 3
 - test weights: last
 -
 -
 - test: devtest


---
weights visualization (blocks, color coded)
zig zag!?
repeat all basic exps with training set
merge?




--sample_from
--k
--filter
--pair_sampling
--N
--epochs
--scorer
--learning_rate
--gamma
--select_weights
[--unit_weight_vector]
[--l1_reg]
[--l1_reg_strength]

---------
corr best = really best?
108010gaps

coltrane:  9
gillespie: 9
staley:    2
io:        6
ioh:       4
         slots


when does overfitting begin?
---
Variables
 k 100..1500 higher better
 N 3/4
 learning rate
 reg/gamma
 epochs -> best on devtest (10..30) (select_weights)
 scorer -> approx_bleu correlates ok (stupid bleu, bleu, smooth bleu)
 sample from -> kbest | forest
 filter -> no uniq (kbest)
 pair sampling -> all 5050 108010 PRO alld
 update_ok -> update towards correctly ranked
 features
  6x tm
  2x lm
  wp
  Glue
  rule ids
  rule ngrams
  rule shape
  span features


PRO
 k = 1500
 N = 4
 learning rate = 0.0005
 gamma = 0
 epochs = 30
 scorer = stupid bleu (Bleu+1)
 sample from = kbest
 filter = no
 pair sampling = PRO
 update_ok
 features = base

cur:
 shard_sz 500 1k 3k
 PRO with forest sampling
 PRO w/o update_ok
 tune learning rate
 all with discard (not only top 50)
 filter kbest uniq?

 -> repeat most on Tset, lXlX stuff
 -> PRO approx bleu
 -> tune gamma
 -> best pair sampling method
 -> reduce k?
 => scorer => approx_bleu (test w PRO)
 -> PRO on training set
 -> PRO more features 
 -> discard + 108010



--
forest vs kbest count vocab?
108010 select discard
approx bleu




---
re-iterate ruleids
r_
10s
p30
stopwords
gillespie wtf
