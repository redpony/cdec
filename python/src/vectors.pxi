from cython.operator cimport preincrement as pinc

cdef class DenseVector:
    cdef vector[weight_t]* vector

    def __getitem__(self, char* fname):
        cdef unsigned fid = FDConvert(fname)
        if fid <= self.vector.size():
            return self.vector[0][fid]
        raise KeyError(fname)
    
    def __setitem__(self, char* fname, float value):
        cdef unsigned fid = FDConvert(<char *>fname)
        if self.vector.size() <= fid:
            self.vector.resize(fid + 1)
        self.vector[0][fid] = value

    def __iter__(self):
        cdef unsigned fid
        for fid in range(1, self.vector.size()):
            yield FDConvert(fid).c_str(), self.vector[0][fid]

    def dot(self, SparseVector other):
        return other.dot(self)

    def tosparse(self):
        cdef SparseVector sparse = SparseVector()
        sparse.vector = new FastSparseVector[weight_t]()
        InitSparseVector(self.vector[0], sparse.vector)
        return sparse

cdef class SparseVector:
    cdef FastSparseVector[weight_t]* vector

    def __getitem__(self, char* fname):
        cdef unsigned fid = FDConvert(fname)
        return self.vector.value(fid)
    
    def __setitem__(self, char* fname, float value):
        cdef unsigned fid = FDConvert(<char *>fname)
        self.vector.set_value(fid, value)

    def __iter__(self):
        cdef FastSparseVector[weight_t].const_iterator* it = new FastSparseVector[weight_t].const_iterator(self.vector[0], False)
        cdef str fname
        for i in range(self.vector.size()):
            fname = FDConvert(it[0].ptr().first).c_str()
            yield (fname, it[0].ptr().second)
            pinc(it[0])

    def dot(self, other):
        if isinstance(other, DenseVector):
            return self.vector.dot((<DenseVector> other).vector[0])
        elif isinstance(other, SparseVector):
            return self.vector.dot((<SparseVector> other).vector[0])
        raise ValueError('cannot take the dot product of %s and SparseVector' % type(other))

    def todense(self):
        cdef DenseVector dense = DenseVector()
        dense.vector = new vector[weight_t]()
        self.vector.init_vector(dense.vector)
        return dense
    
    def __richcmp__(SparseVector self, SparseVector other, int op):
        if op == 2: # ==
            return self.vector[0] == other.vector[0]
        elif op == 3: # !=
            return not (self == other)
        raise NotImplemented('comparison not implemented for SparseVector')

    def __len__(self):
        return self.vector.size()

    def __contains__(self, char* fname):
        return self.vector.nonzero(FDConvert(fname))
    
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

    def __add__(SparseVector self, SparseVector other):
        cdef SparseVector result = SparseVector()
        result.vector = new FastSparseVector[weight_t](self.vector[0] + other.vector[0])
        return result

    def __sub__(SparseVector self, SparseVector other):
        cdef SparseVector result = SparseVector()
        result.vector = new FastSparseVector[weight_t](self.vector[0] - other.vector[0])
        return result
