from libc.stdio cimport FILE

cdef class CIntList:
	cdef int size
	cdef int increment
	cdef int len
	cdef int* arr
	cdef void write_handle(self, FILE* f)
	cdef void read_handle(self, FILE* f)
	cdef void _append(self, int val)
	cdef void _extend(self, CIntList other)
	cdef void _extend_arr(self, int* other, int other_len)
	cdef void _clear(self)
	cdef void set(self, int i, int val)

