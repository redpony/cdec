cdef class Vocabulary:
    cdef object id2word, word2id

    def __init__(self, from_vocabulary=None):
        self.id2word = []
        self.word2id = {}

    def extend(self, vocabulary):
        for word in vocabulary:
            self[word]

    def __iter__(self):
        return iter(self.id2word)

    def __getitem__(self, word):
        v = self.word2id.get(word, -1)
        if v == -1:
            v = len(self.id2word)
            self.id2word.append(word)
            self.word2id[word] = v
        return v

    def get(self, word, default):
        return self.word2id.get(word, default)

    def __len__(self):
        return len(self.id2word)

    cdef void write_handle(self, FILE* f, int offset=0):
        cdef int word_len
        cdef int num_words

        num_words = len(self.id2word) - offset
        fwrite(&(num_words), sizeof(int), 1, f)
        for word in self.id2word[offset:]:
            word_len = len(word) + 1
            fwrite(&(word_len), sizeof(int), 1, f)
            fwrite(<char *>word, sizeof(char), word_len, f)

    cdef void read_handle(self, FILE* f):
        cdef int num_words, word_len
        cdef char* word
        cdef unsigned i

        fread(&(num_words), sizeof(int), 1, f)
        for i in range(num_words):
            fread(&(word_len), sizeof(int), 1, f)
            word = <char*> malloc (word_len * sizeof(char))
            fread(word, sizeof(char), word_len, f)
            self.word2id[word] = len(self.id2word)
            self.id2word.append(word)
            free(word)

    cdef void read_mmaped(self, MemoryMap buf):
        cdef int num_words, word_len
        cdef char* word
        cdef unsigned i

        num_words = buf.read_int()
        for i in range(num_words):
            word_len = buf.read_int()
            word = buf.read_char_array(word_len)
            self.word2id[word] = len(self.id2word)
            self.id2word.append(word)
