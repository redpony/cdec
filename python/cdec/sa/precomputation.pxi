# precomputes a set of collocations by advancing over the text.
# warning: nasty C code

from libc.stdio cimport FILE, fopen, fread, fwrite, fclose
from libc.stdlib cimport malloc, realloc, free
from libc.string cimport memset, memcpy

cdef struct _Trie_Node    # forward decl

cdef struct _Trie_Edge:
    int val
    _Trie_Node* node
    _Trie_Edge* bigger
    _Trie_Edge* smaller

cdef struct _Trie_Node:
    _Trie_Edge* root
    int* arr
    int arr_len

cdef _Trie_Node* new_trie_node():
    cdef _Trie_Node* node
    node = <_Trie_Node*> malloc(sizeof(_Trie_Node))
    node.root = NULL
    node.arr_len = 0
    node.arr = <int*> malloc(sizeof(0*sizeof(int)))
    return node

cdef _Trie_Edge* new_trie_edge(int val):
    cdef _Trie_Edge* edge
    edge = <_Trie_Edge*> malloc(sizeof(_Trie_Edge))
    edge.node = new_trie_node()
    edge.bigger = NULL
    edge.smaller = NULL
    edge.val = val
    return edge

cdef free_trie_node(_Trie_Node* node):
    if node != NULL:
        free_trie_edge(node.root)
        free(node.arr)

cdef free_trie_edge(_Trie_Edge* edge):
    if edge != NULL:
        free_trie_node(edge.node)
        free_trie_edge(edge.bigger)
        free_trie_edge(edge.smaller)

cdef _Trie_Node* trie_find(_Trie_Node* node, int val):
    cdef _Trie_Edge* cur
    cur = node.root
    while cur != NULL and cur.val != val:
        if val > cur.val:
            cur = cur.bigger
        elif val < cur.val:
            cur = cur.smaller
    if cur == NULL:
        return NULL
    else:
        return cur.node

cdef trie_node_data_append(_Trie_Node* node, int val):
    cdef int new_len
    new_len = node.arr_len + 1
    node.arr = <int*> realloc(node.arr, new_len*sizeof(int))
    node.arr[node.arr_len] = val
    node.arr_len = new_len

cdef trie_node_data_extend(_Trie_Node* node, int* vals, int num_vals):
    cdef int new_len
    new_len = node.arr_len + num_vals
    node.arr = <int*> realloc(node.arr, new_len*sizeof(int))
    memcpy(node.arr + node.arr_len, vals, num_vals*sizeof(int))
    node.arr_len = new_len


cdef _Trie_Node* trie_insert(_Trie_Node* node, int val):
    cdef _Trie_Edge** cur
    cur = &node.root
    while cur[0] != NULL and cur[0].val != val:
        if val > cur[0].val:
            cur = &cur[0].bigger
        elif val < cur[0].val:
            cur = &cur[0].smaller
    if cur[0] == NULL:
        cur[0] = new_trie_edge(val)
    return cur[0].node

cdef trie_node_to_map(_Trie_Node* node, result, prefix, int include_zeros):
    cdef IntList arr

    if include_zeros or node.arr_len > 0:
        arr = IntList()
        free(arr.arr)
        arr.arr = <int*> malloc(node.arr_len * sizeof(int))
        memcpy(arr.arr, node.arr, node.arr_len * sizeof(int))
        arr.len = node.arr_len
        arr.size = node.arr_len
        result[prefix] = arr
    trie_edge_to_map(node.root, result, prefix, include_zeros)

cdef trie_edge_to_map(_Trie_Edge* edge, result, prefix, int include_zeros):
    if edge != NULL:
        trie_edge_to_map(edge.smaller, result, prefix, include_zeros)
        trie_edge_to_map(edge.bigger, result, prefix, include_zeros)
        prefix = prefix + (edge.val,)
        trie_node_to_map(edge.node, result, prefix, include_zeros)

