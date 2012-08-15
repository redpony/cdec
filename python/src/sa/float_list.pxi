# defines int arrays in C, with some convenience methods
# for reading arrays directly as globs directly from disk.
# Adam Lopez <alopez@cs.umd.edu>

from libc.stdio cimport FILE, fopen, fread, fwrite, fclose
from libc.stdlib cimport malloc, realloc, free
from libc.string cimport memset, strcpy, strlen

cdef class FloatList:

    def __cinit__(self, int size=0, int increment=1, int initial_len=0):
        if initial_len > size:
            size = initial_len
        self.size = size
        self.increment = increment
        self.len = initial_len
        self.arr = <float*> malloc(size*sizeof(float))
        memset(self.arr, 0, initial_len*sizeof(float))

    def __dealloc__(self):
        free(self.arr)

    def __getitem__(self, i):
        j = i
        if i<0: 
            j = self.len + i
        if j<0 or j>=self.len:
            raise IndexError("Requested index %d of %d-length FloatList" % (i, self.len))
        return self.arr[j]

    cdef void set(self, int i, float v):
        j = i
        if i<0: 
            j = self.len + i
        if j<0 or j>=self.len:
            raise IndexError("Requested index %d of %d-length FloatList" % (i, self.len))
        self.arr[j] = v
    
    def __setitem__(self, i, val):
        self.set(i, val)

    def __len__(self):
        return self.len

    def append(self, float val):
        if self.len == self.size:
            self.size = self.size + self.increment
            self.arr = <float*> realloc(self.arr, self.size*sizeof(float))
        self.arr[self.len] = val
        self.len = self.len + 1

    cdef void write_handle(self, FILE* f):
        fwrite(&(self.len), sizeof(float), 1, f)
        fwrite(self.arr, sizeof(float), self.len, f)

    def write(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "w")
        self.write_handle(f)
        fclose(f)

    cdef void read_handle(self, FILE* f):
        free(self.arr)
        fread(&(self.len), sizeof(float), 1, f)
        self.arr = <float*> malloc(self.len * sizeof(float))
        self.size = self.len
        fread(self.arr, sizeof(float), self.len, f)

    def read(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "r")
        self.read_handle(f)
        fclose(f)
