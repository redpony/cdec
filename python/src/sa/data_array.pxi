# Defines "data arrays" that can be directly written to/read from disk in binary format
# In particular, the array itself is written/read directly as a glob of binary data
# Adam Lopez <alopez@cs.umd.edu>

from libc.stdio cimport FILE, fopen, fread, fwrite, fclose
from libc.stdlib cimport malloc, realloc, free
from libc.string cimport memset, strcpy, strlen

cdef class DataArray:
    cdef word2id
    cdef id2word
    cdef IntList data
    cdef IntList sent_id
    cdef IntList sent_index
    cdef bint use_sent_id

    def __cinit__(self, from_binary=None, from_text=None, side=None, bint use_sent_id=False):
        self.word2id = {"END_OF_FILE":0, "END_OF_LINE":1}
        self.id2word = ["END_OF_FILE", "END_OF_LINE"]
        self.data = IntList(1000,1000)
        self.sent_id = IntList(1000,1000)
        self.sent_index = IntList(1000,1000)
        self.use_sent_id = use_sent_id
        if from_binary:
            self.read_binary(from_binary)
        elif from_text:
            if side:
                self.read_bitext(from_text, (0 if side == 'source' else 1))
            else:
                self.read_text(from_text)

    def __len__(self):
        return len(self.data)

    def getSentId(self, i):
        return self.sent_id.arr[i]

    def getSent(self, i):
        cdef int j, start, stop
        sent = []
        start = self.sent_index.arr[i]
        stop = self.sent_index.arr[i+1]
        for i from start <= i < stop:
            sent.append(self.id2word[self.data.arr[i]])
        return sent

    def getSentPos(self, loc):
        return loc - self.sent_index.arr[self.sent_id.arr[loc]]

    def get_id(self, word):
        if not word in self.word2id:
            self.word2id[word] = len(self.id2word)
            self.id2word.append(word)
        return self.word2id[word]

    def get_word(self, id):
        return self.id2word[id]

    def write_text(self, char* filename):
        with open(filename, "w") as f:
            for w_id in self.data:
                if w_id > 1:
                    f.write("%s " % self.get_word(w_id))
                if w_id == 1:
                    f.write("\n")

    def read_text(self, char* filename):
        with gzip_or_text(filename) as fp:
            self.read_text_data(fp)

    def read_bitext(self, char* filename, int side):
        with gzip_or_text(filename) as fp:
            data = (line.split(' ||| ')[side] for line in fp)
            self.read_text_data(data)

    def read_text_data(self, data):
        cdef int word_count = 0
        for line_num, line in enumerate(data):
            self.sent_index.append(word_count)
            for word in line.split():
                self.data.append(self.get_id(word))
                if self.use_sent_id:
                    self.sent_id.append(line_num)
                word_count = word_count + 1
            self.data.append(1)
            if self.use_sent_id:
                self.sent_id.append(line_num)
            word_count = word_count + 1
        self.data.append(0)
        self.sent_index.append(word_count)


    def read_binary(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "r")
        self.read_handle(f)
        fclose(f)

    cdef void read_handle(self, FILE* f):
        cdef int num_words, word_len
        cdef unsigned i
        cdef char* c_word
        cdef bytes py_word
        self.data.read_handle(f)
        self.sent_index.read_handle(f)
        self.sent_id.read_handle(f)
        fread(&(num_words), sizeof(int), 1, f)
        for i in range(num_words):
            fread(&(word_len), sizeof(int), 1, f)
            c_word = <char*> malloc (word_len * sizeof(char))
            fread(c_word, sizeof(char), word_len, f)
            py_word = c_word
            free(c_word)
            self.word2id[py_word] = len(self.id2word)
            self.id2word.append(py_word)
        if len(self.sent_id) == 0:
            self.use_sent_id = False
        else:
            self.use_sent_id = True

    cdef void write_handle(self, FILE* f):
        cdef int word_len
        cdef int num_words
        cdef char* c_word
        self.data.write_handle(f)
        self.sent_index.write_handle(f)
        self.sent_id.write_handle(f)
        num_words = len(self.id2word) - 2
        fwrite(&(num_words), sizeof(int), 1, f)
        for word in self.id2word[2:]:
            c_word = word
            word_len = strlen(c_word) + 1
            fwrite(&(word_len), sizeof(int), 1, f)
            fwrite(c_word, sizeof(char), word_len, f)

    def write_binary(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "w")
        self.write_handle(f)
        fclose(f)

    def write_enhanced_handle(self, f):
        for i in self.data:
            f.write("%d " %i)
        f.write("\n")
        for i in self.sent_index:
            f.write("%d " %i)
        f.write("\n")
        for i in self.sent_id:
            f.write("%d " %i)
        f.write("\n")
        for word in self.id2word:
            f.write("%s %d " % (word, self.word2id[word]))
        f.write("\n")

    def write_enhanced(self, char* filename):
        with open(filename, "w") as f:
            self.write_enhanced_handle(self, f)
