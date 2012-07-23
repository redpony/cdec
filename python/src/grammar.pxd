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
    cdef cppclass RuleBin "const RuleBin":
        int GetNumRules()
        shared_ptr[TRule] GetIthRule(int i)
        int Arity()

    cdef cppclass GrammarIter "const GrammarIter":
        RuleBin* GetRules()
        GrammarIter* Extend(int symbol)

    cdef cppclass Grammar:
        GrammarIter* GetRoot()
        bint HasRuleForSpan(int i, int j, int distance)
        unsigned GetCTFLevels()
        string GetGrammarName()
        void SetGrammarName(string)

    cdef cppclass TextGrammar(Grammar):
        TextGrammar()
        void AddRule(shared_ptr[TRule]& rule)
