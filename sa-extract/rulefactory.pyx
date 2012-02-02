# Pyrex implementation of the algorithms described in 
# Lopez, EMNLP-CoNLL 2007
# Much faster than the Python numbers reported there.
# Note to reader: this code is closer to C than Python
import sys
import sym
import log
import rule
import monitor
import cintlist
import csuf
import cdat
import cveb
import precomputation
import gc
import cn
import sgml

cimport cmath
cimport csuf
cimport cdat
cimport cintlist
cimport rule
cimport cveb
cimport precomputation
cimport calignment

from libc.stdlib cimport malloc, realloc, free
from libc.string cimport memset, memcpy
from libc.math cimport fmod, ceil, floor

cdef int PRECOMPUTE
cdef int MERGE
cdef int BAEZA_YATES

PRECOMPUTE = 0
MERGE = 1
BAEZA_YATES = 2

#cdef int node_count
#node_count = 0

cdef class TrieNode:
  cdef public children
  #cdef int id

  def __init__(self):
    self.children = {}
    #self.id = node_count
    #node_count += 1


cdef class ExtendedTrieNode(TrieNode):
  cdef public phrase
  cdef public phrase_location
  cdef public suffix_link

  def __init__(self, phrase=None, phrase_location=None, suffix_link=None):
    TrieNode.__init__(self)
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

cdef class BaselineRuleFactory:

  cdef grammar, context_manager
  cdef int max_terminals, max_nonterminals
  cdef int max_initial_size, train_max_initial_size
  cdef int min_gap_size, train_min_gap_size
  cdef int category
  cdef int visit
  cdef float intersect_time, extract_time
  cdef ruleFile, timingFile
  cdef int* last_visit1
  cdef int* last_visit2
  cdef match_node** intersector1
  cdef match_node** intersector2
  cdef csuf.SuffixArray sa
  cdef cintlist.CIntList sent_id

  def input(self, fwords):
    flen = len(fwords)
    start_time = monitor.cpu()
    self.intersect_time = 0.0
    self.extract_time = 0.0

    pyro_phrase_count = 0
    hiero_phrase_count = 0

    frontier = []
    for i in xrange(len(fwords)):
      frontier.append((i, (), False))

    while len(frontier) > 0:
      this_iter_intersect_time = self.intersect_time
      new_frontier = []
      for i, prefix, is_shadow_path in frontier:

        word_id = fwords[i][0][0]
        #print "word_id = %i" % word_id
        phrase = prefix + (word_id,)
        str_phrase = map(sym.tostring, phrase)
        hiero_phrase = rule.Phrase(phrase)

        #log.writeln("pos %2d, '%s'" % (i, hiero_phrase))
        self.lookup(hiero_phrase)
        if hiero_phrase.arity() == 0:
          pyro_phrase_count = pyro_phrase_count + 1
        else:
          hiero_phrase_count = hiero_phrase_count + 1

        if len(phrase) - hiero_phrase.arity() < self.max_terminals and i+1 < len(fwords):
          new_frontier.append((i+1, phrase, is_shadow_path))
          if hiero_phrase.arity() < self.max_nonterminals:
            xcat = sym.setindex(self.category, hiero_phrase.arity()+1)
            for j in xrange(i+1+self.min_gap_size, min(i+self.max_initial_size, len(fwords))):
              new_frontier.append((j, phrase+(xcat,), is_shadow_path))
      log.writeln("This iteration intersect time = %f" % (self.intersect_time - this_iter_intersect_time))
      frontier = new_frontier
    stop_time = monitor.cpu()
    log.writeln("COUNT %d %d" % (pyro_phrase_count, hiero_phrase_count))


  def lookup(self, phrase):
    cdef int j, g, start, stop, sent_id, num_ranges, arity
    cdef match_node** cur_intersector
    cdef match_node** next_intersector
    cdef match_node** tmp_intersector
    cdef match_node* node
    cdef match_node* cur_node
    cdef match_node* prev_node
    cdef match_node** node_ptr
    cdef int* cur_visit
    cdef int* next_visit
    cdef int* tmp_visit
    cdef int* chunklen
    
    #print "\n\nLOOKUP\n\n"
    ranges = []
    sizes = []
    arity = phrase.arity()
    chunklen = <int *> malloc(arity*sizeof(int))
    for i from 0 <= i < arity+1:
      chunk = phrase.getchunk(i)
      chunklen[i] = len(chunk)
      sa_range = None
      phr = ()
      for offset, word_id in enumerate(chunk):
        word = sym.tostring(word_id)
        sa_range = self.context_manager.fsarray.lookup(word, offset, sa_range[0], sa_range[1])
      if sa_range is None:
        #log.writeln("Returned for phrase %s" % rule.Phrase(phr))
        return
      #log.writeln("Found range %s for phrase %s" % (sa_range, rule.Phrase(phr)))
      ranges.append(sa_range)
      sizes.append(sa_range[1]-sa_range[0])
    if phrase.arity() == 0:
      return

    cur_intersector = self.intersector1
    next_intersector = self.intersector2
    cur_visit = self.last_visit1
    next_visit = self.last_visit2

    num_ranges = len(ranges)
    for i from 0 <= i < num_ranges:
      sa_range = ranges[i]
      start_time = monitor.cpu()
      self.visit = self.visit + 1
      intersect_count = 0

      start = sa_range[0]
      stop = sa_range[1]
      for j from start <= j < stop:
        g = self.sa.sa.arr[j]
        sent_id = self.sent_id.arr[g]
        if i==0:
          if next_visit[sent_id] != self.visit:
            # clear intersector
            node = next_intersector[sent_id]
            next_intersector[sent_id] = NULL
            while node != NULL:
              prev_node = node
              node = node.next
              free(prev_node.match)
              free(prev_node)
            next_visit[sent_id] = self.visit
          node_ptr = &(next_intersector[sent_id])
          while node_ptr[0] != NULL:
            node_ptr = &(node_ptr[0].next)
          node_ptr[0] = <match_node*> malloc(sizeof(match_node))
          node_ptr[0].match = <int *> malloc(sizeof(int))
          node_ptr[0].match[0] = g
          node_ptr[0].next = NULL
          intersect_count = intersect_count + 1
        else:
          if cur_visit[sent_id] == self.visit-1:
            cur_node = cur_intersector[sent_id]
            while cur_node != NULL:
              if g - cur_node.match[0] + chunklen[i] <= self.train_max_initial_size and g - cur_node.match[i-1] - chunklen[i-1] >= self.train_min_gap_size:
                if next_visit[sent_id] != self.visit:
                  # clear intersector -- note that we only do this if we've got something to put there
                  node = next_intersector[sent_id]
                  next_intersector[sent_id] = NULL
                  while node != NULL:
                    prev_node = node
                    node = node.next
                    free(prev_node.match)
                    free(prev_node)
                  next_visit[sent_id] = self.visit
                node_ptr = &(next_intersector[sent_id])
                while node_ptr[0] != NULL:
                  node_ptr = &(node_ptr[0].next)
                node_ptr[0] = <match_node*> malloc(sizeof(match_node))
                node_ptr[0].match = <int *> malloc((i+1) * sizeof(int))
                memcpy(node_ptr[0].match, cur_node.match, i*sizeof(int))
                node_ptr[0].match[i] = g
                node_ptr[0].next = NULL
                intersect_count = intersect_count + 1
              cur_node = cur_node.next
      tmp_intersector = cur_intersector
      cur_intersector = next_intersector
      next_intersector = tmp_intersector
      
      tmp_visit = cur_visit
      cur_visit = next_visit
      next_visit = tmp_visit

      intersect_time = monitor.cpu() - start_time
      if i > 0:
        log.writeln("INT %d %d %d %d %f baseline" % 
          (arity, prev_intersect_count, sa_range[1]-sa_range[0], 
          intersect_count, intersect_time))
      if intersect_count == 0:
        return None
      prev_intersect_count = intersect_count
    free(chunklen)



  def __init__(self, max_terminals=5, max_nonterminals=2, 
        max_initial_size=10, train_max_initial_size=10,
        min_gap_size=1, train_min_gap_size=2, 
        category='[PHRASE]', grammar=None,
        ruleFile=None, timingFile=None):
    self.grammar = grammar
    self.max_terminals = max_terminals
    self.max_nonterminals = max_nonterminals 
    self.max_initial_size = max_initial_size
    self.train_max_initial_size = train_max_initial_size
    self.min_gap_size = min_gap_size
    self.train_min_gap_size = train_min_gap_size
    self.category = sym.fromstring(category)
    self.ruleFile = ruleFile
    self.timingFile = timingFile
    self.visit = 0


  def registerContext(self, context_manager):
    cdef int num_sents
    self.context_manager = context_manager
    self.sa = context_manager.fsarray
    self.sent_id = self.sa.darray.sent_id
    
    num_sents = len(self.sa.darray.sent_index)
    self.last_visit1 = <int *> malloc(num_sents * sizeof(int))
    memset(self.last_visit1, 0, num_sents * sizeof(int))
    
    self.last_visit2 = <int *> malloc(num_sents * sizeof(int))
    memset(self.last_visit2, 0, num_sents * sizeof(int))

    self.intersector1 = <match_node **> malloc(num_sents * sizeof(match_node*))
    memset(self.intersector1, 0, num_sents * sizeof(match_node*))

    self.intersector2 = <match_node **> malloc(num_sents * sizeof(match_node*))
    memset(self.intersector2, 0, num_sents * sizeof(match_node*))


