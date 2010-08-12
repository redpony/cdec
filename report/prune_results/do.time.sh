title="In multipass (TM -> TM+3gram), tuning for 1st pass -> BLEU (y) lost to pruning (x)." \
    n4="tuned_unigram" \
    n2="2ndpass_weights_without_lm" \
    n3="tuned_without_lm" \
    xlbl="per-sentence LM rescoring time" \
    obase=time.1stpass.tune \
    ymin=0 ymax=21 \
    ./graph.sh time.2pass- hdt hdt0 hdt1
