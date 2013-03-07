# Implementation of the algorithms described in 
# Lopez, EMNLP-CoNLL 2007
# Much faster than the Python numbers reported there.
# Note to reader: this code is closer to C than Python
import gc
import itertools

from libc.stdlib cimport malloc, realloc, free
from libc.string cimport memset, memcpy
from libc.math cimport fmod, ceil, floor, log

from collections import defaultdict, Counter, namedtuple

FeatureContext = namedtuple('FeatureContext',
    ['fphrase', 
     'ephrase', 
     'paircount', 
     'fcount', 
     'fsample_count',
     'input_span',
     'matches',
     'input_match',
     'test_sentence',
     'f_text',
     'e_text',
     'meta',
     'online'
    ])

OnlineFeatureContext = namedtuple('OnlineFeatureContext',
    ['fcount',
     'fsample_count',
     'paircount',
     'bilex_f',
     'bilex_e',
     'bilex_fe'
    ])

cdef int PRECOMPUTE = 0
cdef int MERGE = 1
cdef int BAEZA_YATES = 2

# NOTE: was encoded as a non-terminal in the previous version
cdef int EPSILON = sym_fromstring('*EPS*', True)

cdef class TrieNode:
    cdef public children

    def __cinit__(self):
        self.children = {}

cdef class ExtendedTrieNode(TrieNode):
    cdef public phrase
    cdef public phrase_location
    cdef public suffix_link

    def __cinit__(self, phrase=None, phrase_location=None, suffix_link=None):
        self.phrase = phrase
        self.phrase_location = phrase_location
        self.suffix_link = suffix_link


cdef class TrieTable:
    cdef public int extended
    cdef public int count
    cdef public root
    def __cinit__(self, extended=False):
        self.count = 0
        self.extended = extended
        if extended:
            self.root = ExtendedTrieNode()
        else:
            self.root = TrieNode()

# linked list structure for storing matches in BaselineRuleFactory
cdef struct match_node:
    int* match
    match_node* next

# encodes information needed to find a (hierarchical) phrase
# in the text.    If phrase is contiguous, that's just a range
# in the suffix array; if discontiguous, it is the set of 
# actual locations (packed into an array)
cdef class PhraseLocation:
    cdef int sa_low
    cdef int sa_high
    cdef int arr_low
    cdef int arr_high
    cdef IntList arr
    cdef int num_subpatterns

    # returns true if sent_id is contained
    cdef int contains(self, int sent_id):
        return 1

    def __cinit__(self, int sa_low=-1, int sa_high=-1, int arr_low=-1, int arr_high=-1,
            arr=None, int num_subpatterns=1):
        self.sa_low = sa_low
        self.sa_high = sa_high
        self.arr_low = arr_low
        self.arr_high = arr_high
        self.arr = arr
        self.num_subpatterns = num_subpatterns
            

cdef class Sampler:
    '''A Sampler implements a logic for choosing
    samples from a population range'''

    cdef int sample_size
    cdef IntList sa

    def __cinit__(self, int sample_size, SuffixArray fsarray):
        self.sample_size = sample_size
        self.sa = fsarray.sa
        if sample_size > 0:
            logger.info("Sampling strategy: uniform, max sample size = %d", sample_size)
        else:
            logger.info("Sampling strategy: no sampling")

    def sample(self, PhraseLocation phrase_location):
        '''Returns a sample of the locations for
        the phrase.    If there are less than self.sample_size
        locations, return all of them; otherwise, return
        up to self.sample_size locations.    In the latter case,
        we choose to sample UNIFORMLY -- that is, the locations
        are chosen at uniform intervals over the entire set, rather
        than randomly.    This makes the algorithm deterministic, which
        is good for things like MERT'''
        cdef IntList sample
        cdef double i, stepsize
        cdef int num_locations, val, j

        sample = IntList()
        if phrase_location.arr is None:
            num_locations = phrase_location.sa_high - phrase_location.sa_low
            if self.sample_size == -1 or num_locations <= self.sample_size:
                sample._extend_arr(self.sa.arr + phrase_location.sa_low, num_locations)
            else:
                stepsize = float(num_locations)/float(self.sample_size)
                i = phrase_location.sa_low
                while i < phrase_location.sa_high and sample.len < self.sample_size:
                    '''Note: int(i) not guaranteed to have the desired
                    effect, according to the python documentation'''
                    if fmod(i,1.0) > 0.5:
                        val = int(ceil(i))
                    else:
                        val = int(floor(i))
                    sample._append(self.sa.arr[val])
                    i = i + stepsize
        else:
            num_locations = (phrase_location.arr_high - phrase_location.arr_low) / phrase_location.num_subpatterns
            if self.sample_size == -1 or num_locations <= self.sample_size:
                sample = phrase_location.arr
            else:
                stepsize = float(num_locations)/float(self.sample_size)
                i = phrase_location.arr_low
                while i < num_locations and sample.len < self.sample_size * phrase_location.num_subpatterns:
                    '''Note: int(i) not guaranteed to have the desired
                    effect, according to the python documentation'''
                    if fmod(i,1.0) > 0.5:
                        val = int(ceil(i))
                    else:
                        val = int(floor(i))
                    j = phrase_location.arr_low + (val*phrase_location.num_subpatterns)
                    sample._extend_arr(phrase_location.arr.arr + j, phrase_location.num_subpatterns)
                    i = i + stepsize
        return sample


# struct used to encapsulate a single matching
cdef struct Matching:
    int* arr
    int start
    int end
    int sent_id
    int size


cdef void assign_matching(Matching* m, int* arr, int start, int step, int* sent_id_arr):
    m.arr = arr
    m.start = start
    m.end = start + step
    m.sent_id = sent_id_arr[arr[start]]
    m.size = step


cdef int* append_combined_matching(int* arr, Matching* loc1, Matching* loc2, 
                                int offset_by_one, int num_subpatterns, int* result_len):
    cdef int i, new_len

    new_len = result_len[0] + num_subpatterns
    arr = <int*> realloc(arr, new_len*sizeof(int))

    for i from 0 <= i < loc1.size:
        arr[result_len[0]+i] = loc1.arr[loc1.start+i]
    if num_subpatterns > loc1.size:
        arr[new_len-1] = loc2.arr[loc2.end-1]
    result_len[0] = new_len
    return arr


cdef int* extend_arr(int* arr, int* arr_len, int* appendix, int appendix_len):
    cdef int new_len
    
    new_len = arr_len[0] + appendix_len
    arr = <int*> realloc(arr, new_len*sizeof(int))
    memcpy(arr+arr_len[0], appendix, appendix_len*sizeof(int))
    arr_len[0] = new_len
    return arr

cdef int median(int low, int high, int step):
    return low + (((high - low)/step)/2)*step


cdef void find_comparable_matchings(int low, int high, int* arr, int step, int loc, int* loc_minus, int* loc_plus):
    # Returns (minus, plus) indices for the portion of the array
    # in which all matchings have the same first index as the one
    # starting at loc
    loc_plus[0] = loc + step
    while loc_plus[0] < high and arr[loc_plus[0]] == arr[loc]:
        loc_plus[0] = loc_plus[0] + step
    loc_minus[0] = loc
    while loc_minus[0]-step >= low and arr[loc_minus[0]-step] == arr[loc]:
        loc_minus[0] = loc_minus[0] - step


