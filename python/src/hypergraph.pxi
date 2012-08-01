cimport hypergraph
cimport kbest

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
        return unicode(GetString(trans).c_str(), 'utf8')

    def viterbi_trees(self):
        f_tree = unicode(hypergraph.ViterbiFTree(self.hg[0]).c_str(), 'utf8')
        e_tree = unicode(hypergraph.ViterbiETree(self.hg[0]).c_str(), 'utf8')
        return (f_tree, e_tree)
    
    def viterbi_features(self):
        cdef SparseVector fmap = SparseVector.__new__(SparseVector)
        fmap.vector = new FastSparseVector[weight_t](hypergraph.ViterbiFeatures(self.hg[0]))
        return fmap

    def viterbi_joshua(self):
        return unicode(hypergraph.JoshuaVisualizationString(self.hg[0]).c_str(), 'utf8')

    def kbest(self, size):
        cdef kbest.KBestDerivations[vector[WordID], kbest.ESentenceTraversal]* derivations = new kbest.KBestDerivations[vector[WordID], kbest.ESentenceTraversal](self.hg[0], size)
        cdef kbest.KBestDerivations[vector[WordID], kbest.ESentenceTraversal].Derivation* derivation
        cdef unsigned k
        try:
            for k in range(size):
                derivation = derivations.LazyKthBest(self.hg.nodes_.size() - 1, k)
                if not derivation: break
                yield unicode(GetString(derivation._yield).c_str(), 'utf8')
        finally:
            del derivations

    def kbest_trees(self, size):
        cdef kbest.KBestDerivations[vector[WordID], kbest.FTreeTraversal]* f_derivations = new kbest.KBestDerivations[vector[WordID], kbest.FTreeTraversal](self.hg[0], size)
        cdef kbest.KBestDerivations[vector[WordID], kbest.FTreeTraversal].Derivation* f_derivation
        cdef kbest.KBestDerivations[vector[WordID], kbest.ETreeTraversal]* e_derivations = new kbest.KBestDerivations[vector[WordID], kbest.ETreeTraversal](self.hg[0], size)
        cdef kbest.KBestDerivations[vector[WordID], kbest.ETreeTraversal].Derivation* e_derivation
        cdef unsigned k
        try:
            for k in range(size):
                f_derivation = f_derivations.LazyKthBest(self.hg.nodes_.size() - 1, k)
                e_derivation = e_derivations.LazyKthBest(self.hg.nodes_.size() - 1, k)
                if not f_derivation or not e_derivation: break
                f_tree = unicode(GetString(f_derivation._yield).c_str(), 'utf8')
                e_tree = unicode(GetString(e_derivation._yield).c_str(), 'utf8')
                yield (f_tree, e_tree)
        finally:
            del f_derivations
            del e_derivations

    def kbest_features(self, size):
        cdef kbest.KBestDerivations[FastSparseVector[weight_t], kbest.FeatureVectorTraversal]* derivations = new kbest.KBestDerivations[FastSparseVector[weight_t], kbest.FeatureVectorTraversal](self.hg[0], size)
        cdef kbest.KBestDerivations[FastSparseVector[weight_t], kbest.FeatureVectorTraversal].Derivation* derivation
        cdef SparseVector fmap
        cdef unsigned k
        try:
            for k in range(size):
                derivation = derivations.LazyKthBest(self.hg.nodes_.size() - 1, k)
                if not derivation: break
                fmap = SparseVector.__new__(SparseVector)
                fmap.vector = new FastSparseVector[weight_t](derivation._yield)
                yield fmap
        finally:
            del derivations

    def sample(self, unsigned n):
        cdef vector[hypergraph.Hypothesis]* hypos = new vector[hypergraph.Hypothesis]()
        if self.rng == NULL:
            self.rng = new MT19937()
        hypergraph.sample_hypotheses(self.hg[0], n, self.rng, hypos)
        cdef unsigned k
        try:
            for k in range(hypos.size()):
                yield unicode(GetString(hypos[0][k].words).c_str(), 'utf8')
        finally:
            del hypos

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

    property edges:
        def __get__(self):
            cdef unsigned i
            for i in range(self.hg.edges_.size()):
                yield HypergraphEdge().init(self.hg, i)

    property nodes:
        def __get__(self):
            cdef unsigned i
            for i in range(self.hg.nodes_.size()):
                yield HypergraphNode().init(self.hg, i)

    property goal:
        def __get__(self):
            return HypergraphNode().init(self.hg, self.hg.GoalNode())

    property npaths:
        def __get__(self):
            return self.hg.NumberOfPaths()

    def inside_outside(self):
        cdef FastSparseVector[prob_t]* result = new FastSparseVector[prob_t]()
        cdef prob_t z = hypergraph.InsideOutside(self.hg[0], result)
        result[0] /= z
        cdef SparseVector vector = SparseVector.__new__(SparseVector)
        vector.vector = new FastSparseVector[double]()
        cdef FastSparseVector[prob_t].const_iterator* it = new FastSparseVector[prob_t].const_iterator(result[0], False)
        cdef unsigned i
        for i in range(result.size()):
            vector.vector.set_value(it[0].ptr().first, log(it[0].ptr().second))
            pinc(it[0]) # ++it
        del it
        del result
        return vector