cdef class TrieMap:

    cdef _Trie_Node** root
    cdef int V

    def __cinit__(self, int alphabet_size):
        self.V = alphabet_size
        self.root = <_Trie_Node**> malloc(self.V * sizeof(_Trie_Node*))
        memset(self.root, 0, self.V * sizeof(_Trie_Node*))


    def __dealloc__(self):
        cdef int i
        for i from 0 <= i < self.V:
            if self.root[i] != NULL:
                free_trie_node(self.root[i])
        free(self.root)


    def insert(self, pattern):
        cdef int* p
        cdef int i, l
        l = len(pattern)
        p = <int*> malloc(l*sizeof(int))
        for i from 0 <= i < l:
            p[i] = pattern[i]
        self._insert(p,l)
        free(p)


    cdef _Trie_Node* _insert(self, int* pattern, int pattern_len):
        cdef int i
        cdef _Trie_Node* node
        if self.root[pattern[0]] == NULL:
            self.root[pattern[0]] = new_trie_node()
        node = self.root[pattern[0]]
        for i from 1 <= i < pattern_len:
            node = trie_insert(node, pattern[i])
        return node

    def contains(self, pattern):
        cdef int* p
        cdef int i, l
        cdef _Trie_Node* node
        l = len(pattern)
        p = <int*> malloc(l*sizeof(int))
        for i from 0 <= i < l:
            p[i] = pattern[i]
        node = self._contains(p,l)
        free(p)
        if node == NULL:
            return False
        else:
            return True

    cdef _Trie_Node* _contains(self, int* pattern, int pattern_len):
        cdef int i
        cdef _Trie_Node* node
        node = self.root[pattern[0]]
        i = 1
        while node != NULL and i < pattern_len:
            node = trie_find(node, pattern[i])
            i = i+1
        return node

    def toMap(self, flag):
        cdef int i, include_zeros

        if flag:
            include_zeros=1
        else:
            include_zeros=0
        result = {}
        for i from 0 <= i < self.V:
            if self.root[i] != NULL:
                trie_node_to_map(self.root[i], result, (i,), include_zeros)
        return result