cdef class HieroCachingRuleFactory:
    '''This RuleFactory implements a caching 
    method using TrieTable, which makes phrase
    generation somewhat speedier -- phrases only
    need to be extracted once (however, it is
    quite possible they need to be scored 
    for each input sentence, for contextual models)'''

    cdef TrieTable rules
    cdef Sampler sampler
    cdef Scorer scorer

    cdef int max_chunks
    cdef int max_target_chunks
    cdef int max_length
    cdef int max_target_length
    cdef int max_nonterminals
    cdef int max_initial_size
    cdef int train_max_initial_size
    cdef int min_gap_size
    cdef int train_min_gap_size
    cdef int category

    cdef precomputed_index
    cdef precomputed_collocations
    cdef precompute_file
    cdef max_rank
    cdef int precompute_rank, precompute_secondary_rank
    cdef bint use_baeza_yates
    cdef bint use_index
    cdef bint use_collocations
    cdef float by_slack_factor

    cdef prev_norm_prefix
    cdef float extract_time
    cdef float intersect_time
    cdef SuffixArray fsa
    cdef DataArray fda
    cdef DataArray eda
    
    cdef Alignment alignment
    cdef IntList eid2symid
    cdef IntList fid2symid
    cdef bint tight_phrases
    cdef bint require_aligned_terminal
    cdef bint require_aligned_chunks

    cdef IntList findexes
    cdef IntList findexes1

    cdef bint online
    cdef samples_f
    cdef phrases_f
    cdef phrases_e
    cdef phrases_fe
    cdef phrases_al
    cdef bilex_f
    cdef bilex_e
    cdef bilex_fe

    def __cinit__(self,
            # compiled alignment object (REQUIRED)
            Alignment alignment,
            # parameter for double-binary search; doesn't seem to matter much
            float by_slack_factor=1.0,
            # name of generic nonterminal used by Hiero
            char* category="[X]",
            # maximum number of contiguous chunks of terminal symbols in RHS of a rule. If None, defaults to max_nonterminals+1
            max_chunks=None,
            # maximum span of a grammar rule in TEST DATA
            unsigned max_initial_size=10,
            # maximum number of symbols (both T and NT) allowed in a rule
            unsigned max_length=5,
            # maximum number of nonterminals allowed in a rule (set >2 at your own risk)
            unsigned max_nonterminals=2,
            # maximum number of contiguous chunks of terminal symbols in target-side RHS of a rule. If None, defaults to max_nonterminals+1
            max_target_chunks=None,
            # maximum number of target side symbols (both T and NT) allowed in a rule. If None, defaults to max_initial_size
            max_target_length=None,
            # minimum span of a nonterminal in the RHS of a rule in TEST DATA
            unsigned min_gap_size=2,
            # filename of file containing precomputed collocations
            precompute_file=None,
            # maximum frequency rank of patterns used to compute triples (don't set higher than 20).
            unsigned precompute_secondary_rank=20,
            # maximum frequency rank of patterns used to compute collocations (no need to set higher than maybe 200-300)
            unsigned precompute_rank=100,
            # require extracted rules to have at least one aligned word
            bint require_aligned_terminal=True,
            # require each contiguous chunk of extracted rules to have at least one aligned word
            bint require_aligned_chunks=False,
            # maximum span of a grammar rule extracted from TRAINING DATA
            unsigned train_max_initial_size=10,
            # minimum span of an RHS nonterminal in a rule extracted from TRAINING DATA
            unsigned train_min_gap_size=2,
            # False if phrases should be loose (better but slower), True otherwise
            bint tight_phrases=True,
            # True to require use of double-binary alg, false otherwise
            bint use_baeza_yates=True,
            # True to enable used of precomputed collocations
            bint use_collocations=True,
            # True to enable use of precomputed inverted indices
            bint use_index=True):
        '''Note: we make a distinction between the min_gap_size
        and max_initial_size used in test and train.    The latter
        are represented by train_min_gap_size and train_max_initial_size,
        respectively.    This is because Chiang's model does not require
        them to be the same, therefore we don't either.'''
        self.rules = TrieTable(True) # cache
        self.rules.root = ExtendedTrieNode(phrase_location=PhraseLocation())
        if alignment is None:
            raise Exception("Must specify an alignment object")
        self.alignment = alignment

        # grammar parameters and settings
        # NOTE: setting max_nonterminals > 2 is not currently supported in Hiero
        self.max_length = max_length
        self.max_nonterminals = max_nonterminals
        self.max_initial_size = max_initial_size
        self.train_max_initial_size = train_max_initial_size
        self.min_gap_size = min_gap_size
        self.train_min_gap_size = train_min_gap_size
        self.category = sym_fromstring(category, False)

        if max_chunks is None:
            self.max_chunks = self.max_nonterminals + 1
        else:
            self.max_chunks = max_chunks

        if max_target_chunks is None:
            self.max_target_chunks = self.max_nonterminals + 1
        else:
            self.max_target_chunks = max_target_chunks

        if max_target_length is None:
            self.max_target_length = max_initial_size
        else:
            self.max_target_length = max_target_length

        # algorithmic parameters and settings
        self.precomputed_collocations = {}
        self.precomputed_index = {}
        self.use_index = use_index
        self.use_collocations = use_collocations
        self.max_rank = {}
        self.precompute_file = precompute_file
        self.precompute_rank = precompute_rank
        self.precompute_secondary_rank = precompute_secondary_rank
        self.use_baeza_yates = use_baeza_yates
        self.by_slack_factor = by_slack_factor
        self.tight_phrases = tight_phrases

        if require_aligned_chunks:
            # one condition is a stronger version of the other.
            self.require_aligned_chunks = self.require_aligned_terminal = True
        elif require_aligned_terminal:
            self.require_aligned_chunks = False
            self.require_aligned_terminal = True
        else:
            self.require_aligned_chunks = self.require_aligned_terminal = False

        # diagnostics
        self.prev_norm_prefix = ()

        self.findexes = IntList(initial_len=10)
        self.findexes1 = IntList(initial_len=10)
        
        # Online stats 
        
        # True after data is added
        self.online = False
        
        # Keep track of everything that can be sampled:
        self.samples_f = defaultdict(int)
        
        # Phrase counts
        self.phrases_f = defaultdict(int)
        self.phrases_e = defaultdict(int)
        self.phrases_fe = defaultdict(lambda: defaultdict(int))
        self.phrases_al = defaultdict(lambda: defaultdict(tuple))

        # Bilexical counts
        self.bilex_f = defaultdict(int)
        self.bilex_e = defaultdict(int)
        self.bilex_fe = defaultdict(lambda: defaultdict(int))

    def configure(self, SuffixArray fsarray, DataArray edarray,
            Sampler sampler, Scorer scorer):
        '''This gives the RuleFactory access to the Context object.
        Here we also use it to precompute the most expensive intersections
        in the corpus quickly.'''
        self.fsa = fsarray
        self.fda = fsarray.darray
        self.eda = edarray
        self.fid2symid = self.set_idmap(self.fda)
        self.eid2symid = self.set_idmap(self.eda)
        self.precompute()
        self.sampler = sampler
        self.scorer = scorer

    cdef set_idmap(self, DataArray darray):
        cdef int word_id, new_word_id, N
        cdef IntList idmap
        
        N = len(darray.id2word)
        idmap = IntList(initial_len=N)
        for word_id from 0 <= word_id < N:
            new_word_id = sym_fromstring(darray.id2word[word_id], True)
            idmap.arr[word_id] = new_word_id
        return idmap


    def pattern2phrase(self, pattern):
        # pattern is a tuple, which we must convert to a hiero Phrase
        result = ()
        arity = 0
        for word_id in pattern:
            if word_id == -1:
                arity = arity + 1
                new_id = sym_setindex(self.category, arity)
            else:
                new_id = sym_fromstring(self.fda.id2word[word_id], True)
            result = result + (new_id,)
        return Phrase(result)

    def pattern2phrase_plus(self, pattern):
        # returns a list containing both the pattern, and pattern
        # suffixed/prefixed with the NT category.
        patterns = []
        result = ()
        arity = 0
        for word_id in pattern:
            if word_id == -1:
                arity = arity + 1
                new_id = sym_setindex(self.category, arity)
            else:
                new_id = sym_fromstring(self.fda.id2word[word_id], True)
            result = result + (new_id,)
        patterns.append(Phrase(result))
        patterns.append(Phrase(result + (sym_setindex(self.category, 1),)))
        patterns.append(Phrase((sym_setindex(self.category, 1),) + result))
        return patterns

    def precompute(self):
        cdef Precomputation pre
        
        if self.precompute_file is not None:
            start_time = monitor_cpu()
            logger.info("Reading precomputed data from file %s... ", self.precompute_file)
            pre = Precomputation(from_binary=self.precompute_file)
            # check parameters of precomputation -- some are critical and some are not
            if pre.max_nonterminals != self.max_nonterminals:
                logger.warn("Precomputation done with max nonterminals %d, decoder uses %d", pre.max_nonterminals, self.max_nonterminals)
            if pre.max_length != self.max_length:
                logger.warn("Precomputation done with max terminals %d, decoder uses %d", pre.max_length, self.max_length)
            if pre.train_max_initial_size != self.train_max_initial_size:
                raise Exception("Precomputation done with max initial size %d, decoder uses %d" % (pre.train_max_initial_size, self.train_max_initial_size))
            if pre.train_min_gap_size != self.train_min_gap_size:
                raise Exception("Precomputation done with min gap size %d, decoder uses %d" % (pre.train_min_gap_size, self.train_min_gap_size))
            if self.use_index:
                logger.info("Converting %d hash keys on precomputed inverted index... ", len(pre.precomputed_index))
                for pattern, arr in pre.precomputed_index.iteritems():
                    phrases = self.pattern2phrase_plus(pattern)
                    for phrase in phrases:
                        self.precomputed_index[phrase] = arr
            if self.use_collocations:
                logger.info("Converting %d hash keys on precomputed collocations... ", len(pre.precomputed_collocations))
                for pattern, arr in pre.precomputed_collocations.iteritems():
                    phrase = self.pattern2phrase(pattern)
                    self.precomputed_collocations[phrase] = arr
            stop_time = monitor_cpu()
            logger.info("Processing precomputations took %f seconds", stop_time - start_time)


    def get_precomputed_collocation(self, phrase):
        if phrase in self.precomputed_collocations:
            arr = self.precomputed_collocations[phrase]
            return PhraseLocation(arr=arr, arr_low=0, arr_high=len(arr), num_subpatterns=phrase.arity()+1)
        return None


    cdef int* baeza_yates_helper(self, int low1, int high1, int* arr1, int step1,
                        int low2, int high2, int* arr2, int step2,
                        int offset_by_one, int len_last, int num_subpatterns, int* result_len):
        cdef int i1, i2, j1, j2, med1, med2, med1_plus, med1_minus, med2_minus, med2_plus
        cdef int d_first, qsetsize, dsetsize, tmp, search_low, search_high
        cdef int med_result_len, low_result_len, high_result_len
        cdef long comparison
        cdef int* result
        cdef int* low_result
        cdef int* med_result
        cdef int* high_result
        cdef Matching loc1, loc2

        result = <int*> malloc(0*sizeof(int*))

        d_first = 0
        if high1 - low1 > high2 - low2:
            d_first = 1

        # First, check to see if we are at any of the recursive base cases
        # Case 1: one of the sets is empty
        if low1 >= high1 or low2 >= high2:
            return result

        # Case 2: sets are non-overlapping
        assign_matching(&loc1, arr1, high1-step1, step1, self.fda.sent_id.arr)
        assign_matching(&loc2, arr2, low2, step2, self.fda.sent_id.arr)
        if self.compare_matchings(&loc1, &loc2, offset_by_one, len_last) == -1:
            return result

        assign_matching(&loc1, arr1, low1, step1, self.fda.sent_id.arr)
        assign_matching(&loc2, arr2, high2-step2, step2, self.fda.sent_id.arr)
        if self.compare_matchings(&loc1, &loc2, offset_by_one, len_last) == 1:
            return result

        # Case 3: query set and data set do not meet size mismatch constraints;
        # We use mergesort instead in this case
        qsetsize = (high1-low1) / step1
        dsetsize = (high2-low2) / step2
        if d_first:
            tmp = qsetsize
            qsetsize = dsetsize
            dsetsize = tmp

        if self.by_slack_factor * qsetsize * log(dsetsize) / log(2) > dsetsize:
            free(result)
            return self.merge_helper(low1, high1, arr1, step1, low2, high2, arr2, step2, offset_by_one, len_last, num_subpatterns, result_len)

        # binary search.    There are two flavors, depending on
        # whether the queryset or dataset is first
        if d_first:
            med2 = median(low2, high2, step2)
            assign_matching(&loc2, arr2, med2, step2, self.fda.sent_id.arr)

            search_low = low1
            search_high = high1
            while search_low < search_high:
                med1 = median(search_low, search_high, step1)
                find_comparable_matchings(low1, high1, arr1, step1, med1, &med1_minus, &med1_plus)
                comparison = self.compare_matchings_set(med1_minus, med1_plus, arr1, step1, &loc2, offset_by_one, len_last)
                if comparison == -1:
                    search_low = med1_plus
                elif comparison == 1:
                    search_high = med1_minus
                else:
                    break
        else:
            med1 = median(low1, high1, step1)
            find_comparable_matchings(low1, high1, arr1, step1, med1, &med1_minus, &med1_plus)

            search_low = low2
            search_high = high2
            while search_low < search_high:
                med2 = median(search_low, search_high, step2)
                assign_matching(&loc2, arr2, med2, step2, self.fda.sent_id.arr)
                comparison = self.compare_matchings_set(med1_minus, med1_plus, arr1, step1, &loc2, offset_by_one, len_last)
                if comparison == -1:
                    search_high = med2
                elif comparison == 1:
                    search_low = med2 + step2
                else:
                    break

        med_result_len = 0
        med_result = <int*> malloc(0*sizeof(int*))
        if search_high > search_low:
            # Then there is a match for the median element of Q
            # What we want to find is the group of all bindings in the first set
            # s.t. their first element == the first element of med1.    Then we
            # want to store the bindings for all of those elements.    We can
            # subsequently throw all of them away.
            med2_minus = med2
            med2_plus = med2 + step2
            i1 = med1_minus
            while i1 < med1_plus:
                assign_matching(&loc1, arr1, i1, step1, self.fda.sent_id.arr)
                while med2_minus-step2 >= low2:
                    assign_matching(&loc2, arr2, med2_minus-step2, step2, self.fda.sent_id.arr)
                    if self.compare_matchings(&loc1, &loc2, offset_by_one, len_last) < 1:
                        med2_minus = med2_minus - step2
                    else:
                        break
                i2 = med2_minus
                while i2 < high2:
                    assign_matching(&loc2, arr2, i2, step2, self.fda.sent_id.arr)
                    comparison = self.compare_matchings(&loc1, &loc2, offset_by_one, len_last)
                    if comparison == 0:
                        pass
                        med_result = append_combined_matching(med_result, &loc1, &loc2, offset_by_one, num_subpatterns, &med_result_len)
                    if comparison == -1:
                        break
                    i2 = i2 + step2
                if i2 > med2_plus:
                    med2_plus = i2
                i1 = i1 + step1

            tmp = med1_minus
            med1_minus = med1_plus
            med1_plus = tmp
        else:
            # No match; need to figure out the point of division in D and Q
            med2_minus = med2
            med2_plus = med2
            if d_first:
                med2_minus = med2_minus + step2
                if comparison == -1:
                    med1_minus = med1_plus
                if comparison == 1:
                    med1_plus = med1_minus
            else:
                tmp = med1_minus
                med1_minus = med1_plus
                med1_plus = tmp
                if comparison == 1:
                    med2_minus = med2_minus + step2
                    med2_plus = med2_plus + step2

        low_result_len = 0
        low_result = self.baeza_yates_helper(low1, med1_plus, arr1, step1, low2, med2_plus, arr2, step2, offset_by_one, len_last, num_subpatterns, &low_result_len)
        high_result_len = 0
        high_result = self.baeza_yates_helper(med1_minus, high1, arr1, step1, med2_minus, high2, arr2, step2, offset_by_one, len_last, num_subpatterns, &high_result_len)

        result = extend_arr(result, result_len, low_result, low_result_len)
        result = extend_arr(result, result_len, med_result, med_result_len)
        result = extend_arr(result, result_len, high_result, high_result_len)
        free(low_result)
        free(med_result)
        free(high_result)

        return result



    cdef long compare_matchings_set(self, int i1_minus, int i1_plus, int* arr1, int step1,
                            Matching* loc2, int offset_by_one, int len_last):
        """
        Compares a *set* of bindings, all with the same first element,
        to a single binding.    Returns -1 if all comparisons == -1, 1 if all
        comparisons == 1, and 0 otherwise.
        """
        cdef int i1, comparison, prev_comparison
        cdef Matching l1_stack
        cdef Matching* loc1

        loc1 = &l1_stack

        i1 = i1_minus
        while i1 < i1_plus:
            assign_matching(loc1, arr1, i1, step1, self.fda.sent_id.arr)
            comparison = self.compare_matchings(loc1, loc2, offset_by_one, len_last)
            if comparison == 0:
                prev_comparison = 0
                break
            elif i1 == i1_minus:
                prev_comparison = comparison
            else:
                if comparison != prev_comparison:
                    prev_comparison = 0
                    break
            i1 = i1 + step1
        return prev_comparison


    cdef long compare_matchings(self, Matching* loc1, Matching* loc2, int offset_by_one, int len_last):
        cdef int i

        if loc1.sent_id > loc2.sent_id:
            return 1
        if loc2.sent_id > loc1.sent_id:
            return -1

        if loc1.size == 1 and loc2.size == 1:
            if loc2.arr[loc2.start] - loc1.arr[loc1.start] <= self.train_min_gap_size:
                return 1

        elif offset_by_one:
            for i from 1 <= i < loc1.size:
                if loc1.arr[loc1.start+i] > loc2.arr[loc2.start+i-1]:
                    return 1
                if loc1.arr[loc1.start+i] < loc2.arr[loc2.start+i-1]:
                    return -1

        else:
            if loc1.arr[loc1.start]+1 > loc2.arr[loc2.start]:
                return 1
            if loc1.arr[loc1.start]+1 < loc2.arr[loc2.start]:
                return -1

            for i from 1 <= i < loc1.size:
                if loc1.arr[loc1.start+i] > loc2.arr[loc2.start+i]:
                    return 1
                if loc1.arr[loc1.start+i] < loc2.arr[loc2.start+i]:
                    return -1

        if loc2.arr[loc2.end-1] + len_last - loc1.arr[loc1.start] > self.train_max_initial_size:
            return -1
        return 0


    cdef int* merge_helper(self, int low1, int high1, int* arr1, int step1,
                    int low2, int high2, int* arr2, int step2,
                    int offset_by_one, int len_last, int num_subpatterns, int* result_len):
        cdef int i1, i2, j1, j2
        cdef long comparison
        cdef int* result
        cdef Matching loc1, loc2

        result_len[0] = 0
        result = <int*> malloc(0*sizeof(int))

        i1 = low1
        i2 = low2
        while i1 < high1 and i2 < high2:
            
            # First, pop all unneeded loc2's off the stack
            assign_matching(&loc1, arr1, i1, step1, self.fda.sent_id.arr)
            while i2 < high2:
                assign_matching(&loc2, arr2, i2, step2, self.fda.sent_id.arr)
                if self.compare_matchings(&loc1, &loc2, offset_by_one, len_last) == 1:
                    i2 = i2 + step2
                else:
                    break

            # Next: process all loc1's with the same starting val
            j1 = i1
            while i1 < high1 and arr1[j1] == arr1[i1]:
                assign_matching(&loc1, arr1, i1, step1, self.fda.sent_id.arr)
                j2 = i2
                while j2 < high2:
                    assign_matching(&loc2, arr2, j2, step2, self.fda.sent_id.arr)
                    comparison = self.compare_matchings(&loc1, &loc2, offset_by_one, len_last)
                    if comparison == 0:
                        result = append_combined_matching(result, &loc1, &loc2, offset_by_one, num_subpatterns, result_len)
                    if comparison == 1:
                        pass
                    if comparison == -1:
                        break
                    else:
                        j2 = j2 + step2
                i1 = i1 + step1
        return result


    cdef void sort_phrase_loc(self, IntList arr, PhraseLocation loc, Phrase phrase):
        cdef int i, j
        cdef VEB veb
        cdef IntList result

        if phrase in self.precomputed_index:
            loc.arr = self.precomputed_index[phrase]
        else:
            loc.arr = IntList(initial_len=loc.sa_high-loc.sa_low)
            veb = VEB(arr.len)
            for i from loc.sa_low <= i < loc.sa_high:
                veb._insert(arr.arr[i])
            i = veb.veb.min_val
            for j from 0 <= j < loc.sa_high-loc.sa_low:
                loc.arr.arr[j] = i
                i = veb._findsucc(i)
        loc.arr_low = 0
        loc.arr_high = loc.arr.len


    cdef intersect_helper(self, Phrase prefix, Phrase suffix,
                PhraseLocation prefix_loc, PhraseLocation suffix_loc, int algorithm):

        cdef IntList arr1, arr2, result
        cdef int low1, high1, step1, low2, high2, step2, offset_by_one, len_last, num_subpatterns, result_len
        cdef int* result_ptr

        result_len = 0

        if sym_isvar(suffix[0]):
            offset_by_one = 1
        else:
            offset_by_one = 0

        len_last = len(suffix.getchunk(suffix.arity()))

        if prefix_loc.arr is None:
            self.sort_phrase_loc(self.fsa.sa, prefix_loc, prefix)
        arr1 = prefix_loc.arr
        low1 = prefix_loc.arr_low
        high1 = prefix_loc.arr_high
        step1 = prefix_loc.num_subpatterns

        if suffix_loc.arr is None:
            self.sort_phrase_loc(self.fsa.sa, suffix_loc, suffix)
        arr2 = suffix_loc.arr
        low2 = suffix_loc.arr_low
        high2 = suffix_loc.arr_high
        step2 = suffix_loc.num_subpatterns

        num_subpatterns = prefix.arity()+1

        if algorithm == MERGE:
            result_ptr = self.merge_helper(low1, high1, arr1.arr, step1,
                                    low2, high2, arr2.arr, step2,
                                    offset_by_one, len_last, num_subpatterns, &result_len)
        else:
            result_ptr = self.baeza_yates_helper(low1, high1, arr1.arr, step1,
                                    low2, high2, arr2.arr, step2,
                                    offset_by_one, len_last, num_subpatterns, &result_len)

        if result_len == 0:
            free(result_ptr)
            return None
        else:
            result = IntList()
            free(result.arr)
            result.arr = result_ptr
            result.len = result_len
            result.size = result_len
            return PhraseLocation(arr_low=0, arr_high=result_len, arr=result, num_subpatterns=num_subpatterns)

    cdef loc2str(self, PhraseLocation loc):
        cdef int i, j
        result = "{"
        i = 0
        while i < loc.arr_high:
            result = result + "("
            for j from i <= j < i + loc.num_subpatterns:
                result = result + ("%d " %loc.arr[j])
            result = result + ")"
            i = i + loc.num_subpatterns
        result = result + "}"
        return result

    cdef PhraseLocation intersect(self, prefix_node, suffix_node, Phrase phrase):
        cdef Phrase prefix, suffix
        cdef PhraseLocation prefix_loc, suffix_loc, result

        prefix = prefix_node.phrase
        suffix = suffix_node.phrase
        prefix_loc = prefix_node.phrase_location
        suffix_loc = suffix_node.phrase_location

        result = self.get_precomputed_collocation(phrase)
        if result is not None:
            intersect_method = "precomputed"

        if result is None:
            if self.use_baeza_yates:
                result = self.intersect_helper(prefix, suffix, prefix_loc, suffix_loc, BAEZA_YATES)
                intersect_method="double binary"
            else:
                result = self.intersect_helper(prefix, suffix, prefix_loc, suffix_loc, MERGE)
                intersect_method="merge"
        return result

    def advance(self, frontier, res, fwords):
        cdef unsigned na
        nf = []
        for (toskip, (i, alt, pathlen)) in frontier:
            spanlen = fwords[i][alt][2]
            if (toskip == 0):
                res.append((i, alt, pathlen))
            ni = i + spanlen
            if (ni < len(fwords) and (pathlen + 1) < self.max_initial_size):
                for na in range(len(fwords[ni])):
                    nf.append((toskip - 1, (ni, na, pathlen + 1)))
        if (len(nf) > 0):
            return self.advance(nf, res, fwords)
        else:
            return res
        
    def get_all_nodes_isteps_away(self, skip, i, spanlen, pathlen, fwords, next_states, reachable_buffer):
        cdef unsigned alt_it
        frontier = []
        if (i+spanlen+skip >= len(next_states)):
            return frontier
        key = tuple([i,spanlen])
        reachable = []
        if (key in reachable_buffer):
            reachable = reachable_buffer[key]
        else:
            reachable = self.reachable(fwords, i, spanlen)
            reachable_buffer[key] = reachable
        for nextreachable in reachable:
            for next_id in next_states[nextreachable]:
                jump = self.shortest(fwords,i,next_id)
                if jump < skip:
                    continue
                if pathlen+jump <= self.max_initial_size:
                    for alt_id in range(len(fwords[next_id])):
                        if (fwords[next_id][alt_id][0] != EPSILON):
                            newel = (next_id,alt_id,pathlen+jump)
                            if newel not in frontier:
                                frontier.append((next_id,alt_id,pathlen+jump))
        return frontier

    def reachable(self, fwords, ifrom, dist):
        ret = []
        if (ifrom >= len(fwords)):
            return ret
        for alt_id in range(len(fwords[ifrom])):
            if (fwords[ifrom][alt_id][0] == EPSILON):
                ret.extend(self.reachable(fwords,ifrom+fwords[ifrom][alt_id][2],dist))
            else:
                if (dist==0):
                    if (ifrom not in ret):
                        ret.append(ifrom)
                else:
                    for ifromchild in self.reachable(fwords,ifrom+fwords[ifrom][alt_id][2],dist-1):
                        if (ifromchild not in ret):
                            ret.append(ifromchild)
                    
        return ret

    def shortest(self, fwords, ifrom, ito):
        cdef unsigned alt_id
        min = 1000
        if (ifrom > ito):
            return min
        if (ifrom == ito):
            return 0
        for alt_id in range(len(fwords[ifrom])):
            currmin = self.shortest(fwords,ifrom+fwords[ifrom][alt_id][2],ito)
            if (fwords[ifrom][alt_id][0] != EPSILON):
                currmin += 1
            if (currmin<min):
                min = currmin
        return min

    def get_next_states(self, _columns, curr_idx, min_dist=2):
        result = []
        candidate = [[curr_idx,0]]

        while len(candidate) > 0:
            curr = candidate.pop()
            if curr[0] >= len(_columns):
                continue
            if curr[0] not in result and min_dist <= curr[1] <= self.max_initial_size:
                result.append(curr[0]);
            curr_col = _columns[curr[0]]
            for alt in curr_col:
                next_id = curr[0]+alt[2]
                jump = 1
                if (alt[0] == EPSILON):
                    jump = 0
                if next_id not in result and min_dist <= curr[1]+jump <= self.max_initial_size+1:
                    candidate.append([next_id,curr[1]+jump])
        return sorted(result);

    def input(self, fwords, meta):
        '''When this function is called on the RuleFactory,
        it looks up all of the rules that can be used to translate
        the input sentence'''
        cdef int i, j, k, flen, arity, num_subpatterns, num_samples, alt, alt_id, nualt
        cdef float start_time
        cdef PhraseLocation phrase_location
        cdef IntList sample, chunklen
        cdef Matching matching
        cdef Phrase hiero_phrase

        flen = len(fwords)
        start_time = monitor_cpu()
        self.extract_time = 0.0
        self.intersect_time = 0.0
        nodes_isteps_away_buffer = {}
        hit = 0
        reachable_buffer = {}

        # Phrase pairs processed by suffix array extractor.  Do not re-extract
        # during online extraction.  This is probably the hackiest part of
        # online grammar extraction.
        seen_phrases = set()
        
        # Do not cache between sentences
        self.rules.root = ExtendedTrieNode(phrase_location=PhraseLocation())

        frontier = []
        for i in range(len(fwords)):
            for alt in range(0, len(fwords[i])):
                if fwords[i][alt][0] != EPSILON:
                    frontier.append((i, i, (i,), alt, 0, self.rules.root, (), False))

        xroot = None
        x1 = sym_setindex(self.category, 1)
        if x1 in self.rules.root.children:
            xroot = self.rules.root.children[x1]
        else:
            xroot = ExtendedTrieNode(suffix_link=self.rules.root, phrase_location=PhraseLocation())
            self.rules.root.children[x1] = xroot

        for i in range(self.min_gap_size, len(fwords)):
            for alt in range(0, len(fwords[i])):
                if fwords[i][alt][0] != EPSILON:
                    frontier.append((i-self.min_gap_size, i, (i,), alt, self.min_gap_size, xroot, (x1,), True))

        next_states = []
        for i in range(len(fwords)):
            next_states.append(self.get_next_states(fwords,i,self.min_gap_size))

        while len(frontier) > 0:
            new_frontier = []
            for k, i, input_match, alt, pathlen, node, prefix, is_shadow_path in frontier:
                word_id = fwords[i][alt][0]
                spanlen = fwords[i][alt][2]
                # TODO get rid of k -- pathlen is replacing it
                if word_id == EPSILON:
                    # skipping because word_id is epsilon
                    if i+spanlen >= len(fwords):
                        continue
                    for nualt in range(0,len(fwords[i+spanlen])):
                        frontier.append((k, i+spanlen, input_match, nualt, pathlen, node, prefix, is_shadow_path))
                    continue
                
                phrase = prefix + (word_id,)
                hiero_phrase = Phrase(phrase)
                arity = hiero_phrase.arity()

                lookup_required = False
                if word_id in node.children:
                    if node.children[word_id] is None:
                        # Path dead-ends at this node
                        continue
                    else:
                        # Path continues at this node
                        node = node.children[word_id]
                else:
                    if node.suffix_link is None:
                        # Current node is root; lookup required
                        lookup_required = True
                    else:
                        if word_id in node.suffix_link.children:
                            if node.suffix_link.children[word_id] is None:
                                # Suffix link reports path is dead end
                                node.children[word_id] = None
                                continue
                            else:
                                # Suffix link indicates lookup is reqired
                                lookup_required = True
                        else:
                            #ERROR: We never get here
                            raise Exception("Keyword trie error")
                # checking whether lookup_required
                if lookup_required:
                    new_node = None
                    if is_shadow_path:
                        # Extending shadow path
                        # on the shadow path we don't do any search, we just use info from suffix link
                        new_node = ExtendedTrieNode(phrase_location=node.suffix_link.children[word_id].phrase_location,
                                suffix_link=node.suffix_link.children[word_id],
                                phrase=hiero_phrase)
                    else:
                        if arity > 0:
                            # Intersecting because of arity > 0
                            intersect_start_time = monitor_cpu()
                            phrase_location = self.intersect(node, node.suffix_link.children[word_id], hiero_phrase)
                            intersect_stop_time = monitor_cpu()
                            self.intersect_time += intersect_stop_time - intersect_start_time
                        else:
                            # Suffix array search
                            phrase_location = node.phrase_location
                            sa_range = self.fsa.lookup(sym_tostring(phrase[-1]), len(phrase)-1, phrase_location.sa_low, phrase_location.sa_high)
                            if sa_range is not None:
                                phrase_location = PhraseLocation(sa_low=sa_range[0], sa_high=sa_range[1])
                            else:
                                phrase_location = None

                        if phrase_location is None:
                            node.children[word_id] = None
                            # Search failed
                            continue
                        # Search succeeded
                        suffix_link = self.rules.root
                        if node.suffix_link is not None:
                            suffix_link = node.suffix_link.children[word_id]
                        new_node = ExtendedTrieNode(phrase_location=phrase_location,
                                suffix_link=suffix_link,
                                phrase=hiero_phrase)
                    node.children[word_id] = new_node
                    node = new_node

                    '''Automatically add a trailing X node, if allowed --
                    This should happen before we get to extraction (so that
                    the node will exist if needed)'''
                    if arity < self.max_nonterminals:
                        xcat_index = arity+1
                        xcat = sym_setindex(self.category, xcat_index)
                        suffix_link_xcat_index = xcat_index
                        if is_shadow_path:
                            suffix_link_xcat_index = xcat_index-1
                        suffix_link_xcat = sym_setindex(self.category, suffix_link_xcat_index)
                        node.children[xcat] = ExtendedTrieNode(phrase_location=node.phrase_location,
                                suffix_link=node.suffix_link.children[suffix_link_xcat],
                                phrase= Phrase(phrase + (xcat,)))

                    # sample from range
                    if not is_shadow_path:
                        sample = self.sampler.sample(node.phrase_location)
                        num_subpatterns = (<PhraseLocation> node.phrase_location).num_subpatterns
                        chunklen = IntList(initial_len=num_subpatterns)
                        for j from 0 <= j < num_subpatterns:
                            chunklen.arr[j] = hiero_phrase.chunklen(j)
                        extracts = []
                        j = 0
                        extract_start = monitor_cpu()
                        while j < sample.len:
                            extract = []

                            assign_matching(&matching, sample.arr, j, num_subpatterns, self.fda.sent_id.arr)
                            loc = tuple(sample[j:j+num_subpatterns])
                            extract = self.extract(hiero_phrase, &matching, chunklen.arr, num_subpatterns)
                            extracts.extend([(e, loc) for e in extract])
                            j = j + num_subpatterns

                        num_samples = sample.len/num_subpatterns
                        extract_stop = monitor_cpu()
                        self.extract_time = self.extract_time + extract_stop - extract_start
                        if len(extracts) > 0:
                            fcount = Counter()
                            fphrases = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
                            for (f, e, count, als), loc in extracts:
                                fcount[f] += count
                                fphrases[f][e][als].append(loc)
                            for f, elist in fphrases.iteritems():
                                for e, alslist in elist.iteritems():
                                    alignment, max_locs = max(alslist.iteritems(), key=lambda x: len(x[1]))
                                    locs = tuple(itertools.chain.from_iterable(alslist.itervalues()))
                                    count = len(locs)
                                    scores = self.scorer.score(FeatureContext(
                                               f, e, count, fcount[f], num_samples,
                                               (k,i+spanlen), locs, input_match, 
                                               fwords, self.fda, self.eda,
                                               meta,
                                               # Include online stats.  None if none.
                                               self.online_ctx_lookup(f, e)))
                                    # Phrase pair processed
                                    if self.online:
                                        seen_phrases.add((f, e))
                                    yield Rule(self.category, f, e, scores, alignment)

                if len(phrase) < self.max_length and i+spanlen < len(fwords) and pathlen+1 <= self.max_initial_size:
                    for alt_id in range(len(fwords[i+spanlen])):
                        new_frontier.append((k, i+spanlen, input_match, alt_id, pathlen + 1, node, phrase, is_shadow_path))
                    num_subpatterns = arity
                    if not is_shadow_path:
                        num_subpatterns = num_subpatterns + 1
                    if len(phrase)+1 < self.max_length and arity < self.max_nonterminals and num_subpatterns < self.max_chunks:
                        xcat = sym_setindex(self.category, arity+1)
                        xnode = node.children[xcat]
                        # I put spanlen=1 below
                        key = tuple([self.min_gap_size, i, 1, pathlen])
                        frontier_nodes = []
                        if (key in nodes_isteps_away_buffer):
                            frontier_nodes = nodes_isteps_away_buffer[key]
                        else:
                            frontier_nodes = self.get_all_nodes_isteps_away(self.min_gap_size, i, 1, pathlen, fwords, next_states, reachable_buffer)
                            nodes_isteps_away_buffer[key] = frontier_nodes
                        
                        for (i, alt, pathlen) in frontier_nodes:
                            new_frontier.append((k, i, input_match + (i,), alt, pathlen, xnode, phrase +(xcat,), is_shadow_path))
            frontier = new_frontier
        
        # Online rule extraction and scoring
        if self.online:
            f_syms = tuple(word[0][0] for word in fwords)
            for (f, lex_i, lex_j) in self.get_f_phrases(f_syms):
                spanlen = (lex_j - lex_i) + 1
                if not sym_isvar(f[0]):
                    spanlen += 1
                if not sym_isvar(f[1]):
                    spanlen += 1
                for e in self.phrases_fe.get(f, ()):
                    if (f, e) not in seen_phrases:
                        # Don't add multiple instances of the same phrase here
                        seen_phrases.add((f, e))
                        scores = self.scorer.score(FeatureContext(
                                f, e, 0, 0, 0,
                                spanlen, None, None, 
                                fwords, self.fda, self.eda,
                                meta,
                                self.online_ctx_lookup(f, e)))
                        alignment = self.phrases_al[f][e]
                        yield Rule(self.category, f, e, scores, alignment)
             
        stop_time = monitor_cpu()
        logger.info("Total time for rule lookup, extraction, and scoring = %f seconds", (stop_time - start_time))
        gc.collect()
        logger.info("    Extract time = %f seconds", self.extract_time)
        logger.info("    Intersect time = %f seconds", self.intersect_time)


    cdef int find_fixpoint(self, 
                        int f_low, f_high, 
                        int* f_links_low, int* f_links_high, 
                        int* e_links_low, int* e_links_high,
                        int e_in_low, int e_in_high, 
                        int* e_low, int* e_high,
                        int* f_back_low, int* f_back_high, 
                        int f_sent_len, int e_sent_len,
                        int max_f_len, int max_e_len, 
                        int min_fx_size, int min_ex_size,
                        int max_new_x,
                        int allow_low_x, int allow_high_x, 
                        int allow_arbitrary_x, int write_log):
        cdef int e_low_prev, e_high_prev, f_low_prev, f_high_prev, new_x, new_low_x, new_high_x

        e_low[0] = e_in_low
        e_high[0] = e_in_high
        self.find_projection(f_low, f_high, f_links_low, f_links_high, e_low, e_high)
        if e_low[0] == -1:
            # low-priority corner case: if phrase w is unaligned,
            # but we don't require aligned terminals, then returning
            # an error here might prevent extraction of allowed
            # rule X -> X_1 w X_2 / X_1 X_2.    This is probably
            # not worth the bother, though.
            return 0
        elif e_in_low != -1 and e_low[0] != e_in_low:
            if e_in_low - e_low[0] < min_ex_size:
                e_low[0] = e_in_low - min_ex_size
                if e_low[0] < 0:
                    return 0

        if e_high[0] - e_low[0] > max_e_len:
            return 0
        elif e_in_high != -1 and e_high[0] != e_in_high:
            if e_high[0] - e_in_high < min_ex_size:
                e_high[0] = e_in_high + min_ex_size
                if e_high[0] > e_sent_len:
                    return 0

        f_back_low[0] = -1
        f_back_high[0] = -1
        f_low_prev = f_low
        f_high_prev = f_high
        new_x = 0
        new_low_x = 0
        new_high_x = 0

        while True:

            if f_back_low[0] == -1:
                self.find_projection(e_low[0], e_high[0], e_links_low, e_links_high, f_back_low, f_back_high)
            else:
                self.find_projection(e_low[0], e_low_prev, e_links_low, e_links_high, f_back_low, f_back_high)
                self.find_projection(e_high_prev, e_high[0], e_links_low, e_links_high, f_back_low, f_back_high)

            if f_back_low[0] > f_low:
                f_back_low[0] = f_low

            if f_back_high[0] < f_high:
                f_back_high[0] = f_high

            if f_back_low[0] == f_low_prev and f_back_high[0] == f_high_prev:
                return 1

            if allow_low_x == 0 and f_back_low[0] < f_low:
                # FAIL: f phrase is not tight
                return 0

            if f_back_high[0] - f_back_low[0] > max_f_len:
                # FAIL: f back projection is too wide
                return 0

            if allow_high_x == 0 and f_back_high[0] > f_high:
                # FAIL: extension on high side not allowed
                return 0

            if f_low != f_back_low[0]:
                if new_low_x == 0:
                    if new_x >= max_new_x:
                        # FAIL: extension required on low side violates max # of gaps
                        return 0
                    else:
                        new_x = new_x + 1
                        new_low_x = 1
                if f_low - f_back_low[0] < min_fx_size:
                    f_back_low[0] = f_low - min_fx_size
                    if f_back_high[0] - f_back_low[0] > max_f_len:
                        # FAIL: extension required on low side violates max initial length
                        return 0
                    if f_back_low[0] < 0:
                        # FAIL: extension required on low side violates sentence boundary
                        return 0

            if f_high != f_back_high[0]:
                if new_high_x == 0:
                    if new_x >= max_new_x:
                        # FAIL: extension required on high side violates max # of gaps
                        return 0
                    else:
                        new_x = new_x + 1
                        new_high_x = 1
                if f_back_high[0] - f_high < min_fx_size:
                    f_back_high[0] = f_high + min_fx_size
                    if f_back_high[0] - f_back_low[0] > max_f_len:
                        # FAIL: extension required on high side violates max initial length
                        return 0
                    if f_back_high[0] > f_sent_len:
                        # FAIL: extension required on high side violates sentence boundary
                        return 0

            e_low_prev = e_low[0]
            e_high_prev = e_high[0]

            self.find_projection(f_back_low[0], f_low_prev, f_links_low, f_links_high, e_low, e_high)
            self.find_projection(f_high_prev, f_back_high[0], f_links_low, f_links_high, e_low, e_high)
            if e_low[0] == e_low_prev and e_high[0] == e_high_prev:
                return 1
            if allow_arbitrary_x == 0:
                # FAIL: arbitrary expansion not permitted
                return 0
            if e_high[0] - e_low[0] > max_e_len:
                # FAIL: re-projection violates sentence max phrase length
                return 0
            f_low_prev = f_back_low[0]
            f_high_prev = f_back_high[0]


    cdef find_projection(self, int in_low, int in_high, int* in_links_low, int* in_links_high, 
                        int* out_low, int* out_high):
        cdef int i
        for i from in_low <= i < in_high:
            if in_links_low[i] != -1:
                if out_low[0] == -1 or in_links_low[i] < out_low[0]:
                    out_low[0] = in_links_low[i]
                if out_high[0] == -1 or in_links_high[i] > out_high[0]:
                    out_high[0] = in_links_high[i]


    cdef int* int_arr_extend(self, int* arr, int* arr_len, int* data, int data_len):
        cdef int new_len
        new_len = arr_len[0] + data_len
        arr = <int*> realloc(arr, new_len*sizeof(int))
        memcpy(arr+arr_len[0], data, data_len*sizeof(int))
        arr_len[0] = new_len
        return arr


    cdef extract_phrases(self, int e_low, int e_high, int* e_gap_low, int* e_gap_high, int* e_links_low, int num_gaps,
                        int f_low, int f_high, int* f_gap_low, int* f_gap_high, int* f_links_low, 
                        int sent_id, int e_sent_len, int e_sent_start):
        cdef int i, j, k, m, n, *e_gap_order, e_x_low, e_x_high, e_x_gap_low, e_x_gap_high
        cdef int *e_gaps1, *e_gaps2, len1, len2, step, num_chunks
        cdef IntList ephr_arr
        cdef result

        result = []
        len1 = 0
        e_gaps1 = <int*> malloc(0)
        ephr_arr = IntList()

        e_gap_order = <int*> malloc(num_gaps*sizeof(int))
        if num_gaps > 0:
            e_gap_order[0] = 0
            for i from 1 <= i < num_gaps:
                for j from 0 <= j < i:
                    if e_gap_low[i] < e_gap_low[j]:
                        for k from j <= k < i:
                            e_gap_order[k+1] = e_gap_order[k]
                        e_gap_order[j] = i
                        break
                else:
                    e_gap_order[i] = i

        e_x_low = e_low
        e_x_high = e_high
        if not self.tight_phrases:
            while e_x_low > 0 and e_high - e_x_low < self.train_max_initial_size and e_links_low[e_x_low-1] == -1:
                e_x_low = e_x_low - 1
            while e_x_high < e_sent_len and e_x_high - e_low < self.train_max_initial_size and e_links_low[e_x_high] == -1:
                e_x_high = e_x_high + 1

        for i from e_x_low <= i <= e_low:
            e_gaps1 = self.int_arr_extend(e_gaps1, &len1, &i, 1)

        for i from 0 <= i < num_gaps:
            e_gaps2 = <int*> malloc(0)
            len2 = 0

            j = e_gap_order[i]
            e_x_gap_low = e_gap_low[j]
            e_x_gap_high = e_gap_high[j]
            if not self.tight_phrases:
                while e_x_gap_low > e_x_low and e_links_low[e_x_gap_low-1] == -1:
                    e_x_gap_low = e_x_gap_low - 1
                while e_x_gap_high < e_x_high and e_links_low[e_x_gap_high] == -1:
                    e_x_gap_high = e_x_gap_high + 1

            k = 0
            step = 1+(i*2)
            while k < len1:
                for m from e_x_gap_low <= m <= e_gap_low[j]:
                    if m >= e_gaps1[k+step-1]:
                        for n from e_gap_high[j] <= n <= e_x_gap_high:
                            if n-m >= 1:    # extractor.py doesn't restrict target-side gap length 
                                e_gaps2 = self.int_arr_extend(e_gaps2, &len2, e_gaps1+k, step)
                                e_gaps2 = self.int_arr_extend(e_gaps2, &len2, &m, 1)
                                e_gaps2 = self.int_arr_extend(e_gaps2, &len2, &n, 1)
                k = k + step
            free(e_gaps1)
            e_gaps1 = e_gaps2
            len1 = len2

        step = 1+(num_gaps*2)
        e_gaps2 = <int*> malloc(0)
        len2 = 0
        for i from e_high <= i <= e_x_high:
            j = 0
            while j < len1:
                if i - e_gaps1[j] <= self.train_max_initial_size and i >= e_gaps1[j+step-1]:
                    e_gaps2 = self.int_arr_extend(e_gaps2, &len2, e_gaps1+j, step)
                    e_gaps2 = self.int_arr_extend(e_gaps2, &len2, &i, 1)
                j = j + step
        free(e_gaps1)
        e_gaps1 = e_gaps2
        len1 = len2

        step = (num_gaps+1)*2
        i = 0
        
        while i < len1:
            ephr_arr._clear()
            num_chunks = 0
            indexes = []
            for j from 0 <= j < num_gaps+1:
                if e_gaps1[i+2*j] < e_gaps1[i+(2*j)+1]:
                    num_chunks = num_chunks + 1
                for k from e_gaps1[i+2*j] <= k < e_gaps1[i+(2*j)+1]:
                    indexes.append(k)
                    ephr_arr._append(self.eid2symid[self.eda.data.arr[e_sent_start+k]])
                if j < num_gaps:
                    indexes.append(sym_setindex(self.category, e_gap_order[j]+1))
                    ephr_arr._append(sym_setindex(self.category, e_gap_order[j]+1))
            i = i + step
            if ephr_arr.len <= self.max_target_length and num_chunks <= self.max_target_chunks:
                result.append((Phrase(ephr_arr),indexes))

        free(e_gaps1)
        free(e_gap_order)
        return result

    cdef IntList create_alignments(self, int* sent_links, int num_links, findexes, eindexes):
        cdef unsigned i
        cdef IntList ret = IntList()
        for i in range(len(findexes)):
            s = findexes[i]
            if (s<0):
                continue
            idx = 0
            while (idx < num_links*2):
                if (sent_links[idx] == s):
                    j = eindexes.index(sent_links[idx+1])
                    ret.append(i*65536+j)
                idx += 2
        return ret
                
    cdef extract(self, Phrase phrase, Matching* matching, int* chunklen, int num_chunks):
        cdef int* sent_links, *e_links_low, *e_links_high, *f_links_low, *f_links_high
        cdef int *f_gap_low, *f_gap_high, *e_gap_low, *e_gap_high, num_gaps, gap_start
        cdef int i, j, k, e_i, f_i, num_links, num_aligned_chunks, met_constraints, x
        cdef int f_low, f_high, e_low, e_high, f_back_low, f_back_high
        cdef int e_sent_start, e_sent_end, f_sent_start, f_sent_end, e_sent_len, f_sent_len
        cdef int e_word_count, f_x_low, f_x_high, e_x_low, e_x_high, phrase_len
        cdef float pair_count
        cdef extracts, phrase_list
        cdef IntList fphr_arr
        cdef Phrase fphr
        cdef reason_for_failure

        fphr_arr = IntList()
        phrase_len = phrase.n
        extracts = []
        sent_links = self.alignment._get_sent_links(matching.sent_id, &num_links)

        e_sent_start = self.eda.sent_index.arr[matching.sent_id]
        e_sent_end = self.eda.sent_index.arr[matching.sent_id+1]
        e_sent_len = e_sent_end - e_sent_start - 1
        f_sent_start = self.fda.sent_index.arr[matching.sent_id]
        f_sent_end = self.fda.sent_index.arr[matching.sent_id+1]
        f_sent_len = f_sent_end - f_sent_start - 1

        self.findexes1.reset()
        sofar = 0
        for i in range(num_chunks):
            for j in range(chunklen[i]):
                self.findexes1.append(matching.arr[matching.start+i]+j-f_sent_start);
                sofar += 1
            if (i+1<num_chunks):
                self.findexes1.append(phrase[sofar])
                sofar += 1
            

        e_links_low = <int*> malloc(e_sent_len*sizeof(int))
        e_links_high = <int*> malloc(e_sent_len*sizeof(int))
        f_links_low = <int*> malloc(f_sent_len*sizeof(int))
        f_links_high = <int*> malloc(f_sent_len*sizeof(int))
        f_gap_low = <int*> malloc((num_chunks+1)*sizeof(int))
        f_gap_high = <int*> malloc((num_chunks+1)*sizeof(int))
        e_gap_low = <int*> malloc((num_chunks+1)*sizeof(int))
        e_gap_high = <int*> malloc((num_chunks+1)*sizeof(int))
        memset(f_gap_low, 0, (num_chunks+1)*sizeof(int))
        memset(f_gap_high, 0, (num_chunks+1)*sizeof(int))
        memset(e_gap_low, 0, (num_chunks+1)*sizeof(int))
        memset(e_gap_high, 0, (num_chunks+1)*sizeof(int))

        reason_for_failure = ""

        for i from 0 <= i < e_sent_len:
            e_links_low[i] = -1
            e_links_high[i] = -1
        for i from 0 <= i < f_sent_len:
            f_links_low[i] = -1
            f_links_high[i] = -1

        # this is really inefficient -- might be good to 
        # somehow replace with binary search just for the f
        # links that we care about (but then how to look up 
        # when we want to check something on the e side?)
        i = 0
        while i < num_links*2:
            f_i = sent_links[i]
            e_i = sent_links[i+1]
            if f_links_low[f_i] == -1 or f_links_low[f_i] > e_i:
                f_links_low[f_i] = e_i
            if f_links_high[f_i] == -1 or f_links_high[f_i] < e_i + 1:
                f_links_high[f_i] = e_i + 1
            if e_links_low[e_i] == -1 or e_links_low[e_i] > f_i:
                e_links_low[e_i] = f_i
            if e_links_high[e_i] == -1 or e_links_high[e_i] < f_i + 1:
                e_links_high[e_i] = f_i + 1
            i = i + 2
        
        als = []
        for x in range(matching.start,matching.end):
            al = (matching.arr[x]-f_sent_start,f_links_low[matching.arr[x]-f_sent_start])
            als.append(al)
        # check all source-side alignment constraints
        met_constraints = 1
        if self.require_aligned_terminal:
            num_aligned_chunks = 0
            for i from 0 <= i < num_chunks:
                for j from 0 <= j < chunklen[i]:
                    if f_links_low[matching.arr[matching.start+i]+j-f_sent_start] > -1:
                        num_aligned_chunks = num_aligned_chunks + 1
                        break
            if num_aligned_chunks == 0:
                reason_for_failure = "No aligned terminals"
                met_constraints = 0
            if self.require_aligned_chunks and num_aligned_chunks < num_chunks:
                reason_for_failure = "Unaligned chunk"
                met_constraints = 0

        if met_constraints and self.tight_phrases:
            # outside edge constraints are checked later
            for i from 0 <= i < num_chunks-1:
                if f_links_low[matching.arr[matching.start+i]+chunklen[i]-f_sent_start] == -1:
                    reason_for_failure = "Gaps are not tight phrases"
                    met_constraints = 0
                    break
                if f_links_low[matching.arr[matching.start+i+1]-1-f_sent_start] == -1:
                    reason_for_failure = "Gaps are not tight phrases"
                    met_constraints = 0
                    break

        f_low = matching.arr[matching.start] - f_sent_start
        f_high = matching.arr[matching.start + matching.size - 1] + chunklen[num_chunks-1] - f_sent_start
        if met_constraints:

            if self.find_fixpoint(f_low, f_high, f_links_low, f_links_high, e_links_low, e_links_high, 
                                -1, -1, &e_low, &e_high, &f_back_low, &f_back_high, f_sent_len, e_sent_len,
                                self.train_max_initial_size, self.train_max_initial_size, 
                                self.train_min_gap_size, 0,
                                self.max_nonterminals - num_chunks + 1, 1, 1, 0, 0):
                gap_error = 0
                num_gaps = 0

                if f_back_low < f_low:
                    f_gap_low[0] = f_back_low
                    f_gap_high[0] = f_low
                    num_gaps = 1
                    gap_start = 0
                    phrase_len = phrase_len+1
                    if phrase_len > self.max_length:
                        gap_error = 1
                    if self.tight_phrases:
                        if f_links_low[f_back_low] == -1 or f_links_low[f_low-1] == -1:
                            gap_error = 1
                            reason_for_failure = "Inside edges of preceding subphrase are not tight"
                else:
                    gap_start = 1
                    if self.tight_phrases and f_links_low[f_low] == -1:
                        # this is not a hard error.    we can't extract this phrase
                        # but we still might be able to extract a superphrase
                        met_constraints = 0

                for i from 0 <= i < matching.size - 1:
                    f_gap_low[1+i] = matching.arr[matching.start+i] + chunklen[i] - f_sent_start
                    f_gap_high[1+i] = matching.arr[matching.start+i+1] - f_sent_start
                    num_gaps = num_gaps + 1

                if f_high < f_back_high:
                    f_gap_low[gap_start+num_gaps] = f_high
                    f_gap_high[gap_start+num_gaps] = f_back_high
                    num_gaps = num_gaps + 1
                    phrase_len = phrase_len+1
                    if phrase_len > self.max_length:
                        gap_error = 1
                    if self.tight_phrases:
                        if f_links_low[f_back_high-1] == -1 or f_links_low[f_high] == -1:
                            gap_error = 1
                            reason_for_failure = "Inside edges of following subphrase are not tight"
                else:
                    if self.tight_phrases and f_links_low[f_high-1] == -1:
                        met_constraints = 0

                if gap_error == 0:
                    e_word_count = e_high - e_low
                    for i from 0 <= i < num_gaps: # check integrity of subphrases
                        if self.find_fixpoint(f_gap_low[gap_start+i], f_gap_high[gap_start+i], 
                                            f_links_low, f_links_high, e_links_low, e_links_high,
                                            -1, -1, e_gap_low+gap_start+i, e_gap_high+gap_start+i, 
                                            f_gap_low+gap_start+i, f_gap_high+gap_start+i,
                                            f_sent_len, e_sent_len, 
                                            self.train_max_initial_size, self.train_max_initial_size, 
                                            0, 0, 0, 0, 0, 0, 0) == 0:
                            gap_error = 1
                            reason_for_failure = "Subphrase [%d, %d] failed integrity check" % (f_gap_low[gap_start+i], f_gap_high[gap_start+i])
                            break

                if gap_error == 0:
                    i = 1
                    self.findexes.reset()
                    if f_back_low < f_low:
                        fphr_arr._append(sym_setindex(self.category, i))
                        i = i+1
                        self.findexes.append(sym_setindex(self.category, i))
                    self.findexes.extend(self.findexes1)
                    for j from 0 <= j < phrase.n:
                        if sym_isvar(phrase.syms[j]):
                            fphr_arr._append(sym_setindex(self.category, i))
                            i = i + 1
                        else:
                            fphr_arr._append(phrase.syms[j])
                    if f_back_high > f_high:
                        fphr_arr._append(sym_setindex(self.category, i))
                        self.findexes.append(sym_setindex(self.category, i))

                    fphr = Phrase(fphr_arr)
                    if met_constraints:
                        phrase_list = self.extract_phrases(e_low, e_high, e_gap_low + gap_start, e_gap_high + gap_start, e_links_low, num_gaps,
                                            f_back_low, f_back_high, f_gap_low + gap_start, f_gap_high + gap_start, f_links_low,
                                            matching.sent_id, e_sent_len, e_sent_start)
                        if len(phrase_list) > 0:
                            pair_count = 1.0 / len(phrase_list)
                        else:
                            pair_count = 0
                            reason_for_failure = "Didn't extract anything from [%d, %d] -> [%d, %d]" % (f_back_low, f_back_high, e_low, e_high)
                        for (phrase2,eindexes) in phrase_list:
                            als1 = self.create_alignments(sent_links,num_links,self.findexes,eindexes)        
                            extracts.append((fphr, phrase2, pair_count, tuple(als1)))
                    if (num_gaps < self.max_nonterminals and
                        phrase_len < self.max_length and
                        f_back_high - f_back_low + self.train_min_gap_size <= self.train_max_initial_size):
                        if (f_back_low == f_low and 
                                f_low >= self.train_min_gap_size and
                                ((not self.tight_phrases) or (f_links_low[f_low-1] != -1 and f_links_low[f_back_high-1] != -1))):
                            f_x_low = f_low-self.train_min_gap_size
                            met_constraints = 1
                            if self.tight_phrases:
                                while f_x_low >= 0 and f_links_low[f_x_low] == -1:
                                    f_x_low = f_x_low - 1
                            if f_x_low < 0 or f_back_high - f_x_low > self.train_max_initial_size:
                                met_constraints = 0

                            if (met_constraints and
                                (self.find_fixpoint(f_x_low, f_back_high,
                                            f_links_low, f_links_high, e_links_low, e_links_high, 
                                            e_low, e_high, &e_x_low, &e_x_high, &f_x_low, &f_x_high, 
                                            f_sent_len, e_sent_len, 
                                            self.train_max_initial_size, self.train_max_initial_size, 
                                            1, 1, 1, 1, 0, 1, 0) == 1) and
                                ((not self.tight_phrases) or f_links_low[f_x_low] != -1) and
                                self.find_fixpoint(f_x_low, f_low,    # check integrity of new subphrase
                                            f_links_low, f_links_high, e_links_low, e_links_high,
                                            -1, -1, e_gap_low, e_gap_high, f_gap_low, f_gap_high, 
                                            f_sent_len, e_sent_len,
                                            self.train_max_initial_size, self.train_max_initial_size,
                                            0, 0, 0, 0, 0, 0, 0)):
                                fphr_arr._clear()
                                i = 1
                                self.findexes.reset()
                                self.findexes.append(sym_setindex(self.category, i))
                                fphr_arr._append(sym_setindex(self.category, i))
                                i = i+1
                                self.findexes.extend(self.findexes1)
                                for j from 0 <= j < phrase.n:
                                    if sym_isvar(phrase.syms[j]):
                                        fphr_arr._append(sym_setindex(self.category, i))
                                        i = i + 1
                                    else:
                                        fphr_arr._append(phrase.syms[j])
                                if f_back_high > f_high:
                                    fphr_arr._append(sym_setindex(self.category, i))
                                    self.findexes.append(sym_setindex(self.category, i))
                                fphr = Phrase(fphr_arr)
                                phrase_list = self.extract_phrases(e_x_low, e_x_high, e_gap_low, e_gap_high, e_links_low, num_gaps+1,
                                                    f_x_low, f_x_high, f_gap_low, f_gap_high, f_links_low, matching.sent_id, 
                                                    e_sent_len, e_sent_start)
                                if len(phrase_list) > 0:
                                    pair_count = 1.0 / len(phrase_list)
                                else:
                                    pair_count = 0
                                for phrase2,eindexes in phrase_list:
                                    als2 = self.create_alignments(sent_links,num_links,self.findexes,eindexes)        
                                    extracts.append((fphr, phrase2, pair_count, tuple(als2)))

                        if (f_back_high == f_high and 
                            f_sent_len - f_high >= self.train_min_gap_size and
                            ((not self.tight_phrases) or (f_links_low[f_high] != -1 and f_links_low[f_back_low] != -1))):
                            f_x_high = f_high+self.train_min_gap_size
                            met_constraints = 1
                            if self.tight_phrases:
                                while f_x_high <= f_sent_len and f_links_low[f_x_high-1] == -1:
                                    f_x_high = f_x_high + 1
                            if f_x_high > f_sent_len or f_x_high - f_back_low > self.train_max_initial_size:
                                met_constraints = 0
                            
                            if (met_constraints and 
                                self.find_fixpoint(f_back_low, f_x_high, 
                                            f_links_low, f_links_high, e_links_low, e_links_high,
                                            e_low, e_high, &e_x_low, &e_x_high, &f_x_low, &f_x_high, 
                                            f_sent_len, e_sent_len, 
                                            self.train_max_initial_size, self.train_max_initial_size, 
                                            1, 1, 1, 0, 1, 1, 0) and
                                ((not self.tight_phrases) or f_links_low[f_x_high-1] != -1) and
                                self.find_fixpoint(f_high, f_x_high,
                                            f_links_low, f_links_high, e_links_low, e_links_high,
                                            -1, -1, e_gap_low+gap_start+num_gaps, e_gap_high+gap_start+num_gaps, 
                                            f_gap_low+gap_start+num_gaps, f_gap_high+gap_start+num_gaps, 
                                            f_sent_len, e_sent_len,
                                            self.train_max_initial_size, self.train_max_initial_size,
                                            0, 0, 0, 0, 0, 0, 0)):
                                fphr_arr._clear()
                                i = 1
                                self.findexes.reset()
                                if f_back_low < f_low:
                                    fphr_arr._append(sym_setindex(self.category, i))
                                    i = i+1
                                    self.findexes.append(sym_setindex(self.category, i))
                                self.findexes.extend(self.findexes1)
                                for j from 0 <= j < phrase.n:
                                    if sym_isvar(phrase.syms[j]):
                                        fphr_arr._append(sym_setindex(self.category, i))
                                        i = i + 1
                                    else:
                                        fphr_arr._append(phrase.syms[j])
                                fphr_arr._append(sym_setindex(self.category, i))
                                self.findexes.append(sym_setindex(self.category, i))
                                fphr = Phrase(fphr_arr)
                                phrase_list = self.extract_phrases(e_x_low, e_x_high, e_gap_low+gap_start, e_gap_high+gap_start, e_links_low, num_gaps+1,
                                                    f_x_low, f_x_high, f_gap_low+gap_start, f_gap_high+gap_start, f_links_low, 
                                                    matching.sent_id, e_sent_len, e_sent_start)
                                if len(phrase_list) > 0:
                                    pair_count = 1.0 / len(phrase_list)
                                else:
                                    pair_count = 0
                                for phrase2, eindexes in phrase_list:
                                    als3 = self.create_alignments(sent_links,num_links,self.findexes,eindexes)        
                                    extracts.append((fphr, phrase2, pair_count, tuple(als3)))
                        if (num_gaps < self.max_nonterminals - 1 and 
                            phrase_len+1 < self.max_length and
                            f_back_high == f_high and 
                            f_back_low == f_low and 
                            f_back_high - f_back_low + (2*self.train_min_gap_size) <= self.train_max_initial_size and
                            f_low >= self.train_min_gap_size and
                            f_high <= f_sent_len - self.train_min_gap_size and
                            ((not self.tight_phrases) or (f_links_low[f_low-1] != -1 and f_links_low[f_high] != -1))):

                            met_constraints = 1
                            f_x_low = f_low-self.train_min_gap_size
                            if self.tight_phrases:
                                while f_x_low >= 0 and f_links_low[f_x_low] == -1:
                                    f_x_low = f_x_low - 1
                            if f_x_low < 0:
                                met_constraints = 0

                            f_x_high = f_high+self.train_min_gap_size
                            if self.tight_phrases:
                                while f_x_high <= f_sent_len and f_links_low[f_x_high-1] == -1:
                                    f_x_high = f_x_high + 1
                            if f_x_high > f_sent_len or f_x_high - f_x_low > self.train_max_initial_size:
                                met_constraints = 0

                            if (met_constraints and
                                (self.find_fixpoint(f_x_low, f_x_high,
                                                f_links_low, f_links_high, e_links_low, e_links_high,
                                                e_low, e_high, &e_x_low, &e_x_high, &f_x_low, &f_x_high, 
                                                f_sent_len, e_sent_len,
                                                self.train_max_initial_size, self.train_max_initial_size, 
                                                1, 1, 2, 1, 1, 1, 1) == 1) and
                                ((not self.tight_phrases) or (f_links_low[f_x_low] != -1 and f_links_low[f_x_high-1] != -1)) and
                                self.find_fixpoint(f_x_low, f_low,
                                                f_links_low, f_links_high, e_links_low, e_links_high,
                                                -1, -1, e_gap_low, e_gap_high, f_gap_low, f_gap_high, 
                                                f_sent_len, e_sent_len,
                                                self.train_max_initial_size, self.train_max_initial_size,
                                                0, 0, 0, 0, 0, 0, 0) and
                                self.find_fixpoint(f_high, f_x_high,
                                                f_links_low, f_links_high, e_links_low, e_links_high,
                                                -1, -1, e_gap_low+1+num_gaps, e_gap_high+1+num_gaps, 
                                                f_gap_low+1+num_gaps, f_gap_high+1+num_gaps, 
                                                f_sent_len, e_sent_len,
                                                self.train_max_initial_size, self.train_max_initial_size,
                                                0, 0, 0, 0, 0, 0, 0)):
                                fphr_arr._clear()
                                i = 1
                                self.findexes.reset()
                                self.findexes.append(sym_setindex(self.category, i))
                                fphr_arr._append(sym_setindex(self.category, i))
                                i = i+1
                                self.findexes.extend(self.findexes1)
                                for j from 0 <= j < phrase.n:
                                    if sym_isvar(phrase.syms[j]):
                                        fphr_arr._append(sym_setindex(self.category, i))
                                        i = i + 1
                                    else:
                                        fphr_arr._append(phrase.syms[j])
                                fphr_arr._append(sym_setindex(self.category, i))
                                self.findexes.append(sym_setindex(self.category, i))
                                fphr = Phrase(fphr_arr)
                                phrase_list = self.extract_phrases(e_x_low, e_x_high, e_gap_low, e_gap_high, e_links_low, num_gaps+2,
                                                    f_x_low, f_x_high, f_gap_low, f_gap_high, f_links_low, 
                                                    matching.sent_id, e_sent_len, e_sent_start)
                                if len(phrase_list) > 0:
                                    pair_count = 1.0 / len(phrase_list)
                                else:
                                    pair_count = 0
                                for phrase2, eindexes in phrase_list:
                                    als4 = self.create_alignments(sent_links,num_links,self.findexes,eindexes)        
                                    extracts.append((fphr, phrase2, pair_count, tuple(als4)))
            else:
                reason_for_failure = "Unable to extract basic phrase"

        free(sent_links)
        free(f_links_low)
        free(f_links_high)
        free(e_links_low)
        free(e_links_high)
        free(f_gap_low)
        free(f_gap_high)
        free(e_gap_low)
        free(e_gap_high)

        return extracts

    #
    # Online grammar extraction handling
    #
    
    # Aggregate stats from a training instance
    # (Extract rules, update counts)
    def add_instance(self, f_words, e_words, alignment):

        self.online = True
        
        # Rules extracted from this instance
        # Track span of lexical items (terminals) to make
        # sure we don't extract the same rule for the same
        # span more than once.
        # (f, e, al, lex_f_i, lex_f_j)
        rules = set()

        f_len = len(f_words)
        e_len = len(e_words)

        # Pre-compute alignment info
        al = [[] for i in range(f_len)]
        fe_span = [[e_len + 1, -1] for i in range(f_len)]
        ef_span = [[f_len + 1, -1] for i in range(e_len)]
        for (f, e) in alignment:
            al[f].append(e)
            fe_span[f][0] = min(fe_span[f][0], e)
            fe_span[f][1] = max(fe_span[f][1], e)
            ef_span[e][0] = min(ef_span[e][0], f)
            ef_span[e][1] = max(ef_span[e][1], f)

        # Target side word coverage
        cover = [0] * e_len
        # Non-terminal coverage
        f_nt_cover = [0] * f_len
        e_nt_cover = [0] * e_len
        
        # Extract all possible hierarchical phrases starting at a source index
        # f_ i and j are current, e_ i and j are previous
        # We care _considering_ f_j, so it is not yet in counts
        def extract(f_i, f_j, e_i, e_j, min_bound, wc, links, nt, nt_open):
            # Phrase extraction limits
            if f_j > (f_len - 1) or (f_j - f_i) + 1 > self.max_initial_size:
                return
            # Unaligned word
            if not al[f_j]:
                # Adjacent to non-terminal: extend (non-terminal now open)
                if nt and nt[-1][2] == f_j - 1:
                    nt[-1][2] += 1
                    extract(f_i, f_j + 1, e_i, e_j, min_bound, wc, links, nt, True)
                    nt[-1][2] -= 1
                # Unless non-terminal already open, always extend with word
                # Make sure adding a word doesn't exceed length
                if not nt_open and wc < self.max_length:
                    extract(f_i, f_j + 1, e_i, e_j, min_bound, wc + 1, links, nt, False)
                return
            # Aligned word
            link_i = fe_span[f_j][0]
            link_j = fe_span[f_j][1]
            new_e_i = min(link_i, e_i)
            new_e_j = max(link_j, e_j)
            # Check reverse links of newly covered words to see if they violate left
            # bound (return) or extend minimum right bound for chunk
            new_min_bound = min_bound
            # First aligned word creates span
            if e_j == -1: 
                for i from new_e_i <= i <= new_e_j:
                    if ef_span[i][0] < f_i:
                        return
                    new_min_bound = max(new_min_bound, ef_span[i][1])
            # Other aligned words extend span
            else:
                for i from new_e_i <= i < e_i:
                    if ef_span[i][0] < f_i:
                        return
                    new_min_bound = max(new_min_bound, ef_span[i][1])
                for i from e_j < i <= new_e_j:
                    if ef_span[i][0] < f_i:
                        return
                    new_min_bound = max(new_min_bound, ef_span[i][1])
            # Extract, extend with word (unless non-terminal open)
            if not nt_open:
                nt_collision = False
                for link in al[f_j]:
                    if e_nt_cover[link]:
                        nt_collision = True
                # Non-terminal collisions block word extraction and extension, but
                # may be okay for continuing non-terminals
                if not nt_collision and wc < self.max_length:
                    plus_links = []
                    for link in al[f_j]:
                        plus_links.append((f_j, link))
                        cover[link] += 1
                    links.append(plus_links)
                    if links and f_j >= new_min_bound:
                        rules.add(self.form_rule(f_i, new_e_i, f_words[f_i:f_j + 1], e_words[new_e_i:new_e_j + 1], nt, links))
                    extract(f_i, f_j + 1, new_e_i, new_e_j, new_min_bound, wc + 1, links, nt, False)
                    links.pop()
                    for link in al[f_j]:
                        cover[link] -= 1
            # Try to add a word to current non-terminal (if any), extract, extend
            if nt and nt[-1][2] == f_j - 1:
                # Add to non-terminal, checking for collisions
                old_last_nt = nt[-1][:]
                nt[-1][2] = f_j
                if link_i < nt[-1][3]:
                    if not span_check(cover, link_i, nt[-1][3] - 1):
                        nt[-1] = old_last_nt
                        return
                    span_inc(cover, link_i, nt[-1][3] - 1)
                    span_inc(e_nt_cover, link_i, nt[-1][3] - 1)
                    nt[-1][3] = link_i
                if link_j > nt[-1][4]:
                    if not span_check(cover, nt[-1][4] + 1, link_j):
                        nt[-1] = old_last_nt
                        return
                    span_inc(cover, nt[-1][4] + 1, link_j)
                    span_inc(e_nt_cover, nt[-1][4] + 1, link_j)
                    nt[-1][4] = link_j
                if links and f_j >= new_min_bound:
                    rules.add(self.form_rule(f_i, new_e_i, f_words[f_i:f_j + 1], e_words[new_e_i:new_e_j + 1], nt, links))
                extract(f_i, f_j + 1, new_e_i, new_e_j, new_min_bound, wc, links, nt, False)
                nt[-1] = old_last_nt
                if link_i < nt[-1][3]:
                    span_dec(cover, link_i, nt[-1][3] - 1)
                    span_dec(e_nt_cover, link_i, nt[-1][3] - 1)
                if link_j > nt[-1][4]:
                    span_dec(cover, nt[-1][4] + 1, link_j)
                    span_dec(e_nt_cover, nt[-1][4] + 1, link_j)
            # Try to start a new non-terminal, extract, extend
            if (not nt or f_j - nt[-1][2] > 1) and wc < self.max_length and len(nt) < self.max_nonterminals:
                # Check for collisions
                if not span_check(cover, link_i, link_j):
                    return
                span_inc(cover, link_i, link_j)
                span_inc(e_nt_cover, link_i, link_j)
                nt.append([(nt[-1][0] + 1) if nt else 1, f_j, f_j, link_i, link_j])
                # Require at least one word in phrase
                if links and f_j >= new_min_bound:
                    rules.add(self.form_rule(f_i, new_e_i, f_words[f_i:f_j + 1], e_words[new_e_i:new_e_j + 1], nt, links))
                extract(f_i, f_j + 1, new_e_i, new_e_j, new_min_bound, wc + 1, links, nt, False)
                nt.pop()
                span_dec(cover, link_i, link_j)
                span_dec(e_nt_cover, link_i, link_j)

        # Try to extract phrases from every f index
        for f_i from 0 <= f_i < f_len:
            # Skip if phrases won't be tight on left side
            if not al[f_i]:
                continue
            extract(f_i, f_i, f_len + 1, -1, f_i, 0, [], [], False)
        
        # Update possible phrases (samples)
        # This could be more efficiently integrated with extraction
        # at the cost of readability
        for (f, lex_i, lex_j) in self.get_f_phrases(f_words):
            self.samples_f[f] += 1
            
        # Update phrase counts
        for rule in rules:
            (f_ph, e_ph, al) = rule[:3]
            self.phrases_f[f_ph] += 1
            self.phrases_e[e_ph] += 1
            self.phrases_fe[f_ph][e_ph] += 1
            if not self.phrases_al[f_ph][e_ph]:
                self.phrases_al[f_ph][e_ph] = al
            
        # Update Bilexical counts
        for e_w in e_words:
            self.bilex_e[e_w] += 1
        for f_w in f_words:
            self.bilex_f[f_w] += 1
            for e_w in e_words:
                self.bilex_fe[f_w][e_w] += 1

    # Create a rule from source, target, non-terminals, and alignments
    def form_rule(self, f_i, e_i, f_span, e_span, nt, al):
    
        # Substitute in non-terminals
        nt_inv = sorted(nt, cmp=lambda x, y: cmp(x[3], y[3]))
        f_sym = list(f_span[:])
        off = f_i
        for next_nt in nt:
            nt_len = (next_nt[2] - next_nt[1]) + 1
            i = 0
            while i < nt_len:
                f_sym.pop(next_nt[1] - off)
                i += 1
            f_sym.insert(next_nt[1] - off, sym_setindex(self.category, next_nt[0]))
            off += (nt_len - 1)
        e_sym = list(e_span[:])
        off = e_i
        for next_nt in nt_inv:
            nt_len = (next_nt[4] - next_nt[3]) + 1
            i = 0
            while i < nt_len:
                e_sym.pop(next_nt[3] - off)
                i += 1
            e_sym.insert(next_nt[3] - off, sym_setindex(self.category, next_nt[0]))
            off += (nt_len - 1)
    
        # Adjusting alignment links takes some doing
        links = [list(link) for sub in al for link in sub]
        links_inv = sorted(links, cmp=lambda x, y: cmp(x[1], y[1]))
        links_len = len(links)
        nt_len = len(nt)
        nt_i = 0
        off = f_i
        i = 0
        while i < links_len:
            while nt_i < nt_len and links[i][0] > nt[nt_i][1]:
                off += (nt[nt_i][2] - nt[nt_i][1])
                nt_i += 1
            links[i][0] -= off
            i += 1
        nt_i = 0
        off = e_i
        i = 0
        while i < links_len:
            while nt_i < nt_len and links_inv[i][1] > nt_inv[nt_i][3]:
                off += (nt_inv[nt_i][4] - nt_inv[nt_i][3])
                nt_i += 1
            links_inv[i][1] -= off
            i += 1
        
        # Find lexical span
        lex_f_i = f_i
        lex_f_j = f_i + (len(f_span) - 1)
        if nt:
            if nt[0][1] == lex_f_i:
                lex_f_i += (nt[0][2] - nt[0][1]) + 1
            if nt[-1][2] == lex_f_j:
                lex_f_j -= (nt[-1][2] - nt[-1][1]) + 1

        # Create rule (f_phrase, e_phrase, links, f_link_min, f_link_max)
        f = Phrase(f_sym)
        e = Phrase(e_sym)
        a = tuple(self.alignment.link(i, j) for (i, j) in links)
        return (f, e, a, lex_f_i, lex_f_j)

    # Rule string from rule
    def fmt_rule(self, f, e, a):
        a_str = ' '.join('{0}-{1}'.format(*self.alignment.unlink(packed)) for packed in a)
        return '[X] ||| {0} ||| {1} ||| {2}'.format(f, e, a_str)
    
    # Debugging
    def dump_online_stats(self):
        logger.info('------------------------------')
        logger.info('         Online Stats         ')
        logger.info('------------------------------')
        logger.info('f')
        for w in self.bilex_f:
            logger.info(sym_tostring(w) + ' : ' + str(self.bilex_f[w]))
        logger.info('e')
        for w in self.bilex_e:
            logger.info(sym_tostring(w) + ' : ' + str(self.bilex_e[w]))
        logger.info('fe')
        for w in self.bilex_fe:
            for w2 in self.bilex_fe[w]:
                logger.info(sym_tostring(w) + ' : ' + sym_tostring(w2) + ' : ' + str(self.bilex_fe[w][w2]))
        logger.info('F')
        for ph in self.phrases_f:
            logger.info(str(ph) + ' ||| ' + str(self.phrases_f[ph]))
        logger.info('E')
        for ph in self.phrases_e:
            logger.info(str(ph) + ' ||| ' + str(self.phrases_e[ph]))
        logger.info('FE')
        self.dump_online_rules()

    def dump_online_rules(self):
        for ph in self.phrases_fe:
            for ph2 in self.phrases_fe[ph]:
                logger.info(self.fmt_rule(str(ph), str(ph2), self.phrases_al[ph][ph2]) + ' ||| ' + str(self.phrases_fe[ph][ph2]))
                    
    # Lookup online stats for phrase pair (f, e).  Return None if no match.
    # IMPORTANT: use get() to avoid adding items to defaultdict
    def online_ctx_lookup(self, f, e):
        if self.online:
            fcount = self.phrases_f.get(f, 0)
            fsample_count = self.samples_f.get(f, 0)
            d = self.phrases_fe.get(f, None)
            paircount = d.get(e, 0) if d else 0
            return OnlineFeatureContext(fcount, fsample_count, paircount, self.bilex_f, self.bilex_e, self.bilex_fe)
        return None
    
    # Find all phrases that we might try to extract
    # (Used for EGivenFCoherent)
    # Return set of (fphrase, lex_i, lex_j)
    def get_f_phrases(self, f_words):

        f_len = len(f_words)
        phrases = set() # (fphrase, lex_i, lex_j)
        
        def extract(f_i, f_j, lex_i, lex_j, wc, ntc, syms):
            # Phrase extraction limits
            if f_j > (f_len - 1) or (f_j - f_i) + 1 > self.max_initial_size:
                return
            # Extend with word
            if wc + ntc < self.max_length:
                syms.append(f_words[f_j])
                f = Phrase(syms)
                new_lex_i = min(lex_i, f_j)
                new_lex_j = max(lex_j, f_j)
                phrases.add((f, new_lex_i, new_lex_j))
                extract(f_i, f_j + 1, new_lex_i, new_lex_j, wc + 1, ntc, syms)
                syms.pop()
            # Extend with existing non-terminal
            if syms and sym_isvar(syms[-1]):
                # Don't re-extract the same phrase
                extract(f_i, f_j + 1, lex_i, lex_j, wc, ntc, syms)
            # Extend with new non-terminal
            if wc + ntc < self.max_length:
                if not syms or (ntc < self.max_nonterminals and not sym_isvar(syms[-1])):
                    syms.append(sym_setindex(self.category, ntc + 1))
                    f = Phrase(syms)
                    if wc > 0:
                        phrases.add((f, lex_i, lex_j))
                    extract(f_i, f_j + 1, lex_i, lex_j, wc, ntc + 1, syms)
                    syms.pop()
            
        # Try to extract phrases from every f index
        for f_i from 0 <= f_i < f_len:
            extract(f_i, f_i, f_len, -1, 0, 0, [])

        return phrases
    
# Spans are _inclusive_ on both ends [i, j]
def span_check(vec, i, j):
    k = i
    while k <= j:
        if vec[k]:
            return False
        k += 1
    return True

def span_inc(vec, i, j):
    k = i
    while k <= j:
        vec[k] += 1
        k += 1

def span_dec(vec, i, j):
    k = i
    while k <= j:
        vec[k] -= 1
        k += 1
