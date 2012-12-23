cimport posix.fcntl
cimport posix.unistd
from posix.unistd cimport off_t

cdef extern from "sys/mman.h":
    void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
    int munmap(void *addr, size_t len)
    enum:
        PROT_READ
    enum:
        MAP_FILE
        MAP_SHARED

cdef extern from "sys/stat.h":
    cdef struct stat:
        off_t st_size
    int fstat(int fildes, stat *buf)

cdef class MemoryMap:
    def __init__(self, filename):
        self.fd = posix.fcntl.open(filename, posix.fcntl.O_RDONLY)
        assert self.fd >= 0
        # Get file size
        cdef stat statbuf
        fstat(self.fd, &statbuf) 
        self.fs = statbuf.st_size
        # Memory map file
        self.map_start = mmap(NULL, self.fs, PROT_READ, MAP_FILE|MAP_SHARED, self.fd, 0)
        self.map_ptr = self.map_start

    def __dealloc__(self):
        posix.unistd.close(self.fd)
        munmap(self.map_start, self.fs)

    cdef int read_int(self):
        cdef int v = (<int*> self.map_ptr)[0]
        self.map_ptr = &(<int*> self.map_ptr)[1]
        return v

    cdef int* read_int_array(self, int size):
        cdef int* v = <int*> self.map_ptr
        self.map_ptr = &(<int*> self.map_ptr)[size]
        return v

    cdef char* read_char_array(self, int size):
        cdef char* v = <char*> self.map_ptr
        self.map_ptr = &(<char*> self.map_ptr)[size]
        return v

    cdef float* read_float_array(self, int size):
        cdef float* v = <float*> self.map_ptr
        self.map_ptr = &(<float*> self.map_ptr)[size]
        return v
