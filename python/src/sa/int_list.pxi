# defines int arrays in C, with some convenience methods
# for reading arrays as globs directly from disk.
# Adam Lopez <alopez@cs.umd.edu>

from libc.stdio cimport FILE, fopen, fread, fwrite, fclose
from libc.stdlib cimport malloc, realloc, free
from libc.string cimport memset, memcpy

cdef class IntList:

    def __cinit__(self, int size=0, int increment=1, int initial_len=0):
        if initial_len > size:
            size = initial_len
        self.size = size
        self.increment = increment
        self.len = initial_len
        self.arr = <int*> malloc(size*sizeof(int))
        memset(self.arr, 0, initial_len*sizeof(int))
        self.mmaped = False

    def __repr__(self):
        return 'IntList(%s)' % list(self)

    def index(self, val):
        cdef unsigned i
        for i in range(self.len):
            if self.arr[i] == val:
                return i
        raise ValueError('%s not in list' % val)

    def reset(self):
        self.len = 0

    cdef void _free_mem(self):
        if self.mmaped:
            self.memory = None
        else:
            free(self.arr)

    def __dealloc__(self):
        self._free_mem()

    def __iter__(self):
        cdef int i
        for i in range(self.len):
            yield self.arr[i]

    def __getitem__(self, index):
        cdef int i, j, k
        if isinstance(index, int):
            j = index
            if j < 0: 
                j = self.len + j
            if j<0 or j>=self.len:
                raise IndexError("Requested index %d of %d-length IntList" % (index, self.len))
            return self.arr[j]
        elif isinstance(index, slice):
            i = index.start
            j = index.stop
            if i < 0:
                i = self.len + i
            if j < 0:
                j = self.len + j
            if i < 0 or i >= self.len or j < 0 or j > self.len:
                raise IndexError("Requested index %d:%d of %d-length IntList" % (index.start, index.stop, self.len))
            result = ()
            for k from i <= k < j:
                result = result + (self.arr[k],)
            return result
        else:
            raise TypeError("Illegal key type %s for IntList" % type(index))

    cdef void set(self, int i, int val):
        j = i
        if i<0:
            j = self.len + i
        if j<0 or j>=self.len:
            raise IndexError("Requested index %d of %d-length IntList" % (i, self.len))
        self.arr[j] = val
        
    def __setitem__(self, i, val):
        self.set(i, val)

    def __len__(self):
        return self.len

    def append(self, int val):
        self._append(val)

    cdef void _append(self, int val):
        if self.len == self.size:
            self.size = self.size + self.increment
            self.arr = <int*> realloc(self.arr, self.size*sizeof(int))
        self.arr[self.len] = val
        self.len = self.len + 1

    def extend(self, IntList other):
        self._extend_arr(other.arr, other.len)

    cdef void _extend_arr(self, int* other, int other_len):
        if self.size < self.len + other_len:
            self.size = self.len + other_len
            self.arr = <int*> realloc(self.arr, self.size*sizeof(int))
        memcpy(self.arr+self.len, other, other_len*sizeof(int))
        self.len = self.len + other_len

    cdef void _clear(self):
        self._free_mem()
        self.len = 0
        self.size = 0
        self.arr = <int*> malloc(0)

    cdef void write_handle(self, FILE* f):
        fwrite(&(self.len), sizeof(int), 1, f)
        fwrite(self.arr, sizeof(int), self.len, f)

    cdef void read_handle(self, FILE* f):
        self._free_mem()
        fread(&(self.len), sizeof(int), 1, f)
        self.arr = <int*> malloc(self.len * sizeof(int))
        self.size = self.len
        fread(self.arr, sizeof(int), self.len, f)

    cdef void read_mmaped(self, MemoryMap buf):
        self._free_mem()
        self.size = self.len = buf.read_int()
        self.arr = buf.read_int_array(self.len)
        self.mmaped = True
        self.memory = buf
