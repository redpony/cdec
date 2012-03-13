from libc.stdio cimport FILE

cdef class CFloatList:
	cdef int size
	cdef int increment
	cdef int len
	cdef float* arr
	cdef void write_handle(self, FILE* f)
	cdef void read_handle(self, FILE* f)
	cdef void set(self, int i, float v)
