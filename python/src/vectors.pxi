from cython.operator cimport preincrement as pinc

cdef class DenseVector:
    cdef vector[weight_t]* vector
    cdef bint owned # if True, do not manage memory

    def __init__(self):
        """DenseVector() -> Dense weight/feature vector."""
        self.vector = new vector[weight_t]()
        self.owned = False

    def __dealloc__(self):
        if not self.owned:
            del self.vector

    def __len__(self):
        return self.vector.size()

    def __getitem__(self, char* fname):
        cdef int fid = FDConvert(fname)
        if 0 <= fid < self.vector.size():
            return self.vector[0][fid]
        raise KeyError(fname)
    
    def __setitem__(self, char* fname, float value):
        cdef int fid = FDConvert(fname)
        if fid < 0: raise KeyError(fname)
        if self.vector.size() <= fid:
            self.vector.resize(fid + 1)
        self.vector[0][fid] = value

    def __iter__(self):
        cdef unsigned fid
        for fid in range(1, self.vector.size()):
            yield str(FDConvert(fid).c_str()), self.vector[0][fid]

    def dot(self, SparseVector other):
        """vector.dot(SparseVector other) -> Dot product of the two vectors."""
        return other.dot(self)

    def tosparse(self):
        """vector.tosparse() -> Equivalent SparseVector."""
        cdef SparseVector sparse = SparseVector.__new__(SparseVector)
        sparse.vector = new FastSparseVector[weight_t]()
        InitSparseVector(self.vector[0], sparse.vector)
        return sparse

cdef class SparseVector:
    cdef FastSparseVector[weight_t]* vector

    def __init__(self):
        """SparseVector() -> Sparse feature/weight vector."""
        self.vector = new FastSparseVector[weight_t]()

    def __dealloc__(self):
        del self.vector

    def copy(self):
        """vector.copy() -> SparseVector copy."""
        return self * 1

    def __getitem__(self, char* fname):
        cdef int fid = FDConvert(fname)
        if fid < 0: raise KeyError(fname)
        return self.vector.value(fid)
    
    def __setitem__(self, char* fname, float value):
        cdef int fid = FDConvert(fname)
        if fid < 0: raise KeyError(fname)
        self.vector.set_value(fid, value)

    def __iter__(self):
        cdef FastSparseVector[weight_t].const_iterator* it = new FastSparseVector[weight_t].const_iterator(self.vector[0], False)
        cdef unsigned i
        try:
            for i in range(self.vector.size()):
                yield (str(FDConvert(it[0].ptr().first).c_str()), it[0].ptr().second)
                pinc(it[0]) # ++it
        finally:
            del it

    def dot(self, other):
        """vector.dot(SparseVector/DenseVector other) -> Dot product of the two vectors."""
        if isinstance(other, DenseVector):
            return self.vector.dot((<DenseVector> other).vector[0])
        elif isinstance(other, SparseVector):
            return self.vector.dot((<SparseVector> other).vector[0])
        raise TypeError('cannot take the dot product of %s and SparseVector' % type(other))
    
    def __richcmp__(SparseVector x, SparseVector y, int op):
        if op == 2: # ==
            return x.vector[0] == y.vector[0]
        elif op == 3: # !=
            return not (x == y)
        raise NotImplemented('comparison not implemented for SparseVector')

    def __len__(self):
        return self.vector.size()

    def __contains__(self, char* fname):
        return self.vector.nonzero(FDConvert(fname))

    def __neg__(self):
        cdef SparseVector result = SparseVector.__new__(SparseVector)
        result.vector = new FastSparseVector[weight_t](self.vector[0])
        result.vector[0] *= -1.0
        return result
    
    def __iadd__(SparseVector self, SparseVector other):
        self.vector[0] += other.vector[0]
        return self

    def __isub__(SparseVector self, SparseVector other):
        self.vector[0] -= other.vector[0]
        return self

    def __imul__(SparseVector self, float scalar):
        self.vector[0] *= scalar
        return self

    def __idiv__(SparseVector self, float scalar):
        self.vector[0] /= scalar
        return self

    def __add__(SparseVector x, SparseVector y):
        cdef SparseVector result = SparseVector.__new__(SparseVector)
        result.vector = new FastSparseVector[weight_t](x.vector[0] + y.vector[0])
        return result

    def __sub__(SparseVector x, SparseVector y):
        cdef SparseVector result = SparseVector.__new__(SparseVector)
        result.vector = new FastSparseVector[weight_t](x.vector[0] - y.vector[0])
        return result

    def __mul__(x, y):
        cdef SparseVector vector
        cdef float scalar
        if isinstance(x, SparseVector): vector, scalar = x, y
        else: vector, scalar = y, x
        cdef SparseVector result = SparseVector.__new__(SparseVector)
        result.vector = new FastSparseVector[weight_t](vector.vector[0] * scalar)
        return result

    def __div__(x, y):
        cdef SparseVector vector
        cdef float scalar
        if isinstance(x, SparseVector): vector, scalar = x, y
        else: vector, scalar = y, x
        cdef SparseVector result = SparseVector.__new__(SparseVector)
        result.vector = new FastSparseVector[weight_t](vector.vector[0] / scalar)
        return result
