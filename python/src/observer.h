#include "decoder/hg.h"
#include "decoder/decoder.h"

struct BasicObserver: public DecoderObserver {
    Hypergraph* hypergraph;
    BasicObserver() : hypergraph(NULL) {}
    ~BasicObserver() {
        if(hypergraph != NULL) delete hypergraph;
    }
    void NotifyDecodingStart(const SentenceMetadata& smeta) {}
    void NotifySourceParseFailure(const SentenceMetadata& smeta) {}
    void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
        if(hypergraph != NULL) delete hypergraph;
        hypergraph = new Hypergraph(*hg);
    }
    void NotifyAlignmentFailure(const SentenceMetadata& semta) {
        if(hypergraph != NULL) delete hypergraph;
    }
    void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg) {}
    void NotifyDecodingComplete(const SentenceMetadata& smeta) {}
};
