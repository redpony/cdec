cdef struct _VEB:
	int top_universe_size
	int num_bottom_bits
	int max_val
	int min_val
	int size
	void* top
	void** bottom


cdef class VEB:
	cdef _VEB* veb
	cdef int _findsucc(self, int i)
	cdef int _insert(self, int i)
	cdef int _first(self)
