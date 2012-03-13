from libc.stdio cimport FILE

cdef class Precomputation:
	cdef int precompute_rank
	cdef int precompute_secondary_rank
	cdef int max_length
	cdef int max_nonterminals
	cdef int train_max_initial_size
	cdef int train_min_gap_size
	cdef precomputed_index
	cdef precomputed_collocations
	cdef read_map(self, FILE* f)
	cdef write_map(self, m, FILE* f)
