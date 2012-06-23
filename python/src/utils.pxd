from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp.pair cimport pair

cdef extern from "<iostream>" namespace "std":
    cdef cppclass istream:
        pass
    cdef cppclass ostream:
        pass
    cdef cppclass istringstream(istream):
        istringstream(char*)
    ostream cout

cdef extern from "utils/weights.h":
    ctypedef double weight_t

cdef extern from "utils/logval.h":
    cdef cppclass LogVal[T]:
        double as_float()

    double log(LogVal[double]&)

cdef extern from "utils/wordid.h":
    ctypedef int WordID

cdef extern from "utils/small_vector.h":
    cdef cppclass SmallVector[T]:
        pass

cdef extern from "utils/sparse_vector.h":
    cdef cppclass FastSparseVector[T]:
        cppclass const_iterator:
            const_iterator(FastSparseVector[T]&, bint is_end)
            pair[unsigned, T]* ptr "operator->" ()
            const_iterator& operator++()
            bint operator==(const_iterator&)
            bint operator!=(const_iterator&)
        FastSparseVector()
        FastSparseVector(FastSparseVector[T]&)
        const_iterator begin()
        const_iterator end()
        void init_vector(vector[T]* vp)
        T value(unsigned k)
        void set_value(unsigned k, T& v)
        size_t size()
        bint nonzero(unsigned k)
        bint operator==(FastSparseVector[T]&)
        T dot(vector[weight_t]&) # cython bug when [T]
        T dot(FastSparseVector[T]&)

    FastSparseVector[weight_t] operator+(FastSparseVector[weight_t]&, FastSparseVector[weight_t]&)
    FastSparseVector[weight_t] operator-(FastSparseVector[weight_t]&, FastSparseVector[weight_t]&)
    ostream operator<<(ostream& out, FastSparseVector[weight_t]& v)

cdef extern from "utils/weights.h" namespace "Weights":
    void InitSparseVector(vector[weight_t]& dv, FastSparseVector[weight_t]* sv)

cdef extern from "utils/tdict.cc" namespace "TD":
    string GetString(vector[WordID]& st)
    unsigned NumWords()
    WordID TDConvert "TD::Convert" (char*)
    char* TDConvert "TD::Convert" (WordID)

cdef extern from "utils/verbose.h":
    void SetSilent(bint)

cdef extern from "utils/fdict.h" namespace "FD":
    WordID FDConvert "FD::Convert" (char*)
    string& FDConvert "FD::Convert" (WordID)

cdef extern from "utils/filelib.h":
    cdef cppclass ReadFile:
        ReadFile(string&)
        istream* stream()

cdef extern from "utils/sampler.h":
    cdef cppclass MT19937:
        pass
