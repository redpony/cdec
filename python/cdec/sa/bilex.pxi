# defines bilexical dictionaries in C, with some convenience methods
# for reading arrays directly as globs directly from disk.
# Adam Lopez <alopez@cs.umd.edu>

from libc.stdio cimport FILE, fopen, fread, fwrite, fclose
from libc.stdlib cimport malloc, realloc, free
from libc.string cimport memset, strcpy

cdef struct _node:
    _node* smaller
    _node* bigger
    int key
    int val

cdef _node* new_node(int key):
    cdef _node* n
    n = <_node*> malloc(sizeof(_node))
    n.smaller = NULL
    n.bigger = NULL
    n.key = key
    n.val = 0
    return n


cdef del_node(_node* n):
    if n.smaller != NULL:
        del_node(n.smaller)
    if n.bigger != NULL:
        del_node(n.bigger)
    free(n)

cdef int* get_val(_node* n, int key):
    if key == n.key:
        return &n.val
    elif key < n.key:
        if n.smaller == NULL:
            n.smaller = new_node(key)
            return &(n.smaller.val)
        return get_val(n.smaller, key)
    else:
        if n.bigger == NULL:
            n.bigger = new_node(key)
            return &(n.bigger.val)
        return get_val(n.bigger, key)


cdef class BiLex:
    cdef FloatList col1
    cdef FloatList col2
    cdef IntList f_index
    cdef IntList e_index
    cdef id2eword, id2fword, eword2id, fword2id

    def __cinit__(self, from_text=None, from_data=False, from_binary=None, 
            earray=None, fsarray=None, alignment=None):
        self.id2eword = []
        self.id2fword = []
        self.eword2id = {}
        self.fword2id = {}
        self.e_index = IntList()
        self.f_index = IntList()
        self.col1 = FloatList()
        self.col2 = FloatList()
        if from_binary:
            self.read_binary(from_binary)
        elif from_data:
            self.compute_from_data(fsarray, earray, alignment)
        else:
            self.read_text(from_text)


    cdef compute_from_data(self, SuffixArray fsa, DataArray eda, Alignment aa):
        cdef int sent_id, num_links, l, i, j, f_i, e_j, I, J, V_E, V_F, num_pairs
        cdef int *fsent, *esent, *alignment, *links, *ealigned, *faligned
        cdef _node** dict
        cdef int *fmargin, *emargin, *count
        cdef int null_word

        null_word = 0
        for word in fsa.darray.id2word: # I miss list comprehensions
            self.id2fword.append(word)
        self.id2fword[null_word] = "NULL"
        for id, word in enumerate(self.id2fword):
            self.fword2id[word] = id

        for word in eda.id2word:
            self.id2eword.append(word)
        self.id2eword[null_word] = "NULL"
        for id, word in enumerate(self.id2eword):
            self.eword2id[word] = id

        num_pairs = 0

        V_E = len(eda.id2word)
        V_F = len(fsa.darray.id2word)
        fmargin = <int*> malloc(V_F*sizeof(int))
        emargin = <int*> malloc(V_E*sizeof(int))
        memset(fmargin, 0, V_F*sizeof(int))
        memset(emargin, 0, V_E*sizeof(int))

        dict = <_node**> malloc(V_F*sizeof(_node*))
        memset(dict, 0, V_F*sizeof(_node*))

        num_sents = len(fsa.darray.sent_index)
        for sent_id from 0 <= sent_id < num_sents-1:

            fsent = fsa.darray.data.arr + fsa.darray.sent_index.arr[sent_id]
            I = fsa.darray.sent_index.arr[sent_id+1] - fsa.darray.sent_index.arr[sent_id] - 1
            faligned = <int*> malloc(I*sizeof(int))
            memset(faligned, 0, I*sizeof(int))

            esent = eda.data.arr + eda.sent_index.arr[sent_id]
            J = eda.sent_index.arr[sent_id+1] - eda.sent_index.arr[sent_id] - 1
            ealigned = <int*> malloc(J*sizeof(int))
            memset(ealigned, 0, J*sizeof(int))

            links = aa._get_sent_links(sent_id, &num_links)

            for l from 0 <= l < num_links:
                i = links[l*2]
                j = links[l*2+1]
                if i >= I or j >= J:
                    raise Exception("%d-%d out of bounds (I=%d,J=%d) in line %d\n" % (i,j,I,J,sent_id+1))
                f_i = fsent[i]
                e_j = esent[j]
                fmargin[f_i] = fmargin[f_i]+1
                emargin[e_j] = emargin[e_j]+1
                if dict[f_i] == NULL:
                    dict[f_i] = new_node(e_j)
                    dict[f_i].val = 1
                    num_pairs = num_pairs + 1
                else:
                    count = get_val(dict[f_i], e_j)
                    if count[0] == 0:
                        num_pairs = num_pairs + 1
                    count[0] = count[0] + 1
                # add count
                faligned[i] = 1
                ealigned[j] = 1
            for i from 0 <= i < I:
                if faligned[i] == 0:
                    f_i = fsent[i]
                    fmargin[f_i] = fmargin[f_i] + 1
                    emargin[null_word] = emargin[null_word] + 1
                    if dict[f_i] == NULL:
                        dict[f_i] = new_node(null_word)
                        dict[f_i].val = 1
                        num_pairs = num_pairs + 1
                    else:
                        count = get_val(dict[f_i], null_word)
                        if count[0] == 0:
                            num_pairs = num_pairs + 1
                        count[0] = count[0] + 1
            for j from 0 <= j < J:
                if ealigned[j] == 0:
                    e_j = esent[j]
                    fmargin[null_word] = fmargin[null_word] + 1
                    emargin[e_j] = emargin[e_j] + 1
                    if dict[null_word] == NULL:
                        dict[null_word] = new_node(e_j)
                        dict[null_word].val = 1
                        num_pairs = num_pairs + 1
                    else:
                        count = get_val(dict[null_word], e_j)
                        if count[0] == 0:
                            num_pairs = num_pairs + 1
                        count[0] = count[0] + 1
            free(links)
            free(faligned)
            free(ealigned)
        self.f_index = IntList(initial_len=V_F)
        self.e_index = IntList(initial_len=num_pairs)
        self.col1 = FloatList(initial_len=num_pairs)
        self.col2 = FloatList(initial_len=num_pairs)

        num_pairs = 0
        for i from 0 <= i < V_F:
            #self.f_index[i] = num_pairs
            self.f_index.set(i, num_pairs)
            if dict[i] != NULL:
                self._add_node(dict[i], &num_pairs, float(fmargin[i]), emargin)
                del_node(dict[i])
        free(fmargin)
        free(emargin)
        free(dict)
        return


    cdef _add_node(self, _node* n, int* num_pairs, float fmargin, int* emargin):
        cdef int loc
        if n.smaller != NULL:
            self._add_node(n.smaller, num_pairs, fmargin, emargin)
        loc = num_pairs[0]
        self.e_index.set(loc, n.key)
        self.col1.set(loc, float(n.val)/fmargin)
        self.col2.set(loc, float(n.val)/float(emargin[n.key]))
        num_pairs[0] = loc + 1
        if n.bigger != NULL:
            self._add_node(n.bigger, num_pairs, fmargin, emargin)


    def write_binary(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "w")
        self.f_index.write_handle(f)
        self.e_index.write_handle(f)
        self.col1.write_handle(f)
        self.col2.write_handle(f)
        self.write_wordlist(self.id2fword, f)
        self.write_wordlist(self.id2eword, f)
        fclose(f)


    cdef write_wordlist(self, wordlist, FILE* f):
        cdef int word_len
        cdef int num_words

        num_words = len(wordlist)
        fwrite(&(num_words), sizeof(int), 1, f)
        for word in wordlist:
            word_len = len(word) + 1
            fwrite(&(word_len), sizeof(int), 1, f)
            fwrite(<char *>word, sizeof(char), word_len, f)


    cdef read_wordlist(self, word2id, id2word, FILE* f):
        cdef int num_words
        cdef int word_len
        cdef char* word

        fread(&(num_words), sizeof(int), 1, f)
        for i from 0 <= i < num_words:
            fread(&(word_len), sizeof(int), 1, f)
            word = <char*> malloc (word_len * sizeof(char))
            fread(word, sizeof(char), word_len, f)
            word2id[word] = len(id2word)
            id2word.append(word)
            free(word)

    def read_binary(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "r")
        self.f_index.read_handle(f)
        self.e_index.read_handle(f)
        self.col1.read_handle(f)
        self.col2.read_handle(f)
        self.read_wordlist(self.fword2id, self.id2fword, f)
        self.read_wordlist(self.eword2id, self.id2eword, f)
        fclose(f)


    def get_e_id(self, eword):
        if eword not in self.eword2id:
            e_id = len(self.id2eword)
            self.id2eword.append(eword)
            self.eword2id[eword] = e_id
        return self.eword2id[eword]


    def get_f_id(self, fword):
        if fword not in self.fword2id:
            f_id = len(self.id2fword)
            self.id2fword.append(fword)
            self.fword2id[fword] = f_id
        return self.fword2id[fword]


    def read_text(self, char* filename):
        cdef i, j, w, e_id, f_id, n_f, n_e, N
        cdef IntList fcount 

        fcount = IntList()
        with gzip_or_text(filename) as f:
            # first loop merely establishes size of array objects
            for line in f:
                (fword, eword, score1, score2) = line.split()
                f_id = self.get_f_id(fword)
                e_id = self.get_e_id(eword)
                while f_id >= len(fcount):
                    fcount.append(0)
                fcount.arr[f_id] = fcount.arr[f_id] + 1

            # Allocate space for dictionary in arrays
            N = 0
            n_f = len(fcount)
            self.f_index = IntList(initial_len=n_f+1)
            for i from 0 <= i < n_f:
                self.f_index.arr[i] = N
                N = N + fcount.arr[i]
                fcount.arr[i] = 0
            self.f_index.arr[n_f] = N
            self.e_index = IntList(initial_len=N)
            self.col1 = FloatList(initial_len=N)
            self.col2 = FloatList(initial_len=N)

            # Re-read file, placing words into buckets
            f.seek(0)
            for line in f:
                (fword, eword, score1, score2) = line.split()
                f_id = self.get_f_id(fword)
                e_id = self.get_e_id(eword)
                index = self.f_index.arr[f_id] + fcount.arr[f_id]
                fcount.arr[f_id] = fcount.arr[f_id] + 1
                self.e_index.arr[index] = int(e_id)
                self.col1[index] = float(score1)
                self.col2[index] = float(score2)

        # Sort buckets by eword
        for b from 0 <= b < n_f:
            i = self.f_index.arr[b]
            j = self.f_index.arr[b+1]
            self.qsort(i,j, "")


    cdef swap(self, int i, int j):
        cdef int itmp
        cdef float ftmp

        if i == j:
            return

        itmp = self.e_index.arr[i]
        self.e_index.arr[i] = self.e_index.arr[j]
        self.e_index.arr[j] = itmp

        ftmp = self.col1.arr[i]
        self.col1.arr[i] = self.col1.arr[j]
        self.col1.arr[j] = ftmp

        ftmp = self.col2.arr[i]
        self.col2.arr[i] = self.col2.arr[j]
        self.col2.arr[j] = ftmp


    cdef qsort(self, int i, int j, pad):
        cdef int pval, p

        if i > j:
            raise Exception("Sort error in CLex")
        if i == j: #empty interval
            return
        if i == j-1: # singleton interval
            return

        p = (i+j)/2
        pval = self.e_index.arr[p]
        self.swap(i, p)
        p = i
        for k from i+1 <= k < j:
            if pval >= self.e_index.arr[k]:
                self.swap(p+1, k)
                self.swap(p, p+1)
                p = p + 1
        self.qsort(i,p, pad+"    ")
        self.qsort(p+1,j, pad+"    ")


    def write_enhanced(self, char* filename):
        with open(filename, "w") as f:
            for i in self.f_index:
                f.write("%d " % i)
            f.write("\n")
            for i, s1, s2 in zip(self.e_index, self.col1, self.col2):
                f.write("%d %f %f " % (i, s1, s2))
            f.write("\n")
            for i, w in enumerate(self.id2fword):
                f.write("%d %s " % (i, w))
            f.write("\n")
            for i, w in enumerate(self.id2eword):
                f.write("%d %s " % (i, w))
            f.write("\n")


    def get_score(self, fword, eword, col):
        cdef e_id, f_id, low, high, midpoint, val

        if eword not in self.eword2id:
            return None
        if fword not in self.fword2id:
            return None
        f_id = self.fword2id[fword]
        e_id = self.eword2id[eword]
        low = self.f_index.arr[f_id]
        high = self.f_index.arr[f_id+1]
        while high - low > 0:
            midpoint = (low+high)/2
            val = self.e_index.arr[midpoint]
            if val == e_id:
                if col == 0:
                    return self.col1.arr[midpoint]
                if col == 1:
                    return self.col2.arr[midpoint]
            if val > e_id:
                high = midpoint
            if val < e_id:
                low = midpoint + 1
        return None


    def write_text(self, char* filename):
        """Note: does not guarantee writing the dictionary in the original order"""
        cdef i, N, e_id, f_id
        
        with open(filename, "w") as f:
            N = len(self.e_index)
            f_id = 0
            for i from 0 <= i < N:
                while self.f_index.arr[f_id+1] == i:
                    f_id = f_id + 1
                e_id = self.e_index.arr[i]
                score1 = self.col1.arr[i]
                score2 = self.col2.arr[i]
                f.write("%s %s %.6f %.6f\n" % (self.id2fword[f_id], self.id2eword[e_id], score1, score2))