# encodes information needed to find a (hierarchical) phrase
# in the text.  If phrase is contiguous, that's just a range
# in the suffix array; if discontiguous, it is the set of 
# actual locations (packed into an array)
cdef class PhraseLocation:
  cdef int sa_low
  cdef int sa_high
  cdef int arr_low
  cdef int arr_high
  cdef cintlist.CIntList arr
  cdef int num_subpatterns

  # returns true if sent_id is contained
  cdef int contains(self, int sent_id):
    return 1

  def __init__(self, sa_low=-1, sa_high=-1, arr_low=-1, arr_high=-1, arr=None, num_subpatterns=1):
    self.sa_low = sa_low
    self.sa_high = sa_high
    self.arr_low = arr_low
    self.arr_high = arr_high
    self.arr = arr
    self.num_subpatterns = num_subpatterns



cdef class Sampler:
  '''A Sampler implements a logic for choosing
  samples from a population range'''

  cdef int sampleSize
  cdef context_manager
  cdef cintlist.CIntList sa

  def __init__(self, sampleSize=0):
    self.sampleSize = sampleSize
    if sampleSize > 0:
      log.writeln("Sampling strategy: uniform, max sample size = %d" % sampleSize, 1)
    else:
      log.writeln("Sampling strategy: no sampling", 1)

  def registerContext(self, context_manager):
    self.context_manager = context_manager
    self.sa = (<csuf.SuffixArray> context_manager.fsarray).sa


  def sample(self, PhraseLocation phrase_location):
    '''Returns a sample of the locations for
    the phrase.  If there are less than self.sampleSize
    locations, return all of them; otherwise, return
    up to self.sampleSize locations.  In the latter case,
    we choose to sample UNIFORMLY -- that is, the locations
    are chosen at uniform intervals over the entire set, rather
    than randomly.  This makes the algorithm deterministic, which
    is good for things like MERT'''
    cdef cintlist.CIntList sample
    cdef double i, stepsize
    cdef int num_locations, val, j

    sample = cintlist.CIntList()
    if phrase_location.arr is None:
      num_locations = phrase_location.sa_high - phrase_location.sa_low
      if self.sampleSize == -1 or num_locations <= self.sampleSize:
        sample._extend_arr(self.sa.arr + phrase_location.sa_low, num_locations)
      else:
        stepsize = float(num_locations)/float(self.sampleSize)
        i = phrase_location.sa_low
        while i < phrase_location.sa_high and sample.len < self.sampleSize:
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
      if self.sampleSize == -1 or num_locations <= self.sampleSize:
        sample = phrase_location.arr
      else:
        stepsize = float(num_locations)/float(self.sampleSize)
        i = phrase_location.arr_low
        while i < num_locations and sample.len < self.sampleSize * phrase_location.num_subpatterns:
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


cdef long nGramCount(PhraseLocation loc):
  return (loc.arr_high - loc.arr_low)/ loc.num_subpatterns


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


#cdef matching2str(Matching* m):
#  cdef int i
#  cdef result

#  result = "("
#  for i from m.start <= i < m.end:
#    result = result + str(m.arr[i]) + " "
#  result = result + ")"
#  return result


cdef int median(int low, int high, int step):
  return low + (((high - low)/step)/2)*step


