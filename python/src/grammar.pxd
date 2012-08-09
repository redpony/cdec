from libcpp.vector cimport vector
from libcpp.string cimport string
from utils cimport *

cdef extern from "decoder/trule.h":
    cdef cppclass AlignmentPoint:
        AlignmentPoint(int s, int t)
        AlignmentPoint Inverted()
        short s_
        short t_

    cdef cppclass TRule:
        vector[WordID] f_
        vector[WordID] e_
        vector[AlignmentPoint] a_
        FastSparseVector[weight_t] scores_
        WordID lhs_
        int arity_
        bint IsUnary()
        bint IsGoal()
        void ComputeArity()

cdef extern from "decoder/grammar.h":
    cdef cppclass RuleBin:
        int GetNumRules()
        shared_ptr[TRule] GetIthRule(int i)
        int Arity()

    ctypedef RuleBin const_RuleBin "const RuleBin"

    cdef cppclass GrammarIter:
        const_RuleBin* GetRules()

    ctypedef GrammarIter const_GrammarIter "const GrammarIter"

    cdef cppclass Grammar:
        const_GrammarIter* GetRoot()
        string GetGrammarName()
        void SetGrammarName(string)

    cdef cppclass TextGrammar(Grammar):
        TextGrammar()
        void AddRule(shared_ptr[TRule]& rule) nogil