cdef class HypergraphEdge:
    cdef hypergraph.Hypergraph* hg
    cdef hypergraph.HypergraphEdge* edge
    cdef public TRule trule

    cdef init(self, hypergraph.Hypergraph* hg, unsigned i):
        self.hg = hg
        self.edge = &hg.edges_[i]
        self.trule = TRule.__new__(TRule)
        self.trule.rule = new shared_ptr[grammar.TRule](self.edge.rule_)
        return self

    def __len__(self):
        return self.edge.tail_nodes_.size()

    property head_node:
        def __get__(self):
            return HypergraphNode().init(self.hg, self.edge.head_node_)

    property tail_nodes:
        def __get__(self):
            cdef unsigned i
            for i in range(self.edge.tail_nodes_.size()):
                yield HypergraphNode().init(self.hg, self.edge.tail_nodes_[i])

    property span:
        def __get__(self):
            return (self.edge.i_, self.edge.j_)

    property feature_values:
        def __get__(self):
            cdef SparseVector vector = SparseVector.__new__(SparseVector)
            vector.vector = new FastSparseVector[double](self.edge.feature_values_)
            return vector

    property prob:
        def __get__(self):
            return self.edge.edge_prob_.as_float()

    def __richcmp__(HypergraphEdge x, HypergraphEdge y, int op):
        if op == 2: # ==
            return x.edge == y.edge
        elif op == 3: # !=
            return not (x == y)
        raise NotImplemented('comparison not implemented for HypergraphEdge')

cdef class HypergraphNode:
    cdef hypergraph.Hypergraph* hg
    cdef hypergraph.HypergraphNode* node

    cdef init(self, hypergraph.Hypergraph* hg, unsigned i):
        self.hg = hg
        self.node = &hg.nodes_[i]
        return self

    property in_edges:
        def __get__(self):
            cdef unsigned i
            for i in range(self.node.in_edges_.size()):
                yield HypergraphEdge().init(self.hg, self.node.in_edges_[i])

    property out_edges:
        def __get__(self):
            cdef unsigned i
            for i in range(self.node.out_edges_.size()):
                yield HypergraphEdge().init(self.hg, self.node.out_edges_[i])

    property span:
        def __get__(self):
            return next(self.in_edges).span

    property cat:
        def __get__(self):
            if self.node.cat_:
                return TDConvert(-self.node.cat_)

    def __richcmp__(HypergraphNode x, HypergraphNode y, int op):
        if op == 2: # ==
            return x.node == y.node
        elif op == 3: # !=
            return not (x == y)
        raise NotImplemented('comparison not implemented for HypergraphNode')
