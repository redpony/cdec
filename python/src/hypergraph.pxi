cimport hypergraph
cimport kbest as kb

cdef class Hypergraph:
    cdef hypergraph.Hypergraph* hg
    cdef MT19937* rng

    def __dealloc__(self):
        del self.hg
        if self.rng != NULL:
            del self.rng

    def viterbi(self):
        cdef vector[WordID] trans
        hypergraph.ViterbiESentence(self.hg[0], &trans)
        cdef str sentence = GetString(trans).c_str()
        return sentence.decode('utf8')

    def viterbi_tree(self):
        cdef str tree = hypergraph.ViterbiETree(self.hg[0]).c_str()
        return tree.decode('utf8')

    def viterbi_source_tree(self):
        cdef str tree = hypergraph.ViterbiFTree(self.hg[0]).c_str()
        return tree.decode('utf8')
    
    def viterbi_features(self):
        cdef SparseVector fmap = SparseVector()
        fmap.vector = new FastSparseVector[weight_t](hypergraph.ViterbiFeatures(self.hg[0]))
        return fmap

    def kbest(self, size):
        cdef kb.KBestDerivations[vector[WordID], kb.ESentenceTraversal]* derivations = new kb.KBestDerivations[vector[WordID], kb.ESentenceTraversal](self.hg[0], size)
        cdef kb.KBestDerivations[vector[WordID], kb.ESentenceTraversal].Derivation* derivation
        cdef bytes sentence
        cdef unsigned k
        try:
            for k in range(size):
                derivation = derivations.LazyKthBest(self.hg.nodes_.size() - 1, k)
                if not derivation: break
                sentence = GetString(derivation._yield).c_str()
                yield sentence.decode('utf8')
        finally:
            del derivations

    def kbest_tree(self, size):
        cdef kb.KBestDerivations[vector[WordID], kb.ETreeTraversal]* derivations = new kb.KBestDerivations[vector[WordID], kb.ETreeTraversal](self.hg[0], size)
        cdef kb.KBestDerivations[vector[WordID], kb.ETreeTraversal].Derivation* derivation
        cdef str tree
        cdef unsigned k
        try:
            for k in range(size):
                derivation = derivations.LazyKthBest(self.hg.nodes_.size() - 1, k)
                if not derivation: break
                tree = GetString(derivation._yield).c_str()
                yield tree.decode('utf8')
        finally:
            del derivations

    def kbest_features(self, size):
        cdef kb.KBestDerivations[FastSparseVector[weight_t], kb.FeatureVectorTraversal]* derivations = new kb.KBestDerivations[FastSparseVector[weight_t], kb.FeatureVectorTraversal](self.hg[0], size)
        cdef kb.KBestDerivations[FastSparseVector[weight_t], kb.FeatureVectorTraversal].Derivation* derivation
        cdef SparseVector fmap
        cdef unsigned k
        try:
            for k in range(size):
                derivation = derivations.LazyKthBest(self.hg.nodes_.size() - 1, k)
                if not derivation: break
                fmap = SparseVector()
                fmap.vector = new FastSparseVector[weight_t](derivation._yield)
                yield fmap
        finally:
            del derivations

    def sample(self, unsigned n):
        cdef vector[hypergraph.Hypothesis]* hypos = new vector[hypergraph.Hypothesis]()
        if self.rng == NULL:
            self.rng = new MT19937()
        hypergraph.sample_hypotheses(self.hg[0], n, self.rng, hypos)
        cdef str sentence
        cdef unsigned k
        try:
            for k in range(hypos.size()):
                sentence = GetString(hypos[0][k].words).c_str()
                yield sentence.decode('utf8')
        finally:
            del hypos

    # TODO richer k-best/sample output (feature vectors, trees?)

    def intersect(self, Lattice lat):
        return hypergraph.Intersect(lat.lattice[0], self.hg)

    def prune(self, beam_alpha=0, density=0, **kwargs):
        cdef hypergraph.EdgeMask* preserve_mask = NULL
        if 'csplit_preserve_full_word' in kwargs:
             preserve_mask = new hypergraph.EdgeMask(self.hg.edges_.size())
             preserve_mask[0][hypergraph.GetFullWordEdgeIndex(self.hg[0])] = True
        self.hg.PruneInsideOutside(beam_alpha, density, preserve_mask, False, 1, False)
        if preserve_mask:
            del preserve_mask

    def lattice(self): # TODO direct hg -> lattice conversion in cdec
        cdef str plf = hypergraph.AsPLF(self.hg[0], True).c_str()
        return Lattice(eval(plf))

    def reweight(self, weights):
        if isinstance(weights, SparseVector):
            self.hg.Reweight((<SparseVector> weights).vector[0])
        elif isinstance(weights, DenseVector):
            self.hg.Reweight((<DenseVector> weights).vector[0])
        else:
            raise TypeError('cannot reweight hypergraph with %s' % type(weights))

    # TODO get feature expectations, get partition function ("inside" score)