cdef class Precomputation:
    cdef int precompute_rank
    cdef int precompute_secondary_rank
    cdef int max_length
    cdef int max_nonterminals
    cdef int train_max_initial_size
    cdef int train_min_gap_size
    cdef precomputed_index
    cdef precomputed_collocations
    cdef read_map(self, FILE* f)
    cdef write_map(self, m, FILE* f)

    def __cinit__(self, fsarray=None, from_stats=None, from_binary=None,
            precompute_rank=1000, precompute_secondary_rank=20,
            max_length=5, max_nonterminals=2,
            train_max_initial_size=10, train_min_gap_size=2):
        self.precompute_rank = precompute_rank
        self.precompute_secondary_rank = precompute_secondary_rank
        self.max_length = max_length
        self.max_nonterminals = max_nonterminals
        self.train_max_initial_size = train_max_initial_size
        self.train_min_gap_size = train_min_gap_size
        if from_binary:
            self.read_binary(from_binary)
        elif from_stats:
            self.precompute(from_stats, fsarray)


    def read_binary(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "r")
        fread(&(self.precompute_rank), sizeof(int), 1, f)
        fread(&(self.precompute_secondary_rank), sizeof(int), 1, f)
        fread(&(self.max_length), sizeof(int), 1, f)
        fread(&(self.max_nonterminals), sizeof(int), 1, f)
        fread(&(self.train_max_initial_size), sizeof(int), 1, f)
        fread(&(self.train_min_gap_size), sizeof(int), 1, f)
        self.precomputed_index = self.read_map(f)
        self.precomputed_collocations = self.read_map(f)
        fclose(f)


    def write_binary(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "w")
        fwrite(&(self.precompute_rank), sizeof(int), 1, f)
        fwrite(&(self.precompute_secondary_rank), sizeof(int), 1, f)
        fwrite(&(self.max_length), sizeof(int), 1, f)
        fwrite(&(self.max_nonterminals), sizeof(int), 1, f)
        fwrite(&(self.train_max_initial_size), sizeof(int), 1, f)
        fwrite(&(self.train_min_gap_size), sizeof(int), 1, f)
        self.write_map(self.precomputed_index, f)
        self.write_map(self.precomputed_collocations, f)
        fclose(f)


    cdef write_map(self, m, FILE* f):
        cdef int i, N
        cdef IntList arr

        N = len(m)
        fwrite(&(N), sizeof(int), 1, f)
        for pattern, val in m.iteritems():
            N = len(pattern)
            fwrite(&(N), sizeof(int), 1, f)
            for word_id in pattern:
                i = word_id
                fwrite(&(i), sizeof(int), 1, f)
            arr = val
            arr.write_handle(f)


    cdef read_map(self, FILE* f):
        cdef int i, j, k, word_id, N
        cdef IntList arr

        m = {}
        fread(&(N), sizeof(int), 1, f)
        for j from 0 <= j < N:
            fread(&(i), sizeof(int), 1, f)
            key = ()
            for k from 0 <= k < i:
                fread(&(word_id), sizeof(int), 1, f)
                key = key + (word_id,)
            arr = IntList()
            arr.read_handle(f)
            m[key] = arr
        return m


    def precompute(self, stats, SuffixArray sarray):
        cdef int i, l, N, max_pattern_len, i1, l1, i2, l2, i3, l3, ptr1, ptr2, ptr3, is_super, sent_count, max_rank
        cdef DataArray darray = sarray.darray
        cdef IntList data, queue, cost_by_rank, count_by_rank
        cdef TrieMap frequent_patterns, super_frequent_patterns, collocations
        cdef _Trie_Node* node

        data = darray.data

        frequent_patterns = TrieMap(len(darray.id2word))
        super_frequent_patterns = TrieMap(len(darray.id2word))
        collocations = TrieMap(len(darray.id2word))

        I_set = set()
        J_set = set()
        J2_set = set()
        IJ_set = set()
        pattern_rank = {}

        logger.info("Precomputing frequent intersections")
        cdef float start_time = monitor_cpu()

        max_pattern_len = 0
        for rank, (_, _, phrase) in enumerate(stats):
            if rank >= self.precompute_rank:
                break
            max_pattern_len = max(max_pattern_len, len(phrase))
            frequent_patterns.insert(phrase)
            I_set.add(phrase)
            pattern_rank[phrase] = rank
            if rank < self.precompute_secondary_rank:
                super_frequent_patterns.insert(phrase)
                J_set.add(phrase)

        queue = IntList(increment=1000)

        logger.info("    Computing inverted indexes...")
        N = len(data)
        for i from 0 <= i < N:
            sa_word_id = data.arr[i]
            if sa_word_id == 1:
                queue._append(-1)
            else:
                for l from 1 <= l <= max_pattern_len:
                    node = frequent_patterns._contains(data.arr+i, l)
                    if node == NULL:
                        break
                    queue._append(i)
                    queue._append(l)
                    trie_node_data_append(node, i)

        logger.info("    Computing collocations...")
        N = len(queue)
        ptr1 = 0
        sent_count = 0
        while ptr1 < N:    # main loop
            i1 = queue.arr[ptr1]
            if i1 > -1:
                l1 = queue.arr[ptr1+1]
                ptr2 = ptr1 + 2
                while ptr2 < N:
                    i2 = queue.arr[ptr2]
                    if i2 == -1 or i2 - i1 >= self.train_max_initial_size:
                        break
                    l2 = queue.arr[ptr2+1]
                    if (i2 - i1 - l1 >= self.train_min_gap_size and
                            i2 + l2 - i1 <= self.train_max_initial_size and
                            l1+l2+1 <= self.max_length):
                        node = collocations._insert(data.arr+i1, l1)
                        node = trie_insert(node, -1)
                        for i from i2 <= i < i2+l2:
                            node = trie_insert(node, data.arr[i])
                        trie_node_data_append(node, i1)
                        trie_node_data_append(node, i2)
                        if super_frequent_patterns._contains(data.arr+i2, l2) != NULL:
                            if super_frequent_patterns._contains(data.arr+i1, l1) != NULL:
                                is_super = 1
                            else:
                                is_super = 0
                            ptr3 = ptr2 + 2
                            while ptr3 < N:
                                i3 = queue.arr[ptr3]
                                if i3 == -1 or i3 - i1 >= self.train_max_initial_size:
                                    break
                                l3 = queue.arr[ptr3+1]
                                if (i3 - i2 - l2 >= self.train_min_gap_size and
                                        i3 + l3 - i1 <= self.train_max_initial_size and
                                        l1+l2+l3+2 <= self.max_length):
                                    if is_super or super_frequent_patterns._contains(data.arr+i3, l3) != NULL:
                                        node = collocations._insert(data.arr+i1, l1)
                                        node = trie_insert(node, -1)
                                        for i from i2 <= i < i2+l2:
                                            node = trie_insert(node, data.arr[i])
                                        node = trie_insert(node, -1)
                                        for i from i3 <= i < i3+l3:
                                            node = trie_insert(node, data.arr[i])
                                        trie_node_data_append(node, i1)
                                        trie_node_data_append(node, i2)
                                        trie_node_data_append(node, i3)
                                ptr3 = ptr3 + 2
                    ptr2 = ptr2 + 2
                ptr1 = ptr1 + 2
            else:
                sent_count = sent_count + 1
                if sent_count % 10000 == 0:
                    logger.debug("        %d sentences", sent_count)
                ptr1 = ptr1 + 1

        self.precomputed_collocations = collocations.toMap(False)
        self.precomputed_index = frequent_patterns.toMap(True)

        x = 0
        for pattern1 in J_set:
            for pattern2 in J_set:
                if len(pattern1) + len(pattern2) + 1 < self.max_length:
                    combined_pattern = pattern1 + (-1,) + pattern2
                    J2_set.add(combined_pattern)

        for pattern1 in I_set:
            for pattern2 in I_set:
                x = x+1
                if len(pattern1) + len(pattern2) + 1 <= self.max_length:
                    combined_pattern = pattern1 + (-1,) + pattern2
                    IJ_set.add(combined_pattern)

        for pattern1 in I_set:
            for pattern2 in J2_set:
                x = x+2
                if len(pattern1) + len(pattern2) + 1<= self.max_length:
                    combined_pattern = pattern1 + (-1,) + pattern2
                    IJ_set.add(combined_pattern)
                    combined_pattern = pattern2 + (-1,) + pattern1
                    IJ_set.add(combined_pattern)

        N = len(pattern_rank)
        cost_by_rank = IntList(initial_len=N)
        count_by_rank = IntList(initial_len=N)
        for pattern, arr in self.precomputed_collocations.iteritems():
            if pattern not in IJ_set:
                s = ""
                for word_id in pattern:
                    if word_id == -1:
                        s = s + "X "
                    else:
                        s = s + darray.id2word[word_id] + " "
                logger.warn("ERROR: unexpected pattern %s in set of precomputed collocations", s)
            else:
                chunk = ()
                max_rank = 0
                arity = 0
                for word_id in pattern:
                    if word_id == -1:
                        max_rank = max(max_rank, pattern_rank[chunk])
                        arity = arity + 1
                        chunk = ()
                    else:
                        chunk = chunk + (word_id,)
                max_rank = max(max_rank, pattern_rank[chunk])
                cost_by_rank.arr[max_rank] = cost_by_rank.arr[max_rank] + (4*len(arr))
                count_by_rank.arr[max_rank] = count_by_rank.arr[max_rank] + (len(arr)/(arity+1))

        cumul_cost = 0
        cumul_count = 0
        for i from 0 <= i < N:
            cumul_cost = cumul_cost + cost_by_rank.arr[i]
            cumul_count = cumul_count + count_by_rank.arr[i]
            logger.debug("RANK %d\tCOUNT, COST: %d    %d\tCUMUL: %d, %d", i, count_by_rank.arr[i], cost_by_rank.arr[i], cumul_count, cumul_cost)

        num_found_patterns = len(self.precomputed_collocations)
        for pattern in IJ_set:
            if pattern not in self.precomputed_collocations:
                self.precomputed_collocations[pattern] = IntList()

        cdef float stop_time = monitor_cpu()
        logger.info("Precomputed collocations for %d patterns out of %d possible (upper bound %d)", num_found_patterns, len(self.precomputed_collocations), x)
        logger.info("Precomputed inverted index for %d patterns ", len(self.precomputed_index))
        logger.info("Precomputation took %f seconds", (stop_time - start_time))