cdef void findComparableMatchings(int low, int high, int* arr, int step, int loc, int* loc_minus, int* loc_plus):
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

  cdef rules, grammar, context_manager

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

  cdef cacheBetweenSents
  cdef precomputed_index
  cdef precomputed_collocations
  cdef precompute_file
  cdef max_rank
  cdef int precompute_rank, precompute_secondary_rank
  cdef useBaezaYates
  cdef use_index
  cdef use_collocations
  cdef float by_slack_factor

  cdef per_sentence_grammar
  cdef rule_filehandler
  cdef rule_file
  cdef pruned_rule_file
  cdef extract_file
  cdef sample_file
  cdef search_file
  cdef timingFile
  cdef log_int_stats
  cdef prev_norm_prefix
  cdef float intersect_time, extract_time
  cdef csuf.SuffixArray fsa
  cdef cdat.DataArray fda
  cdef cdat.DataArray eda
  
  cdef calignment.Alignment alignment
  cdef cintlist.CIntList eid2symid
  cdef cintlist.CIntList fid2symid
  cdef int tight_phrases
  cdef int require_aligned_terminal
  cdef int require_aligned_chunks

  cdef cintlist.CIntList findexes
  cdef cintlist.CIntList findexes1

  cdef int excluded_sent_id       # exclude a sentence id

  def __init__(self, 
        alignment=None,           # compiled alignment object (REQUIRED)
        by_slack_factor=1.0,      # parameter for double-binary search; doesn't seem to matter much
        category="[PHRASE]",      # name of generic nonterminal used by Hiero
        cacheBetweenSents=False,  # prevent flushing of tree between sents; use carefully or you'll run out of memory
        extract_file=None,        # print raw extracted rules to this file
        grammar=None,             # empty grammar object -- must be supplied from outside (REQUIRED)
        log_int_stats=False,      # prints timing data on intersections to stderr
        max_chunks=None,          # maximum number of contiguous chunks of terminal symbols in RHS of a rule. If None, defaults to max_nonterminals+1
        max_initial_size=10,      # maximum span of a grammar rule in TEST DATA
        max_length=5,             # maximum number of symbols (both T and NT) allowed in a rule
        max_nonterminals=2,       # maximum number of nonterminals allowed in a rule (set >2 at your own risk)
        max_target_chunks=None,   # maximum number of contiguous chunks of terminal symbols in target-side RHS of a rule. If None, defaults to max_nonterminals+1
        max_target_length=None,   # maximum number of target side symbols (both T and NT) allowed in a rule. If None, defaults to max_initial_size
        min_gap_size=2,           # minimum span of a nonterminal in the RHS of a rule in TEST DATA
        precompute_file=None,     # filename of file containing precomputed collocations
        precompute_secondary_rank=20, # maximum frequency rank of patterns used to compute triples (don't set higher than 20).
        precompute_rank=100,      # maximum frequency rank of patterns used to compute collocations (no need to set higher than maybe 200-300)
        pruned_rule_file=None,    # if specified, pruned grammars will be written to this filename
        require_aligned_terminal=True, # require extracted rules to have at least one aligned word
        require_aligned_chunks=False, # require each contiguous chunk of extracted rules to have at least one aligned word
        per_sentence_grammar=True,  # generate grammar files for each input segment
        rule_file=None,           # UNpruned grammars will be written to this filename
        sample_file=None,         # Sampling statistics will be written to this filename
        search_file=None,         # lookup statistics will be written to this filename
        train_max_initial_size=10, # maximum span of a grammar rule extracted from TRAINING DATA
        train_min_gap_size=2,     # minimum span of an RHS nonterminal in a rule extracted from TRAINING DATA
        tight_phrases=False,      # True if phrases should be tight, False otherwise (False == slower but better results)
        timingFile=None,          # timing statistics will be written to this filename
        useBaezaYates=True,       # True to require use of double-binary alg, false otherwise
        use_collocations=True,    # True to enable used of precomputed collocations
        use_index=True            # True to enable use of precomputed inverted indices
        ):
    '''Note: we make a distinction between the min_gap_size
    and max_initial_size used in test and train.  The latter
    are represented by train_min_gap_size and train_max_initial_size,
    respectively.  This is because Chiang's model does not require
    them to be the same, therefore we don't either.'''
    self.rules = TrieTable(True) # cache
    self.rules.root = ExtendedTrieNode(phrase_location=PhraseLocation()) 
    self.grammar = grammar
    if alignment is None:
      raise Exception("Must specify an alignment object")
    self.alignment = alignment

    self.excluded_sent_id = -1

    # grammar parameters and settings
    # NOTE: setting max_nonterminals > 2 is not currently supported in Hiero
    self.max_length = max_length
    self.max_nonterminals = max_nonterminals 
    self.max_initial_size = max_initial_size
    self.train_max_initial_size = train_max_initial_size
    self.min_gap_size = min_gap_size
    self.train_min_gap_size = train_min_gap_size
    self.category = sym.fromstring(category)

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
    self.cacheBetweenSents = not per_sentence_grammar
    self.precomputed_collocations = {}
    self.precomputed_index = {}
    self.use_index = use_index
    self.use_collocations = use_collocations
    self.max_rank = {}
    self.precompute_file = precompute_file
    self.precompute_rank = precompute_rank
    self.precompute_secondary_rank = precompute_secondary_rank
    self.useBaezaYates = useBaezaYates
    self.by_slack_factor = by_slack_factor
    if tight_phrases:
      self.tight_phrases = 1
    else:
      self.tight_phrases = 0

    if require_aligned_chunks: 
      # one condition is a stronger version of the other.
      self.require_aligned_chunks = 1
      self.require_aligned_terminal = 1
    elif require_aligned_terminal:
      self.require_aligned_chunks = 0
      self.require_aligned_terminal = 1
    else:
      self.require_aligned_chunks = 0
      self.require_aligned_terminal = 0
    

    self.per_sentence_grammar = per_sentence_grammar
    if not self.per_sentence_grammar:
      self.rule_filehandler = open(rule_file, "w")
    # diagnostics
    #if rule_file is None:
    #  self.rule_file = None
    self.rule_file = rule_file
    if extract_file is None:
      self.extract_file = None
    else:
      self.extract_file = open(extract_file, "w")
    if sample_file is None:
      self.sample_file = None
    else:
      self.sample_file = open(sample_file, "w")
    if search_file is None:
      self.search_file = None
    else:
      self.search_file = open(search_file, "w")
    self.pruned_rule_file = pruned_rule_file
    self.timingFile = timingFile
    self.log_int_stats = log_int_stats
    self.prev_norm_prefix = ()

    self.findexes = cintlist.CIntList(initial_len=10)
    self.findexes1 = cintlist.CIntList(initial_len=10)

  def registerContext(self, context_manager):
    '''This gives the RuleFactory access to the Context object.
    Here we also use it to precompute the most expensive intersections
    in the corpus quickly.'''
    self.context_manager = context_manager
    self.fsa = context_manager.fsarray
    self.fda = self.fsa.darray
    self.eda = context_manager.edarray
    self.fid2symid = self.set_idmap(self.fda)
    self.eid2symid = self.set_idmap(self.eda)
    self.precompute()

  cdef set_idmap(self, cdat.DataArray darray):
    cdef int word_id, new_word_id, N
    cdef cintlist.CIntList idmap
    
    N = len(darray.id2word)
    idmap = cintlist.CIntList(initial_len=N)
    for word_id from 0 <= word_id < N:
      new_word_id = sym.fromstring(darray.id2word[word_id], terminal=True)
      idmap.arr[word_id] = new_word_id
    return idmap


  def pattern2phrase(self, pattern):
    # pattern is a tuple, which we must convert to a hiero rule.Phrase
    result = ()
    arity = 0
    for word_id in pattern:
      if word_id == -1:
        arity = arity + 1
        new_id = sym.setindex(self.category, arity)
      else:
        new_id = sym.fromstring(self.fda.id2word[word_id])
      result = result + (new_id,)
    return rule.Phrase(result)

  def pattern2phrase_plus(self, pattern):
    # returns a list containing both the pattern, and pattern
    # suffixed/prefixed with the NT category.
    patterns = []
    result = ()
    arity = 0
    for word_id in pattern:
      if word_id == -1:
        arity = arity + 1
        new_id = sym.setindex(self.category, arity)
      else:
        new_id = sym.fromstring(self.fda.id2word[word_id])
      result = result + (new_id,)
    patterns.append(rule.Phrase(result))
    patterns.append(rule.Phrase(result + (sym.setindex(self.category, 1),)))
    patterns.append(rule.Phrase((sym.setindex(self.category, 1),) + result))
    return patterns

  def precompute(self):
    cdef precomputation.Precomputation pre
    
    if self.precompute_file is not None:
      start_time = monitor.cpu()
      log.write("Reading precomputed data from file %s... " % self.precompute_file, 1)
      pre = precomputation.Precomputation(self.precompute_file, from_binary=True)
      # check parameters of precomputation -- some are critical and some are not
      if pre.max_nonterminals != self.max_nonterminals:
        log.writeln("\nWARNING: Precomputation done with max nonterminals %d, decoder uses %d" % (pre.max_nonterminals, self.max_nonterminals))
      if pre.max_length != self.max_length:
        log.writeln("\nWARNING: Precomputation done with max terminals %d, decoder uses %d" % (pre.max_length, self.max_length))
      if pre.train_max_initial_size != self.train_max_initial_size:
        log.writeln("\nERROR: Precomputation done with max initial size %d, decoder uses %d" % (pre.train_max_initial_size, self.train_max_initial_size))
        raise Exception("Parameter mismatch with precomputed data")
      if pre.train_min_gap_size != self.train_min_gap_size:
        log.writeln("\nERROR: Precomputation done with min gap size %d, decoder uses %d" % (pre.train_min_gap_size, self.train_min_gap_size))
        raise Exception("Parameter mismatch with precomputed data")
      log.writeln("done.", 1)
      if self.use_index:
        log.write("Converting %d hash keys on precomputed inverted index... " % (len(pre.precomputed_index)), 1)
        for pattern, arr in pre.precomputed_index.iteritems():
          phrases = self.pattern2phrase_plus(pattern)
          for phrase in phrases:
            self.precomputed_index[phrase] = arr
        log.writeln("done.", 1)
      if self.use_collocations:
        log.write("Converting %d hash keys on precomputed collocations... " % (len(pre.precomputed_collocations)), 1)
        for pattern, arr in pre.precomputed_collocations.iteritems():
          phrase = self.pattern2phrase(pattern)
          self.precomputed_collocations[phrase] = arr
        log.writeln("done.", 1)
      stop_time = monitor.cpu()
      log.writeln("Processing precomputations took %f seconds" % (stop_time - start_time), 1)


  def getPrecomputedCollocation(self, phrase):
    if phrase in self.precomputed_collocations:
      arr = self.precomputed_collocations[phrase]
      return PhraseLocation(arr=arr, arr_low=0, arr_high=len(arr), num_subpatterns=phrase.arity()+1)
    return None


  cdef int* baezaYatesHelper(self, int low1, int high1, int* arr1, int step1, 
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
#    log.writeln("%sBY: [%d, %d, %d] [%d, %d, %d]" % (pad, low1, high1, step1, low2, high2, step2,), 5)

    d_first = 0
    if high1 - low1 > high2 - low2:
#      log.writeln("%sD first" % (pad), 5)
      d_first = 1
#    else:
#      log.writeln("%sQ first" % (pad), 5)

#    '''First, check to see if we are at any of the 
#    recursive base cases'''
#
#    '''Case 1: one of the sets is empty'''
    if low1 >= high1 or low2 >= high2:
#      log.writeln("%sRETURN: set is empty" % (pad), 5)
      return result

#    '''Case 2: sets are non-overlapping'''
    assign_matching(&loc1, arr1, high1-step1, step1, self.fda.sent_id.arr)
    assign_matching(&loc2, arr2, low2, step2, self.fda.sent_id.arr)
    if self.compare_matchings(&loc1, &loc2, offset_by_one, len_last) == -1:
#      log.writeln("%s %s < %s" % (pad, tuple(arr1[high1-step1:high1]), tuple(arr2[low2:low2+step2])),5)
#      log.writeln("%sRETURN: non-overlapping sets" % (pad), 5)
      return result

    assign_matching(&loc1, arr1, low1, step1, self.fda.sent_id.arr)
    assign_matching(&loc2, arr2, high2-step2, step2, self.fda.sent_id.arr)
    if self.compare_matchings(&loc1, &loc2, offset_by_one, len_last) == 1:
#      log.writeln("%s %s > %s" % (pad, tuple(arr1[low1:low1+step1]), tuple(arr2[high2-step2:high2])),5)
#      log.writeln("%sRETURN: non-overlapping sets" % (pad), 5)
      return result

    # Case 3: query set and data set do not meet size mismatch constraints;
    # We use mergesort instead in this case
    qsetsize = (high1-low1) / step1
    dsetsize = (high2-low2) / step2
    if d_first:
      tmp = qsetsize
      qsetsize = dsetsize
      dsetsize = tmp

    if self.by_slack_factor * qsetsize * cmath.log(dsetsize) / cmath.log(2) > dsetsize:
      free(result)
      return self.mergeHelper(low1, high1, arr1, step1, low2, high2, arr2, step2, offset_by_one, len_last, num_subpatterns, result_len)

    # binary search.  There are two flavors, depending on 
    # whether the queryset or dataset is first
    if d_first:
      med2 = median(low2, high2, step2)
      assign_matching(&loc2, arr2, med2, step2, self.fda.sent_id.arr)

      search_low = low1
      search_high = high1
      while search_low < search_high:
        med1 = median(search_low, search_high, step1)
        findComparableMatchings(low1, high1, arr1, step1, med1, &med1_minus, &med1_plus)
        comparison = self.compareMatchingsSet(med1_minus, med1_plus, arr1, step1, &loc2, offset_by_one, len_last)
        if comparison == -1:
          search_low = med1_plus
        elif comparison == 1:
          search_high = med1_minus
        else:
          break
    else:
      med1 = median(low1, high1, step1)
      findComparableMatchings(low1, high1, arr1, step1, med1, &med1_minus, &med1_plus)

      search_low = low2
      search_high = high2
      while search_low < search_high:
        med2 = median(search_low, search_high, step2)
        assign_matching(&loc2, arr2, med2, step2, self.fda.sent_id.arr)
        comparison = self.compareMatchingsSet(med1_minus, med1_plus, arr1, step1, &loc2, offset_by_one, len_last)
        if comparison == -1:
          search_high = med2
        elif comparison == 1:
          search_low = med2 + step2
        else:
          break

    med_result_len = 0
    med_result = <int*> malloc(0*sizeof(int*))
    if search_high > search_low:
#      '''Then there is a match for the median element of Q'''
#      
#      '''What we want to find is the group of all bindings in the first set
#      s.t. their first element == the first element of med1.  Then we
#      want to store the bindings for all of those elements.  We can
#      subsequently throw all of them away.'''
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
    low_result = self.baezaYatesHelper(low1, med1_plus, arr1, step1, low2, med2_plus, arr2, step2, offset_by_one, len_last, num_subpatterns, &low_result_len)
    high_result_len = 0
    high_result = self.baezaYatesHelper(med1_minus, high1, arr1, step1, med2_minus, high2, arr2, step2, offset_by_one, len_last, num_subpatterns, &high_result_len)

    result = extend_arr(result, result_len, low_result, low_result_len)
    result = extend_arr(result, result_len, med_result, med_result_len)
    result = extend_arr(result, result_len, high_result, high_result_len)
    free(low_result)
    free(med_result)
    free(high_result)

    return result



  cdef long compareMatchingsSet(self, int i1_minus, int i1_plus, int* arr1, int step1, 
              Matching* loc2, int offset_by_one, int len_last):
#    '''Compares a *set* of bindings, all with the same first element,
#    to a single binding.  Returns -1 if all comparisons == -1, 1 if all
#    comparisons == 1, and 0 otherwise.'''
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


  cdef int* mergeHelper(self, int low1, int high1, int* arr1, int step1, 
          int low2, int high2, int* arr2, int step2, 
          int offset_by_one, int len_last, int num_subpatterns, int* result_len):
    cdef int i1, i2, j1, j2
    cdef long comparison
    cdef int* result
    cdef Matching loc1, loc2
#    cdef int i

#    pad = "  "
#    log.writeln("->mergeHelper", 5)

    result_len[0] = 0
    result = <int*> malloc(0*sizeof(int))

    i1 = low1
    i2 = low2
#    if log.level==5:
#      log.writeln("%sMERGE lists [%d,%d,%d] and [%d,%d,%d]" % (pad,low1,high1,step1,low2,high2,step2), 5)
#      log.writeln("%soffset_by_one: %d, len_last: %d" % (pad, offset_by_one, len_last), 5)
#      log.write("[")
#      for i from low1 <= i < high1:
#        log.write("%d, " % arr1.arr[i],5)
#      log.writeln("]")
#      log.write("[")
#      for i from low2 <= i < high2:
#        log.write("%d, " % arr2.arr[i],5)
#      log.writeln("]")
    while i1 < high1 and i2 < high2:
      
#      '''First, pop all unneeded loc2's off the stack'''
      assign_matching(&loc1, arr1, i1, step1, self.fda.sent_id.arr)
#      if log.level==5:
#        log.writeln("%s TOP1 %s" % (pad,matching2str(loc1)),5)
      while i2 < high2:
        assign_matching(&loc2, arr2, i2, step2, self.fda.sent_id.arr)
        if self.compare_matchings(&loc1, &loc2, offset_by_one, len_last) == 1:
#          if log.level==5:
#            log.writeln("%s %s > %s" % (pad,matching2str(loc1),matching2str(loc2)),5)
#            log.writeln("%s POP2 %s" % (pad,matching2str(loc2)),5)
          i2 = i2 + step2
        else:
          break

#      '''Next: process all loc1's with the same starting val'''
      j1 = i1
      while i1 < high1 and arr1[j1] == arr1[i1]:
        assign_matching(&loc1, arr1, i1, step1, self.fda.sent_id.arr)
        j2 = i2
        while j2 < high2:
          assign_matching(&loc2, arr2, j2, step2, self.fda.sent_id.arr)
          comparison = self.compare_matchings(&loc1, &loc2, offset_by_one, len_last)
          if comparison == 0:
#            if log.level==5:
#              log.writeln("%s %s == %s" % (pad,matching2str(loc1),matching2str(loc2)),5)
            result = append_combined_matching(result, &loc1, &loc2, offset_by_one, num_subpatterns, result_len)
          if comparison == 1:
#            if log.level==5:
#              log.writeln("%s %s > %s" % (pad,matching2str(loc1),matching2str(loc2)),5)
            pass
          if comparison == -1:
#            if log.level==5:
#              log.writeln("%s %s < %s" % (pad,matching2str(loc1),matching2str(loc2)),5)
            break
          else:
            j2 = j2 + step2
#        if log.level==5:
#          log.writeln("%s POP1 %s" % (pad,matching2str(loc1)),5)
        i1 = i1 + step1

#    log.writeln("<-mergeHelper", 5)
    return result


  cdef void sortPhraseLoc(self, cintlist.CIntList arr, PhraseLocation loc, rule.Phrase phrase):
    cdef int i, j
    cdef cveb.VEB veb
    cdef cintlist.CIntList result

    if phrase in self.precomputed_index:
      loc.arr = self.precomputed_index[phrase]
    else:
      loc.arr = cintlist.CIntList(initial_len=loc.sa_high-loc.sa_low)
      veb = cveb.VEB(arr.len)
      for i from loc.sa_low <= i < loc.sa_high:
        veb._insert(arr.arr[i])
      i = veb.veb.min_val
      for j from 0 <= j < loc.sa_high-loc.sa_low:
        loc.arr.arr[j] = i
        i = veb._findsucc(i)
    loc.arr_low = 0
    loc.arr_high = loc.arr.len


  cdef intersectHelper(self, rule.Phrase prefix, rule.Phrase suffix,
        PhraseLocation prefix_loc, PhraseLocation suffix_loc, int algorithm):

    cdef cintlist.CIntList arr1, arr2, result
    cdef int low1, high1, step1, low2, high2, step2, offset_by_one, len_last, num_subpatterns, result_len
    cdef int* result_ptr
    cdef csuf.SuffixArray suf

    result_len = 0

    if sym.isvar(suffix[0]):
      offset_by_one = 1
    else:
      offset_by_one = 0

    len_last = len(suffix.getchunk(suffix.arity()))

    if prefix_loc.arr is None:
      suf = self.context_manager.fsarray
      self.sortPhraseLoc(suf.sa, prefix_loc, prefix)
    arr1 = prefix_loc.arr
    low1 = prefix_loc.arr_low
    high1 = prefix_loc.arr_high
    step1 = prefix_loc.num_subpatterns

    if suffix_loc.arr is None:
      suf = self.context_manager.fsarray
      self.sortPhraseLoc(suf.sa, suffix_loc, suffix)
    arr2 = suffix_loc.arr
    low2 = suffix_loc.arr_low
    high2 = suffix_loc.arr_high
    step2 = suffix_loc.num_subpatterns

    num_subpatterns = prefix.arity()+1

    if algorithm == MERGE:
      result_ptr = self.mergeHelper(low1, high1, arr1.arr, step1, 
                  low2, high2, arr2.arr, step2, 
                  offset_by_one, len_last, num_subpatterns, &result_len)
    else:
      result_ptr = self.baezaYatesHelper(low1, high1, arr1.arr, step1, 
                  low2, high2, arr2.arr, step2, 
                  offset_by_one, len_last, num_subpatterns, &result_len)

    if result_len == 0:
      free(result_ptr)
      return None
    else:
      result = cintlist.CIntList()
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

#  cdef compareResults(self, PhraseLocation loc1, PhraseLocation loc2, phrase, type1, type2):
#    cdef i
#    if loc1 is None and type1=="pre":
#      return
#    if loc1 is None:
#      if loc2 is None or loc2.arr_high == 0:
#        return
#    if loc2 is None:
#      if loc1.arr_high == 0:
#        return
#    if loc1.arr_high != loc2.arr_high:
#      log.writeln("ERROR: %d vs %d (%s vs %s)" % (loc1.arr_high, loc2.arr_high, type1, type2))
#      #log.writeln("  %s" % self.loc2str(loc2))
#    if loc1.arr_high == 0:
#      return
#    elif loc1.num_subpatterns != loc2.num_subpatterns:
#      log.writeln("ERROR 2: %d vs %d (%d v %d) %s" % (loc1.num_subpatterns, loc2.num_subpatterns, loc1.arr_high, loc2.arr_high, phrase))
#    for i from 0 <= i < loc1.arr_high:
#      if loc1.arr[i] != loc2.arr[i]:
#        log.writeln("ERROR 3")
# 
  cdef PhraseLocation intersect(self, prefix_node, suffix_node, rule.Phrase phrase):
    cdef rule.Phrase prefix, suffix
    cdef PhraseLocation prefix_loc, suffix_loc, result

    start_time = monitor.cpu()
    prefix = prefix_node.phrase
    suffix = suffix_node.phrase
    prefix_loc = prefix_node.phrase_location
    suffix_loc = suffix_node.phrase_location

    result = self.getPrecomputedCollocation(phrase)
    if result is not None:
      intersect_method = "precomputed"

    if result is None:
      if self.useBaezaYates:
        result = self.intersectHelper(prefix, suffix, prefix_loc, suffix_loc, BAEZA_YATES)
        intersect_method="double binary"
      else:
        result = self.intersectHelper(prefix, suffix, prefix_loc, suffix_loc, MERGE)
        intersect_method="merge"
    stop_time = monitor.cpu()
    intersect_time = stop_time - start_time
    if self.log_int_stats:
      if intersect_method == "precomputed":
        sort1 = "none"
        sort2 = "none"
      else:
        if prefix in self.precomputed_index:
          sort1 = "index"
        else:
          sort1 = "veb"
        if suffix in self.precomputed_index:
          sort2 = "index"
        else:
          sort2 = "veb"
      result_len=0
      if result is not None:
        result_len = len(result.arr)/result.num_subpatterns
      rank = 0
#      if phrase in self.max_rank:
#        rank = self.max_rank[phrase]
#      else:
#        rank = self.precompute_rank + 10
      log.writeln("INT %d %d %d %d %d %f %d %s %s %s" % 
        (len(prefix)+1 - prefix.arity(), prefix.arity(), 
        nGramCount(prefix_node.phrase_location), 
        nGramCount(suffix_node.phrase_location), 
        result_len, intersect_time, rank, intersect_method, sort1, sort2))
    return result

  def advance(self, frontier, res, fwords):
    nf = []
    for (toskip, (i, alt, pathlen)) in frontier:
      spanlen = fwords[i][alt][2]
      if (toskip == 0):
        #log.writeln("RES: (%d %d %d)" % (i, alt, pathlen), 3)
        res.append((i, alt, pathlen))
      ni = i + spanlen
      #log.writeln("proc: %d (%d %d %d) sl=%d ni=%d len(fwords)=%d" % (toskip, i, alt, pathlen, spanlen, ni, len(fwords)), 3)
      if (ni < len(fwords) and (pathlen + 1) < self.max_initial_size):
        for na in xrange(len(fwords[ni])):
          nf.append((toskip - 1, (ni, na, pathlen + 1)))
    if (len(nf) > 0):
      return self.advance(nf, res, fwords)
    else:
      return res
    
  def get_all_nodes_isteps_away(self, skip, i, spanlen, pathlen, fwords, next_states, reachable_buffer):
    frontier = []
    if (i+spanlen+skip >= len(next_states)):
      return frontier
    #print "get_all_nodes_isteps_away from %i" % (i)
    key = tuple([i,spanlen])
    reachable = []
    if (key in reachable_buffer):
      reachable = reachable_buffer[key]
    else:
      reachable = self.reachable(fwords, i, spanlen)
      reachable_buffer[key] = reachable
    #print "reachable(from=%i,dist=%i) = " % (i,spanlen)
    #print reachable
    for nextreachable in reachable:
      for next_id in next_states[nextreachable]:
        jump = self.shortest(fwords,i,next_id)
        #print "checking next_id = %i, pathlen[sofar] = %i, jump = %i" % (next_id,pathlen,jump)
        #if (next_id - (i+spanlen)) < skip:
        if jump < skip:
          continue
        #if next_id-(i-pathlen) < self.max_initial_size:
        if pathlen+jump <= self.max_initial_size:
          for alt_id in xrange(len(fwords[next_id])):
            if (fwords[next_id][alt_id][0] != cn.epsilon):
              #frontier.append((next_id,alt_id,next_id-(i-pathlen)));
              #print "finding the shortest from %i to %i" % (i, next_id)
              newel = (next_id,alt_id,pathlen+jump)
              if newel not in frontier:
                frontier.append((next_id,alt_id,pathlen+jump))
                #print "appending to frontier = next_id=%i, alt_id=%i, pathlen=%i" % (next_id,alt_id,pathlen+jump)
              #else:
                #print "NOT appending to frontier = next_id=%i, alt_id=%i, pathlen=%i" % (next_id,alt_id,pathlen+jump)
        #else:
          #print "next_id = %s is aborted\n" % next_id
    #print "returning frontier"
    #print frontier
    return frontier

  def reachable(self, fwords, ifrom, dist):
    #print "inside reachable(%i,%i)" % (ifrom,dist)
    ret = []
    if (ifrom >= len(fwords)):
      return ret
    for alt_id in xrange(len(fwords[ifrom])):
      if (fwords[ifrom][alt_id][0] == cn.epsilon):
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
    min = 1000
    #print "shortest ifrom=%i, ito=%i" % (ifrom,ito)
    if (ifrom > ito):
      return min
    if (ifrom == ito):
      return 0
    for alt_id in xrange(len(fwords[ifrom])):
      currmin = self.shortest(fwords,ifrom+fwords[ifrom][alt_id][2],ito)
      if (fwords[ifrom][alt_id][0] != cn.epsilon):
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
        if (alt[0] == cn.epsilon):
          jump = 0
        if next_id not in result and min_dist <= curr[1]+jump <= self.max_initial_size+1:
          candidate.append([next_id,curr[1]+jump])
    return sorted(result);

  def input(self, fwords, meta):
    '''When this function is called on the RuleFactory,
    it looks up all of the rules that can be used to translate
    the input sentence'''
    cdef int i, j, k, flen, arity, num_subpatterns, num_samples
    cdef float start_time
    cdef PhraseLocation phrase_location
    cdef cintlist.CIntList sample, chunklen
    cdef Matching matching
    cdef rule.Phrase hiero_phrase
    
    #fwords = [ ((1,0.0,1),), fwords1 ] #word id for <s> = 1, cost = 0.0, next = 1
    #print fwords
    flen = len(fwords)
    #print "length = %i" % flen
    start_time = monitor.cpu()
    self.intersect_time = 0.0
    self.extract_time = 0.0
    nodes_isteps_away_buffer = {}
    hit = 0
    reachable_buffer = {}
    #print "id = ",meta
    #print "rule_file = ",self.rule_file
    dattrs = sgml.attrs_to_dict(meta)
    id = dattrs.get('id', 'NOID')
    if self.per_sentence_grammar:
      self.rule_filehandler = open(self.rule_file+'.'+id, 'w')
    self.excluded_sent_id = int(dattrs.get('exclude', '-1'))

    #print "max_initial_size = %i" % self.max_initial_size

    if not self.cacheBetweenSents:
      self.rules.root = ExtendedTrieNode(phrase_location=PhraseLocation()) 
      self.grammar.root = [None, {}]

    frontier = []
    for i in xrange(len(fwords)):
      for alt in xrange(0, len(fwords[i])):
        if fwords[i][alt][0] != cn.epsilon:
          frontier.append((i, i, alt, 0, self.rules.root, (), False))

    xroot = None
    x1 = sym.setindex(self.category, 1)
    if x1 in self.rules.root.children:
      xroot = self.rules.root.children[x1]
    else:
      xroot = ExtendedTrieNode(suffix_link=self.rules.root, phrase_location=PhraseLocation()) 
      self.rules.root.children[x1] = xroot

    for i in xrange(self.min_gap_size, len(fwords)):
      for alt in xrange(0, len(fwords[i])):
        if fwords[i][alt][0] != cn.epsilon:
          frontier.append((i-self.min_gap_size, i, alt, self.min_gap_size, xroot, (x1,), True))
    '''for k, i, alt, pathlen, node, prefix, is_shadow_path in frontier:
      if len(prefix)>0:
        print k, i, alt, pathlen, node, map(sym.tostring,prefix), is_shadow_path
      else:
        print k, i, alt, pathlen, node, prefix, is_shadow_path'''

    #for wid in xrange(1000):
    #  print "%i = %s" % (wid, sym.tostring(wid))
    next_states = []
    for i in xrange(len(fwords)):
      next_states.append(self.get_next_states(fwords,i,self.min_gap_size))
      #print "next state of %i" % i
      #print next_states[i]

    while len(frontier) > 0:
      #print "frontier = %i" % len(frontier)
      this_iter_intersect_time = self.intersect_time
      new_frontier = []
      for k, i, alt, pathlen, node, prefix, is_shadow_path in frontier:
        #print "looking at: "
        #if len(prefix)>0:
        #  print k, i, alt, pathlen, node, map(sym.tostring,prefix), is_shadow_path
        #else:
        #  print k, i, alt, pathlen, node, prefix, is_shadow_path
        word_id = fwords[i][alt][0]
        spanlen = fwords[i][alt][2]
        #print "word_id = %i, %s" % (word_id, sym.tostring(word_id))
        # to prevent .. [X] </S>
        #print "prefix = ",prefix
        #if word_id == 2 and len(prefix)>=2:
          #print "at the end: %s" % (prefix[len(prefix)-1])
          #if prefix[len(prefix)-1]<0:
            #print "break"
            #continue
        #print "continuing"
        #if pathlen + spanlen > self.max_initial_size:
          #continue
        # TODO get rid of k -- pathlen is replacing it
        if word_id == cn.epsilon:
          #print "skipping because word_id is epsilon"
          if i+spanlen >= len(fwords): 
            continue
          for nualt in xrange(0,len(fwords[i+spanlen])):
            frontier.append((k, i+spanlen, nualt, pathlen, node, prefix, is_shadow_path))
          continue
        
        phrase = prefix + (word_id,)
        str_phrase = map(sym.tostring, phrase)
        hiero_phrase = rule.Phrase(phrase)
        arity = hiero_phrase.arity()

        #print "pos %2d, node %5d, '%s'" % (i, node.id, hiero_phrase)
        if self.search_file is not None:
          self.search_file.write("%s\n" % hiero_phrase)

        lookup_required = False
        if word_id in node.children:
          if node.children[word_id] is None:
            #print "Path dead-ends at this node\n"
            continue
          else:
            #print "Path continues at this node\n"
            node = node.children[word_id]
        else:
          if node.suffix_link is None:
            #print "Current node is root; lookup required\n"
            lookup_required = True
          else:
            if word_id in node.suffix_link.children:
              if node.suffix_link.children[word_id] is None:
                #print "Suffix link reports path is dead end\n"
                node.children[word_id] = None
                continue
              else:
                #print "Suffix link indicates lookup is reqired\n"
                lookup_required = True
            else:
              #print "ERROR: We never get here\n"
              raise Exception("Keyword trie error")
              #new_frontier.append((k, i, alt, pathlen, node, prefix, is_shadow_path))
        #print "checking whether lookup_required\n"
        if lookup_required:
          new_node = None
          if is_shadow_path:
            #print "Extending shadow path for %s \n"
            # on the shadow path we don't do any search, we just use info from suffix link
            new_node = ExtendedTrieNode(phrase_location=node.suffix_link.children[word_id].phrase_location, 
                              suffix_link=node.suffix_link.children[word_id],
                              phrase=hiero_phrase)
          else:
            if arity > 0:
              #print "Intersecting for %s because of arity > 0\n" % hiero_phrase
              phrase_location = self.intersect(node, node.suffix_link.children[word_id], hiero_phrase)
            else:
              #print "Suffix array search for %s" % hiero_phrase
              phrase_location = node.phrase_location
              sa_range = self.context_manager.fsarray.lookup(str_phrase[-1], len(str_phrase)-1, phrase_location.sa_low, phrase_location.sa_high)
              if sa_range is not None:
                phrase_location = PhraseLocation(sa_low=sa_range[0], sa_high=sa_range[1])
              else:
                phrase_location = None

            if phrase_location is None:
              node.children[word_id] = None
              #print "Search failed\n"
              continue
            #print "Search succeeded\n"
            suffix_link = self.rules.root
            if node.suffix_link is not None:
              suffix_link = node.suffix_link.children[word_id]
            new_node = ExtendedTrieNode(phrase_location=phrase_location, 
                              suffix_link=suffix_link, 
                              phrase=hiero_phrase)
          node.children[word_id] = new_node
          node = new_node
          #print "Added node %d with suffix link %d\n" % (node.id, node.suffix_link.id)

          '''Automatically add a trailing X node, if allowed --
          This should happen before we get to extraction (so that
          the node will exist if needed)'''
          if arity < self.max_nonterminals:
            xcat_index = arity+1
            xcat = sym.setindex(self.category, xcat_index)
            suffix_link_xcat_index = xcat_index
            if is_shadow_path:
              suffix_link_xcat_index = xcat_index-1
            suffix_link_xcat = sym.setindex(self.category, suffix_link_xcat_index)
            node.children[xcat] = ExtendedTrieNode(phrase_location=node.phrase_location, 
                                    suffix_link=node.suffix_link.children[suffix_link_xcat],
                                    phrase= rule.Phrase(phrase + (xcat,)))
            #log.writeln("Added node %d with suffix link %d (for X)" % (node.children[xcat].id, node.children[xcat].suffix_link.id), 4)

          # sample from range
          if not is_shadow_path:
            #print "is_not_shadow_path"
            sample = self.context_manager.sampler.sample(node.phrase_location)
            #print "node.phrase_location %s" % str(node.phrase_location)
            #print "sample.len = %i" % len(sample)
            num_subpatterns = (<PhraseLocation> node.phrase_location).num_subpatterns
            chunklen = cintlist.CIntList(initial_len=num_subpatterns)
            for j from 0 <= j < num_subpatterns:
              chunklen.arr[j] = hiero_phrase.chunklen(j)
            extracts = []
            j = 0
            extract_start = monitor.cpu()
            '''orig_tight_phrases = self.tight_phrases
            orig_require_aligned_terminal = self.require_aligned_terminal
            orig_require_aligned_chunks = self.require_aligned_chunks
            if k==0 or i==len(fwords)-1:
              self.tight_phrases = 0
              self.require_aligned_terminal = 0
              self.require_aligned_chunks = 0'''
            while j < sample.len:
              extract = []

              assign_matching(&matching, sample.arr, j, num_subpatterns, self.fda.sent_id.arr)
              '''print "tight_phrase = "
              print self.tight_phrases
              print "require_aligned_terminal = "
              print self.require_aligned_terminal
              print "require_aligned_chunks = "
              print self.require_aligned_chunks'''
              
              extract = self.extract(hiero_phrase, &matching, chunklen.arr, num_subpatterns)
              extracts.extend(extract)
              j = j + num_subpatterns
            '''self.tight_phrases = orig_tight_phrases
            sttice+sa.nw.normelf.require_aligned_terminal = orig_require_aligned_terminal
            self.require_aligned_chunks = orig_require_aligned_chunks'''
            num_samples = sample.len/num_subpatterns
            extract_stop = monitor.cpu()
            self.extract_time = self.extract_time + extract_stop - extract_start
            #print "extract.size = %i" % len(extracts)
            if len(extracts) > 0:
              fphrases = {}
              fals = {}
              fcount = {}
              for f, e, count, als in extracts:
                fcount.setdefault(f, 0.0)
                fcount[f] = fcount[f] + count
                fphrases.setdefault(f, {})
                fphrases[f].setdefault(e, {})
                #fphrases[f][e] = fphrases[f][e] + count
                fphrases[f][e].setdefault(als,0.0)
                fphrases[f][e][als] = fphrases[f][e][als] + count
                #print "f,e,als ",f," : ",e," : ",als," count = ",fphrases[f][e][als]
                #fals[str(f)+" ||| "+str(e)] = als
              for f, elist in fphrases.iteritems():
                #print "f = '%s'" % f
                #if (str(f) in ['<s>','</s>','<s> [X,1]','[X,1] </s>']):
                #  print "rejected"
                #  continue
                f_margin = fcount[f]
                for e, alslist in elist.iteritems():
                  alignment = None
                  count = 0
                  for als, currcount in alslist.iteritems():
                    #print "als = ",als,", count = ",currcount
                    if currcount > count:
                      alignment = als
                      count = currcount 
                  #alignment = fals[str(f)+" ||| "+str(e)]
                  #print "selected = ",alignment," with count = ",count
                  scores = []
                  for m in self.context_manager.models:
                    scores.append(m.compute_contextless_score(f, e, count, fcount[f], num_samples))
                  r = rule.Rule(self.category, f, e, scores=scores, owner="context", word_alignments = alignment)
                  self.grammar.add(r)
                  if self.rule_filehandler is not None:
                    self.rule_filehandler.write("%s\n" % r.to_line())
                  #print "adding a rule = %s" % r

        #if len(phrase) < self.max_length and i+spanlen < len(fwords) and pathlen+spanlen < self.max_initial_size:
        if len(phrase) < self.max_length and i+spanlen < len(fwords) and pathlen+1 <= self.max_initial_size:
          #to prevent [X] </s>
          #print "lexicalized"
          for alt_id in xrange(len(fwords[i+spanlen])):
            #if (fwords[i+spanlen][alt_id][2]+pathlen+spanlen <= self.max_initial_size):
            #new_frontier.append((k, i+spanlen, alt_id, pathlen + spanlen, node, phrase, is_shadow_path))
            #print "alt_id = %d\n" % alt_id
            new_frontier.append((k, i+spanlen, alt_id, pathlen + 1, node, phrase, is_shadow_path))
            #print (k, i+spanlen, alt_id, pathlen + spanlen, node, map(sym.tostring,phrase), is_shadow_path)
          #print "end lexicalized"
          num_subpatterns = arity
          if not is_shadow_path:
            num_subpatterns = num_subpatterns + 1
          #to avoid <s> X ... we want <s> next to a lexicalized item
          #if k>0 and i<len(fwords)-1 and len(phrase)+1 < self.max_length and arity < self.max_nonterminals and num_subpatterns < self.max_chunks:
          if len(phrase)+1 < self.max_length and arity < self.max_nonterminals and num_subpatterns < self.max_chunks:
            #print "masuk kondisi"
            xcat = sym.setindex(self.category, arity+1)
            xnode = node.children[xcat]
            #frontier_nodes = self.get_all_nodes_isteps_away(self.min_gap_size, i, spanlen, pathlen, fwords, next_states)
            # I put spanlen=1 below
            key = tuple([self.min_gap_size, i, 1, pathlen])
            frontier_nodes = []
            if (key in nodes_isteps_away_buffer):
              frontier_nodes = nodes_isteps_away_buffer[key]
            else:
              frontier_nodes = self.get_all_nodes_isteps_away(self.min_gap_size, i, 1, pathlen, fwords, next_states, reachable_buffer)
              nodes_isteps_away_buffer[key] = frontier_nodes
            
            #print "new frontier:\n"
            for (i, alt, pathlen) in frontier_nodes:
              #if (pathlen+fwords[i][alt][2] <= self.max_initial_size):
              new_frontier.append((k, i, alt, pathlen, xnode, phrase +(xcat,), is_shadow_path))
              #print k, i, alt, pathlen, node, map(sym.tostring,phrase +(xcat,)), is_shadow_path
          #print "all end\n";
          #else:
            #print "no new frontier1\n";
        #else :
          #print "no new frontier2\n"
      if self.log_int_stats:
        log.writeln("This iteration intersect time = %f" % (self.intersect_time - this_iter_intersect_time))
      frontier = new_frontier
        
    stop_time = monitor.cpu()
    log.writeln("Total time for rule lookup, extraction, and scoring = %f seconds" % (stop_time - start_time))
    #log.writeln("  Intersect time = %f seconds" % self.intersect_time)
    gc.collect()
    log.writeln("  Extract time = %f seconds" % self.extract_time)
    if self.pruned_rule_file:
      self.grammar.dump(self.pruned_rule_file)
    if self.per_sentence_grammar:
      self.rule_filehandler.close()

#    else:
#      self.rule_filehandler.write("###EOS_"+ id +"\n")


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
      # rule X -> X_1 w X_2 / X_1 X_2.  This is probably
      # not worth the bother, though.
      #print "find_fixpoint0"
      return 0
    elif e_in_low != -1 and e_low[0] != e_in_low:
      if e_in_low - e_low[0] < min_ex_size:
        e_low[0] = e_in_low - min_ex_size
        if e_low[0] < 0:
          #print "find_fixpoint1"
          return 0

    if e_high[0] - e_low[0] > max_e_len:
      #print "find_fixpoint2"
      return 0
    elif e_in_high != -1 and e_high[0] != e_in_high:
      if e_high[0] - e_in_high < min_ex_size:
        e_high[0] = e_in_high + min_ex_size
        if e_high[0] > e_sent_len:
          #print "find_fixpoint3"
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
#        log.writeln("   FAIL: f phrase is not tight")
        #print "   FAIL: f phrase is not tight"
        return 0

      if f_back_high[0] - f_back_low[0] > max_f_len:
#        log.writeln("   FAIL: f back projection is too wide")
        #print "   FAIL: f back projection is too wide"
        return 0

      if allow_high_x == 0 and f_back_high[0] > f_high:
#        log.writeln("   FAIL: extension on high side not allowed")
        #print "   FAIL: extension on high side not allowed"
        return 0

      if f_low != f_back_low[0]:
        if new_low_x == 0:
          if new_x >= max_new_x:
#            log.writeln("   FAIL: extension required on low side violates max # of gaps")
            #print "   FAIL: extension required on low side violates max # of gaps"
            return 0
          else:
            new_x = new_x + 1
            new_low_x = 1
        if f_low - f_back_low[0] < min_fx_size:
          f_back_low[0] = f_low - min_fx_size
          if f_back_high[0] - f_back_low[0] > max_f_len:
#            log.writeln("   FAIL: extension required on low side violates max initial length")
            #print "   FAIL: extension required on low side violates max initial length"
            return 0
          if f_back_low[0] < 0:
#            log.writeln("   FAIL: extension required on low side violates sentence boundary")
            #print "   FAIL: extension required on low side violates sentence boundary"
            return 0

      if f_high != f_back_high[0]:
        if new_high_x == 0:
          if new_x >= max_new_x:
#            log.writeln("   FAIL: extension required on high side violates max # of gaps")
            #print "   FAIL: extension required on high side violates max # of gaps"
            return 0
          else:
            new_x = new_x + 1
            new_high_x = 1
        if f_back_high[0] - f_high < min_fx_size:
          f_back_high[0] = f_high + min_fx_size
          if f_back_high[0] - f_back_low[0] > max_f_len:
#            log.writeln("   FAIL: extension required on high side violates max initial length")
            #print "   FAIL: extension required on high side violates max initial length"
            return 0
          if f_back_high[0] > f_sent_len:
#            log.writeln("   FAIL: extension required on high side violates sentence boundary")
            #print "   FAIL: extension required on high side violates sentence boundary"
            return 0

      e_low_prev = e_low[0]
      e_high_prev = e_high[0]

      self.find_projection(f_back_low[0], f_low_prev, f_links_low, f_links_high, e_low, e_high)
      self.find_projection(f_high_prev, f_back_high[0], f_links_low, f_links_high, e_low, e_high)
      if e_low[0] == e_low_prev and e_high[0] == e_high_prev:
        return 1
      if allow_arbitrary_x == 0:
#        log.writeln("   FAIL: arbitrary expansion not permitted")
        #print "   FAIL: arbitrary expansion not permitted"
        return 0
      if e_high[0] - e_low[0] > max_e_len:
#        log.writeln("   FAIL: re-projection violates sentence max phrase length")
        #print "   FAIL: re-projection violates sentence max phrase length"
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
    cdef cintlist.CIntList ephr_arr
    cdef result

    #print "inside extract_phrases"
    #print "f_low=%d, f_high=%d" % (f_low,f_high)
    result = []
    len1 = 0
    e_gaps1 = <int*> malloc(0)
    ephr_arr = cintlist.CIntList()

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
    if self.tight_phrases == 0:
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
      if self.tight_phrases == 0:
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
              if n-m >= 1:  # extractor.py doesn't restrict target-side gap length 
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
          indexes.append(sym.setindex(self.category, e_gap_order[j]+1))
          ephr_arr._append(sym.setindex(self.category, e_gap_order[j]+1))
      i = i + step
      if ephr_arr.len <= self.max_target_length and num_chunks <= self.max_target_chunks:
        result.append((rule.Phrase(ephr_arr),indexes))

    free(e_gaps1)
    free(e_gap_order)
    return result

  cdef create_alignments(self, int* sent_links, int num_links, findexes, eindexes):
    #print "create_alignments"
    #s = "sent_links = "
    #i = 0
    #while (i < num_links*2):
    #  s = s+"%d-%d " % (sent_links[i],sent_links[i+1])
    #  i += 2
    #print s
    #print findexes
    #print eindexes
    
    ret = cintlist.CIntList()
    for i in xrange(len(findexes)):
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
        
  cdef extract(self, rule.Phrase phrase, Matching* matching, int* chunklen, int num_chunks):
    cdef int* sent_links, *e_links_low, *e_links_high, *f_links_low, *f_links_high
    cdef int *f_gap_low, *f_gap_high, *e_gap_low, *e_gap_high, num_gaps, gap_start
    cdef int i, j, k, e_i, f_i, num_links, num_aligned_chunks, met_constraints
    cdef int f_low, f_high, e_low, e_high, f_back_low, f_back_high
    cdef int e_sent_start, e_sent_end, f_sent_start, f_sent_end, e_sent_len, f_sent_len
    cdef int e_word_count, f_x_low, f_x_high, e_x_low, e_x_high, phrase_len
    cdef float pair_count
    cdef float available_mass
    cdef extracts, phrase_list
    cdef cintlist.CIntList fphr_arr
    cdef rule.Phrase fphr
    cdef reason_for_failure

    fphr_arr = cintlist.CIntList()
    phrase_len = phrase.n
    extracts = []
    sent_links = self.alignment._get_sent_links(matching.sent_id, &num_links)

    e_sent_start = self.eda.sent_index.arr[matching.sent_id]
    e_sent_end = self.eda.sent_index.arr[matching.sent_id+1]
    e_sent_len = e_sent_end - e_sent_start - 1
    f_sent_start = self.fda.sent_index.arr[matching.sent_id]
    f_sent_end = self.fda.sent_index.arr[matching.sent_id+1]
    f_sent_len = f_sent_end - f_sent_start - 1
    available_mass = 1.0
    if matching.sent_id == self.excluded_sent_id:
      available_mass = 0.0

    self.findexes1.reset()
    sofar = 0
    for i in xrange(num_chunks):
      for j in xrange(chunklen[i]):
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
    for x in xrange(matching.start,matching.end):
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
            # this is not a hard error.  we can't extract this phrase
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
            fphr_arr._append(sym.setindex(self.category, i))
            i = i+1
            self.findexes.append(sym.setindex(self.category, i))
          self.findexes.extend(self.findexes1)
          for j from 0 <= j < phrase.n:
            if sym.isvar(phrase.syms[j]):
              fphr_arr._append(sym.setindex(self.category, i))
              i = i + 1
            else:
              fphr_arr._append(phrase.syms[j])
          if f_back_high > f_high:
            fphr_arr._append(sym.setindex(self.category, i))
            self.findexes.append(sym.setindex(self.category, i))

          fphr = rule.Phrase(fphr_arr)
          if met_constraints:
            phrase_list = self.extract_phrases(e_low, e_high, e_gap_low + gap_start, e_gap_high + gap_start, e_links_low, num_gaps,
                      f_back_low, f_back_high, f_gap_low + gap_start, f_gap_high + gap_start, f_links_low,
                      matching.sent_id, e_sent_len, e_sent_start)
            #print "e_low=%d, e_high=%d, gap_start=%d, num_gaps=%d, f_back_low=%d, f_back_high=%d" & (e_low, e_high, gap_start, num_gaps, f_back_low, f_back_high)
            if len(phrase_list) > 0:
              pair_count = available_mass / len(phrase_list)
            else:
              pair_count = 0
              reason_for_failure = "Didn't extract anything from [%d, %d] -> [%d, %d]" % (f_back_low, f_back_high, e_low, e_high)
            for (phrase2,eindexes) in phrase_list:
              als1 = self.create_alignments(sent_links,num_links,self.findexes,eindexes)    
              extracts.append((fphr, phrase2, pair_count, tuple(als1)))
              if self.extract_file:
                self.extract_file.write("%s ||| %s ||| %f ||| %d: [%d, %d] -> [%d, %d]\n" % (fphr, phrase2, pair_count, matching.sent_id+1, f_back_low, f_back_high, e_low, e_high))
              #print "extract_phrases1: %s ||| %s ||| %f ||| %d: [%d, %d] -> [%d, %d]\n" % (fphr, phrase2, pair_count, matching.sent_id+1, f_back_low, f_back_high, e_low, e_high)

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
                self.find_fixpoint(f_x_low, f_back_high,
                      f_links_low, f_links_high, e_links_low, e_links_high, 
                      e_low, e_high, &e_x_low, &e_x_high, &f_x_low, &f_x_high, 
                      f_sent_len, e_sent_len, 
                      self.train_max_initial_size, self.train_max_initial_size, 
                      1, 1, 1, 1, 0, 1, 0) and
                ((not self.tight_phrases) or f_links_low[f_x_low] != -1) and
                self.find_fixpoint(f_x_low, f_low,  # check integrity of new subphrase
                      f_links_low, f_links_high, e_links_low, e_links_high,
                      -1, -1, e_gap_low, e_gap_high, f_gap_low, f_gap_high, 
                      f_sent_len, e_sent_len,
                      self.train_max_initial_size, self.train_max_initial_size,
                      0, 0, 0, 0, 0, 0, 0)):
                fphr_arr._clear()
                i = 1
                self.findexes.reset()
                self.findexes.append(sym.setindex(self.category, i))
                fphr_arr._append(sym.setindex(self.category, i))
                i = i+1
                self.findexes.extend(self.findexes1)
                for j from 0 <= j < phrase.n:
                  if sym.isvar(phrase.syms[j]):
                    fphr_arr._append(sym.setindex(self.category, i))
                    i = i + 1
                  else:
                    fphr_arr._append(phrase.syms[j])
                if f_back_high > f_high:
                  fphr_arr._append(sym.setindex(self.category, i))
                  self.findexes.append(sym.setindex(self.category, i))
                fphr = rule.Phrase(fphr_arr)
                phrase_list = self.extract_phrases(e_x_low, e_x_high, e_gap_low, e_gap_high, e_links_low, num_gaps+1,
                          f_x_low, f_x_high, f_gap_low, f_gap_high, f_links_low, matching.sent_id, 
                          e_sent_len, e_sent_start)
                if len(phrase_list) > 0:
                  pair_count = available_mass / len(phrase_list)
                else:
                  pair_count = 0
                for phrase2,eindexes in phrase_list:
                  als2 = self.create_alignments(sent_links,num_links,self.findexes,eindexes)    
                  extracts.append((fphr, phrase2, pair_count, tuple(als2)))
                  if self.extract_file:
                    self.extract_file.write("%s ||| %s ||| %f ||| %d: [%d, %d] -> [%d, %d]\n" % (fphr, phrase2, pair_count, matching.sent_id+1, f_x_low, f_x_high, e_x_low, e_x_high))
                  #print "extract_phrases2: %s ||| %s ||| %f ||| %d: [%d, %d] -> [%d, %d]\n" % (fphr, phrase2, pair_count, matching.sent_id+1, f_x_low, f_x_high, e_x_low, e_x_high)

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
                  fphr_arr._append(sym.setindex(self.category, i))
                  i = i+1
                  self.findexes.append(sym.setindex(self.category, i))
                self.findexes.extend(self.findexes1)
                for j from 0 <= j < phrase.n:
                  if sym.isvar(phrase.syms[j]):
                    fphr_arr._append(sym.setindex(self.category, i))
                    i = i + 1
                  else:
                    fphr_arr._append(phrase.syms[j])
                fphr_arr._append(sym.setindex(self.category, i))
                self.findexes.append(sym.setindex(self.category, i))
                fphr = rule.Phrase(fphr_arr)
                phrase_list = self.extract_phrases(e_x_low, e_x_high, e_gap_low+gap_start, e_gap_high+gap_start, e_links_low, num_gaps+1,
                          f_x_low, f_x_high, f_gap_low+gap_start, f_gap_high+gap_start, f_links_low, 
                          matching.sent_id, e_sent_len, e_sent_start)
                if len(phrase_list) > 0:
                  pair_count = available_mass / len(phrase_list)
                else:
                  pair_count = 0
                for phrase2, eindexes in phrase_list:
                  als3 = self.create_alignments(sent_links,num_links,self.findexes,eindexes)    
                  extracts.append((fphr, phrase2, pair_count, tuple(als3)))
                  if self.extract_file:
                    self.extract_file.write("%s ||| %s ||| %f ||| %d: [%d, %d] -> [%d, %d]\n" % (fphr, phrase2, pair_count, matching.sent_id+1, f_x_low, f_x_high, e_x_low, e_x_high))
                  #print "extract_phrases3: %s ||| %s ||| %f ||| %d: [%d, %d] -> [%d, %d]\n" % (fphr, phrase2, pair_count, matching.sent_id+1, f_x_low, f_x_high, e_x_low, e_x_high)
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
                self.find_fixpoint(f_x_low, f_x_high,
                        f_links_low, f_links_high, e_links_low, e_links_high,
                        e_low, e_high, &e_x_low, &e_x_high, &f_x_low, &f_x_high, 
                        f_sent_len, e_sent_len,
                        self.train_max_initial_size, self.train_max_initial_size, 
                        1, 1, 2, 1, 1, 1, 1) and
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
                self.findexes.append(sym.setindex(self.category, i))
                fphr_arr._append(sym.setindex(self.category, i))
                i = i+1
                self.findexes.extend(self.findexes1)
                for j from 0 <= j < phrase.n:
                  if sym.isvar(phrase.syms[j]):
                    fphr_arr._append(sym.setindex(self.category, i))
                    i = i + 1
                  else:
                    fphr_arr._append(phrase.syms[j])
                fphr_arr._append(sym.setindex(self.category, i))
                self.findexes.append(sym.setindex(self.category, i))
                fphr = rule.Phrase(fphr_arr)
                phrase_list = self.extract_phrases(e_x_low, e_x_high, e_gap_low, e_gap_high, e_links_low, num_gaps+2,
                          f_x_low, f_x_high, f_gap_low, f_gap_high, f_links_low, 
                          matching.sent_id, e_sent_len, e_sent_start)
                if len(phrase_list) > 0:
                  pair_count = available_mass / len(phrase_list)
                else:
                  pair_count = 0
                for phrase2, eindexes in phrase_list:
                  als4 = self.create_alignments(sent_links,num_links,self.findexes,eindexes)    
                  extracts.append((fphr, phrase2, pair_count, tuple(als4)))
                  if self.extract_file:
                    self.extract_file.write("%s ||| %s ||| %f ||| %d: [%d, %d] -> [%d, %d]\n" % (fphr, phrase2, pair_count, matching.sent_id+1, f_x_low, f_x_high, e_x_low, e_x_high))
                  #print "extract_phrases4 %s ||| %s ||| %f ||| %d: [%d, %d] -> [%d, %d]\n" % (fphr, phrase2, pair_count, matching.sent_id+1, f_x_low, f_x_high, e_x_low, e_x_high)
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

    if self.sample_file is not None:
      self.sample_file.write("%s ||| %d: [%d, %d] ||| %d ||| %s\n" % (phrase, matching.sent_id+1, f_low, f_high, len(extracts), reason_for_failure))

    #print "%s ||| %d: [%d, %d] ||| %d ||| %s\n" % (phrase, matching.sent_id+1, f_low, f_high, len(extracts), reason_for_failure)

    
    return extracts

