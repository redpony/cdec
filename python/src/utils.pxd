from libcpp.string cimport string
from libcpp.vector cimport vector

cdef extern from "<iostream>" namespace "std":
    cdef cppclass istream:
        pass
    cdef cppclass ostream:
        pass
    cdef cppclass istringstream(istream):
        istringstream(char*)

cdef extern from "utils/weights.h":
    ctypedef double weight_t

cdef extern from "utils/logval.h":
    cdef cppclass LogVal[T]:
        pass

cdef extern from "utils/prob.h":
    cdef cppclass prob_t:
        pass

cdef extern from "utils/wordid.h":
    ctypedef int WordID

cdef extern from "utils/sparse_vector.h":
    cdef cppclass SparseVector[T]:
        pass

cdef extern from "utils/tdict.cc" namespace "TD":
    cdef string GetString(vector[WordID] st)

cdef extern from "utils/verbose.h":
    cdef void SetSilent(bint)

cdef extern from "utils/fdict.h" namespace "FD":
    WordID FDConvert "FD::Convert" (char*)
    string& FDConvert "FD::Convert" (WordID)

cdef extern from "utils/filelib.h":
    cdef cppclass ReadFile:
        ReadFile(string)
        istream* stream()

cdef extern from "utils/sampler.h":
    cdef cppclass MT19937:
        pass

"""
cdef extern from "<boost/shared_ptr.hpp>" namespace "boost":
    cdef cppclass shared_ptr[T]:
        void reset(T*)
"""
