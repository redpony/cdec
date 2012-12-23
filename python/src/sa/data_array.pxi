# Defines "data arrays" that can be directly written to/read from disk in binary format
# In particular, the array itself is written/read directly as a glob of binary data
# Adam Lopez <alopez@cs.umd.edu>

from libc.stdio cimport FILE, fopen, fread, fwrite, fclose
from libc.stdlib cimport malloc, realloc, free
from libc.string cimport memset, strcpy

# kept for compatibility
INIT_VOCABULARY = ('NULL', 'END_OF_LINE')

cdef class DataArray:
    cdef public Vocabulary voc
    cdef public IntList data
    cdef public IntList sent_id
    cdef public IntList sent_index
    cdef bint use_sent_id

    def __cinit__(self, from_binary=None, from_text=None, side=None,
            bint use_sent_id=False, mmaped=False):
        self.voc = Vocabulary()
        self.voc.extend(INIT_VOCABULARY)
        self.data = IntList(1000,1000)
        self.sent_id = IntList(1000,1000)
        self.sent_index = IntList(1000,1000)
        self.use_sent_id = use_sent_id
        if from_binary:
            if mmaped:
                self.read_mmaped(MemoryMap(from_binary))
            else:
                self.read_binary(from_binary)
        elif from_text:
            if side:
                self.read_bitext(from_text, (0 if side == 'source' else 1))
            else:
                self.read_text(from_text)

    def __len__(self):
        return len(self.data)

    def get_sentence_id(self, i):
        return self.sent_id.arr[i]

    def get_sentence(self, i):
        cdef int j, start, stop
        start = self.sent_index.arr[i]
        stop = self.sent_index.arr[i+1]
        sent = [self.voc.id2word[self.data.arr[j]] for j in range(start, stop)]

    def get_id(self, word):
        return self.voc[word]

    def __getitem__(self, loc):
        return self.voc.id2word[self.data.arr[loc]]

    def get_sentence_bounds(self, loc):
        cdef int sid = self.sent_id.arr[loc]
        return (self.sent_index.arr[sid], self.sent_index.arr[sid+1])

    def write_text(self, bytes filename):
        with open(filename, "w") as f:
            for w_id in self.data:
                if w_id > 1:
                    f.write("%s " % self.get_word(w_id))
                if w_id == 1:
                    f.write("\n")

    def read_text(self, bytes filename):
        with gzip_or_text(filename) as fp:
            self.read_text_data(fp)

    def read_bitext(self, bytes filename, int side):
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

    def read_binary(self, bytes filename):
        cdef FILE* f
        f = fopen(filename, "r")
        self.read_handle(f)
        fclose(f)

    cdef void read_mmaped(self, MemoryMap buf):
        self.data.read_mmaped(buf)
        self.sent_index.read_mmaped(buf)
        self.sent_id.read_mmaped(buf)
        self.voc.read_mmaped(buf)
        self.use_sent_id = (len(self.sent_id) > 0)

    cdef void read_handle(self, FILE* f):
        self.data.read_handle(f)
        self.sent_index.read_handle(f)
        self.sent_id.read_handle(f)
        self.voc.read_handle(f)
        self.use_sent_id = (len(self.sent_id) > 0)

    cdef void write_handle(self, FILE* f):
        self.data.write_handle(f)
        self.sent_index.write_handle(f)
        self.sent_id.write_handle(f)
        self.voc.write_handle(f, len(INIT_VOCABULARY))

    def write_binary(self, bytes filename):
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
        for w, word in enumerate(self.voc.id2word):
            f.write("%s %d " % (word, w))
        f.write("\n")

    def write_enhanced(self, bytes filename):
        with open(filename, "w") as f:
            self.write_enhanced_handle(self, f)
